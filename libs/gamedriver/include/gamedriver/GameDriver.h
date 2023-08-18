#ifndef __GAMEDRIVER_H__
#define __GAMEDRIVER_H__ 1

/*!
 * \file GameDriver.h
 * \date 2023.08.15
 *
 * \author Helohuka
 * 
 * Contact: helohuka@outlook.com
 *
 * \brief 
 *
 * TODO: long description
 *
 * \note
*/


#include "gamedriver/BaseLibs.h"
#include "gamedriver/Configure.h"
#include "gamedriver/IBL.h"

namespace filament {
class Renderer;
class Scene;
class View;
} // namespace filament

namespace filagui {
class ImGuiHelper;
} // namespace filagui


enum MaterialSource
{
    JITSHADER,
    UBERSHADER,
};



class IBL;
class MeshAssimp;

using CameraManipulator = filament::camutils::Manipulator<float>;

static std::ifstream::pos_type getFileSize(const char* filename)
{
    std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);
    return in.tellg();
}

static bool loadSettings(const char* filename, filament::viewer:: Settings* out)
{
    auto contentSize = getFileSize(filename);
    if (contentSize <= 0)
    {
        return false;
    }
    std::ifstream     in(filename, std::ifstream::binary | std::ifstream::in);
    std::vector<char> json(static_cast<unsigned long>(contentSize));
    if (!in.read(json.data(), contentSize))
    {
        return false;
    }
    filament::viewer::JsonSerializer serializer;
    return serializer.readJson(json.data(), contentSize, out);
}

/*!
 * \class CView
 *
 * \ingroup Views
 *
 * \brief 
 *
 * TODO: long description
 *
 * \note 
 *
 * \author Helohuka
 *
 * \version 1.0
 *
 * \date 2023.08.15
 *
 * Contact: helohuka@outlook.com
 *
 */
class CView
{
public:
    CView(filament::Renderer& renderer, std::string name);
    virtual ~CView();

    void setCameraManipulator(CameraManipulator* cm);
    void setViewport(filament::Viewport const& viewport);
    void setCamera(filament::Camera* camera);
    bool intersects(ssize_t x, ssize_t y);

    virtual void mouseDown(int button, ssize_t x, ssize_t y);
    virtual void mouseUp(ssize_t x, ssize_t y);
    virtual void mouseMoved(ssize_t x, ssize_t y);
    virtual void mouseWheel(ssize_t x);
    virtual void keyDown(SDL_Scancode scancode);
    virtual void keyUp(SDL_Scancode scancode);

    filament::View const* getView() const { return view; }
    filament::View* getView() { return view; }
    CameraManipulator* getCameraManipulator() { return mCameraManipulator; }

private:
    enum class Mode : uint8_t
    {
        NONE,
        ROTATE,
        TRACK
    };

    filament::Engine& engine;
    filament::Viewport mViewport;
    filament::View* view = nullptr;
    CameraManipulator* mCameraManipulator = nullptr;
    std::string mName;
};

/*!
 * \class GodView
 *
 * \ingroup Views
 *
 * \brief 
 *
 * TODO: long description
 *
 * \note 
 *
 * \author Helohuka
 *
 * \version 1.0
 *
 * \date 2023.08.15
 *
 * Contact: helohuka@outlook.com
 *
 */
class GodView : public CView
{
public:
    using CView::CView;
    void setGodCamera(filament::Camera* camera);
};

/*!
 * \class Window
 *
 * \ingroup Window
 *
 * \brief 
 *
 * TODO: long description
 *
 * \note 
 *
 * \author Helohuka
 *
 * \version 1.0
 *
 * \date 2023.08.15
 *
 * Contact: helohuka@outlook.com
 *
 */
class Window
{
    friend class GameDriver;

public:
    Window(GameDriver* gd, Config& config);
    virtual ~Window();

    void mouseDown(int button, ssize_t x, ssize_t y);
    void mouseUp(ssize_t x, ssize_t y);
    void mouseMoved(ssize_t x, ssize_t y);
    void mouseWheel(ssize_t x);
    void keyDown(SDL_Scancode scancode);
    void keyUp(SDL_Scancode scancode);
    void resize();

