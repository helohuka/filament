#ifndef __BASELIBS_H__
#define __BASELIBS_H__ 1

/*!
 * \file BaseLibs.h
 * \date 2023.08.20
 *
 * \author Helohuka
 * 
 * Contact: helohuka@outlook.com
 *
 * \brief 
 *
 * TODO: long description
 *
 * \note
*/

// C++ 标准库

#include <stdio.h>
#include <iostream>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <cmath>
#include <array>
#include <fstream>
#include <sstream>
//curl 库
#include <curl/curl.h>

// SDL 库
#include <SDL.h>
#    include <SDL_syswm.h>
#if defined(WIN32)
#    include <utils/unwindows.h>
#endif

// 数学库
#include <math/vec2.h>
#include <math/vec3.h>
#include <math/vec4.h>
#include <math/mat3.h>
#include <math/mat4.h>
#include <math/norm.h>

// 渲染库
#include <filament/Engine.h>
#include <filament/Viewport.h>
#include <filament/Camera.h>
#include <filament/Material.h>
#include <filament/MaterialInstance.h>
#include <filament/RenderableManager.h>
#include <filament/VertexBuffer.h>
#include <filament/Renderer.h>
#include <filament/ColorGrading.h>
#include <filament/IndexBuffer.h>
#include <filament/Scene.h>
#include <filament/Skybox.h>
#include <filament/TransformManager.h>
#include <filament/LightManager.h>
#include <filament/Texture.h>
#include <filament/Box.h>
#include <filament/Fence.h>
#include <filament/IndirectLight.h>
#include <filament/View.h>
#include <filament/TextureSampler.h>
#ifndef NDEBUG
#include <filament/DebugRegistry.h>
#endif

#include <camutils/Manipulator.h>
#include <utils/FileUtils.h>
#include <utils/Log.h>
#include <utils/Panic.h>
#include <utils/Path.h>
#include <utils/Entity.h>
#include <utils/EntityManager.h>
#include <utils/compiler.h>
#include <utils/NameComponentManager.h>

//命令行解析
#include <getopt/getopt.h>

#include <stb_image.h>
#include <imgui.h>
#include "gui/ImGuiExtensions.h"

#include <gltfio/AssetLoader.h>
#include <gltfio/FilamentAsset.h>
#include <gltfio/ResourceLoader.h>
#include <gltfio/TextureProvider.h>
#include <gltfio/Animator.h>
#include <gltfio/NodeManager.h>

#include "materials/uberarchive.h"

#include "AutomationEngine.h"
#include "AutomationSpec.h"

#include "Settings.h"
//#include<viewer / ViewerGui.h>

//#include<flatbuffers / idl.h>
//#include <flatbuffers/flatbuffers.h>

//lua
#include <lua.h>
#include <luacode.h>
#include <lualib.h>

#include <Luau/Compiler.h>
#include <Luau/BytecodeBuilder.h>
#include <Luau/Parser.h>

enum MaterialSource
{
    JITSHADER,
    UBERSHADER,
};


using SceneMask = filament::gltfio::NodeManager::SceneMask;

#define SINGLE_INSTANCE_FLAG(T) \
public:\
    static inline T& get(){static T v; return v;} \
    static inline T* getp(){return &get();}\
    T(const T& rhs) = delete;\
    T(T&& rhs) = delete;\
    T& operator=(const T& rhs) = delete;\
    T& operator=(T&& rhs) = delete;\
    ~T();\
private:\
	T();


filament::math::mat4f   fitIntoUnitCube(const filament::Aabb& bounds, float zoffset);
std::ifstream::pos_type getFileSize(const char* filename);
bool                    loadSettings(const char* filename, filament::viewer::Settings* out);
UTILS_NOINLINE bool     notequal(float a, float b) noexcept;
// we do this to circumvent -ffast-math ignoring NaNs
bool is_not_a_number(float v) noexcept;

#endif // __BASELIBS_H__
