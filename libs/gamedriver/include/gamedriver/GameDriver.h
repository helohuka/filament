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

#include "gamedriver/View.h"
#include "gamedriver/Window.h"
#include "gamedriver/ViewGui.h"




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

    void createGroundPlane();
    void createOverdrawVisualizerEntities();

    void preRender(filament::View* view, filament::Scene* scene, filament::Renderer* renderer);
    void postRender(filament::View* view, filament::Scene* scene, filament::Renderer* renderer);

    void animate(double now);

    void gui(filament::Engine*, filament::View* view);

    void initWindow();
    void releaseWindow();


    //void        setWindowTitle(const char* title)
    //{
    //    mWindowTitle = title;
    //    if (mWindowTitle != SDL_GetWindowTitle(mWindow))
    //    {
    //        SDL_SetWindowTitle(mWindow, mWindowTitle.c_str());
    //    }
    //}

public:
    void loadAsset(utils::Path filename);
    void loadResources(utils::Path filename);
    void setup();
    void cleanup();

public:
    void   mainLoop();
    void   close() { mClosed = true; }
    void   addOffscreenView(filament::View* view) { mOffscreenViews.push_back(view); }
    size_t getSkippedFrameCount() const { return mSkippedFrames; }

private:
    void loadIBL();
    void loadDirt();

    std::unique_ptr<Window> mMainWindow;

    filament::Engine::Backend       mBackend      = filament::Engine::Backend::OPENGL;
    filament::backend::FeatureLevel mFeatureLevel = filament::backend::FeatureLevel::FEATURE_LEVEL_3;

    filament::Engine*    mRenderEngine = nullptr;
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
    
    size_t                                mSkippedFrames = 0;
    std::vector<filament::View*>          mOffscreenViews;
    
    //////////////////////////////////////////////////////////////////////////
public:

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

    bool mActualSize = false;

    struct Ground
    {
        utils::Entity           mGroundPlane;
        filament::VertexBuffer* mGroundVertexBuffer;
        filament::IndexBuffer*  mGroundIndexBuffer;
        filament::Material*     mGroundMaterial;
    } mGround;

    struct Overdraw
    {
        filament::Material* mOverdrawMaterial;
        // use layer 7 because 0, 1 and 2 are used by GameDriver
        static constexpr auto                                    OVERDRAW_VISIBILITY_LAYER = 7u; // overdraw renderables View layer
        static constexpr auto                                    OVERDRAW_LAYERS           = 4u; // unique overdraw colors
        std::array<utils::Entity, OVERDRAW_LAYERS>               mOverdrawVisualizer;
        std::array<filament::MaterialInstance*, OVERDRAW_LAYERS> mOverdrawMaterialInstances;
        filament::VertexBuffer*                                  mFullScreenTriangleVertexBuffer;
        filament::IndexBuffer*                                   mFullScreenTriangleIndexBuffer;
    } mOverdraw;

    // zero-initialized so that the first time through is always dirty.
    filament::viewer::ColorGradingSettings mLastColorGradingOptions = {0};
    filament::ColorGrading*                mColorGrading            = nullptr;
    std::string                            mNotificationText;
    std::string                            mMessageBoxText;
    std::string                            mSettingsFile;
    std::string                            mBatchFile;

    filament::viewer::AutomationSpec*   mAutomationSpec   = nullptr;
    filament::viewer::AutomationEngine* mAutomationEngine = nullptr;


private:

    bool mIsResizeable = true;
    

    //////////////////////////////////////////////////////////////////////////

    void initViewGui();
    void releaseViewGui();
    void setAsset(filament::gltfio::FilamentAsset* asset, filament::gltfio::FilamentInstance* instance);

    void populateScene();

    void removeAsset();

    void setIndirectLight(filament::IndirectLight* ibl, filament::math::float3 const* sh3);

    void applyAnimation(double currentTime, filament::gltfio::FilamentInstance* instance = nullptr);

    void updateUserInterface();

    void renderUserInterface(float timeStepInSeconds, filament::View* guiView, float pixelRatio);

    void mouseEvent(float mouseX, float mouseY, bool mouseButton, float mouseWheelY, bool control);
    void keyDownEvent(int keyCode);
    void keyUpEvent(int keyCode);
    void keyPressEvent(int charCode);

    void setUiCallback(std::function<void()> callback) { mCustomUI = callback; }


    void enableWireframe(bool b) { mEnableWireframe = b; }

    void enableSunlight(bool b) { mSettings.lighting.enableSunlight = b; }

    void enableDithering(bool b)
    {
        mSettings.view.dithering = b ? filament::Dithering::TEMPORAL : filament::Dithering::NONE;
    }
    void enableFxaa(bool b)
    {
        mSettings.view.antiAliasing = b ? filament::AntiAliasing::FXAA : filament::AntiAliasing::NONE;
    }

    void enableMsaa(bool b)
    {
        mSettings.view.msaa.sampleCount = 4;
        mSettings.view.msaa.enabled     = b;
    }

    void enableSSAO(bool b) { mSettings.view.ssao.enabled = b; }

    void enableBloom(bool bloom) { mSettings.view.bloom.enabled = bloom; }

    void setIBLIntensity(float brightness) { mSettings.lighting.iblIntensity = brightness; }

    void updateRootTransform();

    void stopAnimation() { mCurrentAnimation = 0; }

    int getCurrentCamera() const { return mCurrentCamera; }

    float getOcularDistance() const { return mOcularDistance; }

private:
    using SceneMask = filament::gltfio::NodeManager::SceneMask;

    bool isRemoteMode() const { return mAsset == nullptr; }

    void sceneSelectionUI();

    utils::Entity           mSunlight;

    filament::IndirectLight* mIndirectLight = nullptr;
    std::function<void()>    mCustomUI;

    // Properties that can be changed from the UI.
    int                                      mCurrentAnimation   = 1; // It is a 1-based index and 0 means not playing animation
    int                                      mCurrentVariant     = 0;
    bool                                     mEnableWireframe    = false;
    int                                      mVsmMsaaSamplesLog2 = 1;
    filament::viewer::Settings               mSettings;
    uint32_t                                 mFlags;
    utils::Entity                            mCurrentMorphingEntity;
    std::vector<float>                       mMorphWeights;
    filament::gltfio::NodeManager::SceneMask mVisibleScenes;
    bool                                     mShowingRestPose = false;

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
