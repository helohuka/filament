
#include "gamedriver/BaseLibs.h"
#include "gamedriver/GameDriver.h"
#include "gamedriver/NativeWindowHelper.h"


void GameDriver::setupWindow()
{

    const int x           = SDL_WINDOWPOS_CENTERED;
    const int y           = SDL_WINDOWPOS_CENTERED;
    uint32_t  windowFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI;
    if (gConfigure.resizeable)
    {
        windowFlags |= SDL_WINDOW_RESIZABLE;
    }

    // Even if we're in headless mode, we still need to create a window, otherwise SDL will not poll
    // events.
    mWindow = SDL_CreateWindow(gConfigure.title.c_str(), x, y, gConfigure.width, gConfigure.height, windowFlags);

    SDL_GL_GetDrawableSize(mWindow, &mWidth, &mHeight);
    int virtualWidth, virtualHeight;
    SDL_GetWindowSize(mWindow, &virtualWidth, &virtualHeight);
    mDpiScaleX = (float)mWidth / virtualWidth;
    mDpiScaleY = (float)mHeight / virtualHeight;

    setupRendererAndSwapchain();
}
void GameDriver::cleanupWindow()
{
    mRenderEngine->destroy(mRenderer);
    mRenderEngine->destroy(mSwapChain);
    SDL_DestroyWindow(mWindow);
}
void GameDriver::setupRendererAndSwapchain()
{
    resetSwapChain();

    mRenderer = mRenderEngine->createRenderer();

}
void GameDriver::resetSwapChain()
{
    void* nativeWindow = getNativeWindow(mWindow);
#if defined(__APPLE__)
    ::prepareNativeWindow(mWindow);

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
void GameDriver::fixupCoordinatesForHdpi(ssize_t& x, ssize_t& y)
{
    x = x * mDpiScaleX;
    y = y * mDpiScaleY;
}
void GameDriver::onWindowResize()
{
    void* nativeWindow = getNativeWindow(mWindow);

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

    SDL_GL_GetDrawableSize(mWindow, &mWidth, &mHeight);
    int virtualWidth, virtualHeight;
    SDL_GetWindowSize(mWindow, &virtualWidth, &virtualHeight);
    mDpiScaleX = (float)mWidth / virtualWidth;
    mDpiScaleY = (float)mHeight / virtualHeight;

    configureCamerasForWindow();
}


