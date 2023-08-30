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

class Window
{
public:
    Window(filament::Engine* engine);
    ~Window();

    SDL_Window*          getSDLWindow() { return mWindow; }
    void*                getNativeWindow() { return (void*)mSystemInfo.info.win.window; }
    void*                getNativeSurface();
    void                 cleanupRenderer();
    void                 setupRenderer();
    void                 setupSwapChain();
    void                 fixupCoordinatesForHdpi(ssize_t& x, ssize_t& y);
    void                 fixupCoordinatesForHdpi(filament::vec2<ssize_t>& coordinate);
    filament::Renderer*  getRenderer() { return mRenderer; }
    filament::SwapChain* getSwapChain() { return mSwapChain; }


    void onResize();

private:
    filament::Engine* mEngine;

    SDL_Window*   mWindow = nullptr;
    SDL_SysWMinfo mSystemInfo;

    filament::Renderer*  mRenderer  = nullptr;
    filament::SwapChain* mSwapChain = nullptr;

    bool        mIsResizeable = true;
    bool        mNeedsDraw    = true;
    uint32_t    mWindowID     = 0;
    int         mWindowWidth    = 0; //VirtualWidth
    int         mWindowHeight   = 0; //VirtualHeight
    int         mDrawableWidth  = 0;
    int         mDrawableHeight = 0;
    float       mDpiScaleX       = 1.0f;
    float       mDpiScaleY       = 1.0f;
    std::string mWindowTitle; //<
};
#endif // __WINDOW_H__
