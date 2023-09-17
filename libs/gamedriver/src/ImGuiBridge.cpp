#include "gamedriver/ImGuiBridge.h"
#include "gamedriver/Window.h"
#include "gamedriver/GameDriver.h"
#include "generated/resources/gamedriver.h"

using namespace filament::math;
using namespace filament;
using namespace utils;

using MinFilter = TextureSampler::MinFilter;
using MagFilter = TextureSampler::MagFilter;



ImGuiContext* gImGuiContext;

filament::Material* gImGuiMaterial = nullptr;
filament::TextureSampler gImGuiSampler;
filament::Texture*       gImGuiTexture = nullptr;


ImGuiWindowImpl::ImGuiWindowImpl():
    mScene(gEngine->createScene())
{
    utils::EntityManager& em = utils::EntityManager::get();
    mCameraEntity            = em.create();
    mCamera                  = gEngine->createCamera(mCameraEntity);
    mView                    = gEngine->createView();
    mView->setCamera(mCamera);

    mView->setPostProcessingEnabled(false);
    mView->setBlendMode(View::BlendMode::TRANSLUCENT);
    mView->setShadowingEnabled(false);

    // Attach a scene for our one and only Renderable.
    mView->setScene(mScene);

    mRenderable = em.create();
    mScene->addEntity(mRenderable);
}


ImGuiWindowImpl::~ImGuiWindowImpl()
{
    gEngine->destroy(mView);
    gEngine->destroy(mSwapChain);
    gEngine->destroy(mRenderer);
    gEngine->destroy(mScene);
    gEngine->destroy(mRenderable);
    gEngine->destroyCameraComponent(mCameraEntity);

    for (auto& mi : mMaterialInstances)
    {
        gEngine->destroy(mi);
    }
   // gEngine->destroy(mMaterial);
    //gEngine->destroy(gImGuiTexture);
    for (auto& vb : mVertexBuffers)
    {
        gEngine->destroy(vb);
    }
    for (auto& ib : mIndexBuffers)
    {
        gEngine->destroy(ib);
    }

    EntityManager& em = utils::EntityManager::get();
    em.destroy(mRenderable);
    em.destroy(mCameraEntity);
}

void ImGuiWindowImpl::processImGuiCommands(ImDrawData* commands)
{
    int x = commands->DisplayPos.x, y = commands->DisplayPos.y;

    // Avoid rendering when minimized and scale coordinates for retina displays.
    int w  = (int)(commands->DisplaySize.x * commands->FramebufferScale.x);
    int h = (int)(commands->DisplaySize.y * commands->FramebufferScale.y);
    if (w == 0 || h == 0)
        return;

    mCamera->setProjection(Camera::Projection::ORTHO, x, x + w, y + h, y, 0.0, 1.0);

    mView->setViewport({0, 0, (uint32_t)w, (uint32_t)h});
    
    mHasSynced = false;
    auto& rcm  = gEngine->getRenderableManager();

     // Ensure that we have enough vertex buffers and index buffers.
    createBuffers(commands->CmdListsCount);

    // Count how many primitives we'll need, then create a Renderable builder.
    size_t                                                    nPrims = 0;
    std::unordered_map<uint64_t, filament::MaterialInstance*> scissorRects;
    for (int cmdListIndex = 0; cmdListIndex < commands->CmdListsCount; cmdListIndex++)
    {
        const ImDrawList* cmds = commands->CmdLists[cmdListIndex];
        nPrims += cmds->CmdBuffer.size();
    }
    auto rbuilder = RenderableManager::Builder(nPrims);
    rbuilder.boundingBox({{0, 0, 0}, {10000, 10000, 10000}}).culling(false);

    // Ensure that we have a material instance for each primitive.
    size_t previousSize = mMaterialInstances.size();
    if (nPrims > mMaterialInstances.size())
    {
        mMaterialInstances.resize(nPrims);
        for (size_t i = previousSize; i < mMaterialInstances.size(); i++)
        {
            mMaterialInstances[i] = gImGuiMaterial->createInstance();
        }
    }

     // Will project scissor/clipping rectangles into framebuffer space
    ImVec2 clip_off   = commands->DisplayPos;        // (0,0) unless using multi-viewports
    ImVec2 clip_scale = commands->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

    // Recreate the Renderable component and point it to the vertex buffers.
    rcm.destroy(mRenderable);
    int bufferIndex = 0;
    int primIndex   = 0;
    for (int cmdListIndex = 0; cmdListIndex < commands->CmdListsCount; cmdListIndex++)
    {
        const ImDrawList* cmds = commands->CmdLists[cmdListIndex];
        populateVertexData(bufferIndex,
                           cmds->VtxBuffer.Size * sizeof(ImDrawVert), cmds->VtxBuffer.Data,
                           cmds->IdxBuffer.Size * sizeof(ImDrawIdx), cmds->IdxBuffer.Data);
        for (const auto& pcmd : cmds->CmdBuffer)
        {
            if (pcmd.UserCallback)
            {
                pcmd.UserCallback(cmds, &pcmd);
            }
            else
            {
                ImVec2 clip_min((pcmd.ClipRect.x - clip_off.x) * clip_scale.x, (pcmd.ClipRect.y - clip_off.y) * clip_scale.y);
                ImVec2 clip_max((pcmd.ClipRect.z - clip_off.x) * clip_scale.x, (pcmd.ClipRect.w - clip_off.y) * clip_scale.y);
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                    continue;
                MaterialInstance* materialInstance = mMaterialInstances[primIndex];
                materialInstance->setScissor(
                    (int)clip_min.x, (int)((float)h - clip_max.y),
                    (int)(clip_max.x - clip_min.x), (int)(clip_max.y - clip_min.y));

                if (pcmd.TextureId)
                {
                    TextureSampler sampler(MinFilter::LINEAR, MagFilter::LINEAR);
                    materialInstance->setParameter("albedo", (Texture const*)pcmd.TextureId, sampler);
                }
                else
                {
                    materialInstance->setParameter("albedo", gImGuiTexture, gImGuiSampler);
                }
                rbuilder
                    .geometry(primIndex, RenderableManager::PrimitiveType::TRIANGLES,
                              mVertexBuffers[bufferIndex], mIndexBuffers[bufferIndex],
                              pcmd.IdxOffset, pcmd.ElemCount)
                    .blendOrder(primIndex, primIndex)
                    .material(primIndex, materialInstance);
                primIndex++;
            }
        }
        bufferIndex++;
    }
    if (commands->CmdListsCount > 0)
    {
        rbuilder.build(*gEngine, mRenderable);
    }
}

