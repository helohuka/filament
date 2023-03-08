/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <jni.h>
#include <android/native_window_jni.h>
#include <SDL.h>

#include <filamentapp/NativeWindowHelper.h>

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
