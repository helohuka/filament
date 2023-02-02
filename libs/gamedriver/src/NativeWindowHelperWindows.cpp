

#include <gamedriver/NativeWindowHelper.h>

#include <utils/Panic.h>

#include <SDL_syswm.h>

void* getNativeWindow(SDL_Window* sdlWindow) {
    SDL_SysWMinfo wmi;
    SDL_VERSION(&wmi.version);
    ASSERT_POSTCONDITION(SDL_GetWindowWMInfo(sdlWindow, &wmi), "SDL version unsupported!");
    HWND win = (HWND) wmi.info.win.window;
    return (void*) win;
}
