#ifndef __WINDOW_H__
#define __WINDOW_H__ 1

/*!
 * \file Window.h
 * \date 2023.08.28
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
#include "gamedriver/View.h"

class Window
{
public:
    Window(filament::Engine* engine);
    ~Window();
    
    void setup();
    void cleanup();
    
    void resetSwapChain();

    SDL_Window* getWindowHandle() { return mWindowHandle; }
    void*       getNativeWindow() { return (void*)mSystemInfo.info.win.window; }
    void*       getNativeSurface();

    void        fixupCoordinatesForHdpi(ssize_t& x, ssize_t& y);
    void        fixupCoordinatesForHdpi(filament::math::vec2<ssize_t>& coordinate);

    filament::Renderer*          getRenderer() { return mRenderer; }
    filament::SwapChain*         getSwapChain() { return mSwapChain; }
    bool                         getIsResizeable() { return mIsResizeable; }
    bool                         getIsSplitView() { return mIsSplitView; }
    filament::math::vec2<int>&   getWindowSize() { return mWindowSize; }
    filament::math::vec2<int>&   getDrawableSize() { return mDrawableSize; }
    filament::math::vec2<float>& getDpiScale() { return mDpiScale; }
    std::string&                 getWindowTitle() { return mWindowTitle; }
    int &                         getSidebarWidth() { return mSidebarWidth; }
    float  &                      getCameraFocalLength() { return mCameraFocalLength; }
    CameraManipulator*           getMainCameraMan() { return mMainCameraMan; }
    CameraManipulator*           getDebugCameraMan() { return mDebugCameraMan; }
    //utils::Entity                       getCameraEntities() { return mCameraEntities; }
    //filament::Camera*                   getCameras() { return mCameras[3]; }
    filament::Camera*                   getMainCamera() { return mMainCamera; }
    filament::Camera*                   getDebugCamera() { return mDebugCamera; }
    filament::Camera*                   getOrthoCamera() { return mOrthoCamera; }
    std::vector<std::unique_ptr<CView>>& getViews() { return mViews; }
    CView*                              getMainView() { return mMainView; }
    CView*                              getUiView() { return mUiView; }
    CView*                              getDepthView() { return mDepthView; }
    CView*                              getGodView() { return mGodView; }
    CView*                              getOrthoView() { return mOrthoView; }


    void onResize();
    void onMouseDown(int button, ssize_t x, ssize_t y);
    void onMouseUp(ssize_t x, ssize_t y);
    void onMouseMoved(ssize_t x, ssize_t y);
    void onMouseWheel(ssize_t x);
    void onKeyDown(SDL_Scancode scancode);
    void onKeyUp(SDL_Scancode scancode);

    void configureCamerasForWindow();

private:
    void updateWindowInfo();
    

private:
    filament::Engine* mRenderEngine;

    uint32_t      mWindowID     = 0;
    SDL_Window*   mWindowHandle = nullptr;
    SDL_SysWMinfo mSystemInfo;

    filament::Renderer*  mRenderer  = nullptr;
    filament::SwapChain* mSwapChain = nullptr;

    bool                        mIsResizeable = true;
    bool                        mIsSplitView  = false;
    filament::math::vec2<int>   mWindowSize;
    filament::math::vec2<int>   mDrawableSize;
    filament::math::vec2<float> mDpiScale;
    std::string                 mWindowTitle; //<

    int                mSidebarWidth      = 300;
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
};
#endif // __WINDOW_H__
