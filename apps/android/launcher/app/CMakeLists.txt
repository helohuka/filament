cmake_minimum_required (VERSION 3.6)


#option(FILAMENT_SUPPORTS_VULKAN "Enables Vulkan on Android" OFF)
#option(FILAMENT_ENABLE_MATDBG "Enables Material debugger" OFF)
#option(FILAMENT_DISABLE_MATOPT "Disables material optimizations" OFF)


add_library(main SHARED
    src/main/cpp/Filament.cpp
)

# Ordering is significant in the following list. The PRIVATE qualifier prevents transitive deps.
target_link_libraries(main

    PRIVATE filament
    PRIVATE backend
    PRIVATE filaflat
    PRIVATE filabridge
    PRIVATE ibl-lite
    PRIVATE log
    PRIVATE GLESv3
    PRIVATE EGL
    PRIVATE android
    PRIVATE jnigraphics
    PRIVATE utils
    PRIVATE camutils
    PRIVATE sdl2
    PRIVATE gamedriver
    PRIVATE sample-resources
    # libgeometry is PUBLIC because gltfio uses it.
    PUBLIC geometry

    $<$<STREQUAL:${FILAMENT_ENABLE_MATDBG},ON>:matdbg>
    $<$<STREQUAL:${FILAMENT_ENABLE_MATDBG},ON>:filamat>
    $<$<STREQUAL:${FILAMENT_SUPPORTS_VULKAN},ON>:bluevk>
    $<$<STREQUAL:${FILAMENT_SUPPORTS_VULKAN},ON>:vkshaders>
    $<$<STREQUAL:${FILAMENT_SUPPORTS_VULKAN},ON>:smol-v>
)

target_include_directories(main PRIVATE
        ..
        ${FILAMENT_DIR}/include
        ../../../filament/backend/include
        ../../../third_party/robin-map
        ../../../libs/utils/include)

