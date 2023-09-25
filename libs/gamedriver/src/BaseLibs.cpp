#include "gamedriver/BaseLibs.h"

void* getNativeWindow(SDL_Window* sdlWindow)
{
    SDL_SysWMinfo wmi;
    SDL_VERSION(&wmi.version);
    ASSERT_POSTCONDITION(SDL_GetWindowWMInfo(sdlWindow, &wmi), "SDL version unsupported!");
    HWND win = (HWND)wmi.info.win.window;
    return (void*)win;
}

filament::math::mat4f fitIntoUnitCube(const filament::Aabb& bounds, float zoffset)
{
    filament::math::float3 minpt = bounds.min;
    filament::math::float3 maxpt = bounds.max;
    float                  maxExtent;
    maxExtent                          = std::max(maxpt.x - minpt.x, maxpt.y - minpt.y);
    maxExtent                          = std::max(maxExtent, maxpt.z - minpt.z);
    float                  scaleFactor = 2.0f / maxExtent;
    filament::math::float3 center      = (minpt + maxpt) / 2.0f;
    center.z += zoffset / scaleFactor;
    return filament::math::mat4f::scaling(filament::math::float3(scaleFactor)) * filament::math::mat4f::translation(-center);
}

std::ifstream::pos_type getFileSize(const char* filename)
{
    std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);
    return in.tellg();
}

bool loadSettings(const char* filename, filament::viewer::Settings* out)
{
    auto contentSize = getFileSize(filename);
    if (contentSize <= 0)
    {
        return false;
    }
    std::ifstream     in(filename, std::ifstream::binary | std::ifstream::in);
    std::vector<char> json(static_cast<unsigned long>(contentSize));
    if (!in.read(json.data(), contentSize))
    {
        return false;
    }
    filament::viewer::JsonSerializer serializer;
    return serializer.readJson(json.data(), contentSize, out);
}

bool notequal(float a, float b) noexcept
{
    return a != b;
}

// we do this to circumvent -ffast-math ignoring NaNs
bool is_not_a_number(float v) noexcept
{
    return notequal(v, v);
}
