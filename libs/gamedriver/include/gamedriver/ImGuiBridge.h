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

struct ImGuiWindowImpl
{
    ImGuiWindowImpl();
    ~ImGuiWindowImpl();

    std::function<void(ImGuiWindowImpl&, double)> mOnNewFrame;

    SDL_Window*                              mWindow    = nullptr;
    filament::Renderer*                      mRenderer  = nullptr;
    filament::SwapChain*                     mSwapChain = nullptr;
    utils::Entity                            mCameraEntity;
    filament::Camera*                        mCamera   = nullptr;
    filament::View*                          mView     = nullptr;
    filament::Scene*                         mScene    = nullptr;
    
    std::vector<filament::VertexBuffer*>     mVertexBuffers;
    std::vector<filament::IndexBuffer*>      mIndexBuffers;
    std::vector<filament::MaterialInstance*> mMaterialInstances;
    utils::Entity                            mRenderable;
    
    bool                                     mHasSynced = false;
  
    bool                                     WindowOwned      = false;
    bool                                     mNeedsDraw       = true;
    double                                   mTime            = 0.0;
    double                                   mLastDrawTime    = 0.0;
    bool                                     MouseButtonsDown = false;

    void processImGuiCommands(ImDrawData* commands);

private:
  
    void createBuffers(int numRequiredBuffers);
    void populateVertexData(size_t bufferIndex, size_t vbSizeInBytes, void* vbData, size_t ibSizeInBytes, void* ibData);
    void createVertexBuffer(size_t bufferIndex, size_t capacity);
    void createIndexBuffer(size_t bufferIndex, size_t capacity);
    void syncThreads();
};

ImGuiKey ImGui_ImplSDL2_KeycodeToImGuiKey(int keycode);
void     ImGui_ImplSDL2_InitPlatformInterface(ImGuiWindowImpl* window, filament::Engine* engine, const utils::Path& fontPath);
void ImGui_ImplSDL2_NewFrame();
bool ImGui_ImplSDL2_ProcessEvent(const SDL_Event* event);


#endif // __IMGUIBRIDGE_H__
