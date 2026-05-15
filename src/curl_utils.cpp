#include "curl_utils.h"

size_t CurlUtils::writeCallback(void* ptr, size_t sz, size_t n, std::vector<uint8_t>* out) {
    auto* p = static_cast<uint8_t*>(ptr);
    out->insert(out->end(), p, p + sz * n);
    return sz * n;
}

std::vector<uint8_t> CurlUtils::download(const std::string& url, long timeoutSec) {
    std::vector<uint8_t> buf;
    CURL* c = curl_easy_init();
    if (!c) return buf;

    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_USERAGENT, "TelemetryBot/1.0 (lab work)");
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, timeoutSec);
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_SSL_VERIFYPEER, 0L);

    CURLcode res = curl_easy_perform(c);
    long httpCode = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(c);

    if (res != CURLE_OK || httpCode != 200) {
        std::cerr << "[CURL] Ошибка: " << url << " (HTTP " << httpCode << ")\n";
        buf.clear();
    }
    return buf;
}

std::vector<uint8_t> CurlUtils::downloadTile(int z, int x, int y) {
    std::string url = "https://tile.openstreetmap.org/" +
                      std::to_string(z) + "/" +
                      std::to_string(x) + "/" +
                      std::to_string(y) + ".png";
    return download(url);
}

bool CurlUtils::saveToFile(const std::string& path, const std::vector<uint8_t>& data) {
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream f(path, std::ios::binary);
    if (f) {
        f.write(reinterpret_cast<const char*>(data.data()),
                static_cast<std::streamsize>(data.size()));
        return true;
    }
    return false;
}

std::vector<uint8_t> CurlUtils::loadFromFile(const std::string& path) {
    std::vector<uint8_t> data;
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (f) {
        auto sz = f.tellg();
        f.seekg(0);
        data.resize(static_cast<size_t>(sz));
        f.read(reinterpret_cast<char*>(data.data()), sz);
    }
    return data;
}