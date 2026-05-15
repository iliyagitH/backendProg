#include "archive_loader.h"
#include "telephony_parser.h"

std::streampos ArchiveLoader::loadOffset(const std::string& offsetFile) {
    std::ifstream f(offsetFile);
    if (!f.is_open()) return 0;
    long long val = 0;
    f >> val;
    return static_cast<std::streampos>(val);
}

void ArchiveLoader::saveOffset(const std::string& offsetFile, std::streampos pos) {
    std::ofstream f(offsetFile, std::ios::trunc);
    f << static_cast<long long>(pos) << "\n";
}

void ArchiveLoader::applyArchiveEntry(const json& entry, Telemetry* data) {
    if (entry.contains("location") && !entry["location"].is_null()) {
        double lat = entry["location"].value("Latitude", 0.0);
        double lon = entry["location"].value("Longitude", 0.0);
        if (lat != 0.0 || lon != 0.0) {
            data->addPoint(lat, lon, entry["location"].value("Altitude", 0.0));
        }
    }
    if (entry.contains("telephony")) {
        auto cells = TelephonyParser::parseTelephonyArray(entry["telephony"]);
        for (const auto& c : cells) {
            if (c.isRegistered || cells.size() == 1) {
                data->addCellData(c);
                data->cells = cells;
                break;
            }
        }
    }
}

void ArchiveLoader::loadArchive(Telemetry* data,
                                const std::string& archiveFile,
                                const std::string& offsetFile) {
    std::streampos startPos = loadOffset(offsetFile);
    std::ifstream f(archiveFile);
    if (!f.is_open()) {
        std::cout << "[Archive] Файл не найден, начинаем с чистого листа\n";
        return;
    }
    f.seekg(0, std::ios::end);
    std::streampos fileSize = f.tellg();
    if (startPos > fileSize) startPos = 0;
    f.seekg(startPos);

    std::string line;
    int loaded = 0;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        try {
            json entry = json::parse(line);
            applyArchiveEntry(entry, data);
            ++loaded;
        } catch (...) { continue; }
    }
    saveOffset(offsetFile, fileSize);
    data->trimHistory();
    std::cout << "[Archive] Загружено: " << loaded << " записей из архива\n";
}