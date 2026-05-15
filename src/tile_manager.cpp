#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"

#include "tile_manager.h"
#include "curl_utils.h"

TileManager::TileManager(const std::string& buildDir) : buildDir_(buildDir) {
    fs::create_directories(buildDir_);
    worker_ = std::thread(&TileManager::loop, this);
}

TileManager::~TileManager() { stop(); }

void TileManager::stop() {
    running_.store(false);
    if (worker_.joinable()) worker_.join();
}

void TileManager::requestTile(int z, int x, int y) {
    std::string key = makeKey(z, x, y);
    {
        std::lock_guard<std::mutex> lk(cacheMtx_);
        if (cache_.count(key)) return;
        cache_[key].loading = true;
    }
    {
        std::lock_guard<std::mutex> qlk(queueMtx_);
        pending_.push(key);
    }
}

GLuint TileManager::getTexture(int z, int x, int y) {
    std::string key = makeKey(z, x, y);
    std::lock_guard<std::mutex> lk(cacheMtx_);
    auto it = cache_.find(key);
    return (it != cache_.end()) ? it->second.texId : 0;
}

void TileManager::uploadPending() {
    std::lock_guard<std::mutex> lk(cacheMtx_);
    for (auto& [key, entry] : cache_) {
        if (!entry.pixels.empty() && entry.texId == 0) {
            glGenTextures(1, &entry.texId);
            glBindTexture(GL_TEXTURE_2D, entry.texId);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                         entry.w, entry.h, 0,
                         GL_RGBA, GL_UNSIGNED_BYTE, entry.pixels.data());
            entry.pixels.clear();
            entry.loading = false;
            std::cout << "[TileManager] → GPU: " << key << "\n";
        }
    }
}

void TileManager::clearCache() {
    std::lock_guard<std::mutex> lk(cacheMtx_);
    for (auto& [key, entry] : cache_)
        if (entry.texId) glDeleteTextures(1, &entry.texId);
    cache_.clear();
}

size_t TileManager::cacheSize() {
    std::lock_guard<std::mutex> lk(cacheMtx_);
    return cache_.size();
}

std::string TileManager::makeKey(int z, int x, int y) const {
    return std::to_string(z) + "/" + std::to_string(x) + "/" + std::to_string(y);
}

std::string TileManager::diskPath(int z, int x, int y) const {
    return buildDir_ + "/" + std::to_string(z) +
           "/" + std::to_string(x) +
           "/" + std::to_string(y) + ".png";
}

bool TileManager::parseKey(const std::string& key, int& z, int& x, int& y) {
    return std::sscanf(key.c_str(), "%d/%d/%d", &z, &x, &y) == 3;
}

void TileManager::loop() {
    while (running_.load()) {
        std::string key;
        bool hasWork = false;
        {
            std::lock_guard<std::mutex> lk(queueMtx_);
            if (!pending_.empty()) {
                key = pending_.front();
                pending_.pop();
                hasWork = true;
            }
        }
        if (!hasWork) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        int z, x, y;
        if (!parseKey(key, z, x, y)) continue;

        std::string path = diskPath(z, x, y);
        std::vector<uint8_t> raw = fs::exists(path)
            ? CurlUtils::loadFromFile(path)
            : CurlUtils::downloadTile(z, x, y);

        if (!raw.empty() && !fs::exists(path)) {
            CurlUtils::saveToFile(path, raw);
        }

        if (raw.empty()) {
            std::lock_guard<std::mutex> lk(cacheMtx_);
            cache_.erase(key);
            continue;
        }

        int w, h, ch;
        uint8_t* img = stbi_load_from_memory(
            raw.data(), static_cast<int>(raw.size()), &w, &h, &ch, 4);
        if (!img) {
            std::lock_guard<std::mutex> lk(cacheMtx_);
            cache_.erase(key);
            continue;
        }

        {
            std::lock_guard<std::mutex> lk(cacheMtx_);
            auto& entry = cache_[key];
            entry.pixels.assign(img, img + w * h * 4);
            entry.w = w;
            entry.h = h;
        }
        stbi_image_free(img);
    }
}