    filament::Renderer*  getRenderer() { return mRenderer; }
    filament::SwapChain* getSwapChain() { return mSwapChain; }

    SDL_Window* getSDLWindow() { return mWindow; }
    void*       getNativeWindow();
    void*       getNativeSurface();
    void setWindowTitle(const char* title)
    {
        mWindowTitle = title;
        if (mWindowTitle != SDL_GetWindowTitle(mWindow))
        {
            SDL_SetWindowTitle(mWindow, mWindowTitle.c_str());
        }
    }

private:
    void configureCamerasForWindow();
    void fixupMouseCoordinatesForHdpi(ssize_t& x, ssize_t& y) const;

    bool mIsResizeable = true;
    bool mIsSplitView = false;

    GameDriver* const mGameDriver = nullptr;

    SDL_Window*               mWindow   = nullptr;
    filament::Renderer*       mRenderer = nullptr;
    filament::Engine::Backend mBackend;

    CameraManipulator*   mMainCameraMan;
    CameraManipulator*   mDebugCameraMan;
    filament::SwapChain* mSwapChain = nullptr;

    utils::Entity     mCameraEntities[3];
    filament::Camera* mCameras[3] = {nullptr};
    filament::Camera* mMainCamera;
    filament::Camera* mDebugCamera;
    filament::Camera* mOrthoCamera;

    std::vector<std::unique_ptr<CView>> mViews;
    CView*                              mMainView;
    CView*                              mUiView;
    CView*                              mDepthView;
    GodView*                            mGodView;
    CView*                              mOrthoView;

    int     mWidth  = 0;
    int     mHeight = 0;
    ssize_t mLastX  = 0;
    ssize_t mLastY  = 0;

    CView*      mMouseEventTarget = nullptr;
    std::string mWindowTitle;
    // Keep track of which view should receive a key's keyUp event.
    std::unordered_map<SDL_Scancode, CView*> mKeyEventTarget;
};

/*!
 * \class GameDriver
 *
 * \ingroup Game
 *
 * \brief 
 *
 * TODO: long description
 *
 * \note 
 *
 * \author Helohuka
 *
 * \version 1.0
 *
 * \date 2023.08.15
 *
 * Contact: helohuka@outlook.com
 *
 */
class GameDriver
{
public:
    using SetupCallback   = std::function<void(filament::Scene*)>;
    using CleanupCallback = std::function<void(filament::Engine*, filament::View*, filament::Scene*)>;
    using PreRenderCallback =
        std::function<void(filament::Engine*, filament::View*, filament::Scene*, filament::Renderer*)>;
    using PostRenderCallback =
        std::function<void(filament::Engine*, filament::View*, filament::Scene*, filament::Renderer*)>;
    using ImGuiCallback  = std::function<void(filament::Engine*, filament::View*)>;
    using AnimCallback   = std::function<void(filament::Engine*, filament::View*, double now)>;
    using ResizeCallback = std::function<void(filament::Engine*, filament::View*)>;
    using DropCallback   = std::function<void(std::string)>;

    static bool manipulatorKeyFromKeycode(SDL_Scancode scancode, CameraManipulator::Key& key);

    SINGLE_INSTANCE_FLAG(GameDriver)
private: 

    ///临时存放 TODO

    void createGroundPlane();
    void createOverdrawVisualizerEntities();

    void preRender(filament::View* view, filament::Scene* scene, filament::Renderer* renderer);
    void postRender(filament::View* view, filament::Scene* scene, filament::Renderer* renderer);
    void animate(filament::View* view, double now);
    void resize(filament::View* view);
    void gui(filament::Engine* ,filament::View* view);

public:

    void loadAsset(utils::Path filename);
    void loadResources(utils::Path filename);
    void setup(filament::View *view);
    void cleanup();

public:
    void animate(AnimCallback animation) { mAnimation = animation; }

    void resize(ResizeCallback resize) { mResize = resize; }

    void setDropHandler(DropCallback handler) { mDropHandler = handler; }

    void mainLoop();

