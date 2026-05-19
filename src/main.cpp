#include "common.h"
#include "telemetry.h"
#include "database.h"
#include "tile_manager.h"
#include "map_renderer.h"
#include "heatmap_generator.h"
#include "zmq_server.h"
#include "startup.h"
#include "gui.h"

int main(int , char** ) {
    curl_global_init(CURL_GLOBAL_ALL);

    Database::Config dbCfg;
    Database         db(dbCfg);
    Telemetry        shared_data;
    TileManager      tileManager("build");
    MapRenderer      mapRenderer;
    HeatmapGenerator hmGen("build");

    Startup::loadInitialData(shared_data, db, hmGen);

    Gui::State guiState;
    if (shared_data.latitude  != 0.0) guiState.mapLat = shared_data.latitude;
    if (shared_data.longitude != 0.0) guiState.mapLon = shared_data.longitude;

    std::atomic<bool> stopFlag{false};
    std::thread serverThread(ZMQServer::run, &shared_data, &db, &stopFlag);

    if (Gui::init()) {
        Gui::runLoop(guiState, shared_data, tileManager, mapRenderer, hmGen, stopFlag);
        Gui::shutdown();
    }

    stopFlag.store(true);
    if (serverThread.joinable()) serverThread.join();
    tileManager.stop();
    curl_global_cleanup();
    return 0;
}
