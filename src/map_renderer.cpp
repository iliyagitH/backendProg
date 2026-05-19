#include "stb_image.h"
#include "map_renderer.h"
#include "math_osm.h"

void MapRenderer::setupAxes(double centerLat, double centerLon, int zoom) {
    double cx = OSM::lon2tile(centerLon, zoom);
    double cy = OSM::lat2tile(centerLat, zoom);

    ImVec2 avail = ImGui::GetContentRegionAvail();
    double halfX = (avail.x > 0) ? (avail.x / TILE_SIZE / 2.0) : 1.5;
    double halfY = (avail.y > 0) ? (avail.y / TILE_SIZE / 2.0) : 1.5;

    ImPlot::SetupAxis(ImAxis_X1, nullptr, ImPlotAxisFlags_NoDecorations);
    ImPlot::SetupAxis(ImAxis_Y1, nullptr,
        ImPlotAxisFlags_NoDecorations | ImPlotAxisFlags_Invert);
    ImPlot::SetupAxisLimits(ImAxis_X1, cx - halfX, cx + halfX, ImGuiCond_Always);
    ImPlot::SetupAxisLimits(ImAxis_Y1, cy - halfY, cy + halfY, ImGuiCond_Always);
}

void MapRenderer::handleInput(double& centerLat, double& centerLon, int& zoom) {
    if (ImPlot::IsPlotHovered()) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel > 0) zoom = std::min(MAX_ZOOM, zoom + 1);
        if (wheel < 0) zoom = std::max(MIN_ZOOM, zoom - 1);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Equal)) zoom = std::min(MAX_ZOOM, zoom + 1);
    if (ImGui::IsKeyPressed(ImGuiKey_Minus)) zoom = std::max(MIN_ZOOM, zoom - 1);

    if (ImPlot::IsPlotHovered() &&
        ImGui::IsMouseDragging(ImGuiMouseButton_Left, 1.0f))
    {
        ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 1.0f);
        ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
        ImPlotRect lims = ImPlot::GetPlotLimits();
        ImVec2 plotSize = ImPlot::GetPlotSize();
        if (plotSize.x > 0 && plotSize.y > 0) {
            double visW   = lims.X.Max - lims.X.Min;
            double visH   = lims.Y.Max - lims.Y.Min;
            double shiftX = delta.x / plotSize.x * visW;
            double shiftY = delta.y / plotSize.y * visH;
            double newTX  = OSM::lon2tile(centerLon, zoom) - shiftX;
            double newTY  = OSM::lat2tile(centerLat, zoom) - shiftY;
            centerLon = OSM::tile2lon(newTX, zoom);
            centerLat = OSM::tile2lat(newTY, zoom);
        }
    }
}

void MapRenderer::render(TileManager& tm, const Telemetry& data, int zoom) {
    ImPlotRect lims = ImPlot::GetPlotLimits();
    int maxIdx = (1 << zoom) - 1;

    int x0 = std::max(0,      (int)std::floor(lims.X.Min));
    int x1 = std::min(maxIdx, (int)std::floor(lims.X.Max));
    int y0 = std::max(0,      (int)std::floor(lims.Y.Min));
    int y1 = std::min(maxIdx, (int)std::floor(lims.Y.Max));

    int count = (x1 - x0 + 1) * (y1 - y0 + 1);
    if (count != lastTileCount_) {
        lastTileCount_ = count;
        std::cout << "[MapRenderer] Тайлов: " << count << "\n";
    }

    for (int tx = x0; tx <= x1; ++tx) {
        for (int ty = y0; ty <= y1; ++ty) {
            tm.requestTile(zoom, tx, ty);
            GLuint tex = tm.getTexture(zoom, tx, ty);
            if (tex) {
                ImPlot::PlotImage("##tile",
                    (void*)(intptr_t)tex,
                    ImVec2((double)tx,       (double)(ty + 1)),
                    ImVec2((double)(tx + 1), (double)ty));
            }
        }
    }

    if (data.path_lon.size() > 1) {
        std::vector<double> px, py;
        px.reserve(data.path_lon.size());
        py.reserve(data.path_lat.size());
        for (size_t i = 0; i < data.path_lon.size(); ++i) {
            px.push_back(OSM::lon2tile(data.path_lon[i], zoom));
            py.push_back(OSM::lat2tile(data.path_lat[i], zoom));
        }
        ImPlot::SetNextLineStyle(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), 2.5f);
        ImPlot::PlotLine("##track", px.data(), py.data(), (int)px.size());
    }

    if (data.latitude != 0.0 && data.longitude != 0.0) {
        double curX = OSM::lon2tile(data.longitude, zoom);
        double curY = OSM::lat2tile(data.latitude,  zoom);
        ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 9,
            ImVec4(0.1f, 0.4f, 1.0f, 1.0f), 2.0f,
            ImVec4(0.1f, 0.4f, 1.0f, 0.5f));
        ImPlot::PlotScatter("##pos", &curX, &curY, 1);
    }

    renderHeatmapOverlay(zoom, lims, maxIdx);
}

void MapRenderer::invalidateHeatmap() {
    std::lock_guard<std::mutex> lk(hmMtx_);
    for (auto& [key, tex] : hmCache_) {
        if (tex) glDeleteTextures(1, &tex);
    }
    hmCache_.clear();
    hmMissing_.clear();
    std::cout << "[MapRenderer] Heatmap-кеш сброшен\n";
}

void MapRenderer::renderHeatmapOverlay(int zoom, const ImPlotRect& lims, int maxIdx) {
    int x0 = std::max(0,      (int)std::floor(lims.X.Min));
    int x1 = std::min(maxIdx, (int)std::floor(lims.X.Max));
    int y0 = std::max(0,      (int)std::floor(lims.Y.Min));
    int y1 = std::min(maxIdx, (int)std::floor(lims.Y.Max));

    for (int tx = x0; tx <= x1; ++tx) {
        for (int ty = y0; ty <= y1; ++ty) {
            std::string key =
                std::to_string(zoom) + "_" +
                std::to_string(tx)   + "_" +
                std::to_string(ty);

            std::lock_guard<std::mutex> lk(hmMtx_);

            if (hmMissing_.count(key)) continue;

            auto it = hmCache_.find(key);
            if (it != hmCache_.end() && it->second != 0) {
                ImPlot::PlotImage("##hm",
                    (void*)(intptr_t)it->second,
                    ImVec2((double)tx,       (double)(ty + 1)),
                    ImVec2((double)(tx + 1), (double)ty),
                    ImVec2(0, 0), ImVec2(1, 1),
                    ImVec4(1, 1, 1, 0.6f));
                continue;
            }

            std::string path =
                "build/" + std::to_string(zoom) + "/" +
                std::to_string(tx) + "/" +
                std::to_string(ty) + "/heatmap.png";

            if (!fs::exists(path)) { hmMissing_.insert(key); continue; }

            int w, h, ch;
            uint8_t* img = stbi_load(path.c_str(), &w, &h, &ch, 4);
            if (!img) { hmMissing_.insert(key); continue; }

            GLuint tex = 0;
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                         w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);
            stbi_image_free(img);
            hmCache_[key] = tex;

            ImPlot::PlotImage("##hm",
                (void*)(intptr_t)tex,
                ImVec2((double)tx,       (double)(ty + 1)),
                ImVec2((double)(tx + 1), (double)ty),
                ImVec2(0, 0), ImVec2(1, 1),
                ImVec4(1, 1, 1, 0.6f));
        }
    }
}