void ImGuiWindowImpl::createVertexBuffer(size_t bufferIndex, size_t capacity)
{
    syncThreads();
    gEngine->destroy(mVertexBuffers[bufferIndex]);
    mVertexBuffers[bufferIndex] = VertexBuffer::Builder()
                                      .vertexCount(capacity)
                                      .bufferCount(1)
                                      .attribute(VertexAttribute::POSITION, 0, VertexBuffer::AttributeType::FLOAT2, 0,
                                                 sizeof(ImDrawVert))
                                      .attribute(VertexAttribute::UV0, 0, VertexBuffer::AttributeType::FLOAT2,
                                                 sizeof(filament::math::float2), sizeof(ImDrawVert))
                                      .attribute(VertexAttribute::COLOR, 0, VertexBuffer::AttributeType::UBYTE4,
                                                 2 * sizeof(filament::math::float2), sizeof(ImDrawVert))
                                      .normalized(VertexAttribute::COLOR)
                                      .build(*gEngine);
}

void ImGuiWindowImpl::createIndexBuffer(size_t bufferIndex, size_t capacity)
{
    syncThreads();
    gEngine->destroy(mIndexBuffers[bufferIndex]);
    mIndexBuffers[bufferIndex] = IndexBuffer::Builder()
                                     .indexCount(capacity)
                                     .bufferType(IndexBuffer::IndexType::USHORT)
                                     .build(*gEngine);
}

void ImGuiWindowImpl::createBuffers(int numRequiredBuffers)
{
    if (numRequiredBuffers > mVertexBuffers.size())
    {
        size_t previousSize = mVertexBuffers.size();
        mVertexBuffers.resize(numRequiredBuffers, nullptr);
        for (size_t i = previousSize; i < mVertexBuffers.size(); i++)
        {
            // Pick a reasonable starting capacity; it will grow if needed.
            createVertexBuffer(i, 1000);
        }
    }
    if (numRequiredBuffers > mIndexBuffers.size())
    {
        size_t previousSize = mIndexBuffers.size();
        mIndexBuffers.resize(numRequiredBuffers, nullptr);
        for (size_t i = previousSize; i < mIndexBuffers.size(); i++)
        {
            // Pick a reasonable starting capacity; it will grow if needed.
            createIndexBuffer(i, 5000);
        }
    }
}

void ImGuiWindowImpl::populateVertexData(size_t bufferIndex, size_t vbSizeInBytes, void* vbImguiData, size_t ibSizeInBytes, void* ibImguiData)
{
    // Create a new vertex buffer if the size isn't large enough, then copy the ImGui data into
    // a staging area since Filament's render thread might consume the data at any time.
    size_t requiredVertCount = vbSizeInBytes / sizeof(ImDrawVert);
    size_t capacityVertCount = mVertexBuffers[bufferIndex]->getVertexCount();
    if (requiredVertCount > capacityVertCount)
    {
        createVertexBuffer(bufferIndex, requiredVertCount);
    }
    size_t nVbBytes       = requiredVertCount * sizeof(ImDrawVert);
    void*  vbFilamentData = malloc(nVbBytes);
    memcpy(vbFilamentData, vbImguiData, nVbBytes);
    mVertexBuffers[bufferIndex]->setBufferAt(*gEngine, 0,
                                             VertexBuffer::BufferDescriptor(
                                                 vbFilamentData, nVbBytes,
                                                 [](void* buffer, size_t size, void* user) {
                                                     free(buffer);
                                                 },
                                                 /* user = */ nullptr));

    // Create a new index buffer if the size isn't large enough, then copy the ImGui data into
    // a staging area since Filament's render thread might consume the data at any time.
    size_t requiredIndexCount = ibSizeInBytes / 2;
    size_t capacityIndexCount = mIndexBuffers[bufferIndex]->getIndexCount();
    if (requiredIndexCount > capacityIndexCount)
    {
        createIndexBuffer(bufferIndex, requiredIndexCount);
    }
    size_t nIbBytes       = requiredIndexCount * 2;
    void*  ibFilamentData = malloc(nIbBytes);
    memcpy(ibFilamentData, ibImguiData, nIbBytes);
    mIndexBuffers[bufferIndex]->setBuffer(*gEngine,
                                          IndexBuffer::BufferDescriptor(
                                              ibFilamentData, nIbBytes,
                                              [](void* buffer, size_t size, void* user) {
                                                  free(buffer);
                                              },
                                              /* user = */ nullptr));
}

