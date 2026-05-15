#pragma once
#include "common.h"

namespace CurlUtils {
    size_t writeCallback(void* ptr, size_t sz, size_t n, std::vector<uint8_t>* out);

    std::vector<uint8_t> download(const std::string& url, long timeoutSec = 10);

    std::vector<uint8_t> downloadTile(int z, int x, int y);

    bool saveToFile(const std::string& path, const std::vector<uint8_t>& data);

    std::vector<uint8_t> loadFromFile(const std::string& path);
}