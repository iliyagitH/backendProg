#include "zmq_server.h"
#include "telephony_parser.h"
#include "archive_loader.h"

void ZMQServer::run(Telemetry* data, Database* db, std::atomic<bool>* stop) {
    const std::string ARCHIVE = "archive.json";
    const std::string ARCHIVE_OFFSET = "archive.offset";

    std::unordered_set<std::string> seenTimes;
    {
        std::ifstream f(ARCHIVE);
        std::string line;
        while (std::getline(f, line)) {
            if (line.empty()) continue;
            try {
                auto j = json::parse(line);
                if (j.contains("location")) {
                    std::string ct = j["location"].value("Current Time", "");
                    if (!ct.empty()) seenTimes.insert(ct);
                }
            } catch (...) {}
        }
        std::cout << "[Archive] Уже известных записей: " << seenTimes.size() << "\n";
    }

    zmq::context_t ctx(1);
    zmq::socket_t sock(ctx, zmq::socket_type::pull);
    sock.set(zmq::sockopt::rcvtimeo, 200);

    bool bound = false;
    for (int attempt = 0; attempt < 5 && !bound; ++attempt) {
        try {
            sock.bind("tcp://0.0.0.0:2222");
            std::cout << "[ZMQ] PULL слушает :2222\n";
            bound = true;
        } catch (...) {
            std::cerr << "[ZMQ] bind не удался, попытка " << attempt + 1 << "/5\n";
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    if (!bound) { std::cerr << "[ZMQ] Не удалось занять порт\n"; return; }

    while (!stop->load()) {
        zmq::message_t msg;
        if (!sock.recv(msg, zmq::recv_flags::none)) continue;

        std::string raw(static_cast<char*>(msg.data()), msg.size());
        json j;
        try { j = json::parse(raw); }
        catch (...) { std::cerr << "[ZMQ] Невалидный JSON\n"; continue; }

        json entries = j.is_array() ? j : json::array({j});

        for (auto& entry : entries) {
            std::string currentTime;
            if (entry.contains("location"))
                currentTime = entry["location"].value("Current Time", "");

            if (!currentTime.empty() && seenTimes.count(currentTime)) continue;
            if (!currentTime.empty()) seenTimes.insert(currentTime);

            {
                std::ofstream arc(ARCHIVE, std::ios::app);
                arc << entry.dump() << "\n";
                ArchiveLoader::saveOffset(ARCHIVE_OFFSET, arc.tellp());
            }

            double lat = 0, lon = 0, alt = 0;
            long long ts = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

            if (entry.contains("location") && !entry["location"].is_null()) {
                lat = entry["location"].value("Latitude", 0.0);
                lon = entry["location"].value("Longitude", 0.0);
                alt = entry["location"].value("Altitude", 0.0);

                std::string ct = entry["location"].value("Current Time", "");
                if (!ct.empty()) {
                    char dow[8] = {}, mon[8] = {};
                    int day = 0, hour = 0, min = 0, sec = 0, year = 0;
                    int n = sscanf(ct.c_str(), "%7s %7s %d %d:%d:%d %*s %d",
                                   dow, mon, &day, &hour, &min, &sec, &year);
                    if (n == 7) {
                        static const char* months[] = {
                            "Jan","Feb","Mar","Apr","May","Jun",
                            "Jul","Aug","Sep","Oct","Nov","Dec"
                        };
                        int month = -1;
                        for (int i = 0; i < 12; ++i)
                            if (strcmp(mon, months[i]) == 0) { month = i; break; }
                        if (month >= 0 && year > 2000) {
                            struct tm tm_val = {};
                            tm_val.tm_year = year - 1900;
                            tm_val.tm_mon = month;
                            tm_val.tm_mday = day;
                            tm_val.tm_hour = hour;
                            tm_val.tm_min = min;
                            tm_val.tm_sec = sec;
                            tm_val.tm_isdst = -1;
                            time_t t = mktime(&tm_val);
                            if (t != -1) ts = static_cast<long long>(t) * 1000LL;
                        }
                    }
                }
            }

            auto parsed = TelephonyParser::parseTelephonyArray(entry.contains("telephony")
                ? entry["telephony"] : json::array());

            {
                std::lock_guard<std::mutex> lk(data->mtx);
                if (lat != 0.0 || lon != 0.0) {
                    data->addPoint(lat, lon, alt);
                }
                data->cells = parsed;
                for (const auto& c : parsed) {
                    if (c.isRegistered || parsed.size() == 1) {
                        data->addCellData(c);
                        break;
                    }
                }
                data->trimHistory();
                data->has_new_data = true;
            }

            if (db->isConnected() && !parsed.empty()) {
                for (const auto& cell : parsed) {
                    db->insertRecord(lat, lon, alt, cell, ts);
                }
            }
        }
    }
    std::cout << "[ZMQ] Сервер остановлен\n";
}