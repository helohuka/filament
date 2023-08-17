#ifndef __BASE_LIBS_H__
#define __BASE_LIBS_H__ 1


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
#if defined(WIN32)
#    include <SDL_syswm.h>
#    include <utils/unwindows.h>
#endif



// 数学库
#include <math/vec3.h>
#include <math/vec4.h>
#include <math/mat3.h>
#include <math/norm.h>

// 渲染库
#include <filament/Engine.h>
#include <filament/Viewport.h>
#include <filament/Camera.h>
#include <filament/IndexBuffer.h>
#include <filament/Material.h>
#include <filament/MaterialInstance.h>
#include <filament/RenderableManager.h>
#include <filament/Scene.h>
#include <filament/Skybox.h>
#include <filament/TransformManager.h>
#include <filament/VertexBuffer.h>
#include <filament/View.h>
#include <filament/Material.h>
#include <filament/Renderer.h>
#include <filament/ColorGrading.h>
#include <filament/Engine.h>
#include <filament/IndexBuffer.h>
#include <filament/RenderableManager.h>
#include <filament/Renderer.h>
#include <filament/Scene.h>
#include <filament/Skybox.h>
#include <filament/TransformManager.h>
#include <filament/VertexBuffer.h>
#include <filament/View.h>

#ifndef NDEBUG
#include <filament/DebugRegistry.h>
#endif

#include <camutils/Manipulator.h>

#include <utils/Panic.h>
#include <utils/Path.h>
#include <utils/Entity.h>
#include <utils/EntityManager.h>
#include <utils/NameComponentManager.h>

//命令行解析
#include <getopt/getopt.h>

#include <stb_image.h>
#include <imgui.h>
#include <filagui/ImGuiHelper.h>
#include <filagui/ImGuiExtensions.h>


#include <gltfio/AssetLoader.h>
#include <gltfio/FilamentAsset.h>
#include <gltfio/ResourceLoader.h>
#include <gltfio/TextureProvider.h>
#include "materials/uberarchive.h"

#include <viewer/AutomationEngine.h>
#include <viewer/AutomationSpec.h>
#include <viewer/ViewerGui.h>

#include <flatbuffers/idl.h>
#include <flatbuffers/flatbuffers.h>

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


#endif 