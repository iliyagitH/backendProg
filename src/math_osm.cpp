#include "math_osm.h"

double OSM::lon2tile(double lon, int z) {
    return (lon + 180.0) / 360.0 * (1 << z);
}

double OSM::lat2tile(double lat, int z) {
    double rad = lat * PI / 180.0;
    return (1.0 - std::log(std::tan(rad) + 1.0 / std::cos(rad)) / PI) / 2.0 * (1 << z);
}

double OSM::tile2lat(double y, int z) {
    double n = PI - 2.0 * PI * y / (1 << z);
    return 180.0 / PI * std::atan(0.5 * (std::exp(n) - std::exp(-n)));
}

double OSM::tile2lon(double x, int z) {
    return x / (double)(1 << z) * 360.0 - 180.0;
}