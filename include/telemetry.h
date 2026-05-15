#pragma once
#include "common.h"

struct CellRecord {
    std::string standard = "Unknown";
    std::string mcc, mnc;
    int pci = -1;
    int timingAdvance = 0;
    bool isRegistered = false;

    int rsrp = -140, rssi = 0, rsrq = 0, sinr = 0;
    int earfcn = -1, band = -1;
    std::string tac, cellId;

    int arfcn = -1, bsic = -1, dbm = -140;
    std::string lac;

    int nrarfcn = -1;
    int ssRsrp = -140, ssRsrq = 0, ssSinr = 0;
    std::string nci;

    int signalStrength = -140;
};

struct Telemetry {
    std::mutex mtx;
    double latitude = 0.0, longitude = 0.0, altitude = 0.0;
    std::vector<double> path_lat, path_lon;
    std::vector<CellRecord> cells;
    std::vector<float> history_rsrp, history_rssi, history_sinr, history_rsrq, history_pci;
    bool has_new_data = false;

    void clear();
    void trimHistory();
    void addPoint(double lat, double lon, double alt);
    void addCellData(const CellRecord& cell);
};