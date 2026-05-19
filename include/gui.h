#pragma once
#include "common.h"
#include "telemetry.h"
#include "tile_manager.h"
#include "map_renderer.h"
#include "heatmap_generator.h"

namespace Gui {

    struct State {

        double mapLat = 55.0084;
        double mapLon = 82.9357;
        int    zoom   = 15;

        int    hmCritIdx    = 0;
        float  hmRadius     = 150.0f;
        int    hmEarfcnIdx  = 0;
        std::unordered_set<int> hmSelectedPcis;
        int    hmLastZoom   = -1;
        bool   hmFirstRun   = true;

        bool   centeredOnce = false;
    };

    bool init();

    void runLoop(State& st, Telemetry& data, TileManager& tm,
                 MapRenderer& mr, HeatmapGenerator& hm,
                 std::atomic<bool>& stopFlag);

    void shutdown();
}
