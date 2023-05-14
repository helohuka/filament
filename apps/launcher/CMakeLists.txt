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

add_envmap("third_party/environments/lightroom_14b.hdr" "lightroom_14b")


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

file(COPY "${ROOT_DIR}/third_party/textures" DESTINATION ${PROJECT_BINARY_DIR})
add_custom_target(textures ALL DEPENDS textures)
add_dependencies(launcher textures)
set_target_properties(textures PROPERTIES FOLDER Assets)

file(COPY "${ROOT_DIR}/assets" DESTINATION ${PROJECT_BINARY_DIR}
        PATTERN "reference" EXCLUDE
        PATTERN "environments" EXCLUDE)
add_custom_target(assets ALL DEPENDS assets)
add_dependencies(launcher assets)
set_target_properties(assets PROPERTIES FOLDER Assets)

add_custom_target(envs DEPENDS ${target_envmaps})
add_dependencies(launcher envs)
set_target_properties(envs PROPERTIES FOLDER Assets)

