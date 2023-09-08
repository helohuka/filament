
#include "gamedriver/BaseLibs.h"
#include "gamedriver/GameDriver.h"

#include "gamedriver/LuauVM.h"
#include "gamedriver/Resource.h"
#include "gamedriver/MeshAssimp.h"
#include "gamedriver/Cube.h"
#include "gamedriver/NativeWindowHelper.h"

#include "gamedriver/Automation.h"

#include "generated/resources/gamedriver.h"

using namespace filament;
using namespace filagui;
using namespace filament::math;
using namespace utils;


GameDriver::GameDriver() {
    ASSERT_POSTCONDITION(SDL_Init(SDL_INIT_EVENTS) == 0, "SDL_Init Failure");
}

GameDriver::~GameDriver() {
    SDL_Quit();
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
    if (mImGuiHelper)
    {
        mImGuiHelper.reset();
    }

    mResourceLoader->asyncCancelLoad();
    mAssetLoader->destroyAsset(mAsset);
    mMaterials->destroyMaterials();

    mGround.reset(); 

    mRenderEngine->destroy(mColorGrading);

    mOverdrawVisualizer.reset();

    mRenderEngine->destroy(mSunlight);
    mImGuiHelper = nullptr;

    delete mMaterials;
    delete mNames;
    delete mResourceLoader;
    delete mStbDecoder;
    delete mKtxDecoder;

    filament::gltfio::AssetLoader::destroy(&mAssetLoader);

    mMainWindow->cleanup();

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

    mMainWindow->getMainView()->getView()->setVisibleLayers(0x4, 0x4);
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

        mMainWindow->getDepthView()->getView()->setVisibleLayers(0x4, 0x4);
        mMainWindow->getGodView()->getView()->setVisibleLayers(0x6, 0x6);
        mMainWindow->getOrthoView()->getView()->setVisibleLayers(0x6, 0x6);

        // only preserve the color buffer for additional views; depth and stencil can be discarded.
        mMainWindow->getDepthView()->getView()->setShadowingEnabled(false);
        mMainWindow->getGodView()->getView()->setShadowingEnabled(false);
        mMainWindow->getOrthoView()->getView()->setShadowingEnabled(false);
    }

    loadDirt();
    loadIBL();
   

    for (auto& view : mMainWindow->getViews())
    {
        if (view.get() != mMainWindow->getUiView())
        {
            view->getView()->setScene(mScene);
        }
    }

    setup();

    ImGuiHelper::Callback imguiCallback = std::bind(&GameDriver::gui, this, mRenderEngine, std::placeholders::_2);

    if (imguiCallback)
    {
        mImGuiHelper                 = std::make_unique<ImGuiHelper>(mRenderEngine, mMainWindow->getUiView()->getView(),
                                                     Resource::get().getRootPath() + "assets/fonts/Roboto-Medium.ttf");
        ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
        ImGuiIO& io  = ImGui::GetIO();
#ifdef WIN32
        io.ImeWindowHandle = mMainWindow->getNativeWindow();
#endif
        io.KeyMap[ImGuiKey_Tab]        = SDL_SCANCODE_TAB;
        io.KeyMap[ImGuiKey_LeftArrow]  = SDL_SCANCODE_LEFT;
        io.KeyMap[ImGuiKey_RightArrow] = SDL_SCANCODE_RIGHT;
        io.KeyMap[ImGuiKey_UpArrow]    = SDL_SCANCODE_UP;
        io.KeyMap[ImGuiKey_DownArrow]  = SDL_SCANCODE_DOWN;
        io.KeyMap[ImGuiKey_PageUp]     = SDL_SCANCODE_PAGEUP;
        io.KeyMap[ImGuiKey_PageDown]   = SDL_SCANCODE_PAGEDOWN;
        io.KeyMap[ImGuiKey_Home]       = SDL_SCANCODE_HOME;
        io.KeyMap[ImGuiKey_End]        = SDL_SCANCODE_END;
        io.KeyMap[ImGuiKey_Insert]     = SDL_SCANCODE_INSERT;
        io.KeyMap[ImGuiKey_Delete]     = SDL_SCANCODE_DELETE;
        io.KeyMap[ImGuiKey_Backspace]  = SDL_SCANCODE_BACKSPACE;
        io.KeyMap[ImGuiKey_Space]      = SDL_SCANCODE_SPACE;
        io.KeyMap[ImGuiKey_Enter]      = SDL_SCANCODE_RETURN;
        io.KeyMap[ImGuiKey_Escape]     = SDL_SCANCODE_ESCAPE;
        io.KeyMap[ImGuiKey_A]          = SDL_SCANCODE_A;
        io.KeyMap[ImGuiKey_C]          = SDL_SCANCODE_C;
        io.KeyMap[ImGuiKey_V]          = SDL_SCANCODE_V;
        io.KeyMap[ImGuiKey_X]          = SDL_SCANCODE_X;
        io.KeyMap[ImGuiKey_Y]          = SDL_SCANCODE_Y;
        io.KeyMap[ImGuiKey_Z]          = SDL_SCANCODE_Z;
        io.SetClipboardTextFn          = [](void*, const char* text) {
            SDL_SetClipboardText(text);
        };
        io.GetClipboardTextFn = [](void*) -> const char* {
            return SDL_GetClipboardText();
        };
        io.ClipboardUserData = nullptr;
    }

    bool mousePressed[3] = {false};

    int   sidebarWidth      = mMainWindow->getSidebarWidth();
    float cameraFocalLength = mMainWindow->getCameraFocalLength();

    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

    while (!mClosed)
    {

        if (mMainWindow->getSidebarWidth() != sidebarWidth || mMainWindow->getCameraFocalLength() != cameraFocalLength)
        {
            mMainWindow->configureCamerasForWindow();
            sidebarWidth      = mMainWindow->getSidebarWidth();
            cameraFocalLength = mMainWindow->getCameraFocalLength();
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
            if (mImGuiHelper)
            {
                ImGuiIO&   io    = ImGui::GetIO();
                SDL_Event* event = &events[nevents];
                switch (event->type)
                {
                    case SDL_MOUSEWHEEL:
                    {
                        if (event->wheel.x > 0) io.MouseWheelH += 1;
                        if (event->wheel.x < 0) io.MouseWheelH -= 1;
                        if (event->wheel.y > 0) io.MouseWheel += 1;
                        if (event->wheel.y < 0) io.MouseWheel -= 1;
                        break;
                    }
                    case SDL_MOUSEBUTTONDOWN:
                    {
                        if (event->button.button == SDL_BUTTON_LEFT) mousePressed[0] = true;
                        if (event->button.button == SDL_BUTTON_RIGHT) mousePressed[1] = true;
                        if (event->button.button == SDL_BUTTON_MIDDLE) mousePressed[2] = true;
                        break;
                    }
                    case SDL_TEXTINPUT:
                    {
                        io.AddInputCharactersUTF8(event->text.text);
                        break;
                    }
                    case SDL_KEYDOWN:
                    case SDL_KEYUP:
                    {
                        int key = event->key.keysym.scancode;
                        IM_ASSERT(key >= 0 && key < IM_ARRAYSIZE(io.KeysDown));
                        io.KeysDown[key] = (event->type == SDL_KEYDOWN);
                        io.KeyShift      = ((SDL_GetModState() & KMOD_SHIFT) != 0);
                        io.KeyAlt        = ((SDL_GetModState() & KMOD_ALT) != 0);
                        io.KeyCtrl       = ((SDL_GetModState() & KMOD_CTRL) != 0);
                        io.KeySuper      = ((SDL_GetModState() & KMOD_GUI) != 0);
                        break;
                    }
                }
            }
            nevents++;
        }

        // Now, loop over the events a second time for app-side processing.
        for (int i = 0; i < nevents; i++)
        {
            const SDL_Event& event = events[i];
            ImGuiIO*         io    = mImGuiHelper ? &ImGui::GetIO() : nullptr;
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
                    mMainWindow->onKeyDown(event.key.keysym.scancode);
                    break;
                case SDL_KEYUP:
                    mMainWindow->onKeyUp(event.key.keysym.scancode);
                    break;
                case SDL_MOUSEWHEEL:
                    if (!io || !io->WantCaptureMouse)
                        mMainWindow->onMouseWheel(event.wheel.y);
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    if (!io || !io->WantCaptureMouse)
                        mMainWindow->onMouseDown(event.button.button, event.button.x, event.button.y);
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (!io || !io->WantCaptureMouse)
                        mMainWindow->onMouseUp(event.button.x, event.button.y);
                    break;
                case SDL_MOUSEMOTION:
                    if (!io || !io->WantCaptureMouse)
                        mMainWindow->onMouseMoved(event.motion.x, event.motion.y);
                    break;
                case SDL_WINDOWEVENT:
                    switch (event.window.event)
                    {
                        case SDL_WINDOWEVENT_RESIZED:
                            mMainWindow->onResize();
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

        // Calculate the time step.
        static Uint64 frequency = SDL_GetPerformanceFrequency();
        Uint64        now       = SDL_GetPerformanceCounter();
        const float   timeStep  = mTime > 0 ? (float)((double)(now - mTime) / frequency) : (float)(1.0f / 60.0f);
        mTime                   = now;

        // Populate the UI scene, regardless of whether Filament wants to a skip frame. We should
        // always let ImGui generate a command list; if it skips a frame it'll destroy its widgets.
        if (mImGuiHelper)
        {

            // Inform ImGui of the current window size in case it was resized.

            int windowWidth, windowHeight;
            int displayWidth, displayHeight;
            SDL_GetWindowSize(mMainWindow->getWindowHandle(), &windowWidth, &windowHeight);
            SDL_GL_GetDrawableSize(mMainWindow->getWindowHandle(), &displayWidth, &displayHeight);
            mImGuiHelper->setDisplaySize(windowWidth, windowHeight,
                                         windowWidth > 0 ? ((float)displayWidth / windowWidth) : 0,
                                         displayHeight > 0 ? ((float)displayHeight / windowHeight) : 0);

            
            // Setup mouse inputs (we already got mouse wheel, keyboard keys & characters
            // from our event handler)
            ImGuiIO& io = ImGui::GetIO();
            int      mx, my;
            Uint32   buttons = SDL_GetMouseState(&mx, &my);
            io.MousePos      = ImVec2(-FLT_MAX, -FLT_MAX);
            io.MouseDown[0]  = mousePressed[0] || (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
            io.MouseDown[1]  = mousePressed[1] || (buttons & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;
            io.MouseDown[2]  = mousePressed[2] || (buttons & SDL_BUTTON(SDL_BUTTON_MIDDLE)) != 0;
            mousePressed[0] = mousePressed[1] = mousePressed[2] = false;

            // TODO: Update to a newer SDL and use SDL_CaptureMouse() to retrieve mouse coordinates
            // outside of the client area; see the imgui SDL example.
            if ((SDL_GetWindowFlags(mMainWindow->getWindowHandle()) & SDL_WINDOW_INPUT_FOCUS) != 0)
            {
                io.MousePos = ImVec2((float)mx, (float)my);
            }

            // Populate the UI Scene.
            mImGuiHelper->render(timeStep, imguiCallback);
        }

        // Update the camera manipulators for each view.
        for (auto const& view : mMainWindow->getViews())
        {
            auto* cm = view->getCameraManipulator();
            if (cm)
            {
                cm->update(timeStep);
            }
        }

        // Update the position and orientation of the two cameras.
        filament::math::float3 eye, center, up;
        mMainWindow->getMainCameraMan()->getLookAt(&eye, &center, &up);
        mMainWindow->getMainCamera()->lookAt(eye, center, up);
        mMainWindow->getDebugCameraMan()->getLookAt(&eye, &center, &up);
        mMainWindow->getDebugCamera()->lookAt(eye, center, up);

        // Update the cube distortion matrix used for frustum visualization.
        const Camera* lightmapCamera = mMainWindow->getMainView()->getView()->getDirectionalLightCamera();
        lightmapCube->mapFrustum(*mRenderEngine, lightmapCamera);
        cameraCube->mapFrustum(*mRenderEngine, mMainWindow->getMainCamera());

        // Delay rendering for roughly one monitor refresh interval
        // TODO: Use SDL_GL_SetSwapInterval for proper vsync
        SDL_DisplayMode Mode;
        int             refreshIntervalMS = (SDL_GetCurrentDisplayMode(
                                     SDL_GetWindowDisplayIndex(mMainWindow->getWindowHandle()), &Mode) == 0 &&
                                 Mode.refresh_rate != 0) ?
                        round(1000.0 / Mode.refresh_rate) :
                        16;
        SDL_Delay(refreshIntervalMS);

        preRender(mMainWindow->getMainView()->getView(), mScene, mMainWindow->getRenderer());

        if (mMainWindow->getRenderer()->beginFrame(mMainWindow->getSwapChain()))
        {
            for (filament::View* offscreenView : mOffscreenViews)
            {
                mMainWindow->getRenderer()->render(offscreenView);
            }
            for (auto const& view : mMainWindow->getViews())
            {
                mMainWindow->getRenderer()->render(view->getView());
            }

            postRender(mMainWindow->getMainView()->getView(), mScene, mMainWindow->getRenderer());

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
    mMainWindow->getCameraFocalLength() = viewerOptions.cameraFocalLength;

    const size_t cameraCount = mAsset->getCameraEntityCount();
    view->setCamera(mMainWindow->getMainCamera());

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
    Automation::get().tick(view, mInstance, renderer, ImGui::GetIO().DeltaTime);
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

void GameDriver::gui(filament::Engine* ,filament::View* view)
{
    updateUserInterface();

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
    mMainWindow->getMainView()->getView()->setAmbientOcclusionOptions({.upsampling = View::QualityLevel::HIGH});

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
