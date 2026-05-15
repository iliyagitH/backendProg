#pragma once

#define _CRT_SECURE_NO_WARNINGS

#include <thread>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <cmath>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <functional>
#include <vector>
#include <string>
#include <memory>
#include <cstring>

#include <GL/glew.h>
#include <SDL.h>
#include <SDL_opengl.h>
#include <imgui.h>
#include <implot.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>
#include <zmq.hpp>
#include <pqxx/pqxx>
#include <curl/curl.h>
#include "json.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

constexpr size_t MAX_HISTORY_SIZE = 2000;
constexpr size_t MAX_PATH_SIZE = 50000;
constexpr int TILE_SIZE = 256;
constexpr int MIN_ZOOM = 1;
constexpr int MAX_ZOOM = 19;