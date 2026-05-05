#pragma once
#include <string>
#include <vector>
#include <mutex>

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
};

struct Telemetry {
    std::mutex mtx;
    double latitude = 0.0;
    double longitude = 0.0;
    double altitude = 0.0;
    std::string currentTime;

    std::vector<CellRecord> cells;

    std::vector<float> history_rsrp;
    std::vector<float> history_rssi;
    std::vector<float> history_sinr;
    std::vector<float> history_rsrq;
    std::vector<float> history_pci;
};
