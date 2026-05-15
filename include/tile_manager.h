#pragma once
#include "common.h"

class TileManager {
public:
    explicit TileManager(const std::string& buildDir = "build");
    ~TileManager();

    void stop();
    void requestTile(int z, int x, int y);
    GLuint getTexture(int z, int x, int y);
    void uploadPending();
    void clearCache();
    size_t cacheSize();

private:
    struct TileEntry {
        GLuint texId = 0;
        int w = 0, h = 0;
        std::vector<uint8_t> pixels;
        bool loading = false;
    };

    std::string makeKey(int z, int x, int y) const;
    std::string diskPath(int z, int x, int y) const;
    static bool parseKey(const std::string& key, int& z, int& x, int& y);
    void loop();

    std::string buildDir_;
    std::unordered_map<std::string, TileEntry> cache_;
    std::queue<std::string> pending_;
    std::mutex cacheMtx_, queueMtx_;
    std::atomic<bool> running_{true};
    std::thread worker_;
};