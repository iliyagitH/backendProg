#pragma once
#include "common.h"
#include "telemetry.h"
#include "tile_manager.h"

class MapRenderer {
public:
    void setupAxes(double centerLat, double centerLon, int zoom);
    void handleInput(double& centerLat, double& centerLon, int& zoom);
    void render(TileManager& tm, const Telemetry& data, int zoom);

private:
    int lastTileCount_ = -1;
};
