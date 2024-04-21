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
#include "gamedriver/Cube.h"
#include "gamedriver/Ground.h"
#include "gamedriver/Overdraw.h"
#include "gamedriver/View.h"

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

class Automation;

class GameDriver : public PrivateAPI
{
public:
    static bool manipulatorKeyFromKeycode(SDL_Scancode scancode, CameraManipulator::Key& key);
    
    GameDriver();
    ~GameDriver();

public:
    void mainLoop();

private:
    void initialize();
    void loadIBL();
    void loadDirt();
    void setup();
    void release();

    void loadAsset(utils::Path filename);

    void close() { mClosed = true; }
    void addOffscreenView(filament::View* view)
    {
        mOffscreenViews.push_back(view);
    }
    size_t getSkippedFrameCount() const { return mSkippedFrames; }

    // 住窗口和视口相关功能
    void configureCamerasForWindow();
    void fixupCoordinatesForHdpi(ssize_t& x, ssize_t& y);
    void setupWindow();
    void cleanupWindow();
    void resetSwapChain();
    void setupRendererAndSwapchain();
    void onWindowResize();

    // UI系统 相关功能
    void setupGui();
    void updateUI();
    void prepareGui();
    void cleanupGui();
    /// 临时存放 TODO

    void preRender(filament::View* view, filament::Scene* scene, filament::Renderer* renderer);
    void postRender(filament::View* view, filament::Scene* scene, filament::Renderer* renderer);
    void animate(double now);

    bool onProcessEvent(const SDL_Event* event);
    void onMouseDown(int button, ssize_t x, ssize_t y);
    void onMouseUp(ssize_t x, ssize_t y);
    void onMouseMoved(ssize_t x, ssize_t y);
    void onMouseWheel(ssize_t x);
    void onKeyDown(SDL_Scancode scancode);
    void onKeyUp(SDL_Scancode scancode);

    void setAsset();
    void populateScene();
    void removeAsset();
    void applyAnimation(double currentTime);

    void enableWireframe(bool b) { mEnableWireframe = b; }
    void enableSunlight(bool b) { gSettings.lighting.enableSunlight = b; }
    void enableDithering(bool b) { gSettings.view.dithering = b ? filament::Dithering::TEMPORAL : filament::Dithering::NONE; }
    void enableFxaa(bool b) { gSettings.view.antiAliasing = b ? filament::AntiAliasing::FXAA : filament::AntiAliasing::NONE; }
    void enableMsaa(bool b)
    {
        gSettings.view.msaa.sampleCount = 4;
        gSettings.view.msaa.enabled     = b;
    }
    void  enableSSAO(bool b) { gSettings.view.ssao.enabled = b; }
    void  enableBloom(bool bloom) { gSettings.view.bloom.enabled = bloom; }
    void  setIBLIntensity(float brightness) { gSettings.lighting.iblIntensity = brightness; }
    void  updateRootTransform();
    void  stopAnimation() { mCurrentAnimation = 0; }
    int   getCurrentCamera() const { return mCurrentCamera; }
    float getOcularDistance() const { return mOcularDistance; }
    bool  isRemoteMode() const { return mAsset == nullptr; }
    void  customUI();
    void  sceneSelectionUI();


    filament::Engine::Backend       mBackend             = filament::Engine::Backend::OPENGL;
    filament::backend::FeatureLevel mFeatureLevel        = filament::backend::FeatureLevel::FEATURE_LEVEL_3;
    filament::Engine*               mRenderEngine        = nullptr;
    filament::Scene*                mScene               = nullptr;
    std::unique_ptr<IBL>            mIBL                 = nullptr;
    filament::Texture*              mDirt                = nullptr;
    bool                            mClosed              = false;
    uint64_t                        mTime                = 0;
    filament::Material const*       mDefaultMaterial     = nullptr;
    filament::Material const*       mTransparentMaterial = nullptr;
    filament::Material const*       mDepthMaterial       = nullptr;
    filament::Material const*       mShadowMaterial      = nullptr;
    filament::Material const*       mOverdrawMaterial    = nullptr;
    filament::MaterialInstance*     mDepthMI             = nullptr;
    size_t                          mSkippedFrames       = 0;
    std::vector<filament::View*>    mOffscreenViews;

    std::unique_ptr<Automation> mAutomation = nullptr;

