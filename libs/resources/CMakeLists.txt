cmake_minimum_required(VERSION 3.19)
project(embed-resources C ASM)


set(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set(GENERATION_ROOT ${CMAKE_CURRENT_BINARY_DIR})
set(RESOURCE_DIR  "${GENERATION_ROOT}/generated/resources")
set(MATERIAL_DIR  "${GENERATION_ROOT}/generated/material")
set(TEXTURE_DIR "${GENERATION_ROOT}/generated/texture")
set(RESOURCE_BINS)

# ==================================================================================================
# Build materials
# ==================================================================================================

#set(MATERIAL_SRCS
#        materials/bakedColor.mat
#        materials/bakedTexture.mat
#        materials/pointSprites.mat
#        materials/aoPreview.mat
#        materials/groundShadow.mat
#        materials/heightfield.mat
#        materials/image.mat
#        materials/mirror.mat
#        materials/overdraw.mat
#        materials/sandboxCloth.mat
#        materials/sandboxLit.mat
#        materials/sandboxLitFade.mat
#        materials/sandboxLitTransparent.mat
#        materials/sandboxLitThinRefraction.mat
#        materials/sandboxLitThinRefractionSsr.mat
#        materials/sandboxLitSolidRefraction.mat
#        materials/sandboxLitSolidRefractionSsr.mat
#        materials/sandboxSpecGloss.mat
#        materials/sandboxSubsurface.mat
#        materials/sandboxUnlit.mat
#        materials/texturedLit.mat
#)

if (CMAKE_CROSSCOMPILING)
    include(${IMPORT_EXECUTABLES})
endif()

#file(MAKE_DIRECTORY ${MATERIAL_DIR})
#
#foreach (mat_src ${MATERIAL_SRCS})
#    get_filename_component(localname "${mat_src}" NAME_WE)
#    get_filename_component(fullname "${mat_src}" ABSOLUTE)
#    set(output_path "${MATERIAL_DIR}/${localname}.filamat")
#    add_custom_command(
#            OUTPUT ${output_path}
#            COMMAND matc ${MATC_BASE_FLAGS} -o ${output_path} ${fullname}
#            MAIN_DEPENDENCY ${mat_src}
#            DEPENDS matc
#            COMMENT "Compiling material [ ${MATC_BASE_FLAGS} ] ${mat_src} to ${output_path}"
#    )
#    list(APPEND RESOURCE_BINS ${output_path})
#endforeach()
# ==================================================================================================
# Build embed resources
# ==================================================================================================

file(MAKE_DIRECTORY ${RESOURCE_DIR})

get_resgen_vars(${RESOURCE_DIR} resources)

#add_custom_command(
#        OUTPUT ${RESGEN_OUTPUTS}
#        COMMAND resgen ${RESGEN_FLAGS} ${RESOURCE_BINS}
#        DEPENDS resgen ${RESOURCE_BINS}
#        COMMENT "Aggregating resources"
#)
#
#if (DEFINED RESGEN_SOURCE_FLAGS)
#    set_source_files_properties(${RESGEN_SOURCE} PROPERTIES COMPILE_FLAGS ${RESGEN_SOURCE_FLAGS})
#endif()
#
## CMake fails to invoke ar on Windows unless there is at least one C/C++ file in the library.
#set(DUMMY_SRC "${RESOURCE_DIR}/dummy.c")
#add_custom_command(OUTPUT ${DUMMY_SRC} COMMAND echo "//" > ${DUMMY_SRC})
#
#add_library(embed-resources ${DUMMY_SRC} ${RESGEN_SOURCE})
#set_target_properties(embed-resources PROPERTIES FOLDER Libs/Resources)
#target_include_directories(embed-resources PUBLIC ${GENERATION_ROOT})
# ==================================================================================================
# Invoke cmgen to build KTX files for the default IBL and skybox
# ==================================================================================================

if(ANDROID)
    set(CMGEN_ARGS --quiet --format=rgb32f --size=256 --extract-blur=0.08)
else()
    set(CMGEN_ARGS --quiet --format=ktx --size=256 --extract-blur=0.1)
endif()
function(add_envmap SOURCE TARGET)
	set(source_envmap "${ROOT_DIR}/${SOURCE}")

    set(target_skybox "${PROJECT_BINARY_DIR}/assets/ibl/${TARGET}/${TARGET}_skybox.ktx")
    set(target_envmap "${PROJECT_BINARY_DIR}/assets/ibl/${TARGET}/${TARGET}_ibl.ktx")

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
# Build suzanne resources
# ==================================================================================================


function(add_ktxfiles SOURCE TARGET EXTRA_ARGS)
    set(source_path "${ROOT_DIR}/${SOURCE}")
    set(target_path "${TEXTURE_DIR}/${TARGET}")
    set(MONKEY_FILES ${MONKEY_FILES} ${target_path} PARENT_SCOPE)
    add_custom_command(
            OUTPUT ${target_path}
            COMMAND mipgen --quiet ${EXTRA_ARGS} ${source_path} ${target_path}
            MAIN_DEPENDENCY ${source_path}
            DEPENDS mipgen
            COMMENT "Generating mip levels for ${source_path}")
endfunction()

function(add_pngfile SOURCE TARGET)
    set(source_path "${ROOT_DIR}/${SOURCE}")
    set(target_path "${TEXTURE_DIR}/${TARGET}")
    set(MONKEY_FILES ${MONKEY_FILES} ${target_path} PARENT_SCOPE)
    add_custom_command(
            OUTPUT ${target_path}
            COMMAND ${CMAKE_COMMAND} -E copy ${source_path} ${target_path}
            MAIN_DEPENDENCY ${source_path})
endfunction()

function(add_mesh SOURCE TARGET)
    set(source_mesh "${ROOT_DIR}/${SOURCE}")
    set(target_mesh "${RESOURCE_DIR}/${TARGET}")
    set(MONKEY_FILES ${MESH_BINS} ${target_mesh} PARENT_SCOPE)
    add_custom_command(
            OUTPUT ${target_mesh}
            COMMAND filamesh --compress ${source_mesh} ${target_mesh}
            MAIN_DEPENDENCY ${source_mesh}
            DEPENDS filamesh)
endfunction()

add_mesh("assets/models/monkey/monkey.obj" "suzanne.filamesh")

set (COMPRESSION "--compression=uastc")
add_ktxfiles("assets/models/monkey/albedo.png" "albedo.ktx2" "${COMPRESSION}")
add_ktxfiles("assets/models/monkey/roughness.png" "roughness.ktx2" "${COMPRESSION};--grayscale;--linear")
add_ktxfiles("assets/models/monkey/metallic.png" "metallic.ktx2" "${COMPRESSION};--grayscale;--linear")
add_ktxfiles("assets/models/monkey/ao.png" "ao.ktx2" "${COMPRESSION};--grayscale;--linear")

add_pngfile("assets/models/monkey/normal.png" "normal.png")

get_resgen_vars(${RESOURCE_DIR} monkey)

add_custom_command(
        OUTPUT ${RESGEN_OUTPUTS}
        COMMAND resgen ${RESGEN_FLAGS} ${MONKEY_FILES}
        DEPENDS resgen ${MONKEY_FILES}
        COMMENT "Aggregating monkey resources"
)

if (DEFINED RESGEN_SOURCE_FLAGS)
    set_source_files_properties(${RESGEN_SOURCE} PROPERTIES COMPILE_FLAGS ${RESGEN_SOURCE_FLAGS})
endif()

add_library(suzanne-resources ${DUMMY_SRC} ${RESGEN_SOURCE})
set_target_properties(suzanne-resources PROPERTIES FOLDER Libs/Resources)

# ==================================================================================================
# Resources for gltf_viewer and gltf_instances
# ==================================================================================================

#add_library(gltf-demo-resources ${DUMMY_SRC})
#set_target_properties(gltf-demo-resources PROPERTIES FOLDER Libs/Resources)
#
#set(GLTF_DEMO_RESOURCES
#        ${ROOT_DIR}/third_party/models/DamagedHelmet/DamagedHelmet.glb
#        ${MATERIAL_DIR}/groundShadow.filamat
#        ${MATERIAL_DIR}/overdraw.filamat
#        )
#
#get_resgen_vars(${RESOURCE_DIR} gltf_demo)
#
#add_custom_command(
#        OUTPUT ${RESGEN_OUTPUTS}
#        COMMAND resgen ${RESGEN_FLAGS} ${GLTF_DEMO_RESOURCES}
#        DEPENDS resgen ${GLTF_DEMO_RESOURCES}
#)
#
#if (DEFINED RESGEN_SOURCE_FLAGS)
#    set_source_files_properties(${RESGEN_SOURCE} PROPERTIES COMPILE_FLAGS ${RESGEN_SOURCE_FLAGS})
#endif()
#
#target_sources(gltf-demo-resources PRIVATE ${RESGEN_SOURCE})

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