void ImGuiWindowImpl::syncThreads()
{
#if UTILS_HAS_THREADING
    if (!mHasSynced)
    {
        // This is called only when ImGui needs to grow a vertex buffer, which occurs a few times
        // after launching and rarely (if ever) after that.
        Fence::waitAndDestroy(gEngine->createFence());
        mHasSynced = true;
    }
#endif
}

ImGuiKey ImGui_ImplSDL2_KeycodeToImGuiKey(int keycode)
{
    switch (keycode)
    {
        case SDLK_TAB: return ImGuiKey_Tab;
        case SDLK_LEFT: return ImGuiKey_LeftArrow;
        case SDLK_RIGHT: return ImGuiKey_RightArrow;
        case SDLK_UP: return ImGuiKey_UpArrow;
        case SDLK_DOWN: return ImGuiKey_DownArrow;
        case SDLK_PAGEUP: return ImGuiKey_PageUp;
        case SDLK_PAGEDOWN: return ImGuiKey_PageDown;
        case SDLK_HOME: return ImGuiKey_Home;
        case SDLK_END: return ImGuiKey_End;
        case SDLK_INSERT: return ImGuiKey_Insert;
        case SDLK_DELETE: return ImGuiKey_Delete;
        case SDLK_BACKSPACE: return ImGuiKey_Backspace;
        case SDLK_SPACE: return ImGuiKey_Space;
        case SDLK_RETURN: return ImGuiKey_Enter;
        case SDLK_ESCAPE: return ImGuiKey_Escape;
        case SDLK_QUOTE: return ImGuiKey_Apostrophe;
        case SDLK_COMMA: return ImGuiKey_Comma;
        case SDLK_MINUS: return ImGuiKey_Minus;
        case SDLK_PERIOD: return ImGuiKey_Period;
        case SDLK_SLASH: return ImGuiKey_Slash;
        case SDLK_SEMICOLON: return ImGuiKey_Semicolon;
        case SDLK_EQUALS: return ImGuiKey_Equal;
        case SDLK_LEFTBRACKET: return ImGuiKey_LeftBracket;
        case SDLK_BACKSLASH: return ImGuiKey_Backslash;
        case SDLK_RIGHTBRACKET: return ImGuiKey_RightBracket;
        case SDLK_BACKQUOTE: return ImGuiKey_GraveAccent;
        case SDLK_CAPSLOCK: return ImGuiKey_CapsLock;
        case SDLK_SCROLLLOCK: return ImGuiKey_ScrollLock;
        case SDLK_NUMLOCKCLEAR: return ImGuiKey_NumLock;
        case SDLK_PRINTSCREEN: return ImGuiKey_PrintScreen;
        case SDLK_PAUSE: return ImGuiKey_Pause;
        case SDLK_KP_0: return ImGuiKey_Keypad0;
        case SDLK_KP_1: return ImGuiKey_Keypad1;
        case SDLK_KP_2: return ImGuiKey_Keypad2;
        case SDLK_KP_3: return ImGuiKey_Keypad3;
        case SDLK_KP_4: return ImGuiKey_Keypad4;
        case SDLK_KP_5: return ImGuiKey_Keypad5;
        case SDLK_KP_6: return ImGuiKey_Keypad6;
        case SDLK_KP_7: return ImGuiKey_Keypad7;
        case SDLK_KP_8: return ImGuiKey_Keypad8;
        case SDLK_KP_9: return ImGuiKey_Keypad9;
        case SDLK_KP_PERIOD: return ImGuiKey_KeypadDecimal;
        case SDLK_KP_DIVIDE: return ImGuiKey_KeypadDivide;
        case SDLK_KP_MULTIPLY: return ImGuiKey_KeypadMultiply;
        case SDLK_KP_MINUS: return ImGuiKey_KeypadSubtract;
        case SDLK_KP_PLUS: return ImGuiKey_KeypadAdd;
        case SDLK_KP_ENTER: return ImGuiKey_KeypadEnter;
        case SDLK_KP_EQUALS: return ImGuiKey_KeypadEqual;
        case SDLK_LCTRL: return ImGuiKey_LeftCtrl;
        case SDLK_LSHIFT: return ImGuiKey_LeftShift;
        case SDLK_LALT: return ImGuiKey_LeftAlt;
        case SDLK_LGUI: return ImGuiKey_LeftSuper;
        case SDLK_RCTRL: return ImGuiKey_RightCtrl;
        case SDLK_RSHIFT: return ImGuiKey_RightShift;
        case SDLK_RALT: return ImGuiKey_RightAlt;
        case SDLK_RGUI: return ImGuiKey_RightSuper;
        case SDLK_APPLICATION: return ImGuiKey_Menu;
        case SDLK_0: return ImGuiKey_0;
        case SDLK_1: return ImGuiKey_1;
        case SDLK_2: return ImGuiKey_2;
        case SDLK_3: return ImGuiKey_3;
        case SDLK_4: return ImGuiKey_4;
        case SDLK_5: return ImGuiKey_5;
        case SDLK_6: return ImGuiKey_6;
        case SDLK_7: return ImGuiKey_7;
        case SDLK_8: return ImGuiKey_8;
        case SDLK_9: return ImGuiKey_9;
        case SDLK_a: return ImGuiKey_A;
        case SDLK_b: return ImGuiKey_B;
        case SDLK_c: return ImGuiKey_C;
        case SDLK_d: return ImGuiKey_D;
        case SDLK_e: return ImGuiKey_E;
        case SDLK_f: return ImGuiKey_F;
        case SDLK_g: return ImGuiKey_G;
        case SDLK_h: return ImGuiKey_H;
        case SDLK_i: return ImGuiKey_I;
        case SDLK_j: return ImGuiKey_J;
        case SDLK_k: return ImGuiKey_K;
        case SDLK_l: return ImGuiKey_L;
        case SDLK_m: return ImGuiKey_M;
        case SDLK_n: return ImGuiKey_N;
        case SDLK_o: return ImGuiKey_O;
        case SDLK_p: return ImGuiKey_P;
        case SDLK_q: return ImGuiKey_Q;
        case SDLK_r: return ImGuiKey_R;
        case SDLK_s: return ImGuiKey_S;
        case SDLK_t: return ImGuiKey_T;
        case SDLK_u: return ImGuiKey_U;
        case SDLK_v: return ImGuiKey_V;
        case SDLK_w: return ImGuiKey_W;
        case SDLK_x: return ImGuiKey_X;
        case SDLK_y: return ImGuiKey_Y;
        case SDLK_z: return ImGuiKey_Z;
        case SDLK_F1: return ImGuiKey_F1;
        case SDLK_F2: return ImGuiKey_F2;
        case SDLK_F3: return ImGuiKey_F3;
        case SDLK_F4: return ImGuiKey_F4;
        case SDLK_F5: return ImGuiKey_F5;
        case SDLK_F6: return ImGuiKey_F6;
        case SDLK_F7: return ImGuiKey_F7;
        case SDLK_F8: return ImGuiKey_F8;
        case SDLK_F9: return ImGuiKey_F9;
        case SDLK_F10: return ImGuiKey_F10;
        case SDLK_F11: return ImGuiKey_F11;
        case SDLK_F12: return ImGuiKey_F12;
    }
    return ImGuiKey_None;
}

