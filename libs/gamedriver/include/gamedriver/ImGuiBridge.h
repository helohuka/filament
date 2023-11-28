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

class CViewUI
{
public:
    CViewUI(filament::Engine& engine, SDL_Window* window, filament::Material* imGuiMaterial, filament::TextureSampler imGuiSampler, filament::Texture* imGuiTexture);
    CViewUI(CViewUI* bd, ImGuiViewport* viewport);
    ~CViewUI();

    filament::Engine&    mRenderEngine;
    SDL_Window*          mWindow    = nullptr;
    filament::Renderer*  mRenderer  = nullptr;
    filament::SwapChain* mSwapChain = nullptr;
    utils::Entity        mCameraEntity;
    filament::Camera*    mCamera = nullptr;
    filament::View*      mView   = nullptr;
    filament::Scene*     mScene  = nullptr;

    std::vector<filament::VertexBuffer*>     mVertexBuffers;
    std::vector<filament::IndexBuffer*>      mIndexBuffers;
    std::vector<filament::MaterialInstance*> mMaterialInstances;
    utils::Entity                            mRenderable;

    filament::Material*      mImGuiMaterial = nullptr;
    filament::TextureSampler mImGuiSampler;
    filament::Texture*       mImGuiTexture = nullptr;

    bool   mHasSynced                    = false;
    bool   WindowOwned                   = false;
    bool   mNeedsDraw                    = true;
    double mTime                         = 0.0;
    double mLastDrawTime                 = 0.0;
    int    MouseWindowID                 = 0;
    int    MouseButtonsDown              = 0;
    int    PendingMouseLeaveFrame        = 0;
    char*  ClipboardTextData             = nullptr;
    bool   MouseCanUseGlobalState        = false;
    bool   MouseCanReportHoveredViewport = false;
    bool   WantUpdateMonitors            = false;

    bool intersects(ssize_t x, ssize_t y);
    void processImGuiCommands(ImDrawData* commands);

private:
    void createBuffers(int numRequiredBuffers);
    void populateVertexData(size_t bufferIndex, size_t vbSizeInBytes, void* vbData, size_t ibSizeInBytes, void* ibData);
    void createVertexBuffer(size_t bufferIndex, size_t capacity);
    void createIndexBuffer(size_t bufferIndex, size_t capacity);
    void syncThreads();
};

#endif // __IMGUIBRIDGE_H__
