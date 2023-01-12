cmake_minimum_required (VERSION 3.6)

#add_library(native_app_glue ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)
#target_link_libraries(native_app_glue log)
#target_include_directories(native_app_glue INTERFACE ${ANDROID_NDK}/sources/android/native_app_glue)
#set_common_target_properties(native_app_glue)
#
#add_subdirectory(ndk_helper)

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
        PRIVATE filamentapp
        #PRIVATE sample-resources
        #PRIVATE native_app_glue
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




# Force a relink when the version script is changed:
#set_target_properties(filament-jni PROPERTIES LINK_DEPENDS ${VERSION_SCRIPT})
