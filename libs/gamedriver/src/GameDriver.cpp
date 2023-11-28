
#include "gamedriver/BaseLibs.h"
#include "gamedriver/GameDriver.h"

#include "gamedriver/LuauVM.h"
#include "gamedriver/Resource.h"
#include "gamedriver/MeshAssimp.h"
#include "gamedriver/NativeWindowHelper.h"
#include "gamedriver/Automation.h"
#include "gamedriver/ImGuiBridge.h"

#include "generated/resources/gamedriver.h"

GameDriver::GameDriver() {
    ASSERT_POSTCONDITION(SDL_Init(SDL_INIT_EVENTS) == 0, "SDL_Init Failure");
}

GameDriver::~GameDriver() {
    SDL_Quit();
}

void GameDriver::configureCamerasForWindow()
{
    const filament::math::float3 at(0, 0, -4);
    const double                 ratio = double(mHeight) / double(mWidth);
     

    // To trigger a floating-point exception, users could shrink the window to be smaller than
    // the sidebar. To prevent this we simply clamp the width of the main viewport.
    const uint32_t mainWidth = std::max(1, (int)mWidth);

    double near = 0.1;
    double far  = 100;
    mMainCamera->setLensProjection(mCameraFocalLength, double(mainWidth) / mHeight, near, far);
    

    // We're in split view when there are more views than just the Main and UI views.
    
        mMainView->setViewport({0, 0, (uint32_t)mainWidth, (uint32_t)mHeight});
     
   // mUiView->setViewport({0, 0, (uint32_t)mWindowSize.x, (uint32_t)mWindowSize.y});


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
    fixupCoordinatesForHdpi(x, y);
    y = mHeight - y;

    if (mMainView->intersects(x, y))
    {
        mMouseEventTarget = mMainView;
        mMainView->mouseDown(button, x, y);
    }

    auto view = mMainView->getView();

    view->pick(x, y, [this](filament::View::PickingQueryResult const& result) {
        if (const char* name = mAsset->getName(result.renderable); name)
        {
            mNotificationText = name;
        }
        else
        {
            mNotificationText.clear();
        }
    });
}
void GameDriver::onMouseWheel(ssize_t x)
{
    if (mMouseEventTarget)
    {
        mMouseEventTarget->mouseWheel(x);
    }
    else
    {
        if (mMainView->intersects(mLastX, mLastY))
        {
            mMainView->mouseWheel(x);
        }
    }
}

void GameDriver::onMouseUp(ssize_t x, ssize_t y)
{
    fixupCoordinatesForHdpi(x, y);
    if (mMouseEventTarget)
    {
        y = mHeight - y;
        mMouseEventTarget->mouseUp(x, y);
        mMouseEventTarget = nullptr;
    }
}

