#pragma once
#include "common.h"
#include "telemetry.h"
#include "database.h"
#include "heatmap_generator.h"

namespace Startup {
    void loadInitialData(Telemetry& data, Database& db, HeatmapGenerator& hm);
}
