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

#include "gamedriver/Ground.h"
#include "gamedriver/Overdraw.h"
#include "gamedriver/View.h"
#include "gamedriver/Window.h"
#include "gamedriver/ViewGui.h"




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
    static bool manipulatorKeyFromKeycode(SDL_Scancode scancode, CameraManipulator::Key& key);

    SINGLE_INSTANCE_FLAG(GameDriver)
private:
    ///临时存放 TODO

    void preRender(filament::View* view, filament::Scene* scene, filament::Renderer* renderer);
    void postRender(filament::View* view, filament::Scene* scene, filament::Renderer* renderer);

    void animate(double now);

public:
    void initialize();
    void release();

public:
    void loadAsset(utils::Path filename);
    void loadResources(utils::Path filename);
    void setup();

public:
    void   mainLoop();
    void   close() { mClosed = true; }
    void   addOffscreenView(filament::View* view) { mOffscreenViews.push_back(view); }
    size_t getSkippedFrameCount() const { return mSkippedFrames; }

private:
    void loadIBL();
    void loadDirt();

  
    void onMouseDown(int button, ssize_t x, ssize_t y);
    void onMouseUp(ssize_t x, ssize_t y);
    void onMouseMoved(ssize_t x, ssize_t y);
    void onMouseWheel(ssize_t x);
    void onKeyDown(SDL_Scancode scancode);
    void onKeyUp(SDL_Scancode scancode);

    void configureCamerasForWindow();

    std::unique_ptr<Window>               mMainWindow;
    filament::Engine::Backend             mBackend      = filament::Engine::Backend::OPENGL;
    filament::backend::FeatureLevel       mFeatureLevel = filament::backend::FeatureLevel::FEATURE_LEVEL_3;
    filament::Engine*                     mRenderEngine = nullptr;
    filament::Scene*                      mScene        = nullptr;
    std::unique_ptr<IBL>                  mIBL;
    filament::Texture*                    mDirt                = nullptr;
    bool                                  mClosed              = false;
    uint64_t                              mTime                = 0;
    filament::Material const*             mDefaultMaterial     = nullptr;
    filament::Material const*             mTransparentMaterial = nullptr;
    filament::Material const*             mDepthMaterial       = nullptr;
    filament::Material const*             mShadowMaterial      = nullptr;
    filament::Material const*             mOverdrawMaterial    = nullptr;
    filament::MaterialInstance*           mDepthMI             = nullptr;
    size_t                                mSkippedFrames = 0;
    std::vector<filament::View*>          mOffscreenViews;

    //////////////////////////////////////////////////////////////////////////
public:
    bool               mIsSplitView       = false;
    float              mCameraFocalLength = 28.0f;
    CameraManipulator* mMainCameraMan;
    CameraManipulator* mDebugCameraMan;

    utils::Entity     mCameraEntities[3];
    filament::Camera* mCameras[3] = {nullptr};
    filament::Camera* mMainCamera;
    filament::Camera* mDebugCamera;
    filament::Camera* mOrthoCamera;

    std::vector<std::unique_ptr<CView>> mViews;
    CView*                              mMainView;
    CView*                              mUiView;
    CView*                              mDepthView;
    CView*                              mGodView;
    CView*                              mOrthoView;

    ssize_t mLastX = 0;
    ssize_t mLastY = 0;

    CView* mMouseEventTarget = nullptr;
    // Keep track of which view should receive a key's keyUp event.
    std::unordered_map<SDL_Scancode, CView*> mKeyEventTarget;

    //////////////////////////////////////////////////////////////////////////
    filament::gltfio::AssetLoader*      mAssetLoader;
    filament::gltfio::FilamentAsset*    mAsset    = nullptr;
    filament::gltfio::FilamentInstance* mInstance = nullptr;
    utils::NameComponentManager*        mNames;
    filament::gltfio::MaterialProvider* mMaterials;
    MaterialSource                      mMaterialSource = JITSHADER;
    //UBERSHADER; //A57 ,powervr 8xxx
    filament::gltfio::ResourceLoader*  mResourceLoader = nullptr;
    filament::gltfio::TextureProvider* mStbDecoder     = nullptr;
    filament::gltfio::TextureProvider* mKtxDecoder     = nullptr;
    bool                               mRecomputeAabb  = false;
    bool                               mActualSize     = false;

    std::unique_ptr<Ground> mGround = nullptr;
    std::unique_ptr<OverdrawVisualizer> mOverdrawVisualizer = nullptr;
    

    // zero-initialized so that the first time through is always dirty.
    filament::viewer::ColorGradingSettings mLastColorGradingOptions = {0};
    filament::ColorGrading*                mColorGrading            = nullptr;
    std::string                            mNotificationText;
    std::string                            mMessageBoxText;
    std::string                            mSettingsFile;

private:
    //////////////////////////////////////////////////////////////////////////

    void setAsset(filament::gltfio::FilamentAsset* asset, filament::gltfio::FilamentInstance* instance);

    void populateScene();

    void removeAsset();

    void applyAnimation(double currentTime, filament::gltfio::FilamentInstance* instance = nullptr);

    void updateUserInterface();

    void mouseEvent(float mouseX, float mouseY, bool mouseButton, float mouseWheelY, bool control);
    void keyDownEvent(int keyCode);
    void keyUpEvent(int keyCode);
    void keyPressEvent(int charCode);

    void enableWireframe(bool b) { mEnableWireframe = b; }

    void enableSunlight(bool b) { gSettings.lighting.enableSunlight = b; }

    void enableDithering(bool b)
    {
        gSettings.view.dithering = b ? filament::Dithering::TEMPORAL : filament::Dithering::NONE;
    }
    void enableFxaa(bool b)
    {
        gSettings.view.antiAliasing = b ? filament::AntiAliasing::FXAA : filament::AntiAliasing::NONE;
    }

    void enableMsaa(bool b)
    {
        gSettings.view.msaa.sampleCount = 4;
        gSettings.view.msaa.enabled     = b;
    }

    void enableSSAO(bool b) { gSettings.view.ssao.enabled = b; }

    void enableBloom(bool bloom) { gSettings.view.bloom.enabled = bloom; }

    void setIBLIntensity(float brightness) { gSettings.lighting.iblIntensity = brightness; }

    void updateRootTransform();

    void stopAnimation() { mCurrentAnimation = 0; }

    int getCurrentCamera() const { return mCurrentCamera; }

    float getOcularDistance() const { return mOcularDistance; }

private:
    bool isRemoteMode() const { return mAsset == nullptr; }
    void customUI();
    void sceneSelectionUI();

    utils::Entity            mSunlight;
    filament::IndirectLight* mIndirectLight = nullptr;


    // Properties that can be changed from the UI.
    int                mCurrentAnimation   = 1; // It is a 1-based index and 0 means not playing animation
    int                mCurrentVariant     = 0;
    bool               mEnableWireframe    = false;
    int                mVsmMsaaSamplesLog2 = 1;
    uint32_t           mFlags;
    utils::Entity      mCurrentMorphingEntity;
    std::vector<float> mMorphWeights;
    SceneMask          mVisibleScenes;
    bool               mShowingRestPose = false;

    // Stereoscopic debugging
    float mOcularDistance = 0.0f;

    // 0 is the default "free camera". Additional cameras come from the gltf file (1-based index).
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
