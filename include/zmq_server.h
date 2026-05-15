#pragma once
#include "common.h"
#include "telemetry.h"
#include "database.h"

namespace ZMQServer {
    void run(Telemetry* data, Database* db, std::atomic<bool>* stop);
}