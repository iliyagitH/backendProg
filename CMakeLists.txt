cmake_minimum_required(VERSION 3.16)
project(telemetry_system)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(libpqxx CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(cppzmq CONFIG REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenGL REQUIRED)

set(THIRD_PARTY_DIR ${PROJECT_SOURCE_DIR}/third_party)
set(SRC_DIR         ${PROJECT_SOURCE_DIR}/src)
set(INC_DIR         ${PROJECT_SOURCE_DIR}/include)

add_library(imgui STATIC
    ${THIRD_PARTY_DIR}/imgui/imgui.cpp
    ${THIRD_PARTY_DIR}/imgui/imgui_draw.cpp
    ${THIRD_PARTY_DIR}/imgui/imgui_tables.cpp
    ${THIRD_PARTY_DIR}/imgui/imgui_widgets.cpp
    ${THIRD_PARTY_DIR}/imgui/backends/imgui_impl_sdl2.cpp
    ${THIRD_PARTY_DIR}/imgui/backends/imgui_impl_opengl3.cpp
    ${THIRD_PARTY_DIR}/implot/implot.cpp
    ${THIRD_PARTY_DIR}/implot/implot_items.cpp
)
target_include_directories(imgui PUBLIC
    ${THIRD_PARTY_DIR}/imgui
    ${THIRD_PARTY_DIR}/implot
    ${THIRD_PARTY_DIR}/imgui/backends
    "C:/dev_libs/SDL2/include"
)

set(PROJECT_SOURCES
    ${SRC_DIR}/main.cpp
    ${SRC_DIR}/database.cpp
)

add_executable(main ${PROJECT_SOURCES})

target_compile_definitions(main PRIVATE SDL_MAIN_HANDLED)
if(MSVC)
    target_compile_options(main PRIVATE /utf-8)
endif()

target_include_directories(main PRIVATE
    ${SRC_DIR}
    ${INC_DIR}
    ${THIRD_PARTY_DIR}
    "C:/dev_libs/SDL2/include"
    "C:/dev_libs/glew-2.3.1/include"
)

target_link_libraries(main PRIVATE
    imgui
    Threads::Threads
    libpqxx::pqxx
    nlohmann_json::nlohmann_json
    cppzmq
    OpenGL::GL
    "C:/dev_libs/SDL2/lib/x64/SDL2.lib"
    "C:/dev_libs/SDL2/lib/x64/SDL2main.lib"
    "C:/dev_libs/glew-2.3.1/lib/Release/x64/glew32.lib"
    opengl32
    ws2_32.lib wldap32.lib crypt32.lib normaliz.lib
)

if(MSVC)
    add_custom_command(TARGET main POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "C:/dev_libs/SDL2/lib/x64/SDL2.dll"
        $<TARGET_FILE_DIR:main>
    )
endif()