static ImGuiWindowImpl* ImGui_ImplSDL2_GetBackendData()
{
    return ImGui::GetCurrentContext() ? (ImGuiWindowImpl*)ImGui::GetIO().BackendPlatformUserData : nullptr;
}
// FIXME: Note that doesn't update with DPI/Scaling change only as SDL2 doesn't have an event for it (SDL3 has).
static void ImGui_ImplSDL2_UpdateMonitors()
{
    //ImGui_ImplSDL2_Data* bd          = ImGui_ImplSDL2_GetBackendData();
    ImGuiPlatformIO&     platform_io = ImGui::GetPlatformIO();
    platform_io.Monitors.resize(0);
    //bd->WantUpdateMonitors = false;
    int display_count      = SDL_GetNumVideoDisplays();
    for (int n = 0; n < display_count; n++)
    {
        // Warning: the validity of monitor DPI information on Windows depends on the application DPI awareness settings, which generally needs to be set in the manifest or at runtime.
        ImGuiPlatformMonitor monitor;
        SDL_Rect             r;
        SDL_GetDisplayBounds(n, &r);
        monitor.MainPos = monitor.WorkPos = ImVec2((float)r.x, (float)r.y);
        monitor.MainSize = monitor.WorkSize = ImVec2((float)r.w, (float)r.h);
#if SDL_HAS_USABLE_DISPLAY_BOUNDS
        SDL_GetDisplayUsableBounds(n, &r);
        monitor.WorkPos  = ImVec2((float)r.x, (float)r.y);
        monitor.WorkSize = ImVec2((float)r.w, (float)r.h);
#endif
#if SDL_HAS_PER_MONITOR_DPI
        // FIXME-VIEWPORT: On MacOS SDL reports actual monitor DPI scale, ignoring OS configuration. We may want to set
        //  DpiScale to cocoa_window.backingScaleFactor here.
        float dpi = 0.0f;
        if (!SDL_GetDisplayDPI(n, &dpi, nullptr, nullptr))
            monitor.DpiScale = dpi / 96.0f;
#endif
        monitor.PlatformHandle = (void*)(intptr_t)n;
        platform_io.Monitors.push_back(monitor);
    }
}

