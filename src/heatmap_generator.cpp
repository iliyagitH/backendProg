
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "heatmap_generator.h"
#include "math_osm.h"
#include <algorithm>

HeatmapGenerator::HeatmapGenerator(std::string buildDir)
    : buildDir_(std::move(buildDir))
{
    fs::create_directories(buildDir_);
    worker_ = std::thread(&HeatmapGenerator::workerLoop, this);
}

HeatmapGenerator::~HeatmapGenerator() {
    running_.store(false);
    if (worker_.joinable()) worker_.join();
}

void HeatmapGenerator::setData(std::vector<Point> pts) {
    std::lock_guard<std::mutex> lk(dataMtx_);
    points_ = std::move(pts);
}

HeatmapGenerator::Settings HeatmapGenerator::settings() const {
    std::lock_guard<std::mutex> lk(settingsMtx_);
    return settings_;
}

void HeatmapGenerator::updateSettings(const Settings& s) {
    std::lock_guard<std::mutex> lk(settingsMtx_);
    settings_ = s;
}

void HeatmapGenerator::requestRegenerate() {
    requested_.store(true);
}

bool HeatmapGenerator::consumeUpdatedFlag() {
    return updated_.exchange(false);
}

std::unordered_map<int, size_t> HeatmapGenerator::pciHistogram() const {
    std::lock_guard<std::mutex> lk(dataMtx_);
    std::unordered_map<int, size_t> h;
    for (const auto& p : points_) {
        if (p.pci >= 0) ++h[p.pci];
    }
    return h;
}

int HeatmapGenerator::dominantPci() const {
    auto h = pciHistogram();
    int best = -1;
    size_t bestCnt = 0;
    for (auto& [pci, cnt] : h) {
        if (cnt > bestCnt) { bestCnt = cnt; best = pci; }
    }
    return best;
}

std::vector<int> HeatmapGenerator::availableEarfcns() const {
    std::lock_guard<std::mutex> lk(dataMtx_);
    std::unordered_set<int> s;
    for (const auto& p : points_) {
        if (p.earfcn > 0) s.insert(p.earfcn);
    }
    std::vector<int> out(s.begin(), s.end());
    std::sort(out.begin(), out.end());
    return out;
}

