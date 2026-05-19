#include "startup.h"
#include "archive_loader.h"

static void trimFront(std::vector<float>& v, size_t max) {
    if (v.size() > max)
        v.erase(v.begin(), v.begin() + (v.size() - max));
}

static std::vector<HeatmapGenerator::Point>
buildHeatmapPoints(const std::vector<Database::DbRow>& rows) {
    std::vector<HeatmapGenerator::Point> pts;
    pts.reserve(rows.size());
    for (const auto& r : rows) {
        if (r.lat == 0.0 && r.lon == 0.0) continue;
        HeatmapGenerator::Point p;
        p.lat    = r.lat;  p.lon  = r.lon;   p.alt   = r.alt;
        p.rsrp   = r.rsrp; p.rsrq = r.rsrq;  p.rssi  = r.rssi;
        p.pci    = r.pci;  p.earfcn = r.earfcn;
        pts.push_back(p);
    }
    return pts;
}

void Startup::loadInitialData(Telemetry& data, Database& db, HeatmapGenerator& hm) {

    bool loadedFromDb = false;
    if (db.isConnected()) {
        data.clear();
        auto rows = db.loadHistory();
        if (!rows.empty()) {
            for (const auto& r : rows) {
                if (r.lat != 0.0 || r.lon != 0.0)
                    data.addPoint(r.lat, r.lon, r.alt);

                {
                    std::lock_guard<std::mutex> lk(data.mtx);
                    data.history_rsrp.push_back(r.rsrp);
                    data.history_rssi.push_back(r.rssi);
                    data.history_sinr.push_back(r.sinr);
                    data.history_rsrq.push_back(r.rsrq);
                    data.history_pci.push_back((float)r.pci);
                }
            }

            {
                std::lock_guard<std::mutex> lk(data.mtx);
                trimFront(data.history_rsrp, MAX_HISTORY_SIZE);
                trimFront(data.history_rssi, MAX_HISTORY_SIZE);
                trimFront(data.history_sinr, MAX_HISTORY_SIZE);
                trimFront(data.history_rsrq, MAX_HISTORY_SIZE);
                trimFront(data.history_pci,  MAX_HISTORY_SIZE);
            }

            std::cout << "[Startup] Из БД: " << rows.size()
                      << " строк, path=" << data.path_lat.size() << " точек\n";
            loadedFromDb = true;
            hm.setData(buildHeatmapPoints(rows));
        }
    }
    if (!loadedFromDb) {
        std::cout << "[Startup] БД пуста — загружаем из archive.json\n";
        ArchiveLoader::loadArchive(&data);
    }
}
