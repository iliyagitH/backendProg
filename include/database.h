#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include <pqxx/pqxx>
#include "telemetry.h"

class Database {
public:
    struct Config {
        std::string host     = "localhost";
        std::string port     = "5432";
        std::string dbname   = "telemetry_db";
        std::string user     = "postgres";
        std::string password = "user";
    };

    struct DbRow {
        double lat, lon, alt;
        float  rsrp, rssi, sinr, rsrq;
        int    pci;
        long long ts;
    };

    explicit Database(const Config& cfg);
    ~Database();

    bool isConnected();
    bool insertRecord(double lat, double lon, double alt,
                      const CellRecord& cell, long long ts);
    std::vector<DbRow> loadHistory();

private:
    bool tryConnect();
    void ensureTable();

    Config cfg_;
    std::unique_ptr<pqxx::connection> conn_;
    std::mutex mtx_;
    bool connected_ = false;
};
