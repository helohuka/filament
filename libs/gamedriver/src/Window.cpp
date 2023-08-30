
#include "gamedriver/BaseLibs.h"
#include "gamedriver/Window.h"
#include "gamedriver/GameDriver.h"
#include "gamedriver/NativeWindowHelper.h"


Window::Window(filament::Engine* engine) :
    mEngine(engine)
{
    mIsResizeable         = gConfigure.resizeable;
    mWindowTitle          = gConfigure.title;
    mWindowWidth                = gConfigure.width;
    mWindowHeight         = gConfigure.height;
    const int x           = SDL_WINDOWPOS_CENTERED;
    const int y           = SDL_WINDOWPOS_CENTERED;
    uint32_t  windowFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI;
    if (mIsResizeable)
    {
        windowFlags |= SDL_WINDOW_RESIZABLE;
    }

    // Even if we're in headless mode, we still need to create a window, otherwise SDL will not poll
    // events.
    mWindow   = SDL_CreateWindow(mWindowTitle.c_str(), x, y, mWindowWidth, mWindowHeight, windowFlags);
    mWindowID = SDL_GetWindowID(mWindow);

    SDL_VERSION(&mSystemInfo.version);
    ASSERT_POSTCONDITION(SDL_GetWindowWMInfo(mWindow, &mSystemInfo), "SDL version unsupported!");
}
Window::~Window()
{
    SDL_DestroyWindow(mWindow);
}

void Window::cleanupRenderer()
{
    mEngine->destroy(mRenderer);
    mEngine->destroy(mSwapChain);
}

void Window::setupRenderer()
{
    mRenderer = mEngine->createRenderer();
}

void Window::setupSwapChain()
{
    void* nativeWindow = getNativeWindow();
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
    mSwapChain = mEngine->createSwapChain(nativeWindow);
}

void Window::fixupCoordinatesForHdpi(filament::vec2<ssize_t>& coordinate)
{
    
    SDL_GL_GetDrawableSize(mWindow, &mDrawableWidth, &mDrawableHeight);
    SDL_GetWindowSize(mWindow, &mWindowWidth, &mWindowHeight);
    coordinate.x = coordinate.x * mDrawableWidth / mWindowWidth;
    coordinate.y = coordinate.y * mDrawableHeight / mWindowWidth;
}

void Window::fixupCoordinatesForHdpi(ssize_t& x, ssize_t& y)
{
    SDL_GL_GetDrawableSize(mWindow, &mDrawableWidth, &mDrawableHeight);
    SDL_GetWindowSize(mWindow, &mWindowWidth, &mWindowHeight);
    x = x * mDrawableWidth / mWindowWidth;
    y = y * mDrawableHeight / mWindowHeight;
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
}

// ------------------------------------------------------------------------------------------------

void GameDriver::initWindow()
{
    mBackend      = gConfigure.backend;
    mFeatureLevel = gConfigure.featureLevel;

     // Create the Engine after the window in case this happens to be a single-threaded platform.
    // For single-threaded platforms, we need to ensure that Filament's OpenGL context is
    // current, rather than the one created by SDL.
    mEngine = filament::Engine::create(mBackend);

    mMainWindow = std::make_unique<Window>(mEngine);

    // get the resolved backend
    mBackend = mEngine->getBackend();
    // Select the feature level to use
    mFeatureLevel = std::min(mFeatureLevel, mEngine->getSupportedFeatureLevel());
    mEngine->setActiveFeatureLevel(mFeatureLevel);

    mMainWindow->setupSwapChain();
    mMainWindow->setupRenderer();

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
    mViews.emplace_back(mMainView = new CView(*mMainWindow->getRenderer(), "Main View"));
    if (mIsSplitView)
    {
        mViews.emplace_back(mDepthView = new CView(*mMainWindow->getRenderer(), "Depth View"));
        mViews.emplace_back(mGodView = new CView(*mMainWindow->getRenderer(), "God View"));
        mViews.emplace_back(mOrthoView = new CView(*mMainWindow->getRenderer(), "Shadow View"));
    }
    mViews.emplace_back(mUiView = new CView(*mMainWindow->getRenderer(), "UI View"));

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
    mMainWindow->cleanupRenderer();

    delete mMainCameraMan;
    delete mDebugCameraMan;
}


