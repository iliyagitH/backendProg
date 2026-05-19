#include "gui.h"
#include <algorithm>
#include <cstdio>
#include <map>

namespace {

SDL_Window*   g_window = nullptr;
SDL_GLContext g_glCtx  = nullptr;

void drawMapPanel(Gui::State& st, Telemetry& data,
                  TileManager& tm, MapRenderer& mr) {
    {
        std::lock_guard<std::mutex> lk(data.mtx);
        if (!st.centeredOnce && data.latitude != 0.0) {
            st.mapLat = data.latitude;
            st.mapLon = data.longitude;
            st.centeredOnce = true;
        }
    }
    ImGui::Text("Карта  Zoom: %d  |  Колесо / [+/-]  |  ЛКМ — перемещение", st.zoom);

    if (ImPlot::BeginPlot("##map", ImVec2(-1, -1),
        ImPlotFlags_NoMouseText | ImPlotFlags_CanvasOnly))
    {
        mr.setupAxes(st.mapLat, st.mapLon, st.zoom);
        mr.handleInput(st.mapLat, st.mapLon, st.zoom);
        Telemetry snap;
        {
            std::lock_guard<std::mutex> lk(data.mtx);
            snap.latitude  = data.latitude;
            snap.longitude = data.longitude;
            snap.path_lat  = data.path_lat;
            snap.path_lon  = data.path_lon;
        }
        mr.render(tm, snap, st.zoom);
        ImPlot::EndPlot();
    }
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

void drawChartByPci(Telemetry& data, const char* label,
                    const std::vector<float>& series, float chartH) {
    std::vector<float> values, pcis;
    {
        std::lock_guard<std::mutex> lk(data.mtx);
        values = series;
        pcis = data.history_pci;
    }
    if (ImPlot::BeginPlot(label, ImVec2(-1, chartH))) {
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

void drawCharts(Telemetry& data) {
    float avail  = ImGui::GetContentRegionAvail().y;
    float chartH = avail * 0.17f;
    drawChartByPci(data, "RSRP (dBm)", data.history_rsrp, chartH);
    drawChartByPci(data, "RSSI (dBm)", data.history_rssi, chartH);
    drawChartByPci(data, "SINR (dB)",  data.history_sinr, chartH);
    drawChartByPci(data, "RSRQ (dB)",  data.history_rsrq, chartH);
}

void drawHeatmapPanel(Gui::State& st, HeatmapGenerator& hm) {
    ImGui::Separator();
    bool hmDirty = false;

    if (ImGui::CollapsingHeader("Тепловая карта (IDW)",
            ImGuiTreeNodeFlags_DefaultOpen))
    {

        const char* critNames[] = { "RSRP", "RSRQ", "RSSI", "Altitude" };
        if (ImGui::Combo("Критерий##hm", &st.hmCritIdx, critNames, 4))
            hmDirty = true;

        if (ImGui::SliderFloat("Радиус (м)##hm",
                &st.hmRadius, 10.0f, 500.0f, "%.0f м"))
            hmDirty = true;
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "Рекомендации по данным архива (медиана шага 4 м):\n"
                "  zoom 13 (весь город) : 300-500 м\n"
                "  zoom 15 (район)      : 100-200 м  ← дефолт 150 м\n"
                "  zoom 17 (улица)      :  20-50 м");

        {
            std::vector<int> earfcns = hm.availableEarfcns();
            std::vector<std::string> labels;
            labels.emplace_back("Все EARFCN");
            for (int e : earfcns) labels.push_back(std::to_string(e));
            std::vector<const char*> cstrs;
            for (const auto& s : labels) cstrs.push_back(s.c_str());
            if (st.hmEarfcnIdx >= (int)cstrs.size()) st.hmEarfcnIdx = 0;
            if (ImGui::Combo("EARFCN##hm", &st.hmEarfcnIdx,
                    cstrs.data(), (int)cstrs.size()))
                hmDirty = true;
        }

        {
            auto hist = hm.pciHistogram();
            std::vector<std::pair<int,size_t>> sorted(hist.begin(), hist.end());
            std::sort(sorted.begin(), sorted.end(),
                [](const auto& a, const auto& b){ return a.second > b.second; });

            if (st.hmFirstRun && !sorted.empty()) {
                st.hmSelectedPcis.clear();
                st.hmSelectedPcis.insert(sorted[0].first);
                st.hmFirstRun = false;
                hmDirty        = true;
            }

            ImGui::Text("PCI:");
            ImGui::SameLine();

            if (ImGui::SmallButton("Выбрать все##pci")) {
                st.hmSelectedPcis.clear();
                for (const auto& [pci, _] : sorted)
                    st.hmSelectedPcis.insert(pci);
                hmDirty = true;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Сбросить##pci")) {
                st.hmSelectedPcis.clear();
                hmDirty = true;
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(%zu / %zu)", st.hmSelectedPcis.size(), sorted.size());

            ImGui::BeginChild("##pci_list", ImVec2(0, 120), true);
            for (const auto& [pci, cnt] : sorted) {
                bool sel = st.hmSelectedPcis.count(pci) > 0;
                char lbl[64];
                std::snprintf(lbl, sizeof(lbl),
                    "PCI %d  (%zu pts)##pci%d", pci, cnt, pci);
                if (ImGui::Checkbox(lbl, &sel)) {
                    if (sel) st.hmSelectedPcis.insert(pci);
                    else     st.hmSelectedPcis.erase(pci);
                    hmDirty = true;
                }
            }
            ImGui::EndChild();
        }

        if (hm.busy())
            ImGui::ProgressBar(hm.progress() / 100.0f,
                ImVec2(-1, 0), "Вычисление...");
        else
            ImGui::ProgressBar(1.0f, ImVec2(-1, 0), "Готово");
        if (ImGui::Button("Регенерировать##hm")) hmDirty = true;

        ImGui::Spacing();
        ImGui::TextDisabled("RSRP:");
        ImGui::SameLine(); ImGui::TextColored({0.86f,0.12f,0.12f,1}, "■ >-80");
        ImGui::SameLine(); ImGui::TextColored({0.94f,0.78f,0.24f,1}, " ■ -90");
        ImGui::SameLine(); ImGui::TextColored({0.24f,0.78f,0.86f,1}, " ■ -100");
        ImGui::SameLine(); ImGui::TextColored({0.16f,0.24f,0.71f,1}, " ■ -110");
    }

    if (st.hmLastZoom != st.zoom) { st.hmLastZoom = st.zoom; hmDirty = true; }

    if (hmDirty && !hm.busy()) {
        std::vector<int> earfcns = hm.availableEarfcns();
        int earfcnFilter = (st.hmEarfcnIdx == 0 ||
                            st.hmEarfcnIdx > (int)earfcns.size())
                           ? -1 : earfcns[st.hmEarfcnIdx - 1];
        HeatmapGenerator::Settings s;
        s.criterion    = (HeatmapGenerator::Criterion)st.hmCritIdx;
        s.radiusMeters = st.hmRadius;
        s.zoom         = st.zoom;
        s.selectedPcis = st.hmSelectedPcis;
        s.earfcn       = earfcnFilter;
        hm.updateSettings(s);
        hm.requestRegenerate();
    }
}

void drawCellsTable(Telemetry& data) {
    ImGui::Separator();
    ImGui::Text("Соты (%zu):", data.cells.size());

    std::vector<CellRecord> cellsCopy;
    { std::lock_guard<std::mutex> lk(data.mtx); cellsCopy = data.cells; }

    if (ImGui::BeginTable("##cells", 6,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_ScrollY, ImVec2(-1, 0)))
    {
        ImGui::TableSetupColumn("Стандарт");
        ImGui::TableSetupColumn("PCI");
        ImGui::TableSetupColumn("RSRP");
        ImGui::TableSetupColumn("RSSI");
        ImGui::TableSetupColumn("SINR");
        ImGui::TableSetupColumn("Детали");
        ImGui::TableHeadersRow();

        for (const auto& c : cellsCopy) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextUnformatted(c.standard.c_str());
            ImGui::TableNextColumn(); ImGui::Text("%d", c.pci);
            ImGui::TableNextColumn(); ImGui::Text("%d", c.rsrp);
            ImGui::TableNextColumn(); ImGui::Text("%d", c.rssi);
            ImGui::TableNextColumn(); ImGui::Text("%d", c.sinr);
            ImGui::TableNextColumn();
            if (c.standard == "LTE")
                ImGui::Text("EARFCN:%d B:%d TAC:%s TA:%d",
                    c.earfcn, c.band, c.tac.c_str(), c.timingAdvance);
            else if (c.standard == "GSM")
                ImGui::Text("ARFCN:%d BSIC:%d LAC:%s dBm:%d TA:%d",
                    c.arfcn, c.bsic, c.lac.c_str(), c.dbm, c.timingAdvance);
            else if (c.standard == "5G_NR")
                ImGui::Text("NRARFCN:%d ssRSRP:%d ssRSRQ:%d ssSINR:%d NCI:%s",
                    c.nrarfcn, c.ssRsrp, c.ssRsrq, c.ssSinr, c.nci.c_str());
        }
        ImGui::EndTable();
    }
}

}

bool Gui::init() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::cerr << "[GUI] Ошибка SDL_Init: " << SDL_GetError() << "\n";
        return false;
    }

