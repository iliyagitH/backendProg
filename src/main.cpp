#define _CRT_SECURE_NO_WARNINGS

#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <cstring>

#include <GL/glew.h>
#include <SDL.h>
#include <SDL_opengl.h>
#include <imgui.h>
#include <implot.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>
#include <zmq.hpp>
#include "json.hpp"

using json = nlohmann::json;

struct CellRecord {
    std::string standard = "Unknown";
    std::string mcc, mnc;
    int pci = -1;
    int timingAdvance = 0;
    bool isRegistered = false;

    int rsrp = -140, rssi = 0, rsrq = 0, sinr = 0;
    int earfcn = -1, band = -1;
    std::string tac, cellId;

    int arfcn = -1, bsic = -1, dbm = -140;
    std::string lac;

    int nrarfcn = -1;
    int ssRsrp = -140, ssRsrq = 0, ssSinr = 0;
    std::string nci;
};

struct Telemetry {
    std::mutex mtx;
    double latitude = 0.0;
    double longitude = 0.0;
    double altitude = 0.0;
    std::string currentTime;

    std::vector<CellRecord> cells;

    std::vector<float> history_rsrp;
    std::vector<float> history_rssi;
    std::vector<float> history_sinr;
    std::vector<float> history_rsrq;
};

int getInt(const json& c, const char* key, int def) {
    if (!c.contains(key)) return def;
    try {
        if (c[key].is_number()) return c[key].get<int>();
        if (c[key].is_string()) {
            std::string s = c[key].get<std::string>();
            if (!s.empty()) return std::stoi(s);
        }
    } catch (...) {}
    return def;
}

std::string getStr(const json& c, const char* key) {
    if (!c.contains(key)) return "";
    try {
        if (c[key].is_string()) return c[key].get<std::string>();
        if (c[key].is_number()) return std::to_string(c[key].get<int>());
    } catch (...) {}
    return "";
}

CellRecord parseCell(const json& c) {
    CellRecord r;

    if (c.contains("isRegistered")) {
        try { r.isRegistered = c["isRegistered"].get<bool>(); } catch (...) {}
    }
    r.pci = getInt(c, "pci", -1);
    r.mcc = getStr(c, "mcc");
    r.mnc = getStr(c, "mnc");

    std::string type = getStr(c, "type");

    if (type == "LTE") {
        r.standard = "LTE";
        r.rsrp = getInt(c, "rsrp", -140);
        r.rssi = getInt(c, "rssi", 0);
        r.rsrq = getInt(c, "rsrq", 0);
        r.sinr = getInt(c, "rssnr", 0);
        r.earfcn = getInt(c, "earfcn", -1);
        r.tac = getStr(c, "tac");
        r.timingAdvance = getInt(c, "timingAdvance", 0);
    } else if (type == "GSM") {
        r.standard = "GSM";
        r.arfcn = getInt(c, "arfcn", -1);
        r.bsic = getInt(c, "bsic", -1);
        r.dbm = getInt(c, "dbm", -140);
        r.lac = getStr(c, "lac");
    } else if (type == "NR") {
        r.standard = "5G_NR";
        r.nrarfcn = getInt(c, "nrarfcn", -1);
        r.nci = getStr(c, "nci");
        r.ssRsrp = getInt(c, "ssRsrp", -140);
        r.ssRsrq = getInt(c, "ssRsrq", 0);
        r.ssSinr = getInt(c, "ssSinr", 0);
        r.rsrp = r.ssRsrp;
        r.sinr = r.ssSinr;
    }
    return r;
}

void run_server(Telemetry* data, std::atomic<bool>* stop) {
    zmq::context_t ctx(1);
    zmq::socket_t sock(ctx, zmq::socket_type::pull);
    sock.set(zmq::sockopt::rcvtimeo, 200);

    try {
        sock.bind("tcp://0.0.0.0:2222");
        std::cout << "[ZMQ] PULL слушает порт 2222\n";
    } catch (...) {
        std::cerr << "[ZMQ] Не удалось занять порт\n";
        return;
    }

    while (!stop->load()) {
        zmq::message_t msg;
        if (!sock.recv(msg, zmq::recv_flags::none)) continue;

        std::string raw(static_cast<char*>(msg.data()), msg.size());
        json j;
        try { j = json::parse(raw); }
        catch (...) { std::cerr << "[ZMQ] Невалидный JSON\n"; continue; }

        json entries;
        if (j.is_array()) entries = j;
        else { entries = json::array(); entries.push_back(j); }

        for (auto& entry : entries) {
            double lat = 0, lon = 0, alt = 0;
            std::string ct = "";

            if (entry.contains("location")) {
                lat = entry["location"].value("Latitude", 0.0);
                lon = entry["location"].value("Longitude", 0.0);
                alt = entry["location"].value("Altitude", 0.0);
                ct = entry["location"].value("Current Time", "");
            }

            std::vector<CellRecord> parsed;
            if (entry.contains("telephony") && entry["telephony"].is_array()) {
                for (auto& c : entry["telephony"]) {
                    parsed.push_back(parseCell(c));
                }
            }

            std::lock_guard<std::mutex> lk(data->mtx);
            data->latitude = lat;
            data->longitude = lon;
            data->altitude = alt;
            data->currentTime = ct;
            data->cells = parsed;

            for (auto& c : parsed) {
                if (c.isRegistered || parsed.size() == 1) {
                    data->history_rsrp.push_back((float)c.rsrp);
                    data->history_rssi.push_back((float)c.rssi);
                    data->history_sinr.push_back((float)c.sinr);
                    data->history_rsrq.push_back((float)c.rsrq);
                    break;
                }
            }

            std::ofstream out("server_log.json", std::ios::app);
            out << entry.dump() << "\n";
        }
    }
    std::cout << "[ZMQ] Сервер остановлен\n";
}

