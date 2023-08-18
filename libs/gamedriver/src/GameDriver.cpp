
#include "gamedriver/BaseLibs.h"
#include "gamedriver/GameDriver.h"

#include "gamedriver/ScriptVM.h"
#include "gamedriver/Resource.h"
#include "gamedriver/MeshAssimp.h"
#include "gamedriver/Cube.h"
#include "gamedriver/NativeWindowHelper.h"
#include "generated/resources/gamedriver.h"

using namespace filament;
using namespace filagui;
using namespace filament::math;
using namespace utils;


GameDriver::GameDriver() {
    ASSERT_POSTCONDITION(SDL_Init(SDL_INIT_EVERYTHING) == 0, "SDL_Init Failure");
}

GameDriver::~GameDriver() {
    SDL_Quit();
}

View* GameDriver::getGuiView() const noexcept {
    return mImGuiHelper->getView();
}

void GameDriver::mainLoop()
{
    static const char* DEFAULT_IBL = "assets/ibl/lightroom_14b";
    mConfig.iblDirectory = Resource::get().getRootPath() + DEFAULT_IBL;
  
    std::unique_ptr<Window> window(new Window(this, mConfig));

    mDepthMaterial = Material::Builder()
            .package(GAMEDRIVER_DEPTHVISUALIZER_DATA, GAMEDRIVER_DEPTHVISUALIZER_SIZE)
            .build(*mEngine);

    mDepthMI = mDepthMaterial->createInstance();

    mDefaultMaterial = Material::Builder()
            .package(GAMEDRIVER_AIDEFAULTMAT_DATA, GAMEDRIVER_AIDEFAULTMAT_SIZE)
            .build(*mEngine);

    mTransparentMaterial = Material::Builder()
            .package(GAMEDRIVER_TRANSPARENTCOLOR_DATA, GAMEDRIVER_TRANSPARENTCOLOR_SIZE)
            .build(*mEngine);

    std::unique_ptr<Cube> cameraCube(new Cube(*mEngine, mTransparentMaterial, {1,0,0}));
    // we can't cull the light-frustum because it's not applied a rigid transform
    // and currently, filament assumes that for culling
    std::unique_ptr<Cube> lightmapCube(new Cube(*mEngine, mTransparentMaterial, {0,1,0}, false));
    mScene = mEngine->createScene();

    window->mMainView->getView()->setVisibleLayers(0x4, 0x4);
    if (mConfig.splitView)
    {
        auto& rcm = mEngine->getRenderableManager();

        rcm.setLayerMask(rcm.getInstance(cameraCube->getSolidRenderable()), 0x3, 0x2);
        rcm.setLayerMask(rcm.getInstance(cameraCube->getWireFrameRenderable()), 0x3, 0x2);

        rcm.setLayerMask(rcm.getInstance(lightmapCube->getSolidRenderable()), 0x3, 0x2);
        rcm.setLayerMask(rcm.getInstance(lightmapCube->getWireFrameRenderable()), 0x3, 0x2);

        // Create the camera mesh
        mScene->addEntity(cameraCube->getWireFrameRenderable());
        mScene->addEntity(cameraCube->getSolidRenderable());

        mScene->addEntity(lightmapCube->getWireFrameRenderable());
        mScene->addEntity(lightmapCube->getSolidRenderable());

        window->mDepthView->getView()->setVisibleLayers(0x4, 0x4);
        window->mGodView->getView()->setVisibleLayers(0x6, 0x6);
        window->mOrthoView->getView()->setVisibleLayers(0x6, 0x6);

        // only preserve the color buffer for additional views; depth and stencil can be discarded.
        window->mDepthView->getView()->setShadowingEnabled(false);
        window->mGodView->getView()->setShadowingEnabled(false);
        window->mOrthoView->getView()->setShadowingEnabled(false);
    }

    loadDirt();
    loadIBL();
    if (mIBL != nullptr) {
        mIBL->getSkybox()->setLayerMask(0x7, 0x4);
        mScene->setSkybox(mIBL->getSkybox());
        mScene->setIndirectLight(mIBL->getIndirectLight());
    }

    for (auto& view : window->mViews) {
        if (view.get() != window->mUiView) {
            view->getView()->setScene(mScene);
        }
    }

    setup(window->mMainView->getView());

    ImGuiCallback imguiCallback = std::bind(&GameDriver::gui, this, mEngine, std::placeholders::_2);

    if (imguiCallback) {
        mImGuiHelper = std::make_unique<ImGuiHelper>(mEngine, window->mUiView->getView(),
            Resource::get().getRootPath() + "assets/fonts/Roboto-Medium.ttf");
        ImGuiIO& io = ImGui::GetIO();
        #ifdef WIN32
            SDL_SysWMinfo wmInfo;
            SDL_VERSION(&wmInfo.version);
            SDL_GetWindowWMInfo(window->getSDLWindow(), &wmInfo);
            io.ImeWindowHandle = wmInfo.info.win.window;
        #endif
        io.KeyMap[ImGuiKey_Tab] = SDL_SCANCODE_TAB;
        io.KeyMap[ImGuiKey_LeftArrow] = SDL_SCANCODE_LEFT;
        io.KeyMap[ImGuiKey_RightArrow] = SDL_SCANCODE_RIGHT;
        io.KeyMap[ImGuiKey_UpArrow] = SDL_SCANCODE_UP;
        io.KeyMap[ImGuiKey_DownArrow] = SDL_SCANCODE_DOWN;
        io.KeyMap[ImGuiKey_PageUp] = SDL_SCANCODE_PAGEUP;
        io.KeyMap[ImGuiKey_PageDown] = SDL_SCANCODE_PAGEDOWN;
        io.KeyMap[ImGuiKey_Home] = SDL_SCANCODE_HOME;
        io.KeyMap[ImGuiKey_End] = SDL_SCANCODE_END;
        io.KeyMap[ImGuiKey_Insert] = SDL_SCANCODE_INSERT;
        io.KeyMap[ImGuiKey_Delete] = SDL_SCANCODE_DELETE;
        io.KeyMap[ImGuiKey_Backspace] = SDL_SCANCODE_BACKSPACE;
        io.KeyMap[ImGuiKey_Space] = SDL_SCANCODE_SPACE;
        io.KeyMap[ImGuiKey_Enter] = SDL_SCANCODE_RETURN;
        io.KeyMap[ImGuiKey_Escape] = SDL_SCANCODE_ESCAPE;
        io.KeyMap[ImGuiKey_A] = SDL_SCANCODE_A;
        io.KeyMap[ImGuiKey_C] = SDL_SCANCODE_C;
        io.KeyMap[ImGuiKey_V] = SDL_SCANCODE_V;
        io.KeyMap[ImGuiKey_X] = SDL_SCANCODE_X;
        io.KeyMap[ImGuiKey_Y] = SDL_SCANCODE_Y;
        io.KeyMap[ImGuiKey_Z] = SDL_SCANCODE_Z;
        io.SetClipboardTextFn = [](void*, const char* text) {
            SDL_SetClipboardText(text);
        };
        io.GetClipboardTextFn = [](void*) -> const char* {
            return SDL_GetClipboardText();
        };
        io.ClipboardUserData = nullptr;
    }

    bool mousePressed[3] = { false };

    int sidebarWidth = mSidebarWidth;
    float cameraFocalLength = mCameraFocalLength;

    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

    while (!mClosed) {
        
        if (mSidebarWidth != sidebarWidth || mCameraFocalLength != cameraFocalLength) {
            window->configureCamerasForWindow();
            sidebarWidth = mSidebarWidth;
            cameraFocalLength = mCameraFocalLength;
        }

        if (!UTILS_HAS_THREADING) {
            mEngine->execute();
        }

        // Allow the app to animate the scene if desired.
        //if (mAnimation) {
        {
            double now = (double)SDL_GetPerformanceCounter() / SDL_GetPerformanceFrequency();
            animate(window->mMainView->getView(), now);
        }
            //}

        // Loop over fresh events twice: first stash them and let ImGui process them, then allow
        // the app to process the stashed events. This is done because ImGui might wish to block
        // certain events from the app (e.g., when dragging the mouse over an obscuring window).
        constexpr int kMaxEvents = 16;
        SDL_Event events[kMaxEvents];
        int nevents = 0;
        while (nevents < kMaxEvents && SDL_PollEvent(&events[nevents]) != 0) {
            if (mImGuiHelper) {
                ImGuiIO& io = ImGui::GetIO();
                SDL_Event* event = &events[nevents];
                switch (event->type) {
                    case SDL_MOUSEWHEEL: {
                        if (event->wheel.x > 0) io.MouseWheelH += 1;
                        if (event->wheel.x < 0) io.MouseWheelH -= 1;
                        if (event->wheel.y > 0) io.MouseWheel += 1;
                        if (event->wheel.y < 0) io.MouseWheel -= 1;
                        break;
                    }
                    case SDL_MOUSEBUTTONDOWN: {
                        if (event->button.button == SDL_BUTTON_LEFT) mousePressed[0] = true;
                        if (event->button.button == SDL_BUTTON_RIGHT) mousePressed[1] = true;
                        if (event->button.button == SDL_BUTTON_MIDDLE) mousePressed[2] = true;
                        break;
                    }
                    case SDL_TEXTINPUT: {
                        io.AddInputCharactersUTF8(event->text.text);
                        break;
                    }
                    case SDL_KEYDOWN:
                    case SDL_KEYUP: {
                        int key = event->key.keysym.scancode;
                        IM_ASSERT(key >= 0 && key < IM_ARRAYSIZE(io.KeysDown));
                        io.KeysDown[key] = (event->type == SDL_KEYDOWN);
                        io.KeyShift = ((SDL_GetModState() & KMOD_SHIFT) != 0);
                        io.KeyAlt = ((SDL_GetModState() & KMOD_ALT) != 0);
                        io.KeyCtrl = ((SDL_GetModState() & KMOD_CTRL) != 0);
                        io.KeySuper = ((SDL_GetModState() & KMOD_GUI) != 0);
                        break;
                    }
                }
            }
            nevents++;
        }

        // Now, loop over the events a second time for app-side processing.
        for (int i = 0; i < nevents; i++) {
            const SDL_Event& event = events[i];
            ImGuiIO* io = mImGuiHelper ? &ImGui::GetIO() : nullptr;
            switch (event.type) {
                case SDL_QUIT:
                    mClosed = true;
                    break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                        mClosed = true;
                    }
#ifndef NDEBUG
                    if (event.key.keysym.scancode == SDL_SCANCODE_PRINTSCREEN) {
                        DebugRegistry& debug = mEngine->getDebugRegistry();
                        bool* captureFrame =
                                debug.getPropertyAddress<bool>("d.renderer.doFrameCapture");
                        *captureFrame = true;
                    }
#endif
                    window->keyDown(event.key.keysym.scancode);
                    break;
                case SDL_KEYUP:
                    window->keyUp(event.key.keysym.scancode);
                    break;
                case SDL_MOUSEWHEEL:
                    if (!io || !io->WantCaptureMouse)
                        window->mouseWheel(event.wheel.y);
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    if (!io || !io->WantCaptureMouse)
                        window->mouseDown(event.button.button, event.button.x, event.button.y);
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (!io || !io->WantCaptureMouse)
                        window->mouseUp(event.button.x, event.button.y);
                    break;
                case SDL_MOUSEMOTION:
                    if (!io || !io->WantCaptureMouse)
                        window->mouseMoved(event.motion.x, event.motion.y);
                    break;
                case SDL_DROPFILE:
                    if (mDropHandler) {
                        mDropHandler(event.drop.file);
                    }
                    SDL_free(event.drop.file);
                    break;
                case SDL_WINDOWEVENT:
                    switch (event.window.event) {
                        case SDL_WINDOWEVENT_RESIZED:
                            window->resize();
                            break;
                        default:
                            break;
                    }
                    break;
                case SDL_APP_DIDENTERBACKGROUND:

                    break;
                case SDL_APP_DIDENTERFOREGROUND:
                    mEngine->setActiveFeatureLevel(mConfig.featureLevel);
                    window->mSwapChain = mEngine->createSwapChain(::getNativeWindow(window->mWindow));
                    break;
                default:
                    break;
            }
        }

        // Calculate the time step.
        static Uint64 frequency = SDL_GetPerformanceFrequency();
        Uint64 now = SDL_GetPerformanceCounter();
        const float timeStep = mTime > 0 ? (float)((double)(now - mTime) / frequency) :
                (float)(1.0f / 60.0f);
        mTime = now;

        // Populate the UI scene, regardless of whether Filament wants to a skip frame. We should
        // always let ImGui generate a command list; if it skips a frame it'll destroy its widgets.
        if (mImGuiHelper) {

            // Inform ImGui of the current window size in case it was resized.
            
                int windowWidth, windowHeight;
                int displayWidth, displayHeight;
                SDL_GetWindowSize(window->mWindow, &windowWidth, &windowHeight);
                SDL_GL_GetDrawableSize(window->mWindow, &displayWidth, &displayHeight);
                mImGuiHelper->setDisplaySize(windowWidth, windowHeight,
                        windowWidth > 0 ? ((float)displayWidth / windowWidth) : 0,
                        displayHeight > 0 ? ((float)displayHeight / windowHeight) : 0);
            

            // Setup mouse inputs (we already got mouse wheel, keyboard keys & characters
            // from our event handler)
            ImGuiIO& io = ImGui::GetIO();
            int mx, my;
            Uint32 buttons = SDL_GetMouseState(&mx, &my);
            io.MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
            io.MouseDown[0] = mousePressed[0] || (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
            io.MouseDown[1] = mousePressed[1] || (buttons & SDL_BUTTON(SDL_BUTTON_RIGHT)) != 0;
            io.MouseDown[2] = mousePressed[2] || (buttons & SDL_BUTTON(SDL_BUTTON_MIDDLE)) != 0;
            mousePressed[0] = mousePressed[1] = mousePressed[2] = false;

            // TODO: Update to a newer SDL and use SDL_CaptureMouse() to retrieve mouse coordinates
            // outside of the client area; see the imgui SDL example.
            if ((SDL_GetWindowFlags(window->mWindow) & SDL_WINDOW_INPUT_FOCUS) != 0) {
                io.MousePos = ImVec2((float)mx, (float)my);
            }

            // Populate the UI Scene.
            mImGuiHelper->render(timeStep, imguiCallback);
        }

        // Update the camera manipulators for each view.
        for (auto const& view : window->mViews) {
            auto* cm = view->getCameraManipulator();
            if (cm) {
                cm->update(timeStep);
            }
        }

        // Update the position and orientation of the two cameras.
        filament::math::float3 eye, center, up;
        window->mMainCameraMan->getLookAt(&eye, &center, &up);
        window->mMainCamera->lookAt(eye, center, up);
        window->mDebugCameraMan->getLookAt(&eye, &center, &up);
        window->mDebugCamera->lookAt(eye, center, up);

        // Update the cube distortion matrix used for frustum visualization.
        const Camera* lightmapCamera = window->mMainView->getView()->getDirectionalLightCamera();
        lightmapCube->mapFrustum(*mEngine, lightmapCamera);
        cameraCube->mapFrustum(*mEngine, window->mMainCamera);

        // Delay rendering for roughly one monitor refresh interval
        // TODO: Use SDL_GL_SetSwapInterval for proper vsync
        SDL_DisplayMode Mode;
        int refreshIntervalMS = (SDL_GetCurrentDisplayMode(
            SDL_GetWindowDisplayIndex(window->mWindow), &Mode) == 0 &&
            Mode.refresh_rate != 0) ? round(1000.0 / Mode.refresh_rate) : 16;
        SDL_Delay(refreshIntervalMS);

        Renderer* renderer = window->getRenderer();

        //if (preRender) {
            preRender( window->mViews[0]->getView(), mScene, renderer);
        //}

        if (renderer->beginFrame(window->getSwapChain())) {
            for (filament::View* offscreenView : mOffscreenViews) {
                renderer->render(offscreenView);
            }
            for (auto const& view : window->mViews) {
                renderer->render(view->getView());
            }
            //if (postRender) {
                postRender(window->mViews[0]->getView(), mScene, renderer);
            //}
            renderer->endFrame();

        } else {
            ++mSkippedFrames;
        }
    }

    if (mImGuiHelper) {
        mImGuiHelper.reset();
    }

    cleanup();

    cameraCube.reset();
    lightmapCube.reset();
    window.reset();

    mIBL.reset();
    mEngine->destroy(mDepthMI);
    mEngine->destroy(mDepthMaterial);
    mEngine->destroy(mDefaultMaterial);
    mEngine->destroy(mTransparentMaterial);
    mEngine->destroy(mScene);
    Engine::destroy(&mEngine);
    mEngine = nullptr;
}