    filament::Material const* getDefaultMaterial() const noexcept { return mDefaultMaterial; }
    filament::Material const* getTransparentMaterial() const noexcept { return mTransparentMaterial; }
    IBL*                      getIBL() const noexcept { return mIBL.get(); }
    filament::Texture*        getDirtTexture() const noexcept { return mDirt; }
    filament::View*           getGuiView() const noexcept;

    void close() { mClosed = true; }

    void   setSidebarWidth(int width) { mSidebarWidth = width; }
   

    void addOffscreenView(filament::View* view) { mOffscreenViews.push_back(view); }

    size_t getSkippedFrameCount() const { return mSkippedFrames; }

private:
    friend class Window;

    void loadIBL();
    void loadDirt();


    Config mConfig;

    filament::Engine*    mEngine = nullptr;
    filament::Scene*     mScene  = nullptr;
    std::unique_ptr<IBL> mIBL;
    filament::Texture*   mDirt   = nullptr;
    bool                 mClosed = false;
    uint64_t             mTime   = 0;

    filament::Material const*             mDefaultMaterial     = nullptr;
    filament::Material const*             mTransparentMaterial = nullptr;
    filament::Material const*             mDepthMaterial       = nullptr;
    filament::MaterialInstance*           mDepthMI             = nullptr;
    std::unique_ptr<filagui::ImGuiHelper> mImGuiHelper;
    AnimCallback                          mAnimation;
    ResizeCallback                        mResize;
    DropCallback                          mDropHandler;
    int                                   mSidebarWidth  = 0;
    size_t                                mSkippedFrames = 0;
    std::vector<filament::View*>          mOffscreenViews;
    float                                 mCameraFocalLength = 28.0f;



    //////////////////////////////////////////////////////////////////////////
public:
    filament::viewer::ViewerGui* mViewerGUI;
    filament::Camera*            mMainCamera;

    filament::gltfio::AssetLoader*      mAssetLoader;
    filament::gltfio::FilamentAsset*    mAsset    = nullptr;
    filament::gltfio::FilamentInstance* mInstance = nullptr;
    utils::NameComponentManager*        mNames;

    filament::gltfio::MaterialProvider* mMaterials;
    MaterialSource                      mMaterialSource = UBERSHADER; //A57 ,powervr 8xxx

    filament::gltfio::ResourceLoader*  mResourceLoader = nullptr;
    filament::gltfio::TextureProvider* mStbDecoder     = nullptr;
    filament::gltfio::TextureProvider* mKtxDecoder     = nullptr;
    bool                     mRecomputeAabb  = false;

    bool mActualSize = false;

    struct Ground
    {
        utils::Entity        mGroundPlane;
        filament::VertexBuffer* mGroundVertexBuffer;
        filament::IndexBuffer*  mGroundIndexBuffer;
        filament::Material*     mGroundMaterial;
    } mGround;

    struct Overdraw
    {
        filament::Material* mOverdrawMaterial;
        // use layer 7 because 0, 1 and 2 are used by FilamentApp
        static constexpr auto                          OVERDRAW_VISIBILITY_LAYER = 7u; // overdraw renderables View layer
        static constexpr auto                          OVERDRAW_LAYERS           = 4u; // unique overdraw colors
        std::array<utils::Entity, OVERDRAW_LAYERS>     mOverdrawVisualizer;
        std::array<filament::MaterialInstance*, OVERDRAW_LAYERS> mOverdrawMaterialInstances;
        filament::VertexBuffer*                        mFullScreenTriangleVertexBuffer;
        filament::IndexBuffer*                         mFullScreenTriangleIndexBuffer;
    } mOverdraw;

    // zero-initialized so that the first time through is always dirty.
    filament::viewer::ColorGradingSettings mLastColorGradingOptions = {0};
    filament::ColorGrading* mColorGrading = nullptr;
    std::string mNotificationText;
    std::string mMessageBoxText;
    std::string mSettingsFile;
    std::string mBatchFile;

    filament::viewer::AutomationSpec*   mAutomationSpec   = nullptr;
    filament::viewer::AutomationEngine* mAutomationEngine = nullptr;

};


#endif // __GAMEDRIVER_H__