void drawChart(const char* label, std::vector<float>& series, ImVec4 col) {
    if (ImPlot::BeginPlot(label, ImVec2(-1, 150))) {
        ImPlot::SetupAxis(ImAxis_X1, nullptr, ImPlotAxisFlags_NoTickLabels);
        ImPlot::SetNextLineStyle(col, 1.5f);
        if (!series.empty()) {
            ImPlot::PlotLine("##l", series.data(), (int)series.size());
        }
        ImPlot::EndPlot();
    }
}

void run_gui(Telemetry* data, std::atomic<bool>* stop) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::cerr << "[GUI] Ошибка SDL_Init\n";
        return;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Telemetry Monitor",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1000, 700,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    SDL_GLContext glCtx = SDL_GL_CreateContext(window);
    SDL_GL_SetSwapInterval(1);
    glewInit();

    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF(
        "C:\\Windows\\Fonts\\arial.ttf", 18.0f, nullptr,
        io.Fonts->GetGlyphRangesCyrillic());

    ImGui_ImplSDL2_InitForOpenGL(window, glCtx);
    ImGui_ImplOpenGL3_Init("#version 130");

    while (!stop->load()) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            ImGui_ImplSDL2_ProcessEvent(&ev);
            if (ev.type == SDL_QUIT) stop->store(true);
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("##root", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);

        double lat, lon, alt;
        std::string ct;
        std::vector<float> rsrp, rssi, sinr, rsrq;
        std::vector<CellRecord> cells;
        {
            std::lock_guard<std::mutex> lk(data->mtx);
            lat = data->latitude;
            lon = data->longitude;
            alt = data->altitude;
            ct = data->currentTime;
            rsrp = data->history_rsrp;
            rssi = data->history_rssi;
            sinr = data->history_sinr;
            rsrq = data->history_rsrq;
            cells = data->cells;
        }

        ImGui::Text("Местоположение");
        ImGui::Text("Широта: %f", lat);
        ImGui::Text("Долгота: %f", lon);
        ImGui::Text("Высота: %f", alt);
        ImGui::Text("Время: %s", ct.c_str());

        ImGui::Separator();
        ImGui::Text("Графики уровня сигнала");
        drawChart("RSRP (dBm)", rsrp, ImVec4(1.0f, 0.4f, 0.1f, 1));
        drawChart("RSSI (dBm)", rssi, ImVec4(1.0f, 0.9f, 0.1f, 1));
        drawChart("SINR (dB)", sinr, ImVec4(0.2f, 1.0f, 0.3f, 1));
        drawChart("RSRQ (dB)", rsrq, ImVec4(0.2f, 0.7f, 1.0f, 1));

        ImGui::Separator();
        ImGui::Text("Соты: %d", (int)cells.size());
        if (ImGui::BeginTable("##cells", 5, ImGuiTableFlags_Borders)) {
            ImGui::TableSetupColumn("Стандарт");
            ImGui::TableSetupColumn("PCI");
            ImGui::TableSetupColumn("RSRP");
            ImGui::TableSetupColumn("RSSI");
            ImGui::TableSetupColumn("SINR");
            ImGui::TableHeadersRow();
            for (auto& c : cells) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("%s", c.standard.c_str());
                ImGui::TableNextColumn(); ImGui::Text("%d", c.pci);
                ImGui::TableNextColumn(); ImGui::Text("%d", c.rsrp);
                ImGui::TableNextColumn(); ImGui::Text("%d", c.rssi);
                ImGui::TableNextColumn(); ImGui::Text("%d", c.sinr);
            }
            ImGui::EndTable();
        }

        ImGui::End();

        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.08f, 0.08f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(glCtx);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

int main() {
    static Telemetry shared_data;
    std::atomic<bool> stopFlag{false};

    std::thread server_thread(run_server, &shared_data, &stopFlag);
    run_gui(&shared_data, &stopFlag);

    stopFlag.store(true);
    if (server_thread.joinable()) server_thread.join();
    return 0;
}