    // Window 相关
    SDL_Window*           mWindow            = nullptr;
    filament::Renderer*   mRenderer          = nullptr;
    filament::SwapChain*  mSwapChain         = nullptr;
    int                   mWidth             = 0;
    int                   mHeight            = 0;
    float                 mDpiScaleX         = 1.0f;
    float                 mDpiScaleY         = 1.0f;
    ssize_t               mLastX             = 0;
    ssize_t               mLastY             = 0;
    float                 mCameraFocalLength = 28.0f;
    std::unique_ptr<Cube> mCameraCube        = nullptr;
    std::unique_ptr<Cube> mLightmapCube      = nullptr;
    CameraManipulator*    mMainCameraMan     = nullptr;
    CameraManipulator*    mDebugCameraMan    = nullptr;

    utils::Entity     mMainCameraEntity;
    filament::Camera* mMainCamera = nullptr;
    CView*            mMainView   = nullptr;

    filament::Material*      mImGuiMaterial = nullptr;
    filament::TextureSampler mImGuiSampler;
    filament::Texture*       mImGuiTexture = nullptr;
    class CViewUI*           mMainUI       = nullptr;

    CView* mMouseEventTarget = nullptr;
    // Keep track of which view should receive a key's keyUp event.
    std::unordered_map<SDL_Scancode, CView*> mKeyEventTarget;

    class MeshAssimp*                         mAssimp             = nullptr;
    filament::gltfio::AssetLoader*      mAssetLoader        = nullptr;
    filament::gltfio::FilamentAsset*    mAsset              = nullptr;
    filament::gltfio::FilamentInstance* mInstance           = nullptr;
    utils::NameComponentManager*        mNames              = nullptr;
    filament::gltfio::MaterialProvider* mMaterials          = nullptr;
    MaterialSource                      mMaterialSource     = JITSHADER; // UBERSHADER; //A57 ,powervr 8xxx
    filament::gltfio::ResourceLoader*   mResourceLoader     = nullptr;
    filament::gltfio::TextureProvider*  mStbDecoder         = nullptr;
    filament::gltfio::TextureProvider*  mKtxDecoder         = nullptr;
    bool                                mRecomputeAabb      = false;
    bool                                mActualSize         = false;
    std::unique_ptr<Ground>             mGround             = nullptr;
    std::unique_ptr<OverdrawVisualizer> mOverdrawVisualizer = nullptr;

    // zero-initialized so that the first time through is always dirty.
    filament::viewer::ColorGradingSettings mLastColorGradingOptions = {0};
    filament::ColorGrading*                mColorGrading            = nullptr;
    std::string                            mNotificationText;
    std::string                            mMessageBoxText;
    std::string                            mSettingsFile;
    filament::viewer::Settings             mSettings;
    utils::Path                            mFontPath;

    utils::Entity            mSunlight;
    filament::IndirectLight* mIndirectLight = nullptr;

    // Properties that can be changed from the UI.
    int                mCurrentAnimation   = 1; // It is a 1-based index and 0 means not playing animation
    int                mCurrentVariant     = 0;
    bool               mEnableWireframe    = false;
    int                mVsmMsaaSamplesLog2 = 1;
    uint32_t           mFlags = 0;
    utils::Entity      mCurrentMorphingEntity;
    std::vector<float> mMorphWeights;
    SceneMask          mVisibleScenes;
    bool               mShowingRestPose = false;

    // Stereoscopic debugging
    float mOcularDistance = 0.0f;

    // 0 is the default "free camera". Additional cameras come from the gltf
    // file (1-based index).
    int mCurrentCamera = 0;

    // Cross fade animation parameters.
    float  mCrossFadeDuration = 0.5f; // number of seconds to transition between animations
    int    mPreviousAnimation = 0;    // one-based index of the previous animation
    double mCurrentStartTime  = 0.0f; // start time of most recent cross-fade (seconds)
    double mPreviousStartTime = 0.0f; // start time of previous cross-fade (seconds)
    bool   mResetAnimation    = true; // set when building ImGui widgets, honored in applyAnimation

    // Color grading UI state.
    float mToneMapPlot[1024];
    float mRangePlot[1024 * 3];
    float mCurvePlot[1024 * 3];
};

#endif // __GAMEDRIVER_H__