void HeatmapGenerator::workerLoop() {
    while (running_.load()) {

        if (!requested_.exchange(false)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        Settings s = settings();
        busy_.store(true);
        progress_.store(0);

        try {
            doGenerate(s);
        } catch (const std::exception& e) {
            std::cerr << "[Heatmap] Ошибка генерации: " << e.what() << "\n";
        }

        busy_.store(false);
        progress_.store(100);
        updated_.store(true);
    }
}

void HeatmapGenerator::clearAllExistingHeatmaps() {
    if (!fs::exists(buildDir_)) return;
    std::error_code ec;
    for (auto& zd : fs::directory_iterator(buildDir_, ec)) {
        if (!zd.is_directory()) continue;
        for (auto& xd : fs::directory_iterator(zd.path(), ec)) {
            if (!xd.is_directory()) continue;
            for (auto& yd : fs::directory_iterator(xd.path(), ec)) {
                if (!yd.is_directory()) continue;
                fs::path hm = yd.path() / "heatmap.png";
                if (fs::exists(hm, ec)) fs::remove(hm, ec);
            }
        }
    }
}

void HeatmapGenerator::doGenerate(Settings s) {

    std::vector<Point> pts;
    {
        std::lock_guard<std::mutex> lk(dataMtx_);
        pts = points_;
    }

    std::vector<Point> filt;
    filt.reserve(pts.size());
    for (const auto& p : pts) {
        if (p.lat == 0.0 && p.lon == 0.0) continue;
        if (!s.selectedPcis.empty() && !s.selectedPcis.count(p.pci)) continue;
        if (s.earfcn > 0 && p.earfcn > 0 && p.earfcn != s.earfcn) continue;

        switch (s.criterion) {
            case Criterion::RSRP:
                if (p.rsrp <= -140.0f || p.rsrp > 0.0f) continue;
                break;
            case Criterion::RSRQ:
                if (p.rsrq < -40.0f || p.rsrq > 5.0f) continue;
                break;
            case Criterion::RSSI:
                if (p.rssi <= -140.0f || p.rssi > 0.0f) continue;
                break;
            case Criterion::Altitude:
                break;
        }
        filt.push_back(p);
    }

    clearAllExistingHeatmaps();

    if (filt.empty()) {
        std::cout << "[Heatmap] Нет точек для выбранных фильтров\n";
        return;
    }

    double minLat =  1e18, maxLat = -1e18;
    double minLon =  1e18, maxLon = -1e18;
    for (const auto& p : filt) {
        minLat = std::min(minLat, p.lat); maxLat = std::max(maxLat, p.lat);
        minLon = std::min(minLon, p.lon); maxLon = std::max(maxLon, p.lon);
    }

    int z = std::clamp(s.zoom, MIN_ZOOM, MAX_ZOOM);

    int tx0 = (int)std::floor(OSM::lon2tile(minLon, z)) - 1;
    int tx1 = (int)std::floor(OSM::lon2tile(maxLon, z)) + 1;
    int ty0 = (int)std::floor(OSM::lat2tile(maxLat, z)) - 1;
    int ty1 = (int)std::floor(OSM::lat2tile(minLat, z)) + 1;

    int maxIdx = (1 << z) - 1;
    tx0 = std::max(0, tx0); tx1 = std::min(maxIdx, tx1);
    ty0 = std::max(0, ty0); ty1 = std::min(maxIdx, ty1);

    int totalTiles = (tx1 - tx0 + 1) * (ty1 - ty0 + 1);
    int done = 0;

    double radDeg = s.radiusMeters / 111320.0;

    std::cout << "[Heatmap] Старт z=" << z
              << " тайлов=" << totalTiles
              << " точек=" << filt.size() << "\n";

    for (int tx = tx0; tx <= tx1 && running_.load(); ++tx) {
        for (int ty = ty0; ty <= ty1 && running_.load(); ++ty) {

            double latTop = OSM::tile2lat((double)ty,       z);
            double latBot = OSM::tile2lat((double)(ty + 1), z);
            double lonL   = OSM::tile2lon((double)tx,       z);
            double lonR   = OSM::tile2lon((double)(tx + 1), z);

            double midLat  = (latTop + latBot) * 0.5;
            double cosLat  = std::cos(midLat * OSM::PI / 180.0);
            double radDegLon = radDeg / std::max(0.01, cosLat);

            double fLatMin = std::min(latTop, latBot) - radDeg;
            double fLatMax = std::max(latTop, latBot) + radDeg;
            double fLonMin = lonL - radDegLon;
            double fLonMax = lonR + radDegLon;

            std::vector<const Point*> local;
            local.reserve(64);
            for (const auto& p : filt) {
                if (p.lat < fLatMin || p.lat > fLatMax) continue;
                if (p.lon < fLonMin || p.lon > fLonMax) continue;
                local.push_back(&p);
            }

            if (!local.empty()) {
                std::string outPath =
                    buildDir_ + "/" + std::to_string(z) + "/" +
                    std::to_string(tx) + "/" +
                    std::to_string(ty) + "/heatmap.png";
                generateTile(z, tx, ty, local, s.criterion, s.radiusMeters, outPath);
            }

            ++done;
            progress_.store(done * 100 / std::max(1, totalTiles));
        }
    }

    std::cout << "[Heatmap] Завершено: " << done << " тайлов\n";
}

bool HeatmapGenerator::generateTile(int z, int tx, int ty,
                                     const std::vector<const Point*>& local,
                                     Criterion crit, float radius,
                                     const std::string& outPath)
{
    const int S = TILE_SIZE;
    const float radSq = radius * radius;
    std::vector<uint8_t> rgba(static_cast<size_t>(S) * S * 4, 0u);

    for (int py = 0; py < S; ++py) {

        double lat    = OSM::tile2lat(ty + (py + 0.5) / (double)S, z);
        double cosLat = std::cos(lat * OSM::PI / 180.0);

        for (int px = 0; px < S; ++px) {

            double lon = OSM::tile2lon(tx + (px + 0.5) / (double)S, z);

            double wSum = 0.0, vSum = 0.0;
            for (const Point* pp : local) {
                double dlat = (pp->lat - lat) * 111320.0;
                double dlon = (pp->lon - lon) * 111320.0 * cosLat;
                double d2   = dlat * dlat + dlon * dlon;
                if (d2 > (double)radSq) continue;

                double w = (d2 < 0.25) ? 1e9 : (1.0 / d2);

                double v = 0.0;
                switch (crit) {
                    case Criterion::RSRP:     v = pp->rsrp; break;
                    case Criterion::RSRQ:     v = pp->rsrq; break;
                    case Criterion::RSSI:     v = pp->rssi; break;
                    case Criterion::Altitude: v = pp->alt;  break;
                }
                wSum += w;
                vSum += w * v;
            }
            if (wSum < 1e-12) continue;

            float v = (float)(vSum / wSum);
            uint8_t r, g, b, a;
            valueToColor(v, crit, r, g, b, a);

            size_t idx = (static_cast<size_t>(py) * S + px) * 4;
            rgba[idx]     = r;
            rgba[idx + 1] = g;
            rgba[idx + 2] = b;
            rgba[idx + 3] = a;
        }
    }

    std::error_code ec;
    fs::create_directories(fs::path(outPath).parent_path(), ec);
    int ok = stbi_write_png(outPath.c_str(), S, S, 4,
                            rgba.data(), S * 4);
    return ok != 0;
}

void HeatmapGenerator::valueToColor(float v, Criterion c,
                                     uint8_t& r, uint8_t& g,
                                     uint8_t& b, uint8_t& a)
{
    float t = 0.0f;
    switch (c) {
        case Criterion::RSRP:
            if (v < -110.0f) { r = g = b = a = 0; return; }
            t = (v + 110.0f) / 30.0f;
            break;
        case Criterion::RSRQ:
            if (v < -20.0f) { r = g = b = a = 0; return; }
            t = (v + 20.0f) / 17.0f;
            break;
        case Criterion::RSSI:
            if (v < -100.0f) { r = g = b = a = 0; return; }
            t = (v + 100.0f) / 50.0f;
            break;
        case Criterion::Altitude:

            if (v < 0.0f) { r = g = b = a = 0; return; }
            t = std::clamp(v / 500.0f, 0.0f, 1.0f);
            break;
    }
    t = std::clamp(t, 0.0f, 1.0f);

    float fr, fg, fb;
    constexpr float s1 = 1.0f / 3.0f;
    constexpr float s2 = 2.0f / 3.0f;

    if (t < s1) {
        float u = t / s1;
        fr =  40 + ( 60 -  40) * u;
        fg =  60 + (200 -  60) * u;
        fb = 180 + (220 - 180) * u;
    } else if (t < s2) {
        float u = (t - s1) / s1;
        fr =  60 + (240 -  60) * u;
        fg = 200 + (220 - 200) * u;
        fb = 220 + ( 60 - 220) * u;
    } else {
        float u = (t - s2) / s1;
        fr = 240 + (220 - 240) * u;
        fg = 220 + ( 30 - 220) * u;
        fb =  60 + ( 30 -  60) * u;
    }

    r = (uint8_t)std::clamp(fr, 0.0f, 255.0f);
    g = (uint8_t)std::clamp(fg, 0.0f, 255.0f);
    b = (uint8_t)std::clamp(fb, 0.0f, 255.0f);
    a = 200;
}