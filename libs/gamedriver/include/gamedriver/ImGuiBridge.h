#ifndef __IMGUIBRIDGE_H__
#define __IMGUIBRIDGE_H__ 1

/*!
 * \file ImGuiBridge.h
 * \date 2023.09.08
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

ImGuiKey ImGui_ImplSDL2_KeycodeToImGuiKey(int keycode);

void ImGui_ImplSDL2_InitPlatformInterface(SDL_Window* window);


#endif // __IMGUIBRIDGE_H__
