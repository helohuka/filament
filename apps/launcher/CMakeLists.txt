cmake_minimum_required(VERSION 3.19)
project(launcher C ASM)


set(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)

add_executable(launcher src/main.cpp)


# Ordering is significant in the following list. The PRIVATE qualifier prevents transitive deps.
target_link_libraries(launcher
    PRIVATE gamedriver
)


# ==================================================================================================
# Invoke cmgen to build KTX files for the default IBL and skybox
# ==================================================================================================

set(CMGEN_ARGS --quiet --format=ktx --size=256 --extract-blur=0.1)

function(add_envmap SOURCE TARGET)
    message( "${ROOT_DIR}/${SOURCE}")
    set(source_envmap "${ROOT_DIR}/${SOURCE}")

    set(target_skybox "${PROJECT_BINARY_DIR}/assets/ibl/${TARGET}/${TARGET}_skybox.ktx")
    set(target_envmap "${PROJECT_BINARY_DIR}/assets/ibl/${TARGET}/${TARGET}_ibl.ktx")
    
     message( "${PROJECT_BINARY_DIR}/assets/ibl/${TARGET}/${TARGET}_skybox.ktx")

    set(target_envmaps ${target_envmaps} ${target_skybox} PARENT_SCOPE)
    set(target_envmaps ${target_envmaps} ${target_envmap} PARENT_SCOPE)

    add_custom_command(OUTPUT ${target_skybox} ${target_envmap}
        COMMAND cmgen -x assets/ibl/${TARGET} ${CMGEN_ARGS} ${source_envmap}
        MAIN_DEPENDENCY ${source_envmap}
        DEPENDS cmgen
        COMMENT "Generating environment map ${target_envmap}")
endfunction()

add_envmap("third_party/environments/flower_road_2k.hdr"            "flower_road_2k")
add_envmap("third_party/environments/flower_road_no_sun_2k.hdr"     "flower_road_no_sun_2k")
add_envmap("third_party/environments/graffiti_shelter_2k.hdr"       "graffiti_shelter_2k")
add_envmap("third_party/environments/lightroom_14b.hdr"             "lightroom_14b")
add_envmap("third_party/environments/noon_grass_2k.hdr"             "noon_grass_2k")
add_envmap("third_party/environments/parking_garage_2k.hdr"         "parking_garage_2k")
add_envmap("third_party/environments/pillars_2k.hdr"                "pillars_2k")
add_envmap("third_party/environments/studio_small_02_2k.hdr"        "studio_small_02_2k")
add_envmap("third_party/environments/syferfontein_18d_clear_2k.hdr" "syferfontein_18d_clear_2k")
add_envmap("third_party/environments/the_sky_is_on_fire_2k.hdr"     "the_sky_is_on_fire_2k")
add_envmap("third_party/environments/venetian_crossroads_2k.hdr"    "venetian_crossroads_2k")
add_envmap("third_party/environments/earth.png"                     "earth")
add_envmap("assets/environments/debug/debug.png"                    "debug")
add_envmap("assets/environments/white_furnace/white_furnace.exr"    "white_furnace")
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

file(COPY "${ROOT_DIR}/third_party/textures" DESTINATION ${PROJECT_BINARY_DIR}/assets)
add_custom_target(textures ALL DEPENDS textures)
add_dependencies(launcher textures)
set_target_properties(textures PROPERTIES FOLDER Assets)

file(COPY "${ROOT_DIR}/third_party/models" DESTINATION ${PROJECT_BINARY_DIR}/assets)
add_custom_target(models ALL DEPENDS models)
add_dependencies(launcher models)
set_target_properties(models PROPERTIES FOLDER Assets)

file(COPY "${ROOT_DIR}/assets" DESTINATION ${PROJECT_BINARY_DIR}
        PATTERN "reference" EXCLUDE
        PATTERN "environments" EXCLUDE)
add_custom_target(assets ALL DEPENDS assets)
add_dependencies(launcher assets)
set_target_properties(assets PROPERTIES FOLDER Assets)

add_custom_target(envs DEPENDS ${target_envmaps})
add_dependencies(launcher envs)
set_target_properties(envs PROPERTIES FOLDER Assets)