void GameDriver::onMouseMoved(ssize_t x, ssize_t y)
{
    fixupCoordinatesForHdpi(x, y);
    y = mHeight - y;
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
        if (mMainView->intersects(mLastX, mLastY))
        {
            targetView = mMainView;
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
    SDL_Log("%s\n", "void GameDriver::initialize() -0");
    mFontPath = utils::Path::getCurrentExecutable().getParent() + "assets/fonts/Roboto-Medium.ttf";

    gConfigure.iblDirectory = Resource::get().getRootPath() + "assets/ibl/lightroom_14b";

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

    setupWindow();
    SDL_Log("%s\n", "void GameDriver::initialize() -1");
    {
        utils::EntityManager& em = utils::EntityManager::get();
        mMainCameraEntity        = em.create();
        mMainCamera              = mRenderEngine->createCamera(mMainCameraEntity);

        // set exposure
        mMainCamera->setExposure(16.0f, 1 / 125.0f, 100.0f);
        // create views
        mMainView = new CView(*mRenderEngine, "Main View");
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

        configureCamerasForWindow();

        mMainCamera->lookAt({4, 0, -4}, {0, 0, -4}, {0, 1, 0});
    }

    SDL_Log("%s\n", "void GameDriver::initialize() -2");
    mDepthMaterial = filament::Material::Builder()
                         .package(GAMEDRIVER_DEPTHVISUALIZER_DATA, GAMEDRIVER_DEPTHVISUALIZER_SIZE)
                         .build(*mRenderEngine);

    mDepthMI = mDepthMaterial->createInstance();

    mDefaultMaterial = filament::Material::Builder()
                           .package(GAMEDRIVER_AIDEFAULTMAT_DATA, GAMEDRIVER_AIDEFAULTMAT_SIZE)
                           .build(*mRenderEngine);

    mTransparentMaterial = filament::Material::Builder()
                               .package(GAMEDRIVER_TRANSPARENTCOLOR_DATA, GAMEDRIVER_TRANSPARENTCOLOR_SIZE)
                               .build(*mRenderEngine);

    mShadowMaterial = filament::Material::Builder()
                          .package(GAMEDRIVER_GROUNDSHADOW_DATA, GAMEDRIVER_GROUNDSHADOW_SIZE)
                          .build(*mRenderEngine);

    mOverdrawMaterial = filament::Material::Builder()
                            .package(GAMEDRIVER_OVERDRAW_DATA, GAMEDRIVER_OVERDRAW_SIZE)
                            .build(*mRenderEngine);

    mCameraCube = std::make_unique<Cube>(*mRenderEngine, mTransparentMaterial, filament::math::float3{1, 0, 0});
    // we can't cull the light-frustum because it's not applied a rigid transform
    // and currently, filament assumes that for culling
    mLightmapCube = std::make_unique<Cube>(*mRenderEngine, mTransparentMaterial, filament::math::float3{0, 1, 0}, false);
    mScene        = mRenderEngine->createScene();

    mMainView->getView()->setVisibleLayers(0x4, 0x4);
    SDL_Log("%s\n", "void GameDriver::initialize() -3");
    loadDirt();
    loadIBL();

    mMainView->getView()->setScene(mScene);
    SDL_Log("%s\n", "void GameDriver::initialize() -4 ");
    setup();
    SDL_Log("%s\n", "void GameDriver::initialize() -5 ");
    setupGui();

    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

   
}

void GameDriver::setup()
{
    SDL_Log("%s\n", "void GameDriver::setup() -0 ");
    mNames                                     = new utils::NameComponentManager(utils::EntityManager::get());
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
    SDL_Log("%s\n", "void GameDriver::setup() -1 ");
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
    mAssimp       = new MeshAssimp(*mRenderEngine);
    auto filename = utils::Path(gConfigure.filename.c_str());
    SDL_Log("%s\n", "void GameDriver::setup() -2 ");
    loadAsset(filename);
    SDL_Log("%s\n", "void GameDriver::setup() -3 ");
    setAsset();

    mGround = std::make_unique<Ground>(*mRenderEngine, mScene, mShadowMaterial);
    SDL_Log("%s\n", "void GameDriver::setup() -4 ");
    mOverdrawVisualizer = std::make_unique<OverdrawVisualizer>(*mRenderEngine, mScene, mOverdrawMaterial);
}

void GameDriver::release()
{
    mCameraCube.reset();
    mLightmapCube.reset();

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

    cleanupGui();
    cleanupWindow();

    delete mMainView;
    utils::EntityManager& em = utils::EntityManager::get();
     
    mRenderEngine->destroyCameraComponent(mMainCameraEntity);
    em.destroy(mMainCameraEntity);
    
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
    filament::Engine::destroy(&mRenderEngine);
    mRenderEngine = nullptr;
}

void GameDriver::mainLoop()
{
    initialize();

    bool mousePressed[3] = {false};

    float cameraFocalLength = mCameraFocalLength;

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

            if (onProcessEvent(&events[nevents]))
            {
                //   continue;
            }

            nevents++;
        }

        // Now, loop over the events a second time for app-side processing.
        ImGuiIO& io = ImGui::GetIO();
        for (int i = 0; i < nevents; i++)
        {
            const SDL_Event& event = events[i];
          

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
                        filament::DebugRegistry& debug = mRenderEngine->getDebugRegistry();
                        bool*                    captureFrame =
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
                    if (!io.WantCaptureMouse)
                        onMouseWheel(event.wheel.y);
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    if (!io.WantCaptureMouse)
                        onMouseDown(event.button.button, event.button.x, event.button.y);
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (!io.WantCaptureMouse)
                        onMouseUp(event.button.x, event.button.y);
                    break;
                case SDL_MOUSEMOTION:
                    if (!io.WantCaptureMouse)
                        onMouseMoved(event.motion.x, event.motion.y);
                    break;
                case SDL_WINDOWEVENT:
                    switch (event.window.event)
                    {
                        case SDL_WINDOWEVENT_RESIZED:
                            onWindowResize();
                            break;
                        default:
                            break;
                    }
                    break;
                case SDL_APP_DIDENTERBACKGROUND:

                    break;
                case SDL_APP_DIDENTERFOREGROUND:
                    mRenderEngine->setActiveFeatureLevel(mFeatureLevel);
                    resetSwapChain();
                    break;

                default:
                    break;
            }
        }

        prepareGui();

        // Calculate the time step.
        static Uint64 frequency = SDL_GetPerformanceFrequency();
        Uint64        now       = SDL_GetPerformanceCounter();
        const float   timeStep  = mTime > 0 ? (float)((double)(now - mTime) / frequency) : (float)(1.0f / 60.0f);
        mTime                   = now;

        // Update the camera manipulators for each view.
        mMainView->tick(timeStep);

        // Update the cube distortion matrix used for frustum visualization.
        const filament::Camera* lightmapCamera = mMainView->getView()->getDirectionalLightCamera();
        mLightmapCube->mapFrustum(*mRenderEngine, lightmapCamera);
        mCameraCube->mapFrustum(*mRenderEngine, mMainCamera);

        // Delay rendering for roughly one monitor refresh interval
        // TODO: Use SDL_GL_SetSwapInterval for proper vsync
        SDL_DisplayMode Mode;
        int             refreshIntervalMS = (SDL_GetCurrentDisplayMode(
                                     SDL_GetWindowDisplayIndex(mWindow), &Mode) == 0 &&
                                 Mode.refresh_rate != 0) ?
                        round(1000.0 / Mode.refresh_rate) :
                        16;
        SDL_Delay(refreshIntervalMS);

        preRender(mMainView->getView(), mScene, mRenderer);

        if (mRenderer->beginFrame(mSwapChain))
        {
            for (filament::View* offscreenView : mOffscreenViews)
            {
                mRenderer->render(offscreenView);
            }

            mRenderer->render(mMainView->getView());


            if (mMainUI)
            {
                mRenderer->render(mMainUI->mView);
            }

            postRender(mMainView->getView(), mScene, mRenderer);

            mRenderer->endFrame();
        }
        else
        {
            ++mSkippedFrames;
        }
    }

    release();
}

