#pragma once
#include "common.h"

class HeatmapGenerator {
public:
    enum class Criterion : int {
        RSRP = 0,
        RSRQ = 1,
        RSSI = 2,
        Altitude = 3
    };
    static constexpr const char* kCriterionNames[] = {
        "RSRP", "RSRQ", "RSSI", "Altitude"
    };

    struct Point {
        double lat   = 0.0;
        double lon   = 0.0;
        double alt   = 0.0;
        float  rsrp  = -140.0f;
        float  rsrq  = 0.0f;
        float  rssi  = 0.0f;
        int    pci   = -1;
        int    earfcn = -1;
    };

    struct Settings {
        Criterion criterion = Criterion::RSRP;
        float     radiusMeters = 25.0f;
        int       zoom = 15;
        int       earfcn = -1;
        std::unordered_set<int> selectedPcis;
    };

    explicit HeatmapGenerator(std::string buildDir = "build");
    ~HeatmapGenerator();

    HeatmapGenerator(const HeatmapGenerator&) = delete;
    HeatmapGenerator& operator=(const HeatmapGenerator&) = delete;

    void setData(std::vector<Point> pts);

    Settings settings() const;
    void     updateSettings(const Settings& s);

    void requestRegenerate();

    bool busy()     const { return busy_.load(); }
    int  progress() const { return progress_.load(); }

    bool consumeUpdatedFlag();

    std::unordered_map<int, size_t> pciHistogram() const;
    int dominantPci() const;
    std::vector<int> availableEarfcns() const;

private:
    void workerLoop();
    void doGenerate(Settings s);
    bool generateTile(int z, int tx, int ty,
                      const std::vector<const Point*>& local,
                      Criterion crit, float radius,
                      const std::string& outPath);
    void clearAllExistingHeatmaps();

    static void valueToColor(float v, Criterion c,
                             uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& a);

    std::string buildDir_;

    mutable std::mutex dataMtx_;
    std::vector<Point> points_;

    mutable std::mutex settingsMtx_;
    Settings settings_;

    std::atomic<bool> requested_{false};
    std::atomic<bool> updated_{false};
    std::atomic<bool> busy_{false};
    std::atomic<int>  progress_{0};
    std::atomic<bool> running_{true};

    std::thread worker_;
};