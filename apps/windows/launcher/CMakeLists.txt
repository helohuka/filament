cmake_minimum_required(VERSION 3.19)
project(launcher C ASM)


add_executable(launcher src/main.cpp)


# Ordering is significant in the following list. The PRIVATE qualifier prevents transitive deps.
target_link_libraries(launcher
    PRIVATE gamedriver
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


file(COPY ../../../third_party/textures DESTINATION ${PROJECT_BINARY_DIR})
add_custom_target(textures ALL DEPENDS textures)
add_dependencies(launcher textures)
#set_target_properties(textures PROPERTIES FOLDER Samples/Assets)

file(COPY ../../../assets DESTINATION ${PROJECT_BINARY_DIR}
        PATTERN "reference" EXCLUDE
        PATTERN "environments" EXCLUDE)
add_custom_target(assets ALL DEPENDS assets)
add_dependencies(launcher assets)
#set_target_properties(assets PROPERTIES FOLDER Samples/Assets)

add_custom_target(envs DEPENDS ${target_envmaps})
add_dependencies(launcher envs)
#set_target_properties(envs PROPERTIES FOLDER Samples/Assets)

