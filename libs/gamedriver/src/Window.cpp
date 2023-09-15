
#include "gamedriver/BaseLibs.h"
#include "gamedriver/Window.h"
#include "gamedriver/GameDriver.h"
#include "gamedriver/NativeWindowHelper.h"


Window::Window(filament::Engine* engine) :
    mRenderEngine(engine)
{
    mIsResizeable         = gConfigure.resizeable;
    mWindowTitle          = gConfigure.title;
    mWindowSize.x           = gConfigure.width;
    mWindowSize.y         = gConfigure.height;
    const int x             = SDL_WINDOWPOS_CENTERED;
    const int y =  SDL_WINDOWPOS_CENTERED;
    uint32_t  windowFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI;
    if (mIsResizeable)
    {
        windowFlags |= SDL_WINDOW_RESIZABLE;
    }

    // Even if we're in headless mode, we still need to create a window, otherwise SDL will not poll
    // events.
    mWindowHandle = SDL_CreateWindow(mWindowTitle.c_str(), x, y, mWindowSize.x, mWindowSize.y, windowFlags);
    mWindowID = SDL_GetWindowID(mWindowHandle);

    SDL_VERSION(&mSystemInfo.version);
    ASSERT_POSTCONDITION(SDL_GetWindowWMInfo(mWindowHandle, &mSystemInfo), "SDL version unsupported!");

    updateWindowInfo();
}
Window::~Window()
{
    SDL_DestroyWindow(mWindowHandle);
}

void Window::setup()
{
    resetSwapChain();

    mRenderer = mRenderEngine->createRenderer();
    // create cameras
    
}

void Window::cleanup()
{

    mRenderEngine->destroy(mRenderer);
    mRenderEngine->destroy(mSwapChain);
}

void Window::resetSwapChain()
{
    void* nativeWindow = getNativeWindow();
#if defined(__APPLE__)
    ::prepareNativeWindow(mWindowHandle);

    void* metalLayer = nullptr;
    if (mBackend == filament::Engine::Backend::METAL)
    {
        metalLayer = setUpMetalLayer(nativeWindow);
        // The swap chain on Metal is a CAMetalLayer.
        nativeSwapChain = metalLayer;
    }

#    if defined(FILAMENT_DRIVER_SUPPORTS_VULKAN)
    if (mBackend == filament::Engine::Backend::VULKAN)
    {
        // We request a Metal layer for rendering via MoltenVK.
        setUpMetalLayer(nativeWindow);
    }
#    endif

#endif
    mSwapChain = mRenderEngine->createSwapChain(nativeWindow);
}

void Window::fixupCoordinatesForHdpi(ssize_t& x, ssize_t& y)
{
    x = mDpiScale.x * x;
    y = mDpiScale.y * y;
}

void Window::fixupCoordinatesForHdpi(filament::math::vec2<ssize_t>& coordinate)
{
    coordinate.x = mDpiScale.x * coordinate.x;
    coordinate.y = mDpiScale.y * coordinate.y;
}

void Window::onResize()
{
    void* nativeWindow = getNativeWindow();

#if defined(__APPLE__)

    if (mBackend == filament::Engine::Backend::METAL)
    {
        resizeMetalLayer(nativeWindow);
    }

#    if defined(FILAMENT_DRIVER_SUPPORTS_VULKAN)
    if (mBackend == filament::Engine::Backend::VULKAN)
    {
        resizeMetalLayer(nativeWindow);
    }
#    endif

#endif

    updateWindowInfo();
}


void Window::updateWindowInfo()
{
    SDL_GL_GetDrawableSize(mWindowHandle, &mDrawableSize.x, &mDrawableSize.y);
    SDL_GetWindowSize(mWindowHandle, &mWindowSize.x, &mWindowSize.y);

    mDpiScale.x = (float)mDrawableSize.x / mWindowSize.x;
    mDpiScale.y = (float)mDrawableSize.y / mWindowSize.y;
}

