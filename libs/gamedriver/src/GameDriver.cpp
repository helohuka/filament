
#include "gamedriver/BaseLibs.h"
#include "gamedriver/GameDriver.h"

#include "gamedriver/LuauVM.h"
#include "gamedriver/Resource.h"
#include "gamedriver/MeshAssimp.h"
#include "gamedriver/Cube.h"
#include "gamedriver/NativeWindowHelper.h"

#include "gamedriver/Automation.h"

#include "generated/resources/gamedriver.h"

#include "gamedriver/ImGuiBridge.h"

using namespace filament;
using namespace filament::math;
using namespace utils;


GameDriver::GameDriver() {
    ASSERT_POSTCONDITION(SDL_Init(SDL_INIT_EVENTS) == 0, "SDL_Init Failure");
}

GameDriver::~GameDriver() {
    SDL_Quit();
}

void GameDriver::configureCamerasForWindow()
{
    auto                         mWindowSize = mMainWindow->getWindowSize();
    const filament::math::float3 at(0, 0, -4);
    const double                 ratio = double(mWindowSize.y) / double(mWindowSize.x);

    const bool splitview = mViews.size() > 2;

    // To trigger a floating-point exception, users could shrink the window to be smaller than
    // the sidebar. To prevent this we simply clamp the width of the main viewport.
    const uint32_t mainWidth = splitview ? mWindowSize.x : std::max(1, (int)mWindowSize.x);

    double near = 0.1;
    double far  = 100;
    mMainCamera->setLensProjection(mCameraFocalLength, double(mainWidth) / mWindowSize.y, near, far);
    mDebugCamera->setProjection(45.0, double(mainWidth) / mWindowSize.y, 0.0625, 4096, filament::Camera::Fov::VERTICAL);

    // We're in split view when there are more views than just the Main and UI views.
    if (splitview)
    {
        uint32_t vpw = mainWidth / 2;
        uint32_t vph = mWindowSize.y / 2;
        mMainView->setViewport({0, 0, vpw, vph});
        mDepthView->setViewport({int32_t(vpw), 0, vpw, vph});
        mGodView->setViewport({int32_t(vpw), int32_t(vph), vpw, mWindowSize.y - vph});
        mOrthoView->setViewport({0, int32_t(vph), vpw, mWindowSize.y - vph});
    }
    else
    {
        mMainView->setViewport({0, 0, (uint32_t)mainWidth, (uint32_t)mWindowSize.y});
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


void GameDriver::onMouseDown(int button, ssize_t x, ssize_t y)
{
    mMainWindow->fixupCoordinatesForHdpi(x, y);
    y = mMainWindow->getDrawableSize().y - y;
    for (auto const& view : mViews)
    {
        if (view->intersects(x, y))
        {
            mMouseEventTarget = view.get();
            view->mouseDown(button, x, y);
            break;
        }
    }
}

void GameDriver::onMouseWheel(ssize_t x)
{
    if (mMouseEventTarget)
    {
        mMouseEventTarget->mouseWheel(x);
    }
    else
    {
        for (auto const& view : mViews)
        {
            if (view->intersects(mLastX, mLastY))
            {
                view->mouseWheel(x);
                break;
            }
        }
    }
}

void GameDriver::onMouseUp(ssize_t x, ssize_t y)
{
    mMainWindow->fixupCoordinatesForHdpi(x, y);
    if (mMouseEventTarget)
    {
        y = mMainWindow->getDrawableSize().y - y;
        mMouseEventTarget->mouseUp(x, y);
        mMouseEventTarget = nullptr;
    }
}

void GameDriver::onMouseMoved(ssize_t x, ssize_t y)
{
    mMainWindow->fixupCoordinatesForHdpi(x, y);
    y = mMainWindow->getDrawableSize().y - y;
    if (mMouseEventTarget)
    {
        mMouseEventTarget->mouseMoved(x, y);
    }
    mLastX = x;
    mLastY = y;
}

void GameDriver::onKeyDown(SDL_Scancode key)
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

void GameDriver::onKeyUp(SDL_Scancode key)
{
    auto& eventTarget = mKeyEventTarget[key];
    if (!eventTarget)
    {
        return;
    }
    eventTarget->keyUp(key);
    eventTarget = nullptr;
}

void GameDriver::initialize()
{
    static const char* DEFAULT_IBL = "assets/ibl/lightroom_14b";
    gConfigure.iblDirectory        = Resource::get().getRootPath() + DEFAULT_IBL;

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

    {
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
        mViews.emplace_back(mMainView = new CView(*mRenderEngine, "Main View"));
        if (mIsSplitView)
        {
            mViews.emplace_back(mDepthView = new CView(*mRenderEngine, "Depth View"));
            mViews.emplace_back(mGodView = new CView(*mRenderEngine, "God View"));
            mViews.emplace_back(mOrthoView = new CView(*mRenderEngine, "Shadow View"));
        }
        mViews.emplace_back(mUiView = new CView(*mRenderEngine, "UI View"));

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


    mDepthMaterial = Material::Builder()
                         .package(GAMEDRIVER_DEPTHVISUALIZER_DATA, GAMEDRIVER_DEPTHVISUALIZER_SIZE)
                         .build(*mRenderEngine);

    mDepthMI = mDepthMaterial->createInstance();

    mDefaultMaterial = Material::Builder()
                           .package(GAMEDRIVER_AIDEFAULTMAT_DATA, GAMEDRIVER_AIDEFAULTMAT_SIZE)
                           .build(*mRenderEngine);

    mTransparentMaterial = Material::Builder()
                               .package(GAMEDRIVER_TRANSPARENTCOLOR_DATA, GAMEDRIVER_TRANSPARENTCOLOR_SIZE)
                               .build(*mRenderEngine);

    mShadowMaterial = Material::Builder()
                                   .package(GAMEDRIVER_GROUNDSHADOW_DATA, GAMEDRIVER_GROUNDSHADOW_SIZE)
                                   .build(*mRenderEngine);

    mOverdrawMaterial = Material::Builder()
                             .package(GAMEDRIVER_OVERDRAW_DATA, GAMEDRIVER_OVERDRAW_SIZE)
                             .build(*mRenderEngine);

}

void GameDriver::release()
{
    mResourceLoader->asyncCancelLoad();
    mAssetLoader->destroyAsset(mAsset);
    mMaterials->destroyMaterials();

    mGround.reset(); 

    mRenderEngine->destroy(mColorGrading);

    mOverdrawVisualizer.reset();

    mRenderEngine->destroy(mSunlight);

    delete mMaterials;
    delete mNames;
    delete mResourceLoader;
    delete mStbDecoder;
    delete mKtxDecoder;

    filament::gltfio::AssetLoader::destroy(&mAssetLoader);

    mMainWindow->cleanup();

    mViews.clear();
    utils::EntityManager& em = utils::EntityManager::get();
    for (auto e : mCameraEntities)
    {
        mRenderEngine->destroyCameraComponent(e);
        em.destroy(e);
    }

    delete mMainCameraMan;
    delete mDebugCameraMan;

    mIBL.reset();
    mRenderEngine->destroy(mDepthMI);
    mRenderEngine->destroy(mDepthMaterial);
    mRenderEngine->destroy(mDefaultMaterial);
    mRenderEngine->destroy(mTransparentMaterial);
    mRenderEngine->destroy(mShadowMaterial);
    mRenderEngine->destroy(mOverdrawMaterial);
    mRenderEngine->destroy(mScene);
    Engine::destroy(&mRenderEngine);
    mRenderEngine = nullptr;
}

void GameDriver::mainLoop()
{
   
    initialize();


    std::unique_ptr<Cube> cameraCube(new Cube(*mRenderEngine, mTransparentMaterial, {1, 0, 0}));
    // we can't cull the light-frustum because it's not applied a rigid transform
    // and currently, filament assumes that for culling
    std::unique_ptr<Cube> lightmapCube(new Cube(*mRenderEngine, mTransparentMaterial, {0, 1, 0}, false));
    mScene = mRenderEngine->createScene();

    mMainView->getView()->setVisibleLayers(0x4, 0x4);
    if (gConfigure.splitView)
    {
        auto& rcm = mRenderEngine->getRenderableManager();

        rcm.setLayerMask(rcm.getInstance(cameraCube->getSolidRenderable()), 0x3, 0x2);
        rcm.setLayerMask(rcm.getInstance(cameraCube->getWireFrameRenderable()), 0x3, 0x2);

        rcm.setLayerMask(rcm.getInstance(lightmapCube->getSolidRenderable()), 0x3, 0x2);
        rcm.setLayerMask(rcm.getInstance(lightmapCube->getWireFrameRenderable()), 0x3, 0x2);

        // Create the camera mesh
        mScene->addEntity(cameraCube->getWireFrameRenderable());
        mScene->addEntity(cameraCube->getSolidRenderable());

        mScene->addEntity(lightmapCube->getWireFrameRenderable());
        mScene->addEntity(lightmapCube->getSolidRenderable());

        mDepthView->getView()->setVisibleLayers(0x4, 0x4);
        mGodView->getView()->setVisibleLayers(0x6, 0x6);
        mOrthoView->getView()->setVisibleLayers(0x6, 0x6);

        // only preserve the color buffer for additional views; depth and stencil can be discarded.
        mDepthView->getView()->setShadowingEnabled(false);
        mGodView->getView()->setShadowingEnabled(false);
        mOrthoView->getView()->setShadowingEnabled(false);
    }

    loadDirt();
    loadIBL();
   

    for (auto& view : mViews)
    {
        if (view.get() != mUiView)
        {
            view->getView()->setScene(mScene);
        }
    }

    setup();

    ImGui_ImplSDL2_InitPlatformInterface(mMainWindow.get(), mRenderEngine);

   
    bool mousePressed[3] = {false};

    float cameraFocalLength = mCameraFocalLength;

    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

    while (!mClosed)
    {

        if (mCameraFocalLength != cameraFocalLength)
        {
            configureCamerasForWindow();
            cameraFocalLength = mCameraFocalLength;
        }

        if (!UTILS_HAS_THREADING)
        {
            mRenderEngine->execute();
        }

        // Allow the app to animate the scene if desired.
        {
            double now = (double)SDL_GetPerformanceCounter() / SDL_GetPerformanceFrequency();
            animate(now);
        }

        // Loop over fresh events twice: first stash them and let ImGui process them, then allow
        // the app to process the stashed events. This is done because ImGui might wish to block
        // certain events from the app (e.g., when dragging the mouse over an obscuring window).
        constexpr int kMaxEvents = 16;
        SDL_Event     events[kMaxEvents];
        int           nevents = 0;
        while (nevents < kMaxEvents && SDL_PollEvent(&events[nevents]) != 0)
        {

            if (ImGui_ImplSDL2_ProcessEvent(&events[nevents]))
            {
            }

            nevents++;
        }

        // Now, loop over the events a second time for app-side processing.
        for (int i = 0; i < nevents; i++)
        {
            const SDL_Event& event = events[i];
            ImGuiIO*         io    =   nullptr;

            
            switch (event.type)
            {
                case SDL_QUIT:
                    mClosed = true;
                    break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE)
                    {
                        mClosed = true;
                    }
#ifndef NDEBUG
                    if (event.key.keysym.scancode == SDL_SCANCODE_PRINTSCREEN)
                    {
                        DebugRegistry& debug = mRenderEngine->getDebugRegistry();
                        bool*          captureFrame =
                            debug.getPropertyAddress<bool>("d.renderer.doFrameCapture");
                        *captureFrame = true;
                    }
#endif
                    onKeyDown(event.key.keysym.scancode);
                    break;
                case SDL_KEYUP:
                    onKeyUp(event.key.keysym.scancode);
                    break;
                case SDL_MOUSEWHEEL:
                    if (!io || !io->WantCaptureMouse)
                       onMouseWheel(event.wheel.y);
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    if (!io || !io->WantCaptureMouse)
                        onMouseDown(event.button.button, event.button.x, event.button.y);
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (!io || !io->WantCaptureMouse)
                       onMouseUp(event.button.x, event.button.y);
                    break;
                case SDL_MOUSEMOTION:
                    if (!io || !io->WantCaptureMouse)
                        onMouseMoved(event.motion.x, event.motion.y);
                    break;
                case SDL_WINDOWEVENT:
                    switch (event.window.event)
                    {
                        case SDL_WINDOWEVENT_RESIZED:
                            mMainWindow->onResize();
                            configureCamerasForWindow();
                            break;
                        default:
                            break;
                    }
                    break;
                case SDL_APP_DIDENTERBACKGROUND:

                    break;
                case SDL_APP_DIDENTERFOREGROUND:
                    mRenderEngine->setActiveFeatureLevel(mFeatureLevel);
                    mMainWindow->resetSwapChain();
                    break;
                   
                default:
                    break;
            }

           
        }

         ImGui_ImplSDL2_NewFrame();
         ImGui::NewFrame();
         
         updateUserInterface();

         ImGui::Render();

        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
         {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
         }

        ImGuiWindowImpl* vd = (ImGuiWindowImpl*)ImGui::GetIO().BackendPlatformUserData;

        vd->processImGuiCommands(ImGui::GetDrawData());

        // Calculate the time step.
        static Uint64 frequency = SDL_GetPerformanceFrequency();
        Uint64        now       = SDL_GetPerformanceCounter();
        const float   timeStep  = mTime > 0 ? (float)((double)(now - mTime) / frequency) : (float)(1.0f / 60.0f);
        mTime                   = now;


        // Update the camera manipulators for each view.

         for (auto const& view : mViews)
        {
            view->tick(timeStep);
        }

        // Update the cube distortion matrix used for frustum visualization.
        const Camera* lightmapCamera = mMainView->getView()->getDirectionalLightCamera();
        lightmapCube->mapFrustum(*mRenderEngine, lightmapCamera);
        cameraCube->mapFrustum(*mRenderEngine, mMainCamera);

        // Delay rendering for roughly one monitor refresh interval
        // TODO: Use SDL_GL_SetSwapInterval for proper vsync
        SDL_DisplayMode Mode;
        int             refreshIntervalMS = (SDL_GetCurrentDisplayMode(
                                     SDL_GetWindowDisplayIndex(mMainWindow->getWindowHandle()), &Mode) == 0 &&
                                 Mode.refresh_rate != 0) ?
                        round(1000.0 / Mode.refresh_rate) :
                        16;
        SDL_Delay(refreshIntervalMS);

        preRender(mMainView->getView(), mScene, mMainWindow->getRenderer());

        if (mMainWindow->getRenderer()->beginFrame(mMainWindow->getSwapChain()))
        {
            for (filament::View* offscreenView : mOffscreenViews)
            {
                mMainWindow->getRenderer()->render(offscreenView);
            }
            for (auto const& view : mViews)
            {
                mMainWindow->getRenderer()->render(view->getView());
            }

            postRender(mMainView->getView(), mScene, mMainWindow->getRenderer());

            mMainWindow->getRenderer()->endFrame();
        }
        else
        {
            ++mSkippedFrames;
        }
    }

    cameraCube.reset();
    lightmapCube.reset();

    release();
}

void GameDriver::loadIBL()
{
    if (!gConfigure.iblDirectory.empty())
    {
        Path iblPath(gConfigure.iblDirectory);

        if (!iblPath.exists())
        {
            std::cerr << "The specified IBL path does not exist: " << iblPath << std::endl;
            return;
        }

        mIBL = std::make_unique<IBL>(*mRenderEngine);

        if (!iblPath.isDirectory())
        {
            if (!mIBL->loadFromEquirect(iblPath))
            {
                std::cerr << "Could not load the specified IBL: " << iblPath << std::endl;
                mIBL.reset(nullptr);
                return;
            }
        }
        else
        {
            if (!mIBL->loadFromDirectory(iblPath))
            {
                std::cerr << "Could not load the specified IBL: " << iblPath << std::endl;
                mIBL.reset(nullptr);
                return;
            }
        }
    }

    mIBL->getSkybox()->setLayerMask(0x7, 0x4);
    mScene->setSkybox(mIBL->getSkybox());
    mScene->setIndirectLight(mIBL->getIndirectLight());

    gSettings.view.fog.color = mIBL->getSphericalHarmonics()[0];
    mIndirectLight           = mIBL->getIndirectLight();
    if (mIndirectLight)
    {
        float3 const d        = filament::IndirectLight::getDirectionEstimate(mIBL->getSphericalHarmonics());
        float4 const c        = filament::IndirectLight::getColorEstimate(mIBL->getSphericalHarmonics(), d);
        bool const   dIsValid = std::none_of(std::begin(d.v), std::end(d.v), is_not_a_number);
        bool const   cIsValid = std::none_of(std::begin(c.v), std::end(c.v), is_not_a_number);
        if (dIsValid && cIsValid)
        {
            gSettings.lighting.sunlightDirection = d;
            gSettings.lighting.sunlightColor     = c.rgb;
            gSettings.lighting.sunlightIntensity = c[3] * mIndirectLight->getIntensity();
        }
    }
}
void GameDriver::loadDirt()
{
    if (!gConfigure.dirt.empty())
    {
        Path dirtPath(gConfigure.dirt);

        if (!dirtPath.exists())
        {
            std::cerr << "The specified dirt file does not exist: " << dirtPath << std::endl;
            return;
        }

        if (!dirtPath.isFile())
        {
            std::cerr << "The specified dirt path is not a file: " << dirtPath << std::endl;
            return;
        }

        int w, h, n;

        unsigned char* data = stbi_load(dirtPath.getAbsolutePath().c_str(), &w, &h, &n, 3);
        assert(n == 3);

        mDirt = Texture::Builder()
                    .width(w)
                    .height(h)
                    .format(Texture::InternalFormat::RGB8)
                    .build(*mRenderEngine);

        mDirt->setImage(*mRenderEngine, 0, {data, size_t(w * h * 3), Texture::Format::RGB, Texture::Type::UBYTE, (Texture::PixelBufferDescriptor::Callback)&stbi_image_free});
    }
}

void GameDriver::loadAsset(utils::Path filename)
{
    // Peek at the file size to allow pre-allocation.
    long contentSize = static_cast<long>(getFileSize(filename.c_str()));
    if (contentSize <= 0)
    {
        std::cerr << "Unable to open " << filename << std::endl;
        exit(1);
    }
   
    // Consume the glTF file.
    std::ifstream        in(filename.c_str(), std::ifstream::binary | std::ifstream::in);
    std::vector<uint8_t> buffer(static_cast<unsigned long>(contentSize));
    if (!in.read((char*)buffer.data(), contentSize))
    {
        std::cerr << "Unable to read " << filename << std::endl;
        exit(1);
    }

    // Parse the glTF file and create Filament entities.
    mAsset    = mAssetLoader->createAsset(buffer.data(), buffer.size());
    mInstance = mAsset->getInstance();
    buffer.clear();
    buffer.shrink_to_fit();

    if (!mAsset)
    {
        std::cerr << "Unable to parse " << filename << std::endl;
        exit(1);
    }
};

void GameDriver::loadResources(utils::Path filename)
{
    // Load external textures and buffers.
    std::string                             gltfPath      = filename.getAbsolutePath();
    filament::gltfio::ResourceConfiguration configuration = {};
    configuration.engine                                  = mRenderEngine;
    configuration.gltfPath                                = gltfPath.c_str();
    configuration.normalizeSkinningWeights                = true;

    if (!mResourceLoader)
    {
        mResourceLoader = new gltfio::ResourceLoader(configuration);
        mStbDecoder     = filament::gltfio::createStbProvider(mRenderEngine);
        mKtxDecoder     = filament::gltfio::createKtx2Provider(mRenderEngine);
        mResourceLoader->addTextureProvider("image/png", mStbDecoder);
        mResourceLoader->addTextureProvider("image/jpeg", mStbDecoder);
        mResourceLoader->addTextureProvider("image/ktx2", mKtxDecoder);
    }

    if (!mResourceLoader->asyncBeginLoad(mAsset))
    {
        std::cerr << "Unable to start loading resources for " << filename << std::endl;
        exit(1);
    }

    if (mRecomputeAabb)
    {
        mAsset->getInstance()->recomputeBoundingBoxes();
    }

    mAsset->releaseSourceData();

    // Enable stencil writes on all material instances.
    const size_t                             matInstanceCount = mInstance->getMaterialInstanceCount();
    filament::MaterialInstance* const* const instances        = mInstance->getMaterialInstances();
    for (int mi = 0; mi < matInstanceCount; mi++)
    {
        instances[mi]->setStencilWrite(true);
        instances[mi]->setStencilOpDepthStencilPass(filament::MaterialInstance::StencilOperation::INCR);
    }
 
}

void GameDriver::preRender(View* view, Scene* scene, Renderer* renderer)
{
    auto&       rcm           = mRenderEngine->getRenderableManager();
    auto        instance      = rcm.getInstance(mGround->getSolidRenderable());
    const auto  viewerOptions = gSettings.viewer; //mAutomationEngine->getViewerOptions();
    const auto& dofOptions    = gSettings.view.dof;
    rcm.setLayerMask(instance, 0xff, viewerOptions.groundPlaneEnabled? 0xff : 0x00);

    mRenderEngine->setAutomaticInstancingEnabled(viewerOptions.autoInstancingEnabled);

    // Note that this focal length might be different from the slider value because the
    // automation engine applies Camera::computeEffectiveFocalLength when DoF is enabled.
    mCameraFocalLength = viewerOptions.cameraFocalLength;

    const size_t cameraCount = mAsset->getCameraEntityCount();
    view->setCamera(mMainCamera);

    const int currentCamera = getCurrentCamera();
    if (currentCamera > 0 && currentCamera <= cameraCount)
    {
        const utils::Entity* cameras = mAsset->getCameraEntities();
        Camera*              camera  = mRenderEngine->getCameraComponent(cameras[currentCamera - 1]);
        assert_invariant(camera);
        view->setCamera(camera);

        // Override the aspect ratio in the glTF file and adjust the aspect ratio of this
        // camera to the viewport.
        const Viewport& vp          = view->getViewport();
        double          aspectRatio = (double)vp.width / vp.height;
        camera->setScaling({1.0 / aspectRatio, 1.0});
    }

    mGround->getMI()->setParameter("strength", viewerOptions.groundShadowStrength);

    // This applies clear options, the skybox mask, and some camera settings.
    Camera& camera = view->getCamera();
    Skybox* skybox = scene->getSkybox();
    filament::viewer::applySettings(mRenderEngine, gSettings.viewer, &camera, skybox, renderer);

    // Check if color grading has changed.
    filament::viewer::ColorGradingSettings& options = gSettings.view.colorGrading;
    if (options.enabled)
    {
        if (options != mLastColorGradingOptions)
        {
            filament::ColorGrading* colorGrading = filament::viewer::createColorGrading(options, mRenderEngine);
            mRenderEngine->destroy(mColorGrading);
            mColorGrading            = colorGrading;
            mLastColorGradingOptions = options;
        }
        view->setColorGrading(mColorGrading);
    }
    else
    {
        view->setColorGrading(nullptr);
    }
}
void GameDriver::postRender(View* view, Scene* scene, Renderer* renderer)
{
    //Automation::get().tick(view, mInstance, renderer, ImGui::GetIO().DeltaTime);
}

void GameDriver::animate(double now)
{
    mResourceLoader->asyncUpdateLoad();

    // Optionally fit the model into a unit cube at the origin.
    updateRootTransform();

    // Gradually add renderables to the scene as their textures become ready.
    populateScene();

    applyAnimation(now);
}

void GameDriver::setup()
{
    mNames = new utils::NameComponentManager(EntityManager::get());
    gSettings.view.shadowType                  = filament::ShadowType::PCF;
    gSettings.view.vsmShadowOptions.anisotropy = 0;
    gSettings.view.dithering                   = filament::Dithering::TEMPORAL;
    gSettings.view.antiAliasing                = filament::AntiAliasing::FXAA;
    gSettings.view.msaa                        = {.enabled = false, .sampleCount = 4}; //A57 必须关闭
    gSettings.view.ssao.enabled                = true;
    gSettings.view.bloom.enabled               = true;
    mSunlight                                  = utils::EntityManager::get().create();
    using namespace filament;
    LightManager::Builder(LightManager::Type::SUN)
        .color(gSettings.lighting.sunlightColor)
        .intensity(gSettings.lighting.sunlightIntensity)
        .direction(normalize(gSettings.lighting.sunlightDirection))
        .castShadows(true)
        .sunAngularRadius(gSettings.lighting.sunlightAngularRadius)
        .sunHaloSize(gSettings.lighting.sunlightHaloSize)
        .sunHaloFalloff(gSettings.lighting.sunlightHaloFalloff)
        .build(*mRenderEngine, mSunlight);
    if (gSettings.lighting.enableSunlight)
    {
        mScene->addEntity(mSunlight);
    }
    mMainView->getView()->setAmbientOcclusionOptions({.upsampling = View::QualityLevel::HIGH});

    gSettings.viewer.autoScaleEnabled = !mActualSize;

    Automation::get().setup();

    if (mSettingsFile.size() > 0)
    {
        bool success = loadSettings(mSettingsFile.c_str(), &gSettings);
        if (success)
        {
            std::cout << "Loaded settings from " << mSettingsFile << std::endl;
        }
        else
        {
            std::cerr << "Failed to load settings from " << mSettingsFile << std::endl;
        }
    }

    mMaterials = (mMaterialSource == JITSHADER) ? filament::gltfio::createJitShaderProvider(mRenderEngine) : filament::gltfio::createUbershaderProvider(mRenderEngine, UBERARCHIVE_DEFAULT_DATA, UBERARCHIVE_DEFAULT_SIZE);

    mAssetLoader = filament::gltfio::AssetLoader::create({mRenderEngine, mMaterials, mNames});

    auto filename = utils::Path(gConfigure.filename.c_str());

    loadAsset(filename);
    loadResources(filename);

    setAsset(mAsset, mInstance);

    mGround = std::make_unique<Ground>(*mRenderEngine, mScene, mShadowMaterial);

    mOverdrawVisualizer = std::make_unique<OverdrawVisualizer>(*mRenderEngine, mScene, mOverdrawMaterial);
}
