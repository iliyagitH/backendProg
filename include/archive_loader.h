#pragma once
#include "common.h"
#include "telemetry.h"

namespace ArchiveLoader {
    std::streampos loadOffset(const std::string& offsetFile);
    void saveOffset(const std::string& offsetFile, std::streampos pos);
    void applyArchiveEntry(const json& entry, Telemetry* data);
    void loadArchive(Telemetry* data,
                     const std::string& archiveFile = "archive.json",
                     const std::string& offsetFile = "archive.offset");
}