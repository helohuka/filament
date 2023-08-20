
#include "gamedriver/BaseLibs.h"
#include "gamedriver/GameDriver.h"
#include "gamedriver/NativeWindowHelper.h"

using namespace filament;
using namespace filagui;
using namespace filament::math;
using namespace utils;
// ------------------------------------------------------------------------------------------------

void GameDriver::initWindow() 
   
{

     mIsResizeable = mConfig.resizeable;
    mIsSplitView  = mConfig.splitView;
     mWindowTitle  = mConfig.title;
    mWidth         = mConfig.width; 
     mHeight        = mConfig.height;
    mBackend       = mConfig.backend;

    const int x           = SDL_WINDOWPOS_CENTERED;
    const int y           = SDL_WINDOWPOS_CENTERED;
    uint32_t  windowFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI;
    if (mIsResizeable)
    {
        windowFlags |= SDL_WINDOW_RESIZABLE;
    }

    // Even if we're in headless mode, we still need to create a window, otherwise SDL will not poll
    // events.
    mWindow = SDL_CreateWindow(mWindowTitle.c_str(), x, y, mWidth, mHeight, windowFlags);

    // Create the Engine after the window in case this happens to be a single-threaded platform.
    // For single-threaded platforms, we need to ensure that Filament's OpenGL context is
    // current, rather than the one created by SDL.
    mEngine = Engine::create(mBackend);

    // get the resolved backend
    mBackend = mEngine->getBackend();

    void* nativeSwapChain = getNativeWindow();


#if defined(__APPLE__)
    ::prepareNativeWindow(mWindow);

    void* metalLayer = nullptr;
    if (config.backend == filament::Engine::Backend::METAL)
    {
        metalLayer = setUpMetalLayer(nativeWindow);
        // The swap chain on Metal is a CAMetalLayer.
        nativeSwapChain = metalLayer;
    }

#    if defined(FILAMENT_DRIVER_SUPPORTS_VULKAN)
    if (config.backend == filament::Engine::Backend::VULKAN)
    {
        // We request a Metal layer for rendering via MoltenVK.
        setUpMetalLayer(nativeWindow);
    }
#    endif

#endif

    // Select the feature level to use
    //config.featureLevel = std::min(config.featureLevel,
    //        mGameDriver->mEngine->getSupportedFeatureLevel());
    mEngine->setActiveFeatureLevel(mConfig.featureLevel);

    mSwapChain = mEngine->createSwapChain(nativeSwapChain);

    mRenderer = mEngine->createRenderer();

    // create cameras
    utils::EntityManager& em = utils::EntityManager::get();
    em.create(3, mCameraEntities);
    mCameras[0] = mMainCamera = mEngine->createCamera(mCameraEntities[0]);
    mCameras[1] = mDebugCamera = mEngine->createCamera(mCameraEntities[1]);
    mCameras[2] = mOrthoCamera = mEngine->createCamera(mCameraEntities[2]);

    // set exposure
    for (auto camera : mCameras)
    {
        camera->setExposure(16.0f, 1 / 125.0f, 100.0f);
    }

    // create views
    mViews.emplace_back(mMainView = new CView(*mRenderer, "Main View"));
    if (mIsSplitView)
    {
        mViews.emplace_back(mDepthView = new CView(*mRenderer, "Depth View"));
        mViews.emplace_back(mGodView = new GodView(*mRenderer, "God View"));
        mViews.emplace_back(mOrthoView = new CView(*mRenderer, "Shadow View"));
    }
    mViews.emplace_back(mUiView = new CView(*mRenderer, "UI View"));

    // set-up the camera manipulators
    mMainCameraMan = CameraManipulator::Builder()
                         .targetPosition(0, 0, -4)
                         .flightMoveDamping(15.0)
                         .build(mConfig.cameraMode);
    mDebugCameraMan = CameraManipulator::Builder()
                          .targetPosition(0, 0, -4)
                          .flightMoveDamping(15.0)
                          .build(mConfig.cameraMode);

    mMainView->setCamera(mMainCamera);
    mMainView->setCameraManipulator(mMainCameraMan);
    if (mIsSplitView)
    {
        // Depth view always uses the main camera
        mDepthView->setCamera(mMainCamera);
        mDepthView->setCameraManipulator(mMainCameraMan);

        // The god view uses the main camera for culling, but the debug camera for viewing
        mGodView->setCamera(mMainCamera);
        mGodView->setGodCamera(mDebugCamera);
        mGodView->setCameraManipulator(mDebugCameraMan);

        // Ortho view obviously uses an ortho camera
        mOrthoView->setCamera((Camera*)mMainView->getView()->getDirectionalLightCamera());
    }

    // configure the cameras
    configureCamerasForWindow();

    mMainCamera->lookAt({4, 0, -4}, {0, 0, -4}, {0, 1, 0});
}

void GameDriver::releaseWindow()
{
    mViews.clear();
    utils::EntityManager& em = utils::EntityManager::get();
    for (auto e : mCameraEntities)
    {
        mEngine->destroyCameraComponent(e);
        em.destroy(e);
    }
    mEngine->destroy(mRenderer);
    mEngine->destroy(mSwapChain);
    SDL_DestroyWindow(mWindow);
    delete mMainCameraMan;
    delete mDebugCameraMan;
}

void* GameDriver::getNativeWindow()
{
    SDL_SysWMinfo wmi;
    SDL_VERSION(&wmi.version);
    ASSERT_POSTCONDITION(SDL_GetWindowWMInfo(mWindow, &wmi), "SDL version unsupported!");
    HWND win = (HWND)wmi.info.win.window;
    return (void*)win;
}