static void ImGui_ImplSDL2_CreateWindow(ImGuiViewport* viewport)
{
    ImGuiWindowImpl* bd        = ImGui_ImplSDL2_GetBackendData();
    ImGuiWindowImpl* vd            = IM_NEW(ImGuiWindowImpl)();
    viewport->PlatformUserData = vd;

   Uint32 sdl_flags = 0;
    sdl_flags |= SDL_GetWindowFlags(bd->mWindow) & SDL_WINDOW_ALLOW_HIGHDPI;
    sdl_flags |= SDL_WINDOW_HIDDEN;
    sdl_flags |= (viewport->Flags & ImGuiViewportFlags_NoDecoration) ? SDL_WINDOW_BORDERLESS : 0;
    sdl_flags |= (viewport->Flags & ImGuiViewportFlags_NoDecoration) ? 0 : SDL_WINDOW_RESIZABLE;

    vd->mWindow     = SDL_CreateWindow("No Title Yet", (int)viewport->Pos.x, (int)viewport->Pos.y, (int)viewport->Size.x, (int)viewport->Size.y, sdl_flags);
    vd->WindowOwned = true;
    
    viewport->PlatformHandle    = (void*)vd->mWindow;
    viewport->PlatformHandleRaw = nullptr;
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(vd->mWindow, &info))
    {
#if defined(SDL_VIDEO_DRIVER_WINDOWS)
        viewport->PlatformHandleRaw = info.info.win.window;
#elif defined(__APPLE__) && defined(SDL_VIDEO_DRIVER_COCOA)
        viewport->PlatformHandleRaw = (void*)info.info.cocoa.window;
#endif
    }

    vd->mSwapChain = gEngine->createSwapChain(viewport->PlatformHandleRaw);
    vd->mRenderer  = gEngine->createRenderer();
}
static void ImGui_ImplSDL2_DestroyWindow(ImGuiViewport* viewport)
{
    if (ImGuiWindowImpl* vd = (ImGuiWindowImpl*)viewport->PlatformUserData)
    {
        if (vd->mWindow && vd->WindowOwned)
            SDL_DestroyWindow(vd->mWindow);
       
        vd->mWindow = nullptr;
        IM_DELETE(vd);
        
    }
    viewport->PlatformUserData = viewport->PlatformHandle = nullptr;
}

static void ImGui_ImplSDL2_ShowWindow(ImGuiViewport* viewport)
{
    ImGuiWindowImpl* vd = (ImGuiWindowImpl*)viewport->PlatformUserData;
    //#if defined(_WIN32)
//    HWND hwnd = (HWND)viewport->PlatformHandleRaw;
//
//    // SDL hack: Hide icon from task bar
//    // Note: SDL 2.0.6+ has a SDL_WINDOW_SKIP_TASKBAR flag which is supported under Windows but the way it create the window breaks our seamless transition.
//    if (viewport->Flags & ImGuiViewportFlags_NoTaskBarIcon)
//    {
//        LONG ex_style = ::GetWindowLong(hwnd, GWL_EXSTYLE);
//        ex_style &= ~WS_EX_APPWINDOW;
//        ex_style |= WS_EX_TOOLWINDOW;
//        ::SetWindowLong(hwnd, GWL_EXSTYLE, ex_style);
//    }
//
//    // SDL hack: SDL always activate/focus windows :/
//    if (viewport->Flags & ImGuiViewportFlags_NoFocusOnAppearing)
//    {
//        ::ShowWindow(hwnd, SW_SHOWNA);
//        return;
//    }
//#endif
//
    SDL_ShowWindow(vd->mWindow);
}

static ImVec2 ImGui_ImplSDL2_GetWindowPos(ImGuiViewport* viewport)
{
    ImGuiWindowImpl* vd = (ImGuiWindowImpl*)viewport->PlatformUserData;
    int                          x = 0, y = 0;
    SDL_GetWindowPosition(vd->mWindow, &x, &y);
    return ImVec2((float)x, (float)y);
}

static void ImGui_ImplSDL2_SetWindowPos(ImGuiViewport* viewport, ImVec2 pos)
{
    ImGuiWindowImpl* vd = (ImGuiWindowImpl*)viewport->PlatformUserData;
    SDL_SetWindowPosition(vd->mWindow, (int)pos.x, (int)pos.y);
}

static ImVec2 ImGui_ImplSDL2_GetWindowSize(ImGuiViewport* viewport)
{
    ImGuiWindowImpl* vd = (ImGuiWindowImpl*)viewport->PlatformUserData;
    int                          w = 0, h = 0;
    SDL_GetWindowSize(vd->mWindow, &w, &h);
    return ImVec2((float)w, (float)h);
}

static void ImGui_ImplSDL2_SetWindowSize(ImGuiViewport* viewport, ImVec2 size)
{
    ImGuiWindowImpl* vd = (ImGuiWindowImpl*)viewport->PlatformUserData;
    SDL_SetWindowSize(vd->mWindow, (int)size.x, (int)size.y);
}

static void ImGui_ImplSDL2_SetWindowTitle(ImGuiViewport* viewport, const char* title)
{
    ImGuiWindowImpl* vd = (ImGuiWindowImpl*)viewport->PlatformUserData;
    SDL_SetWindowTitle(vd->mWindow, title);
}

