#pragma once
#include "common.h"

namespace OSM {
    constexpr double PI = 3.14159265358979323846;

    double lon2tile(double lon, int z);
    double lat2tile(double lat, int z);
    double tile2lat(double y, int z);
    double tile2lon(double x, int z);
}