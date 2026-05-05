#define _CRT_SECURE_NO_WARNINGS

#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <string>
#include <map>
#include <iostream>
#include <fstream>
#include <cstring>
#include <chrono>
#include <ctime>

#include <GL/glew.h>
#include <SDL.h>
#include <SDL_opengl.h>
#include <imgui.h>
#include <implot.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>
#include <zmq.hpp>
#include "json.hpp"

#include "telemetry.h"
#include "database.h"

using json = nlohmann::json;

int getInt(const json& c, std::initializer_list<const char*> keys, int def) {
    for (const char* k : keys) {
        if (!c.contains(k)) continue;
        try {
            int val = def;
            if (c[k].is_number_integer()) val = c[k].get<int>();
            else if (c[k].is_number_float()) val = (int)c[k].get<double>();
            else if (c[k].is_string()) {
                std::string s = c[k].get<std::string>();
                if (!s.empty()) val = std::stoi(s);
                else continue;
            } else continue;

            if (val == 2147483647 || val == -2147483648) return def;
            return val;
        } catch (...) {}
    }
    return def;
}

std::string getStr(const json& c, std::initializer_list<const char*> keys) {
    for (const char* k : keys) {
        if (!c.contains(k)) continue;
        try {
            if (c[k].is_string()) return c[k].get<std::string>();
            if (c[k].is_number_integer()) return std::to_string(c[k].get<int>());
        } catch (...) {}
    }
    return "";
}

CellRecord parseCell(const json& c) {
    CellRecord r;

    for (const char* k : {"isRegistered"}) {
        if (c.contains(k)) {
            try { r.isRegistered = c[k].get<bool>(); } catch (...) {}
        }
    }
    r.pci = getInt(c, {"pci", "PCI"}, -1);
    r.mcc = getStr(c, {"mcc", "MCC"});
    r.mnc = getStr(c, {"mnc", "MNC"});

    std::string type = getStr(c, {"type", "Type"});

    if (type == "LTE" || type == "CellInfoLte") {
        r.standard = "LTE";
        r.rsrp = getInt(c, {"rsrp", "RSRP"}, -140);
        r.rssi = getInt(c, {"rssi", "RSSI"}, 0);
        r.rsrq = getInt(c, {"rsrq", "RSRQ"}, 0);
        r.sinr = getInt(c, {"sinr", "rssnr", "RSSNR"}, 0);
        r.earfcn = getInt(c, {"earfcn", "EARFCN"}, -1);
        r.tac = getStr(c, {"tac", "TAC"});
        r.timingAdvance = getInt(c, {"timingAdvance", "Timing Advance"}, 0);
    } else if (type == "GSM" || type == "CellInfoGsm") {
        r.standard = "GSM";
        r.arfcn = getInt(c, {"arfcn", "ARFCN"}, -1);
        r.bsic = getInt(c, {"bsic", "BSIC"}, -1);
        r.dbm = getInt(c, {"dbm", "DBM"}, -140);
        r.lac = getStr(c, {"lac", "LAC"});
        r.rssi = getInt(c, {"rssi", "RSSI"}, 0);
    } else if (type == "NR" || type == "5G_NR" || type == "CellInfoNr") {
        r.standard = "5G_NR";
        r.nrarfcn = getInt(c, {"nrarfcn", "NRARFCN"}, -1);
        r.nci = getStr(c, {"nci", "NCI"});
        r.ssRsrp = getInt(c, {"ssRsrp", "SSRsrp"}, -140);
        r.ssRsrq = getInt(c, {"ssRsrq", "SSRsrq"}, 0);
        r.ssSinr = getInt(c, {"ssSinr", "SSSinr"}, 0);
        r.rsrp = r.ssRsrp;
        r.sinr = r.ssSinr;
    }
    return r;
}

long long parseTime(const std::string& ct) {
    long long ts = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    if (ct.empty()) return ts;

    char dow[8] = {}, mon[8] = {};
    int day = 0, hour = 0, min = 0, sec = 0, year = 0;
    int n = sscanf(ct.c_str(), "%7s %7s %d %d:%d:%d %*s %d",
                   dow, mon, &day, &hour, &min, &sec, &year);
    if (n == 7) {
        const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                "Jul","Aug","Sep","Oct","Nov","Dec"};
        int month = -1;
        for (int i = 0; i < 12; i++) {
            if (strcmp(mon, months[i]) == 0) { month = i; break; }
        }
        if (month >= 0 && year > 2000) {
            struct tm tm_val = {};
            tm_val.tm_year = year - 1900;
            tm_val.tm_mon = month;
            tm_val.tm_mday = day;
            tm_val.tm_hour = hour;
            tm_val.tm_min = min;
            tm_val.tm_sec = sec;
            tm_val.tm_isdst = -1;
            time_t t = mktime(&tm_val);
            if (t != -1) ts = (long long)t * 1000LL;
        }
    }
    return ts;
}