#if SDL_HAS_WINDOW_ALPHA
static void ImGui_ImplSDL2_SetWindowAlpha(ImGuiViewport* viewport, float alpha)
{
    ImGui_ImplSDL2_ViewportData* vd = (ImGui_ImplSDL2_ViewportData*)viewport->PlatformUserData;
    SDL_SetWindowOpacity(vd->Window, alpha);
}
#endif

static void ImGui_ImplSDL2_SetWindowFocus(ImGuiViewport* viewport)
{
    ImGuiWindowImpl* vd = (ImGuiWindowImpl*)viewport->PlatformUserData;
    SDL_RaiseWindow(vd->mWindow);
}

static bool ImGui_ImplSDL2_GetWindowFocus(ImGuiViewport* viewport)
{
    ImGuiWindowImpl* vd = (ImGuiWindowImpl*)viewport->PlatformUserData;
    return (SDL_GetWindowFlags(vd->mWindow) & SDL_WINDOW_INPUT_FOCUS) != 0;
    //return true;
}

static bool ImGui_ImplSDL2_GetWindowMinimized(ImGuiViewport* viewport)
{
    ImGuiWindowImpl* vd = (ImGuiWindowImpl*)viewport->PlatformUserData;
    return (SDL_GetWindowFlags(vd->mWindow) & SDL_WINDOW_MINIMIZED) != 0;
    //return true;
}

static void ImGui_ImplSDL2_RenderWindow(ImGuiViewport* viewport, void*)
{
    ImGuiWindowImpl* vd = (ImGuiWindowImpl*)viewport->PlatformUserData;

    vd->processImGuiCommands(viewport->DrawData);

    if (vd->mRenderer->beginFrame(vd->mSwapChain))
    {
        vd->mRenderer->render(vd->mView);

        vd->mRenderer->endFrame();
    }
}

static void ImGui_ImplSDL2_SwapBuffers(ImGuiViewport* viewport, void*)
{
    //ImGuiWindowImpl* vd = (ImGuiWindowImpl*)viewport->PlatformUserData;
}

void ImGui_ImplSDL2_NewFrame()
{
    ImGui_ImplSDL2_UpdateMonitors();

     ImGuiWindowImpl* bd = ImGui_ImplSDL2_GetBackendData();
    IM_ASSERT(bd != nullptr && "Did you call ImGui_ImplSDL2_Init()?");
    ImGuiIO& io = ImGui::GetIO();

    // Setup display size (every frame to accommodate for window resizing)
    int w, h;
    int display_w, display_h;
    SDL_GetWindowSize(bd->mWindow, &w, &h);
    if (SDL_GetWindowFlags(bd->mWindow) & SDL_WINDOW_MINIMIZED)
        w = h = 0;

    SDL_GL_GetDrawableSize(bd->mWindow, &display_w, &display_h);
    io.DisplaySize = ImVec2((float)w, (float)h);
    if (w > 0 && h > 0)
        io.DisplayFramebufferScale = ImVec2((float)display_w / w, (float)display_h / h);


    // Update monitors
    //if (bd->WantUpdateMonitors)
        ImGui_ImplSDL2_UpdateMonitors();

    // Setup time step (we don't use SDL_GetTicks() because it is using millisecond resolution)
    // (Accept SDL_GetPerformanceCounter() not returning a monotonically increasing value. Happens in VMs and Emscripten, see #6189, #6114, #3644)
    /*static Uint64 frequency    = SDL_GetPerformanceFrequency();
    Uint64        current_time = SDL_GetPerformanceCounter();
    if (current_time <= bd->Time)
        current_time = bd->Time + 1;
    io.DeltaTime = bd->Time > 0 ? (float)((double)(current_time - bd->Time) / frequency) : (float)(1.0f / 60.0f);
    bd->Time     = current_time;

    if (bd->PendingMouseLeaveFrame && bd->PendingMouseLeaveFrame >= ImGui::GetFrameCount() && bd->MouseButtonsDown == 0)
    {
        bd->MouseWindowID          = 0;
        bd->PendingMouseLeaveFrame = 0;
        io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
    }*/

    // Our io.AddMouseViewportEvent() calls will only be valid when not capturing.
    // Technically speaking testing for 'bd->MouseButtonsDown == 0' would be more rygorous, but testing for payload reduces noise and potential side-effects.
   /* if (bd->MouseCanReportHoveredViewport && ImGui::GetDragDropPayload() == nullptr)
        io.BackendFlags |= ImGuiBackendFlags_HasMouseHoveredViewport;
    else
        io.BackendFlags &= ~ImGuiBackendFlags_HasMouseHoveredViewport;*/
}

