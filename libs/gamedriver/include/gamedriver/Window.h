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
    filament::math::vec2<int>&   getWindowSize() { return mWindowSize; }
    filament::math::vec2<int>&   getDrawableSize() { return mDrawableSize; }
    filament::math::vec2<float>& getDpiScale() { return mDpiScale; }
    std::string&                 getWindowTitle() { return mWindowTitle; }

    void onResize();
    

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
    
    filament::math::vec2<int>   mWindowSize;
    filament::math::vec2<int>   mDrawableSize;
    filament::math::vec2<float> mDpiScale;
    std::string                 mWindowTitle; //<
};
#endif // __WINDOW_H__
