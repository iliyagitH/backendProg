#include "database.h"
#include <iostream>

Database::Database(const Config& cfg) : cfg_(cfg) {
    tryConnect();
    if (connected_) ensureTable();
}

Database::~Database() {
    if (conn_ && conn_->is_open()) conn_->close();
}

bool Database::isConnected() {
    std::lock_guard<std::mutex> lk(mtx_);
    if (connected_ && conn_ && conn_->is_open()) return true;
    return tryConnect();
}

bool Database::tryConnect() {
    try {
        std::string cs =
            "host=" + cfg_.host +
            " port=" + cfg_.port +
            " dbname=" + cfg_.dbname +
            " user=" + cfg_.user +
            " password=" + cfg_.password;
        conn_ = std::make_unique<pqxx::connection>(cs);
        connected_ = conn_->is_open();
        if (connected_) {
            std::cout << "[DB] Подключено к " << cfg_.dbname << "\n";
        }
    } catch (const std::exception& e) {
        connected_ = false;
        std::cerr << "[DB] Ошибка подключения: " << e.what() << "\n";
    }
    return connected_;
}

void Database::ensureTable() {
    try {
        pqxx::work txn(*conn_);
        txn.exec(
            "CREATE TABLE IF NOT EXISTS user_equipment ("
            "id SERIAL PRIMARY KEY,"
            "lat DOUBLE PRECISION NOT NULL,"
            "lon DOUBLE PRECISION NOT NULL,"
            "alt DOUBLE PRECISION DEFAULT 0,"
            "standard TEXT NOT NULL,"
            "pci INTEGER DEFAULT -1,"
            "rsrp INTEGER DEFAULT -140,"
            "rssi INTEGER DEFAULT 0,"
            "rsrq INTEGER DEFAULT 0,"
            "sinr INTEGER DEFAULT 0,"
            "ts BIGINT NOT NULL"
            ")"
        );
        txn.commit();
    } catch (const std::exception& e) {
        std::cerr << "[DB] Ошибка создания таблицы: " << e.what() << "\n";
    }
}

bool Database::insertRecord(double lat, double lon, double alt,
                            const CellRecord& cell, long long ts) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (!connected_ || !conn_ || !conn_->is_open()) {
        if (!tryConnect()) return false;
    }
    if (cell.standard == "Unknown") return false;

    try {
        pqxx::work txn(*conn_);
        txn.exec_params(
            "INSERT INTO user_equipment "
            "(lat, lon, alt, standard, pci, rsrp, rssi, rsrq, sinr, ts) "
            "VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9,$10)",
            lat, lon, alt,
            cell.standard, cell.pci,
            cell.rsrp, cell.rssi, cell.rsrq, cell.sinr,
            ts
        );
        txn.commit();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[DB] Ошибка вставки: " << e.what() << "\n";
        connected_ = false;
        return false;
    }
}

std::vector<Database::DbRow> Database::loadHistory() {
    std::lock_guard<std::mutex> lk(mtx_);
    std::vector<DbRow> rows;
    if (!connected_ || !conn_ || !conn_->is_open()) {
        if (!tryConnect()) return rows;
    }

    try {
        pqxx::work txn(*conn_);
        auto res = txn.exec(
            "SELECT lat, lon, alt, rsrp, rssi, sinr, rsrq, pci, ts "
            "FROM user_equipment "
            "ORDER BY ts ASC"
        );
        txn.commit();
        for (const auto& row : res) {
            DbRow r;
            r.lat = row[0].as<double>(0.0);
            r.lon = row[1].as<double>(0.0);
            r.alt = row[2].as<double>(0.0);
            r.rsrp = row[3].as<float>(-140.0f);
            r.rssi = row[4].as<float>(0.0f);
            r.sinr = row[5].as<float>(0.0f);
            r.rsrq = row[6].as<float>(0.0f);
            r.pci = row[7].as<int>(-1);
            r.ts = row[8].as<long long>(0);
            rows.push_back(r);
        }
        std::cout << "[DB] Загружено: " << rows.size() << " записей\n";
    } catch (const std::exception& e) {
        std::cerr << "[DB] Ошибка загрузки: " << e.what() << "\n";
    }
    return rows;
}
