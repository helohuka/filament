
#include <jni.h>
#include <android/native_window_jni.h>
#include <SDL.h>

#include <gamedriver/NativeWindowHelper.h>

#include <utils/Panic.h>

#include <SDL_syswm.h>
#include <SDL_video.h>
extern "C" void* getNativeWindow(SDL_Window* sdlWindow) {
//   SDL_SysWMinfo wmi;
//   SDL_VERSION(&wmi.version);
//   ASSERT_POSTCONDITION(SDL_GetWindowWMInfo(sdlWindow, &wmi), "SDL version unsupported!");
//
//    return wmi.info.android.window;
   JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    //SDL_GLContext context  = SDL_GL_GetCurrentContext();

    jclass class_sdl_activity   = env->FindClass("org/libsdl/app/SDLActivity");
    jmethodID method_get_native_surface = env->GetStaticMethodID(class_sdl_activity, "getNativeSurface", "()Landroid/view/Surface;");
    jobject raw_surface = env->CallStaticObjectMethod(class_sdl_activity, method_get_native_surface);
    ANativeWindow* native_window = ANativeWindow_fromSurface(env, raw_surface);

    if ( !native_window )
        return nullptr;


    return native_window;
}

extern "C" void* getNativeSurface(SDL_Window* sdlWindow) {
    SDL_SysWMinfo wmi;
    SDL_VERSION(&wmi.version);
    ASSERT_POSTCONDITION(SDL_GetWindowWMInfo(sdlWindow, &wmi), "SDL version unsupported!");

    return wmi.info.android.surface;

    return nullptr;
}