void GameDriver::mouseDown(int button, ssize_t x, ssize_t y)
{
    mMainWindow->fixupCoordinatesForHdpi(x, y);
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
    mMainWindow->fixupCoordinatesForHdpi(x, y);
    if (mMouseEventTarget) {
        y = mHeight - y;
        mMouseEventTarget->mouseUp(x, y);
        mMouseEventTarget = nullptr;
    }
}

void GameDriver::mouseMoved(ssize_t x, ssize_t y)
{
    mMainWindow->fixupCoordinatesForHdpi(x, y);
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

void GameDriver::resize()
{
    mMainWindow->onResize();

    configureCamerasForWindow();

    // Call the resize callback, if this GameDriver has one. This must be done after
    // configureCamerasForWindow, so the viewports are correct.
    {
        auto    view   = mMainView->getView();
        filament::Camera& camera = view->getCamera();
        if (&camera == mMainCamera)
        {
            // Don't adjust the aspect ratio of the main camera, this is done inside of
            // FilamentGameDriver.cpp
            return;
        }
        const filament::Viewport& vp          = view->getViewport();
        double          aspectRatio = (double)vp.width / vp.height;
        camera.setScaling({1.0 / aspectRatio, 1.0});
    }
}

void GameDriver::configureCamerasForWindow()
{
    float dpiScaleX = 1.0f;
    float dpiScaleY = 1.0f;

    // If the app is not headless, query the window for its physical & virtual sizes.

    SDL_GL_GetDrawableSize(mMainWindow->getSDLWindow(), &mWidth, &mHeight);
 
    int virtualWidth, virtualHeight;
    SDL_GetWindowSize(mMainWindow->getSDLWindow(), &virtualWidth, &virtualHeight);
    dpiScaleX = (float)mWidth / virtualWidth;
    dpiScaleY = (float)mHeight / virtualHeight;


    const filament::math::float3 at(0, 0, -4);
    const double ratio   = double(mHeight) / double(mWidth);
    const int    sidebar = mSidebarWidth * dpiScaleX;

    const bool splitview = mViews.size() > 2;

    // To trigger a floating-point exception, users could shrink the window to be smaller than
    // the sidebar. To prevent this we simply clamp the width of the main viewport.
    const uint32_t mainWidth = splitview ? mWidth - sidebar : std::max(1, (int)mWidth - sidebar);

    double near = 0.1;
    double far  = 100;
    mMainCamera->setLensProjection(mCameraFocalLength, double(mainWidth) / mHeight, near, far);
    mDebugCamera->setProjection(45.0, double(mainWidth) / mHeight, 0.0625, 4096, filament::Camera::Fov::VERTICAL);

    // We're in split view when there are more views than just the Main and UI views.
    if (splitview)
    {
        uint32_t vpw = mainWidth / 2;
        uint32_t vph = mHeight / 2;
        mMainView->setViewport({sidebar, 0, vpw, vph});
        mDepthView->setViewport({int32_t(sidebar + vpw), 0, vpw, vph});
        mGodView->setViewport({int32_t(sidebar + vpw), int32_t(vph), vpw, mHeight - vph});
        mOrthoView->setViewport({sidebar, int32_t(vph), vpw, mHeight - vph});
    }
    else
    {
        mMainView->setViewport({sidebar, 0, (uint32_t)mainWidth, (uint32_t)mHeight});
    }
    mUiView->setViewport({0, 0, (uint32_t)mWidth, (uint32_t)mHeight});
}
