cmake_minimum_required(VERSION 3.19)
project(launcher C ASM)


add_executable(launcher src/main.cpp)


# Ordering is significant in the following list. The PRIVATE qualifier prevents transitive deps.
target_link_libraries(launcher

    PRIVATE filament
    PRIVATE backend
    PRIVATE filaflat
    PRIVATE filabridge
    PRIVATE ibl-lite
    PRIVATE utils
    PRIVATE camutils
    PRIVATE gamedriver
    PRIVATE SDL2-static
    PRIVATE embed-resources
    # libgeometry is PUBLIC because gltfio uses it.
    PUBLIC geometry
           PRIVATE   matdbg
        PRIVATE  filamat

)


# ==================================================================================================
# Compiler flags
# ==================================================================================================

if (MSVC)
    set(COMPILER_FLAGS $<$<CONFIG:Release>:/fp:fast>)
else()
    set(COMPILER_FLAGS
            -Wno-extern-c-compat
            $<$<NOT:$<PLATFORM_ID:Linux>>:-Wno-address-of-packed-member>
            $<$<CONFIG:Release>:-ffast-math>)
endif()

    target_compile_options(launcher PRIVATE $<$<CONFIG:Release>:/fp:fast>)