void run_server(Telemetry* data, Database* db, std::atomic<bool>* stop) {
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

            long long ts = parseTime(ct);

            std::vector<CellRecord> parsed;
            if (entry.contains("telephony")) {
                if (entry["telephony"].is_array()) {
                    for (auto& c : entry["telephony"]) {
                        parsed.push_back(parseCell(c));
                    }
                } else if (entry["telephony"].is_object()) {
                    parsed.push_back(parseCell(entry["telephony"]));
                }
            }

            {
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
                        data->history_pci.push_back((float)c.pci);
                        break;
                    }
                }
            }

            std::ofstream out("server_log.json", std::ios::app);
            out << entry.dump() << "\n";

            if (db->isConnected() && !parsed.empty()) {
                for (auto& cell : parsed) {
                    db->insertRecord(lat, lon, alt, cell, ts);
                }
            }
        }
    }
    std::cout << "[ZMQ] Сервер остановлен\n";
}

ImVec4 pciColor(int pci) {
    float h = (float)((pci * 47) % 360) / 360.0f;
    float r, g, b;
    int i = (int)(h * 6.0f);
    float f = h * 6.0f - i;
    float q = 1.0f - f;
    switch (i % 6) {
        case 0: r = 1; g = f; b = 0; break;
        case 1: r = q; g = 1; b = 0; break;
        case 2: r = 0; g = 1; b = f; break;
        case 3: r = 0; g = q; b = 1; break;
        case 4: r = f; g = 0; b = 1; break;
        default: r = 1; g = 0; b = q; break;
    }
    return ImVec4(r, g, b, 1.0f);
}

void drawChartByPci(const char* label, Telemetry* data,
                    std::vector<float>& series) {
    std::vector<float> values, pcis;
    {
        std::lock_guard<std::mutex> lk(data->mtx);
        values = series;
        pcis = data->history_pci;
    }

    if (ImPlot::BeginPlot(label, ImVec2(-1, 160))) {
        ImPlot::SetupAxis(ImAxis_X1, nullptr, ImPlotAxisFlags_NoTickLabels);

        std::map<int, std::vector<float>> byPci;
        std::map<int, std::vector<float>> byPciX;
        for (size_t i = 0; i < values.size() && i < pcis.size(); i++) {
            int pci = (int)pcis[i];
            float v = values[i];
            if (v < -200.0f || v > 200.0f) continue;
            byPci[pci].push_back(v);
            byPciX[pci].push_back((float)i);
        }

        for (auto& pair : byPci) {
            int pci = pair.first;
            std::string name = "PCI " + std::to_string(pci);
            ImPlot::SetNextLineStyle(pciColor(pci), 1.5f);
            ImPlot::PlotLine(name.c_str(),
                byPciX[pci].data(), pair.second.data(),
                (int)pair.second.size());
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
        1000, 750,
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
        std::vector<CellRecord> cells;
        {
            std::lock_guard<std::mutex> lk(data->mtx);
            lat = data->latitude;
            lon = data->longitude;
            alt = data->altitude;
            ct = data->currentTime;
            cells = data->cells;
        }

        ImGui::Text("Местоположение");
        ImGui::Text("Широта: %f", lat);
        ImGui::Text("Долгота: %f", lon);
        ImGui::Text("Высота: %f", alt);
        ImGui::Text("Время: %s", ct.c_str());

        ImGui::Separator();
        ImGui::Text("Графики уровня сигнала (по PCI)");
        drawChartByPci("RSRP (dBm)", data, data->history_rsrp);
        drawChartByPci("RSSI (dBm)", data, data->history_rssi);
        drawChartByPci("SINR (dB)", data, data->history_sinr);

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

void load_history(Telemetry* data, Database* db) {
    if (!db->isConnected()) {
        std::cout << "[Startup] БД недоступна\n";
        return;
    }
    auto rows = db->loadHistory();
    std::lock_guard<std::mutex> lk(data->mtx);
    for (auto& r : rows) {
        data->latitude = r.lat;
        data->longitude = r.lon;
        data->altitude = r.alt;
        data->history_rsrp.push_back(r.rsrp);
        data->history_rssi.push_back(r.rssi);
        data->history_sinr.push_back(r.sinr);
        data->history_rsrq.push_back(r.rsrq);
        data->history_pci.push_back((float)r.pci);
    }
}

int main() {
    static Telemetry shared_data;
    std::atomic<bool> stopFlag{false};

    Database::Config dbCfg;
    Database db(dbCfg);

    load_history(&shared_data, &db);

    std::thread server_thread(run_server, &shared_data, &db, &stopFlag);
    run_gui(&shared_data, &stopFlag);

    stopFlag.store(true);
    if (server_thread.joinable()) server_thread.join();
    return 0;
}