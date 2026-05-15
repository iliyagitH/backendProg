#include "telemetry.h"

void Telemetry::clear() {
    std::lock_guard<std::mutex> lk(mtx);
    path_lat.clear();
    path_lon.clear();
    cells.clear();
    history_rsrp.clear();
    history_rssi.clear();
    history_sinr.clear();
    history_rsrq.clear();
    history_pci.clear();
    latitude = longitude = altitude = 0.0;
    has_new_data = false;
}

void Telemetry::trimHistory() {
    std::lock_guard<std::mutex> lk(mtx);
    auto trim = [](std::vector<float>& v, size_t m) {
        if (v.size() > m) v.erase(v.begin(), v.begin() + (v.size() - m));
    };
    auto trimD = [](std::vector<double>& v, size_t m) {
        if (v.size() > m) v.erase(v.begin(), v.begin() + (v.size() - m));
    };
    trim(history_rsrp, MAX_HISTORY_SIZE);
    trim(history_rssi, MAX_HISTORY_SIZE);
    trim(history_sinr, MAX_HISTORY_SIZE);
    trim(history_rsrq, MAX_HISTORY_SIZE);
    trim(history_pci, MAX_HISTORY_SIZE);
    trimD(path_lat, MAX_PATH_SIZE);
    trimD(path_lon, MAX_PATH_SIZE);
}

void Telemetry::addPoint(double lat, double lon, double alt) {
    std::lock_guard<std::mutex> lk(mtx);
    if (lat != 0.0 || lon != 0.0) {
        latitude = lat;
        longitude = lon;
        altitude = alt;
        path_lat.push_back(lat);
        path_lon.push_back(lon);
    }
}

void Telemetry::addCellData(const CellRecord& cell) {
    std::lock_guard<std::mutex> lk(mtx);
    history_rsrp.push_back(static_cast<float>(cell.rsrp));
    history_rssi.push_back(static_cast<float>(cell.rssi));
    history_sinr.push_back(static_cast<float>(cell.sinr));
    history_rsrq.push_back(static_cast<float>(cell.rsrq));
    history_pci.push_back(static_cast<float>(cell.pci));
    has_new_data = true;
}