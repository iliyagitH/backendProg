#pragma once
#include "common.h"
#include "telemetry.h"
#include "tile_manager.h"
#include "map_renderer.h"

namespace Gui {

    struct State {
        double mapLat = 55.0084;
        double mapLon = 82.9357;
        int    zoom   = 15;
        bool   centeredOnce = false;
    };

    bool init();
    void runLoop(State& st, Telemetry& data, TileManager& tm,
                 MapRenderer& mr, std::atomic<bool>& stopFlag);
    void shutdown();
}
