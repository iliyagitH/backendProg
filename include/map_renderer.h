#pragma once
#include "common.h"
#include "telemetry.h"
#include "tile_manager.h"

class MapRenderer {
public:
    void setupAxes(double centerLat, double centerLon, int zoom);
    void handleInput(double& centerLat, double& centerLon, int& zoom);
    void render(TileManager& tm, const Telemetry& data, int zoom);

    void invalidateHeatmap();

private:
    void renderHeatmapOverlay(int zoom, const ImPlotRect& lims, int maxIdx);

    int lastTileCount_ = -1;
    std::unordered_map<std::string, GLuint> hmCache_;
    std::unordered_set<std::string>         hmMissing_;
    std::mutex                              hmMtx_;
};