void ImGui_ImplSDL2_InitPlatformInterface(ImGuiWindowImpl* vd, Engine* engine, const Path& fontPath)
{
        gEngine = engine;
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        (void)io;
        // Create a simple alpha-blended 2D blitting material.
        gImGuiMaterial = Material::Builder()
                             .package(GAMEDRIVER_UIBLIT_DATA, GAMEDRIVER_UIBLIT_SIZE)
                             .build(*gEngine);

        // If the given font path is invalid, ImGui will silently fall back to proggy, which is a
        // tiny "pixel art" texture that is compiled into the library.

        if (fontPath.isFile())
        {
            io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 16.0f);
        }
        gEngine->destroy(gImGuiTexture);

        // Create the grayscale texture that ImGui uses for its glyph atlas.
        static unsigned char* pixels;
        int                   width, height;

        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        size_t                         size = (size_t)(width * height * 4);
        Texture::PixelBufferDescriptor pb(
            pixels, size,
            Texture::Format::RGBA, Texture::Type::UBYTE);
        gImGuiTexture = Texture::Builder()
                            .width((uint32_t)width)
                            .height((uint32_t)height)
                            .levels((uint8_t)1)
                            .format(Texture::InternalFormat::RGBA8)
                            .sampler(Texture::Sampler::SAMPLER_2D)
                            .build(*gEngine);
        gImGuiTexture->setImage(*gEngine, 0, std::move(pb));

        gImGuiSampler = TextureSampler(MinFilter::LINEAR, MagFilter::LINEAR);
        gImGuiMaterial->setDefaultParameter("albedo", gImGuiTexture, gImGuiSampler);
       
        if (!fontPath.isFile())
        { // For proggy, switch to NEAREST for pixel-perfect text.
            gImGuiSampler = TextureSampler(MinFilter::NEAREST, MagFilter::NEAREST);
            gImGuiMaterial->setDefaultParameter("albedo", gImGuiTexture, gImGuiSampler);
        }

        ImGui::StyleColorsDark();

        Path settingsPath;
        settingsPath.setPath(
            Path::getUserSettingsDirectory() +
            Path(std::string(".") + Path::getCurrentExecutable().getNameWithoutExtension()) +
            Path("imgui_settings.ini"));
        settingsPath.getParent().mkdirRecursive();
        io.IniFilename = settingsPath.c_str();

        io.BackendPlatformUserData = vd;
        // return;

        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;
        io.BackendFlags |= ImGuiBackendFlags_PlatformHasViewports;

        // Register platform interface (will be coupled with a renderer interface)
        ImGuiPlatformIO& platform_io            = ImGui::GetPlatformIO();
        platform_io.Platform_CreateWindow       = ImGui_ImplSDL2_CreateWindow;
        platform_io.Platform_DestroyWindow      = ImGui_ImplSDL2_DestroyWindow;
        platform_io.Platform_ShowWindow         = ImGui_ImplSDL2_ShowWindow;
        platform_io.Platform_SetWindowPos       = ImGui_ImplSDL2_SetWindowPos;
        platform_io.Platform_GetWindowPos       = ImGui_ImplSDL2_GetWindowPos;
        platform_io.Platform_SetWindowSize      = ImGui_ImplSDL2_SetWindowSize;
        platform_io.Platform_GetWindowSize      = ImGui_ImplSDL2_GetWindowSize;
        platform_io.Platform_SetWindowFocus     = ImGui_ImplSDL2_SetWindowFocus;
        platform_io.Platform_GetWindowFocus     = ImGui_ImplSDL2_GetWindowFocus;
        platform_io.Platform_GetWindowMinimized = ImGui_ImplSDL2_GetWindowMinimized;
        platform_io.Platform_SetWindowTitle     = ImGui_ImplSDL2_SetWindowTitle;
        platform_io.Platform_RenderWindow       = ImGui_ImplSDL2_RenderWindow;
        platform_io.Platform_SwapBuffers        = ImGui_ImplSDL2_SwapBuffers;
#if SDL_HAS_WINDOW_ALPHA
        platform_io.Platform_SetWindowAlpha = ImGui_ImplSDL2_SetWindowAlpha;
#endif
#if SDL_HAS_VULKAN
        platform_io.Platform_CreateVkSurface = ImGui_ImplSDL2_CreateVkSurface;
#endif

        // Register main window handle (which is owned by the main application, not by us)
        // This is mostly for simplicity and consistency, so that our code (e.g. mouse handling etc.) can use same logic for main and secondary viewports.
        ImGuiViewport* main_viewport    = ImGui::GetMainViewport();
        main_viewport->PlatformUserData = vd;
        main_viewport->PlatformHandle   = vd->mWindow;
}