    g_window = SDL_CreateWindow(
        "Telemetry Monitor",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1500, 850,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!g_window) {
        std::cerr << "[GUI] Ошибка SDL_CreateWindow: " << SDL_GetError() << "\n";
        return false;
    }

    g_glCtx = SDL_GL_CreateContext(g_window);
    SDL_GL_SetSwapInterval(1);
    glewInit();

    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF(
        "C:\\Windows\\Fonts\\arial.ttf", 18.0f, nullptr,
        io.Fonts->GetGlyphRangesCyrillic());
    ImGui_ImplSDL2_InitForOpenGL(g_window, g_glCtx);
    ImGui_ImplOpenGL3_Init("#version 130");
    return true;
}

void Gui::runLoop(State& st, Telemetry& data, TileManager& tm,
                  MapRenderer& mr, HeatmapGenerator& hm,
                  std::atomic<bool>& stopFlag) {
    ImGuiIO& io = ImGui::GetIO();

    while (!stopFlag.load()) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            ImGui_ImplSDL2_ProcessEvent(&ev);
            if (ev.type == SDL_QUIT) stopFlag.store(true);
        }

        tm.uploadPending();
        if (hm.consumeUpdatedFlag())
            mr.invalidateHeatmap();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("##root", nullptr,
            ImGuiWindowFlags_NoTitleBar  |
            ImGuiWindowFlags_NoResize    |
            ImGuiWindowFlags_NoBringToFrontOnFocus);

        ImGui::Columns(2, "split");
        ImGui::SetColumnWidth(0, ImGui::GetWindowWidth() * 0.55f);

        drawMapPanel(st, data, tm, mr);

        ImGui::NextColumn();

        drawCharts(data);
        drawHeatmapPanel(st, hm);
        drawCellsTable(data);

        ImGui::Columns(1);
        ImGui::End();

        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.08f, 0.08f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(g_window);
    }
}

void Gui::shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(g_glCtx);
    SDL_DestroyWindow(g_window);
    SDL_Quit();
}
