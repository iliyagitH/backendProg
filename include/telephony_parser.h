#pragma once
#include "common.h"
#include "telemetry.h"

namespace TelephonyParser {
    CellRecord parseCellJson(const json& c);
    std::vector<CellRecord> parseTelephonyArray(const json& telephony);
}