void GameDriver::loadIBL()
{
    if (!mConfig.iblDirectory.empty())
    {
        Path iblPath(mConfig.iblDirectory);

        if (!iblPath.exists())
        {
            std::cerr << "The specified IBL path does not exist: " << iblPath << std::endl;
            return;
        }

        mIBL = std::make_unique<IBL>(*mEngine);

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
}

void GameDriver::loadDirt()
{
    if (!mConfig.dirt.empty())
    {
        Path dirtPath(mConfig.dirt);

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
                    .build(*mEngine);

        mDirt->setImage(*mEngine, 0, {data, size_t(w * h * 3), Texture::Format::RGB, Texture::Type::UBYTE, (Texture::PixelBufferDescriptor::Callback)&stbi_image_free});
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

void GameDriver::loadResources( utils::Path filename)
{
    // Load external textures and buffers.
    std::string           gltfPath         = filename.getAbsolutePath();
    filament::gltfio::ResourceConfiguration configuration    = {};
    configuration.engine                   = mEngine;
    configuration.gltfPath                 = gltfPath.c_str();
    configuration.normalizeSkinningWeights = true;

    if (!mResourceLoader)
    {
        mResourceLoader = new gltfio::ResourceLoader(configuration);
        mStbDecoder     = filament::gltfio::createStbProvider(mEngine);
        mKtxDecoder     = filament::gltfio::createKtx2Provider(mEngine);
        mResourceLoader->addTextureProvider("image/png", mStbDecoder);
        mResourceLoader->addTextureProvider("image/jpeg",mStbDecoder);
        mResourceLoader->addTextureProvider("image/ktx2",mKtxDecoder);
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
    const size_t                   matInstanceCount = mInstance->getMaterialInstanceCount();
    filament::MaterialInstance* const* const instances        = mInstance->getMaterialInstances();
    for (int mi = 0; mi < matInstanceCount; mi++)
    {
        instances[mi]->setStencilWrite(true);
        instances[mi]->setStencilOpDepthStencilPass(filament::MaterialInstance::StencilOperation::INCR);
    }

    if (mIBL)
    {
        mViewerGUI->setIndirectLight(mIBL->getIndirectLight(), mIBL->getSphericalHarmonics());
    }
};

void GameDriver::createGroundPlane()
{
    auto&     em             = EntityManager::get();
    Material* shadowMaterial = Material::Builder()
                                   .package(GAMEDRIVER_GROUNDSHADOW_DATA, GAMEDRIVER_GROUNDSHADOW_SIZE)
                                   .build(*mEngine);
    auto& viewerOptions = mViewerGUI->getSettings().viewer;
    shadowMaterial->setDefaultParameter("strength", viewerOptions.groundShadowStrength);

    const static uint32_t indices[] = {
        0, 1, 2, 2, 3, 0};

    Aabb aabb = mAsset->getBoundingBox();
    if (!mActualSize)
    {
        mat4f transform = filament::viewer::fitIntoUnitCube(aabb, 4);
        aabb            = aabb.transform(transform);
    }

    float3 planeExtent{10.0f * aabb.extent().x, 0.0f, 10.0f * aabb.extent().z};

    const static float3 vertices[] = {
        {-planeExtent.x, 0, -planeExtent.z},
        {-planeExtent.x, 0, planeExtent.z},
        {planeExtent.x, 0, planeExtent.z},
        {planeExtent.x, 0, -planeExtent.z},
    };

    short4 tbn = packSnorm16(
        mat3f::packTangentFrame(
            mat3f{
                float3{1.0f, 0.0f, 0.0f},
                float3{0.0f, 0.0f, 1.0f},
                float3{0.0f, 1.0f, 0.0f}})
            .xyzw);

    const static short4 normals[]{tbn, tbn, tbn, tbn};

    VertexBuffer* vertexBuffer = VertexBuffer::Builder()
                                     .vertexCount(4)
                                     .bufferCount(2)
                                     .attribute(VertexAttribute::POSITION,
                                                0, VertexBuffer::AttributeType::FLOAT3)
                                     .attribute(VertexAttribute::TANGENTS,
                                                1, VertexBuffer::AttributeType::SHORT4)
                                     .normalized(VertexAttribute::TANGENTS)
                                     .build(*mEngine);

    vertexBuffer->setBufferAt(*mEngine, 0, VertexBuffer::BufferDescriptor(vertices, vertexBuffer->getVertexCount() * sizeof(vertices[0])));
    vertexBuffer->setBufferAt(*mEngine, 1, VertexBuffer::BufferDescriptor(normals, vertexBuffer->getVertexCount() * sizeof(normals[0])));

    IndexBuffer* indexBuffer = IndexBuffer::Builder()
                                   .indexCount(6)
                                   .build(*mEngine);

    indexBuffer->setBuffer(*mEngine, IndexBuffer::BufferDescriptor(indices, indexBuffer->getIndexCount() * sizeof(uint32_t)));

    Entity groundPlane = em.create();
    RenderableManager::Builder(1)
        .boundingBox({{}, {planeExtent.x, 1e-4f, planeExtent.z}})
        .material(0, shadowMaterial->getDefaultInstance())
        .geometry(0, RenderableManager::PrimitiveType::TRIANGLES,
                  vertexBuffer, indexBuffer, 0, 6)
        .culling(false)
        .receiveShadows(true)
        .castShadows(false)
        .build(*mEngine, groundPlane);

    mScene->addEntity(groundPlane);

    auto& tcm = mEngine->getTransformManager();
    tcm.setTransform(tcm.getInstance(groundPlane),
                     mat4f::translation(float3{0, aabb.min.y, -4}));

    auto& rcm      = mEngine->getRenderableManager();
    auto  instance = rcm.getInstance(groundPlane);
    rcm.setLayerMask(instance, 0xff, 0x00);

    mGround.mGroundPlane        = groundPlane;
    mGround.mGroundVertexBuffer = vertexBuffer;
    mGround.mGroundIndexBuffer  = indexBuffer;
    mGround.mGroundMaterial     = shadowMaterial;
}

void GameDriver::createOverdrawVisualizerEntities()
{
    static constexpr float4 sFullScreenTriangleVertices[3] = {
        {-1.0f, -1.0f, 1.0f, 1.0f},
        {3.0f, -1.0f, 1.0f, 1.0f},
        {-1.0f, 3.0f, 1.0f, 1.0f}};

    static const uint16_t sFullScreenTriangleIndices[3] = {0, 1, 2};

    Material* material = Material::Builder()
                             .package(GAMEDRIVER_OVERDRAW_DATA, GAMEDRIVER_OVERDRAW_SIZE)
                             .build(*mEngine);

    const float3 overdrawColors[GameDriver::Overdraw::OVERDRAW_LAYERS] = {
        {0.0f, 0.0f, 1.0f}, // blue         (overdrawn 1 time)
        {0.0f, 1.0f, 0.0f}, // green        (overdrawn 2 times)
        {1.0f, 0.0f, 1.0f}, // magenta      (overdrawn 3 times)
        {1.0f, 0.0f, 0.0f}  // red          (overdrawn 4+ times)
    };

    for (auto i = 0; i < GameDriver::Overdraw::OVERDRAW_LAYERS; i++)
    {
        MaterialInstance* matInstance = material->createInstance();
        // TODO: move this to the material definition.
        matInstance->setStencilCompareFunction(MaterialInstance::StencilCompareFunc::E);
        // The stencil value represents the number of times the fragment has been written to.
        // We want 0-1 writes to be the regular color. Overdraw visualization starts at 2+ writes,
        // which represents a fragment overdrawn 1 time.
        matInstance->setStencilReferenceValue(i + 2);
        matInstance->setParameter("color", overdrawColors[i]);
        mOverdraw.mOverdrawMaterialInstances[i] = matInstance;
    }
    auto& lastMi = mOverdraw.mOverdrawMaterialInstances[GameDriver::Overdraw::OVERDRAW_LAYERS - 1];
    // This seems backwards, but it isn't. The comparison function compares:
    // the reference value (left side) <= stored stencil value (right side)
    lastMi->setStencilCompareFunction(MaterialInstance::StencilCompareFunc::LE);

    VertexBuffer* vertexBuffer = VertexBuffer::Builder()
                                     .vertexCount(3)
                                     .bufferCount(1)
                                     .attribute(VertexAttribute::POSITION, 0, VertexBuffer::AttributeType::FLOAT4, 0)
                                     .build(*mEngine);

    vertexBuffer->setBufferAt(
        *mEngine, 0, {sFullScreenTriangleVertices, sizeof(sFullScreenTriangleVertices)});

    IndexBuffer* indexBuffer = IndexBuffer::Builder()
                                   .indexCount(3)
                                   .bufferType(IndexBuffer::IndexType::USHORT)
                                   .build(*mEngine);

    indexBuffer->setBuffer(*mEngine,
                           {sFullScreenTriangleIndices, sizeof(sFullScreenTriangleIndices)});

    auto&       em           = EntityManager::get();
    const auto& matInstances = mOverdraw.mOverdrawMaterialInstances;
    for (auto i = 0; i < GameDriver::Overdraw::OVERDRAW_LAYERS; i++)
    {
        Entity overdrawEntity = em.create();
        RenderableManager::Builder(1)
            .boundingBox({{}, {1.0f, 1.0f, 1.0f}})
            .material(0, matInstances[i])
            .geometry(0, RenderableManager::PrimitiveType::TRIANGLES, vertexBuffer, indexBuffer, 0, 3)
            .culling(false)
            .priority(7u) // ensure the overdraw primitives are drawn last
            .layerMask(0xFF, 1u << GameDriver::Overdraw::OVERDRAW_VISIBILITY_LAYER)
            .build(*mEngine, overdrawEntity);
        mScene->addEntity(overdrawEntity);
        mOverdraw.mOverdrawVisualizer[i] = overdrawEntity;
    }

    mOverdraw.mOverdrawMaterial               = material;
    mOverdraw.mFullScreenTriangleVertexBuffer = vertexBuffer;
    mOverdraw.mFullScreenTriangleIndexBuffer  = indexBuffer;
}

static void onClick(GameDriver& app, View* view, ImVec2 pos)
{
    view->pick(pos.x, pos.y, [&app](View::PickingQueryResult const& result) {
        if (const char* name = app.mAsset->getName(result.renderable); name)
        {
            app.mNotificationText = name;
        }
        else
        {
            app.mNotificationText.clear();
        }
    });
}

void GameDriver::preRender(View* view, Scene* scene, Renderer* renderer)
{
    auto&       rcm           = mEngine->getRenderableManager();
    auto        instance      = rcm.getInstance(mGround.mGroundPlane);
    const auto  viewerOptions = mAutomationEngine->getViewerOptions();
    const auto& dofOptions    = mViewerGUI->getSettings().view.dof;
    rcm.setLayerMask(instance, 0xff, viewerOptions.groundPlaneEnabled ? 0xff : 0x00);

    mEngine->setAutomaticInstancingEnabled(viewerOptions.autoInstancingEnabled);

    // Note that this focal length might be different from the slider value because the
    // automation engine applies Camera::computeEffectiveFocalLength when DoF is enabled.
    mCameraFocalLength = viewerOptions.cameraFocalLength;

    const size_t cameraCount = mAsset->getCameraEntityCount();
    view->setCamera(mMainCamera);

    const int currentCamera = mViewerGUI->getCurrentCamera();
    if (currentCamera > 0 && currentCamera <= cameraCount)
    {
        const utils::Entity* cameras = mAsset->getCameraEntities();
        Camera*              camera  = mEngine->getCameraComponent(cameras[currentCamera - 1]);
        assert_invariant(camera);
        view->setCamera(camera);

        // Override the aspect ratio in the glTF file and adjust the aspect ratio of this
        // camera to the viewport.
        const Viewport& vp          = view->getViewport();
        double          aspectRatio = (double)vp.width / vp.height;
        camera->setScaling({1.0 / aspectRatio, 1.0});
    }

    mGround.mGroundMaterial->setDefaultParameter("strength", viewerOptions.groundShadowStrength);

    // This applies clear options, the skybox mask, and some camera settings.
    Camera& camera = view->getCamera();
    Skybox* skybox = scene->getSkybox();
    filament::viewer::applySettings(mEngine, mViewerGUI->getSettings().viewer, &camera, skybox, renderer);

    // Check if color grading has changed.
    filament::viewer::ColorGradingSettings& options = mViewerGUI->getSettings().view.colorGrading;
    if (options.enabled)
    {
        if (options != mLastColorGradingOptions)
        {
            filament::ColorGrading* colorGrading = filament::viewer::createColorGrading(options, mEngine);
            mEngine->destroy(mColorGrading);
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
    if (mAutomationEngine->shouldClose())
    {
        close();
        return;
    }
    filament::viewer:: AutomationEngine::ViewerContent content = {
        .view          = view,
        .renderer      = renderer,
        .materials     = mInstance->getMaterialInstances(),
        .materialCount = mInstance->getMaterialInstanceCount(),
    };
    mAutomationEngine->tick(mEngine, content, ImGui::GetIO().DeltaTime);
}

void GameDriver::animate(filament::View* view, double now)
{
    mResourceLoader->asyncUpdateLoad();

    // Optionally fit the model into a unit cube at the origin.
    mViewerGUI->updateRootTransform();

    // Gradually add renderables to the scene as their textures become ready.
    mViewerGUI->populateScene();

    mViewerGUI->applyAnimation(now);
}

void GameDriver::resize(filament::View* view)
{
    Camera& camera = view->getCamera();
    if (&camera == mMainCamera)
    {
        // Don't adjust the aspect ratio of the main camera, this is done inside of
        // FilamentGameDriver.cpp
        return;
    }
    const Viewport& vp          = view->getViewport();
    double          aspectRatio = (double)vp.width / vp.height;
    camera.setScaling({1.0 / aspectRatio, 1.0});
}

void GameDriver::gui(filament::Engine* ,filament::View* view)
{
    mViewerGUI->updateUserInterface();

    setSidebarWidth(mViewerGUI->getSidebarWidth());
}

void GameDriver::setup(filament::View* view)
{
    mNames                                         = new utils::NameComponentManager(EntityManager::get());
    mViewerGUI                                        = new filament::viewer::ViewerGui(mEngine, mScene, view, 410);
    mViewerGUI->getSettings().viewer.autoScaleEnabled = !mActualSize;

    const bool batchMode = !mBatchFile.empty();

    // First check if a custom automation spec has been provided. If it fails to load, the app
    // must be closed since it could be invoked from a script.
    if (batchMode && mBatchFile != "default")
    {
        auto size = getFileSize(mBatchFile.c_str());
        if (size > 0)
        {
            std::ifstream     in(mBatchFile, std::ifstream::binary | std::ifstream::in);
            std::vector<char> json(static_cast<unsigned long>(size));
            in.read(json.data(), size);
            mAutomationSpec = filament::viewer::AutomationSpec::generate(json.data(), size);
            if (!mAutomationSpec)
            {
                std::cerr << "Unable to parse automation spec: " << mBatchFile << std::endl;
                exit(1);
            }
        }
        else
        {
            std::cerr << "Unable to load automation spec: " << mBatchFile << std::endl;
            exit(1);
        }
    }

    // If no custom spec has been provided, or if in interactive mode, load the default spec.
    if (!mAutomationSpec)
    {
        mAutomationSpec = filament::viewer::AutomationSpec::generateDefaultTestCases();
    }

    mAutomationEngine = new filament::viewer::AutomationEngine(mAutomationSpec, &mViewerGUI->getSettings());

    if (batchMode)
    {
        mAutomationEngine->startBatchMode();
        auto options              = mAutomationEngine->getOptions();
        options.sleepDuration     = 0.0;
        options.exportScreenshots = true;
        options.exportSettings    = true;
        mAutomationEngine->setOptions(options);
        mViewerGUI->stopAnimation();
    }

    if (mSettingsFile.size() > 0)
    {
        bool success = loadSettings(mSettingsFile.c_str(), &mViewerGUI->getSettings());
        if (success)
        {
            std::cout << "Loaded settings from " << mSettingsFile << std::endl;
        }
        else
        {
            std::cerr << "Failed to load settings from " << mSettingsFile << std::endl;
        }
    }

    mMaterials = (mMaterialSource == JITSHADER) ? filament::gltfio::createJitShaderProvider(mEngine) : filament::gltfio::createUbershaderProvider(mEngine, UBERARCHIVE_DEFAULT_DATA, UBERARCHIVE_DEFAULT_SIZE);

    mAssetLoader = filament::gltfio::AssetLoader::create({mEngine, mMaterials, mNames});
    mMainCamera  = &view->getCamera();

    auto filename = utils::Path(mConfig.filename.c_str());

    loadAsset(filename);
    loadResources(filename);

    mViewerGUI->setAsset(mAsset, mInstance);

    createGroundPlane();
    createOverdrawVisualizerEntities();

    mViewerGUI->setUiCallback(
        [this, view]() {
            auto& automation = *mAutomationEngine;

            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                ImVec2 pos = ImGui::GetMousePos();
                pos.x -= mViewerGUI->getSidebarWidth();
                pos.x *= ImGui::GetIO().DisplayFramebufferScale.x;
                pos.y *= ImGui::GetIO().DisplayFramebufferScale.y;
                if (pos.x > 0)
                {
                    pos.y = view->getViewport().height - 1 - pos.y;
                    onClick(*this, view, pos);
                }
            }

            const ImVec4 yellow(1.0f, 1.0f, 0.0f, 1.0f);

            if (!mNotificationText.empty())
            {
                ImGui::TextColored(yellow, "Picked %s", mNotificationText.c_str());
                ImGui::Spacing();
            }

            float progress = mResourceLoader->asyncGetLoadProgress();
            if (progress < 1.0)
            {
                ImGui::ProgressBar(progress);
            }
            else
            {
                // The model is now fully loaded, so let automation know.
                automation.signalBatchMode();
            }

            // The screenshots do not include the UI, but we auto-open the Automation UI group
            // when in batch mode. This is useful when a human is observing progress.
            const int flags = automation.isBatchModeEnabled() ? ImGuiTreeNodeFlags_DefaultOpen : 0;

            if (ImGui::CollapsingHeader("Automation", flags))
            {
                ImGui::Indent();

                if (automation.isRunning())
                {
                    ImGui::TextColored(yellow, "Test case %zu / %zu", automation.currentTest(),
                                       automation.testCount());
                }
                else
                {
                    ImGui::TextColored(yellow, "%zu test cases", automation.testCount());
                }

                auto options = automation.getOptions();

                ImGui::PushItemWidth(150);
                ImGui::SliderFloat("Sleep (seconds)", &options.sleepDuration, 0.0, 5.0);
                ImGui::PopItemWidth();

                // Hide the tooltip during automation to avoid photobombing the screenshot.
                if (ImGui::IsItemHovered() && !automation.isRunning())
                {
                    ImGui::SetTooltip("Specifies the amount of time to sleep between test cases.");
                }

                ImGui::Checkbox("Export screenshot for each test", &options.exportScreenshots);
                ImGui::Checkbox("Export settings JSON for each test", &options.exportSettings);

                automation.setOptions(options);

                if (automation.isRunning())
                {
                    if (ImGui::Button("Stop batch test"))
                    {
                        automation.stopRunning();
                    }
                }
                else if (ImGui::Button("Run batch test"))
                {
                    automation.startRunning();
                }

                if (ImGui::Button("Export view settings"))
                {
                    automation.exportSettings(mViewerGUI->getSettings(), "settings.json");
                    mMessageBoxText = automation.getStatusMessage();
                    ImGui::OpenPopup("MessageBox");
                }
                ImGui::Unindent();
            }

            if (ImGui::CollapsingHeader("Stats"))
            {
                ImGui::Indent();
                ImGui::Text("%zu entities in the asset", mAsset->getEntityCount());
                ImGui::Text("%zu renderables (excluding UI)", mScene->getRenderableCount());
                ImGui::Text("%zu skipped frames", GameDriver::get().getSkippedFrameCount());
                ImGui::Unindent();
            }

            if (ImGui::CollapsingHeader("Debug"))
            {
                auto& debug = mEngine->getDebugRegistry();
                if (ImGui::Button("Capture frame"))
                {
                    bool* captureFrame = debug.getPropertyAddress<bool>("d.renderer.doFrameCapture");
                    *captureFrame      = true;
                }
                ImGui::Checkbox("Disable buffer padding",
                                debug.getPropertyAddress<bool>("d.renderer.disable_buffer_padding"));
                ImGui::Checkbox("Camera at origin", debug.getPropertyAddress<bool>("d.view.camera_at_origin"));
                auto dataSource = debug.getDataSource("d.view.frame_info");
                if (dataSource.data)
                {
                    ImGuiExt::PlotLinesSeries(
                        "FrameInfo", 6,
                        [](int series) {
                            const ImVec4 colors[] = {
                                {1, 0, 0, 1},    // target
                                {0, 0.5f, 0, 1}, // frame-time
                                {0, 1, 0, 1},    // frame-time denoised
                                {1, 1, 0, 1},    // i
                                {1, 0, 1, 1},    // d
                                {0, 1, 1, 1},    // e

                            };
                            ImGui::PushStyleColor(ImGuiCol_PlotLines, colors[series]);
                        },
                        [](int series, void* buffer, int i) -> float {
                            auto const* p = (DebugRegistry::FrameHistory const*)buffer + i;
                            switch (series)
                            {
                                case 0:
                                    return 0.03f * p->target;
                                case 1:
                                    return 0.03f * p->frameTime;
                                case 2:
                                    return 0.03f * p->frameTimeDenoised;
                                case 3:
                                    return p->pid_i * 0.5f / 100.0f + 0.5f;
                                case 4:
                                    return p->pid_d * 0.5f / 0.100f + 0.5f;
                                case 5:
                                    return p->pid_e * 0.5f / 1.000f + 0.5f;
                                default:
                                    return 0.0f;
                            }
                        },
                        [](int series) {
                            if (series < 6)
                                ImGui::PopStyleColor();
                        },
                        const_cast<void*>(dataSource.data), int(dataSource.count), 0, nullptr, 0.0f, 1.0f,
                        {0, 100});
                }
#ifndef NDEBUG
                ImGui::SliderFloat("Kp", debug.getPropertyAddress<float>("d.view.pid.kp"), 0, 2);
                ImGui::SliderFloat("Ki", debug.getPropertyAddress<float>("d.view.pid.ki"), 0, 10);
                ImGui::SliderFloat("Kd", debug.getPropertyAddress<float>("d.view.pid.kd"), 0, 10);
#endif
                const auto overdrawVisibilityBit = (1u << GameDriver::Overdraw::OVERDRAW_VISIBILITY_LAYER);
                bool       visualizeOverdraw     = view->getVisibleLayers() & overdrawVisibilityBit;
                // TODO: enable after stencil buffer supported is added for Vulkan.
                const bool overdrawDisabled = mEngine->getBackend() == backend::Backend::VULKAN;
                ImGui::BeginDisabled(overdrawDisabled);
                ImGui::Checkbox(!overdrawDisabled ? "Visualize overdraw" : "Visualize overdraw (disabled for Vulkan)",
                                &visualizeOverdraw);
                ImGui::EndDisabled();
                view->setVisibleLayers(overdrawVisibilityBit,
                                       (uint8_t)visualizeOverdraw << GameDriver::Overdraw::OVERDRAW_VISIBILITY_LAYER);
                view->setStencilBufferEnabled(visualizeOverdraw);
            }

            if (ImGui::BeginPopupModal("MessageBox", NULL, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::Text("%s", mMessageBoxText.c_str());
                if (ImGui::Button("OK", ImVec2(120, 0)))
                {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        });
}

void GameDriver::cleanup()
{

    mAutomationEngine->terminate();
    mResourceLoader->asyncCancelLoad();
    mAssetLoader->destroyAsset(mAsset);
    mMaterials->destroyMaterials();

    mEngine->destroy(mGround.mGroundPlane);
    mEngine->destroy(mGround.mGroundVertexBuffer);
    mEngine->destroy(mGround.mGroundIndexBuffer);
    mEngine->destroy(mGround.mGroundMaterial);
    mEngine->destroy(mColorGrading);

    mEngine->destroy(mOverdraw.mFullScreenTriangleVertexBuffer);
    mEngine->destroy(mOverdraw.mFullScreenTriangleIndexBuffer);

    auto& em = EntityManager::get();
    for (auto e : mOverdraw.mOverdrawVisualizer)
    {
        mEngine->destroy(e);
        em.destroy(e);
    }

    for (auto mi : mOverdraw.mOverdrawMaterialInstances)
    {
        mEngine->destroy(mi);
    }
    mEngine->destroy(mOverdraw.mOverdrawMaterial);

    delete mViewerGUI;
    delete mMaterials;
    delete mNames;
    delete mResourceLoader;
    delete mStbDecoder;
    delete mKtxDecoder;

    filament::gltfio::AssetLoader::destroy(&mAssetLoader);
}