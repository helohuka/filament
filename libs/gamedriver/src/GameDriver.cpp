
#include "gamedriver/BaseLibs.h"
#include "gamedriver/GameDriver.h"

#include "gamedriver/Cube.h"
#include "gamedriver/NativeWindowHelper.h"
#include "generated/resources/gamedriver.h"

using namespace filament;
using namespace filagui;
using namespace filament::math;
using namespace utils;

GameDriver& GameDriver::get() {
    static GameDriver gd;
    return gd;
}

GameDriver::GameDriver() {
    initSDL();
}

GameDriver::~GameDriver() {
    SDL_Quit();
}

View* GameDriver::getGuiView() const noexcept {
    return mImGuiHelper->getView();
}

void GameDriver::run(const Config& config, SetupCallback setupCallback,
        CleanupCallback cleanupCallback, ImGuiCallback imguiCallback,
        PreRenderCallback preRender, PostRenderCallback postRender,
        size_t width, size_t height) {

    


    mWindowTitle = config.title;
    std::unique_ptr<GameDriver::Window> window(
            new GameDriver::Window(this, config, config.title, width, height));

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
    if (config.splitView) {
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

    loadDirt(config);
    loadIBL(config);
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

    setupCallback(mEngine, window->mMainView->getView(), mScene);

    if (imguiCallback) {
        mImGuiHelper = std::make_unique<ImGuiHelper>(mEngine, window->mUiView->getView(),
            getRootAssetsPath() + "assets/fonts/Roboto-Medium.ttf");
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
    SDL_Window* sdlWindow = window->getSDLWindow();

    while (!mClosed) {
        if (mWindowTitle != SDL_GetWindowTitle(sdlWindow)) {
            SDL_SetWindowTitle(sdlWindow, mWindowTitle.c_str());
        }

        if (mSidebarWidth != sidebarWidth || mCameraFocalLength != cameraFocalLength) {
            window->configureCamerasForWindow();
            sidebarWidth = mSidebarWidth;
            cameraFocalLength = mCameraFocalLength;
        }

        if (!UTILS_HAS_THREADING) {
            mEngine->execute();
        }

        // Allow the app to animate the scene if desired.
        if (mAnimation) {
            double now = (double) SDL_GetPerformanceCounter() / SDL_GetPerformanceFrequency();
            mAnimation(mEngine, window->mMainView->getView(), now);
        }

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
                    mEngine->setActiveFeatureLevel(config.featureLevel);
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
            if (config.headless) {
                mImGuiHelper->setDisplaySize(window->mWidth, window->mHeight);
            } else {
                int windowWidth, windowHeight;
                int displayWidth, displayHeight;
                SDL_GetWindowSize(window->mWindow, &windowWidth, &windowHeight);
                SDL_GL_GetDrawableSize(window->mWindow, &displayWidth, &displayHeight);
                mImGuiHelper->setDisplaySize(windowWidth, windowHeight,
                        windowWidth > 0 ? ((float)displayWidth / windowWidth) : 0,
                        displayHeight > 0 ? ((float)displayHeight / windowHeight) : 0);
            }

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

        if (preRender) {
            preRender(mEngine, window->mViews[0]->getView(), mScene, renderer);
        }

        if (renderer->beginFrame(window->getSwapChain())) {
            for (filament::View* offscreenView : mOffscreenViews) {
                renderer->render(offscreenView);
            }
            for (auto const& view : window->mViews) {
                renderer->render(view->getView());
            }
            if (postRender) {
                postRender(mEngine, window->mViews[0]->getView(), mScene, renderer);
            }
            renderer->endFrame();

        } else {
            ++mSkippedFrames;
        }
    }

    if (mImGuiHelper) {
        mImGuiHelper.reset();
    }

    cleanupCallback(mEngine, window->mMainView->getView(), mScene);

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

// RELATIVE_ASSET_PATH is set inside samples/CMakeLists.txt and used to support multi-configuration
// generators, like Visual Studio or Xcode.
#ifndef RELATIVE_ASSET_PATH
#define RELATIVE_ASSET_PATH "."
#endif

const utils::Path& GameDriver::getRootAssetsPath() {
    static const utils::Path root = utils::Path::getCurrentExecutable().getParent() + RELATIVE_ASSET_PATH;
    return root;
}

void GameDriver::loadIBL(const Config& config) {
    if (!config.iblDirectory.empty()) {
        Path iblPath(config.iblDirectory);

        if (!iblPath.exists()) {
            std::cerr << "The specified IBL path does not exist: " << iblPath << std::endl;
            return;
        }

        mIBL = std::make_unique<IBL>(*mEngine);

        if (!iblPath.isDirectory()) {
            if (!mIBL->loadFromEquirect(iblPath)) {
                std::cerr << "Could not load the specified IBL: " << iblPath << std::endl;
                mIBL.reset(nullptr);
                return;
            }
        } else {
            if (!mIBL->loadFromDirectory(iblPath)) {
                std::cerr << "Could not load the specified IBL: " << iblPath << std::endl;
                mIBL.reset(nullptr);
                return;
            }
        }
    }
}

void GameDriver::loadDirt(const Config& config) {
    if (!config.dirt.empty()) {
        Path dirtPath(config.dirt);

        if (!dirtPath.exists()) {
            std::cerr << "The specified dirt file does not exist: " << dirtPath << std::endl;
            return;
        }

        if (!dirtPath.isFile()) {
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

        mDirt->setImage(*mEngine, 0, { data, size_t(w * h * 3),
                Texture::Format::RGB, Texture::Type::UBYTE,
                (Texture::PixelBufferDescriptor::Callback)&stbi_image_free });
    }
}

void GameDriver::initSDL() {
    ASSERT_POSTCONDITION(SDL_Init(SDL_INIT_EVERYTHING) == 0, "SDL_Init Failure");
}

