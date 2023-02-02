

#include <gamedriver/NativeWindowHelper.h>

#include <utils/Panic.h>

#include <SDL_syswm.h>

void* getNativeWindow(SDL_Window* sdlWindow) {
    SDL_SysWMinfo wmi;
    SDL_VERSION(&wmi.version);
    ASSERT_POSTCONDITION(SDL_GetWindowWMInfo(sdlWindow, &wmi), "SDL version unsupported!");
    if (wmi.subsystem == SDL_SYSWM_X11) {
#if defined(FILAMENT_SUPPORTS_X11)
        Window win = (Window) wmi.info.x11.window;
        return (void *) win;
#endif
    }
    else if (wmi.subsystem == SDL_SYSWM_WAYLAND) {
#if defined(FILAMENT_SUPPORTS_WAYLAND)
        // Static is used here to allocate the struct pointer for the lifetime of the program.
        // Without static the valid struct quickyly goes out of scope, and ends with seemingly
        // random segfaults.
        static struct {
            struct wl_display *display;
            struct wl_surface *surface;
        } wayland {
            wmi.info.wl.display,
            wmi.info.wl.surface,
        };
        return (void *) &wayland;
#endif
    }
    return nullptr;
}
