#pragma once
#include "common.h"
#include "telemetry.h"
#include "database.h"

namespace Startup {
    void loadInitialData(Telemetry& data, Database& db);
}
