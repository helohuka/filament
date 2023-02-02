

#ifndef __WINDOW_HELPER_H
#define __WINDOW_HELPER_H

struct SDL_Window;

extern "C" void* getNativeWindow(SDL_Window* sdlWindow);

extern "C" void* getNativeSurface(SDL_Window* sdlWindow);

#if defined(__APPLE__)
// Add a backing CAMetalLayer to the NSView and return the layer.
extern "C" void* setUpMetalLayer(void* nativeWindow);
// Setup the window the way Filament expects (color space, etc.).
extern "C" void prepareNativeWindow(SDL_Window* sdlWindow);
// Resize the backing CAMetalLayer's drawable to match the new view's size. Returns the layer.
extern "C" void* resizeMetalLayer(void* nativeView);
#endif

#endif // __WINDOW_HELPER_H