bool ImGui_ImplSDL2_ProcessEvent(const SDL_Event* event)
{
    ImGuiIO&             io = ImGui::GetIO();
    ImGuiWindowImpl* bd = ImGui_ImplSDL2_GetBackendData();

    switch (event->type)
    {
        case SDL_MOUSEMOTION:
        {
            
            ImVec2 mouse_pos((float)event->motion.x, (float)event->motion.y);
            if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
            {
                int window_x, window_y;
                SDL_GetWindowPosition(SDL_GetWindowFromID(event->motion.windowID), &window_x, &window_y);
                mouse_pos.x += window_x;
                mouse_pos.y += window_y;
            }
            io.AddMouseSourceEvent(event->motion.which == SDL_TOUCH_MOUSEID ? ImGuiMouseSource_TouchScreen : ImGuiMouseSource_Mouse);
            io.AddMousePosEvent(mouse_pos.x, mouse_pos.y);
            return true;
        }
        case SDL_MOUSEWHEEL:
        {
            //IMGUI_DEBUG_LOG("wheel %.2f %.2f, precise %.2f %.2f\n", (float)event->wheel.x, (float)event->wheel.y, event->wheel.preciseX, event->wheel.preciseY);
#if SDL_VERSION_ATLEAST(2, 0, 18) // If this fails to compile on Emscripten: update to latest Emscripten!
            float wheel_x = -event->wheel.preciseX;
            float wheel_y = event->wheel.preciseY;
#else
            float wheel_x = -(float)event->wheel.x;
            float wheel_y = (float)event->wheel.y;
#endif
#ifdef __EMSCRIPTEN__
            wheel_x /= 100.0f;
#endif
            io.AddMouseSourceEvent(event->wheel.which == SDL_TOUCH_MOUSEID ? ImGuiMouseSource_TouchScreen : ImGuiMouseSource_Mouse);
            io.AddMouseWheelEvent(wheel_x, wheel_y);
            return true;
        }
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
        {
            int mouse_button = -1;
            if (event->button.button == SDL_BUTTON_LEFT) { mouse_button = 0; }
            if (event->button.button == SDL_BUTTON_RIGHT) { mouse_button = 1; }
            if (event->button.button == SDL_BUTTON_MIDDLE) { mouse_button = 2; }
            if (event->button.button == SDL_BUTTON_X1) { mouse_button = 3; }
            if (event->button.button == SDL_BUTTON_X2) { mouse_button = 4; }
            if (mouse_button == -1)
                break;
            io.AddMouseSourceEvent(event->button.which == SDL_TOUCH_MOUSEID ? ImGuiMouseSource_TouchScreen : ImGuiMouseSource_Mouse);
            io.AddMouseButtonEvent(mouse_button, (event->type == SDL_MOUSEBUTTONDOWN));
            bd->MouseButtonsDown = (event->type == SDL_MOUSEBUTTONDOWN) ? (bd->MouseButtonsDown | (1 << mouse_button)) : (bd->MouseButtonsDown & ~(1 << mouse_button));
            return true;
        }
        case SDL_TEXTINPUT:
        {
            io.AddInputCharactersUTF8(event->text.text);
            return true;
        }
        case SDL_KEYDOWN:
        case SDL_KEYUP:
        {
            //ImGui_ImplSDL2_UpdateKeyModifiers((SDL_Keymod)event->key.keysym.mod);
           // ImGuiKey key = ImGui_ImplSDL2_KeycodeToImGuiKey(event->key.keysym.sym);
            //io.AddKeyEvent(key, (event->type == SDL_KEYDOWN));
            //io.SetKeyEventNativeData(key, event->key.keysym.sym, event->key.keysym.scancode, event->key.keysym.scancode); // To support legacy indexing (<1.87 user code). Legacy backend uses SDLK_*** as indices to IsKeyXXX() functions.
            return true;
        }
#if SDL_HAS_DISPLAY_EVENT
        case SDL_DISPLAYEVENT:
        {
            // 2.0.26 has SDL_DISPLAYEVENT_CONNECTED/SDL_DISPLAYEVENT_DISCONNECTED/SDL_DISPLAYEVENT_ORIENTATION,
            // so change of DPI/Scaling are not reflected in this event. (SDL3 has it)
            bd->WantUpdateMonitors = true;
            return true;
        }
#endif
        case SDL_WINDOWEVENT:
        {
            // - When capturing mouse, SDL will send a bunch of conflicting LEAVE/ENTER event on every mouse move, but the final ENTER tends to be right.
            // - However we won't get a correct LEAVE event for a captured window.
            // - In some cases, when detaching a window from main viewport SDL may send SDL_WINDOWEVENT_ENTER one frame too late,
            //   causing SDL_WINDOWEVENT_LEAVE on previous frame to interrupt drag operation by clear mouse position. This is why
            //   we delay process the SDL_WINDOWEVENT_LEAVE events by one frame. See issue #5012 for details.
            Uint8 window_event = event->window.event;
           /* if (window_event == SDL_WINDOWEVENT_ENTER)
            {
                bd->MouseWindowID          = event->window.windowID;
                bd->PendingMouseLeaveFrame = 0;
            }
            if (window_event == SDL_WINDOWEVENT_LEAVE)
                bd->PendingMouseLeaveFrame = ImGui::GetFrameCount() + 1;*/
            if (window_event == SDL_WINDOWEVENT_FOCUS_GAINED)
                io.AddFocusEvent(true);
            else if (window_event == SDL_WINDOWEVENT_FOCUS_LOST)
                io.AddFocusEvent(false);
            if (window_event == SDL_WINDOWEVENT_CLOSE || window_event == SDL_WINDOWEVENT_MOVED || window_event == SDL_WINDOWEVENT_RESIZED)
                if (ImGuiViewport* viewport = ImGui::FindViewportByPlatformHandle((void*)SDL_GetWindowFromID(event->window.windowID)))
                {
                    if (window_event == SDL_WINDOWEVENT_CLOSE)
                        viewport->PlatformRequestClose = true;
                    if (window_event == SDL_WINDOWEVENT_MOVED)
                        viewport->PlatformRequestMove = true;
                    if (window_event == SDL_WINDOWEVENT_RESIZED)
                        viewport->PlatformRequestResize = true;
                    return true;
                }
            return true;
        }
    }
    return false;
}