void* GameDriver::getNativeSurface()
{
    //SDL_SysWMinfo wmi;
    //SDL_VERSION(&wmi.version);
    //ASSERT_POSTCONDITION(SDL_GetWindowWMInfo(mWindow, &wmi), "SDL version unsupported!");

    //return wmi.info.;

    return nullptr;
}

void GameDriver::mouseDown(int button, ssize_t x, ssize_t y)
{
    fixupMouseCoordinatesForHdpi(x, y);
    y = mHeight - y;
    for (auto const& view : mViews) {
        if (view->intersects(x, y)) {
            mMouseEventTarget = view.get();
            view->mouseDown(button, x, y);
            break;
        }
    }
}

void GameDriver::mouseWheel(ssize_t x)
{
    if (mMouseEventTarget) {
        mMouseEventTarget->mouseWheel(x);
    } else {
        for (auto const& view : mViews) {
            if (view->intersects(mLastX, mLastY)) {
                view->mouseWheel(x);
                break;
            }
        }
    }
}

void GameDriver::mouseUp(ssize_t x, ssize_t y)
{
    fixupMouseCoordinatesForHdpi(x, y);
    if (mMouseEventTarget) {
        y = mHeight - y;
        mMouseEventTarget->mouseUp(x, y);
        mMouseEventTarget = nullptr;
    }
}

void GameDriver::mouseMoved(ssize_t x, ssize_t y)
{
    fixupMouseCoordinatesForHdpi(x, y);
    y = mHeight - y;
    if (mMouseEventTarget) {
        mMouseEventTarget->mouseMoved(x, y);
    }
    mLastX = x;
    mLastY = y;
}

void GameDriver::keyDown(SDL_Scancode key)
{
    auto& eventTarget = mKeyEventTarget[key];

    // keyDown events can be sent multiple times per key (for key repeat)
    // If this key is already down, do nothing.
    if (eventTarget)
    {
        return;
    }

    // Decide which view will get this key's corresponding keyUp event.
    // If we're currently in a mouse grap session, it should be the mouse grab's target view.
    // Otherwise, it should be whichever view we're currently hovering over.
    CView* targetView = nullptr;
    if (mMouseEventTarget)
    {
        targetView = mMouseEventTarget;
    }
    else
    {
        for (auto const& view : mViews)
        {
            if (view->intersects(mLastX, mLastY))
            {
                targetView = view.get();
                break;
            }
        }
    }

    if (targetView)
    {
        targetView->keyDown(key);
        eventTarget = targetView;
    }
}

void GameDriver::keyUp(SDL_Scancode key)
{
    auto& eventTarget = mKeyEventTarget[key];
    if (!eventTarget) {
        return;
    }
    eventTarget->keyUp(key);
    eventTarget = nullptr;
}

void GameDriver::fixupMouseCoordinatesForHdpi(ssize_t& x, ssize_t& y) const
{
    int dw, dh, ww, wh;
    SDL_GL_GetDrawableSize(mWindow, &dw, &dh);
    SDL_GetWindowSize(mWindow, &ww, &wh);
    x = x * dw / ww;
    y = y * dh / wh;
}

void GameDriver::resize()
{
    void* nativeWindow = getNativeWindow();

#if defined(__APPLE__)

    if (mBackend == filament::Engine::Backend::METAL) {
        resizeMetalLayer(nativeWindow);
    }

#if defined(FILAMENT_DRIVER_SUPPORTS_VULKAN)
    if (mBackend == filament::Engine::Backend::VULKAN) {
        resizeMetalLayer(nativeWindow);
    }
#endif

#endif

    configureCamerasForWindow();

    // Call the resize callback, if this GameDriver has one. This must be done after
    // configureCamerasForWindow, so the viewports are correct.
    if (mResize) {
        mResize(mEngine, mMainView->getView());
    }
}

void GameDriver::configureCamerasForWindow()
{
    float dpiScaleX = 1.0f;
    float dpiScaleY = 1.0f;

    // If the app is not headless, query the window for its physical & virtual sizes.

    SDL_GL_GetDrawableSize(mWindow, &mWidth, &mHeight);
 
    int virtualWidth, virtualHeight;
    SDL_GetWindowSize(mWindow, &virtualWidth, &virtualHeight);
    dpiScaleX = (float)mWidth / virtualWidth;
    dpiScaleY = (float)mHeight / virtualHeight;


    const float3 at(0, 0, -4);
    const double ratio   = double(mHeight) / double(mWidth);
    const int    sidebar = mSidebarWidth * dpiScaleX;

    const bool splitview = mViews.size() > 2;

    // To trigger a floating-point exception, users could shrink the window to be smaller than
    // the sidebar. To prevent this we simply clamp the width of the main viewport.
    const uint32_t mainWidth = splitview ? mWidth : std::max(1, (int)mWidth - sidebar);

    double near = 0.1;
    double far  = 100;
    mMainCamera->setLensProjection(mCameraFocalLength, double(mainWidth) / mHeight, near, far);
    mDebugCamera->setProjection(45.0, double(mWidth) / mHeight, 0.0625, 4096, Camera::Fov::VERTICAL);

    // We're in split view when there are more views than just the Main and UI views.
    if (splitview)
    {
        uint32_t vpw = mWidth / 2;
        uint32_t vph = mHeight / 2;
        mMainView->setViewport({0, 0, vpw, vph});
        mDepthView->setViewport({int32_t(vpw), 0, mWidth - vpw, vph});
        mGodView->setViewport({int32_t(vpw), int32_t(vph), mWidth - vpw, mHeight - vph});
        mOrthoView->setViewport({0, int32_t(vph), vpw, mHeight - vph});
    }
    else
    {
        mMainView->setViewport({sidebar, 0, (uint32_t)mainWidth, (uint32_t)mHeight});
    }
    mUiView->setViewport({0, 0, (uint32_t)mWidth, (uint32_t)mHeight});
}