void GameDriver::loadIBL()
{
    if (!gConfigure.iblDirectory.empty())
    {
        utils::Path iblPath(gConfigure.iblDirectory);

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
        filament::math::float3 const d        = filament::IndirectLight::getDirectionEstimate(mIBL->getSphericalHarmonics());
        filament::math::float4 const c        = filament::IndirectLight::getColorEstimate(mIBL->getSphericalHarmonics(), d);
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
        utils::Path dirtPath(gConfigure.dirt);

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

        mDirt = filament::Texture::Builder()
                    .width(w)
                    .height(h)
                    .format(filament::Texture::InternalFormat::RGB8)
                    .build(*mRenderEngine);

        mDirt->setImage(*mRenderEngine, 0, {data, size_t(w * h * 3), filament::Texture::Format::RGB, filament::Texture::Type::UBYTE, (filament::Texture::PixelBufferDescriptor::Callback)&stbi_image_free});
    }
}
std::map<std::string, filament::MaterialInstance*> materials;
void GameDriver::loadAsset(utils::Path filename)
{
    //utils::Path                                        filename1("E:/1/filament/assets/models/camera/camera.obj");
    
    //mAssimp->addFromFile(filename1, materials, true);

    
    //mScene->addEntities(mAssimp->getRenderables().data(), mAssimp->getRenderables().size());
     

    // Peek at the file size to allow pre-allocation.
    long contentSize = static_cast<long>(getFileSize(filename.c_str()));
    if (contentSize <= 0)
    {
        SDL_Log("Unable to open %s\n", filename.c_str());
        exit(1);
    }
   
    // Consume the glTF file.
    std::ifstream        in(filename.c_str(), std::ifstream::binary | std::ifstream::in);
    std::vector<uint8_t> buffer(static_cast<unsigned long>(contentSize));
    if (!in.read((char*)buffer.data(), contentSize))
    {
        SDL_Log("Unable to read %s\n", filename.c_str());
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
 
    // Load external textures and buffers.
    std::string                             gltfPath      = filename.getAbsolutePath();
    filament::gltfio::ResourceConfiguration configuration = {};
    configuration.engine                                  = mRenderEngine;
    configuration.gltfPath                                = gltfPath.c_str();
    configuration.normalizeSkinningWeights                = true;

    if (!mResourceLoader)
    {
        mResourceLoader = new filament::gltfio::ResourceLoader(configuration);
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
void GameDriver::preRender(filament::View* view, filament::Scene* scene, filament::Renderer* renderer)
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
        filament::Camera*    camera  = mRenderEngine->getCameraComponent(cameras[currentCamera - 1]);
        assert_invariant(camera);
        view->setCamera(camera);

        // Override the aspect ratio in the glTF file and adjust the aspect ratio of this
        // camera to the viewport.
        const filament::Viewport& vp          = view->getViewport();
        double          aspectRatio = (double)vp.width / vp.height;
        camera->setScaling({1.0 / aspectRatio, 1.0});
    }

    mGround->getMI()->setParameter("strength", viewerOptions.groundShadowStrength);

    // This applies clear options, the skybox mask, and some camera settings.
    filament::Camera& camera = view->getCamera();
    filament::Skybox* skybox = scene->getSkybox();
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
void GameDriver::postRender(filament::View* view, filament::Scene* scene, filament::Renderer* renderer)
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
