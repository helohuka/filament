
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
    const int x           = SDL_WINDOWPOS_CENTERED;
    const int y           = SDL_WINDOWPOS_CENTERED;
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
    utils::EntityManager& em = utils::EntityManager::get();
    em.create(3, mCameraEntities);
    mCameras[0] = mMainCamera = mRenderEngine->createCamera(mCameraEntities[0]);
    mCameras[1] = mDebugCamera = mRenderEngine->createCamera(mCameraEntities[1]);
    mCameras[2] = mOrthoCamera = mRenderEngine->createCamera(mCameraEntities[2]);

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
        mViews.emplace_back(mGodView = new CView(*mRenderer, "God View"));
        mViews.emplace_back(mOrthoView = new CView(*mRenderer, "Shadow View"));
    }
    mViews.emplace_back(mUiView = new CView(*mRenderer, "UI View"));

    // set-up the camera manipulators
    mMainCameraMan = CameraManipulator::Builder()
                         .targetPosition(0, 0, -4)
                         .flightMoveDamping(15.0)
                         .build(gConfigure.cameraMode);
    mDebugCameraMan = CameraManipulator::Builder()
                          .targetPosition(0, 0, -4)
                          .flightMoveDamping(15.0)
                          .build(gConfigure.cameraMode);

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
        mOrthoView->setCamera((filament::Camera*)mMainView->getView()->getDirectionalLightCamera());
    }

    configureCamerasForWindow();

    mMainCamera->lookAt({4, 0, -4}, {0, 0, -4}, {0, 1, 0});
}

void Window::cleanup()
{
    mViews.clear();
    utils::EntityManager& em = utils::EntityManager::get();
    for (auto e : mCameraEntities)
    {
        mRenderEngine->destroyCameraComponent(e);
        em.destroy(e);
    }

    mRenderEngine->destroy(mRenderer);
    mRenderEngine->destroy(mSwapChain);

    delete mMainCameraMan;
    delete mDebugCameraMan;
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

    configureCamerasForWindow();
}

void Window::configureCamerasForWindow()
{
    
    const filament::math::float3 at(0, 0, -4);
    const double                 ratio   = double(mWindowSize.y) / double(mWindowSize.x);
    const int                    sidebar = mSidebarWidth * mDpiScale.x;

    const bool splitview = mViews.size() > 2;

    // To trigger a floating-point exception, users could shrink the window to be smaller than
    // the sidebar. To prevent this we simply clamp the width of the main viewport.
    const uint32_t mainWidth = splitview ? mWindowSize.x - sidebar : std::max(1, (int)mWindowSize.x - sidebar);

    double near = 0.1;
    double far  = 100;
    mMainCamera->setLensProjection(mCameraFocalLength, double(mainWidth) / mWindowSize.y, near, far);
    mDebugCamera->setProjection(45.0, double(mainWidth) / mWindowSize.y, 0.0625, 4096, filament::Camera::Fov::VERTICAL);

    // We're in split view when there are more views than just the Main and UI views.
    if (splitview)
    {
        uint32_t vpw = mainWidth / 2;
        uint32_t vph = mWindowSize.y / 2;
        mMainView->setViewport({sidebar, 0, vpw, vph});
        mDepthView->setViewport({int32_t(sidebar + vpw), 0, vpw, vph});
        mGodView->setViewport({int32_t(sidebar + vpw), int32_t(vph), vpw, mWindowSize.y - vph});
        mOrthoView->setViewport({sidebar, int32_t(vph), vpw, mWindowSize.y - vph});
    }
    else
    {
        mMainView->setViewport({sidebar, 0, (uint32_t)mainWidth, (uint32_t)mWindowSize.y});
    }
    mUiView->setViewport({0, 0, (uint32_t)mWindowSize.x, (uint32_t)mWindowSize.y});


     {
        auto              view   = mMainView->getView();
        filament::Camera& camera = view->getCamera();
        if (&camera == mMainCamera)
        {
            // Don't adjust the aspect ratio of the main camera, this is done inside of
            // GameDriver.cpp
            return;
        }
        const filament::Viewport& vp          = view->getViewport();
        double                    aspectRatio = (double)vp.width / vp.height;
        camera.setScaling({1.0 / aspectRatio, 1.0});
    }
}

void Window::updateWindowInfo()
{
    SDL_GL_GetDrawableSize(mWindowHandle, &mDrawableSize.x, &mDrawableSize.y);
    SDL_GetWindowSize(mWindowHandle, &mWindowSize.x, &mWindowSize.y);

    mDpiScale.x = (float)mDrawableSize.x / mWindowSize.x;
    mDpiScale.y = (float)mDrawableSize.y / mWindowSize.y;
}

void Window::onMouseDown(int button, ssize_t x, ssize_t y)
{
    fixupCoordinatesForHdpi(x, y);
    y = mDrawableSize.y - y;
    for (auto const& view : mViews) {
        if (view->intersects(x, y)) {
            mMouseEventTarget = view.get();
            view->mouseDown(button, x, y);
            break;
        }
    }
}

void Window::onMouseWheel(ssize_t x)
{
    if (mMouseEventTarget) {
        mMouseEventTarget->mouseWheel(x);
    } else {
        for (auto const& view : mViews)
        {
            if (view->intersects(mLastX, mLastY)) {
                view->mouseWheel(x);
                break;
            }
        }
    }
}

void Window::onMouseUp(ssize_t x, ssize_t y)
{
    fixupCoordinatesForHdpi(x, y);
    if (mMouseEventTarget) {
        y = mDrawableSize.y - y;
        mMouseEventTarget->mouseUp(x, y);
        mMouseEventTarget = nullptr;
    }
}

void Window::onMouseMoved(ssize_t x, ssize_t y)
{
    fixupCoordinatesForHdpi(x, y);
    y = mDrawableSize.y - y;
    if (mMouseEventTarget) {
        mMouseEventTarget->mouseMoved(x, y);
    }
    mLastX = x;
    mLastY = y;
}

void Window::onKeyDown(SDL_Scancode key)
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

void Window::onKeyUp(SDL_Scancode key)
{
    auto& eventTarget = mKeyEventTarget[key];
    if (!eventTarget) {
        return;
    }
    eventTarget->keyUp(key);
    eventTarget = nullptr;
}


// ------------------------------------------------------------------------------------------------

void GameDriver::initWindow()
{
    mBackend      = gConfigure.backend;
    mFeatureLevel = gConfigure.featureLevel;

    // Create the Engine after the window in case this happens to be a single-threaded platform.
    // For single-threaded platforms, we need to ensure that Filament's OpenGL context is
    // current, rather than the one created by SDL.
    mRenderEngine = filament::Engine::create(mBackend);
    // Select the feature level to use
    mFeatureLevel = std::min(mFeatureLevel, mRenderEngine->getSupportedFeatureLevel());
    mRenderEngine->setActiveFeatureLevel(mFeatureLevel);
    // get the resolved backend
    mBackend = mRenderEngine->getBackend();

    mMainWindow = std::make_unique<Window>(mRenderEngine);

    mMainWindow->setup();
}

void GameDriver::releaseWindow()
{
    mMainWindow->cleanup();
}
