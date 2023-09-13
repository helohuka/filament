#ifndef __IMGUIHELPER_H__
#define __IMGUIHELPER_H__ 1

/*!
 * \file ImGuiHelper.h
 * \date 2023.09.12
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

struct ImDrawData;
struct ImGuiIO;
struct ImGuiContext;


// Translates ImGui's draw commands into Filament primitives, textures, vertex buffers, etc.
// Creates a UI-specific Scene object and populates it with a Renderable. Does not handle
// event processing; clients can simply call ImGui::GetIO() directly and set the mouse state.
class ImGuiHelper
{
public:
    // Using std::function instead of a vanilla C callback to make it easy for clients to pass in
    // lambdas that have captures.
    using Callback = std::function<void(filament::Engine*, filament::View*)>;

    // The constructor creates its own Scene and places it in the given View.
    ImGuiHelper(filament::Engine* engine, filament::View* view, const utils::Path& fontPath, ImGuiContext* imGuiContext = nullptr);
    ~ImGuiHelper();

    // Informs ImGui of the current display size, as well as a scaling factor when scissoring.
    void setDisplaySize(int width, int height, float scaleX = 1.0f, float scaleY = 1.0f, bool flipVertical = false);

    // High-level utility method that takes a callback for creating all ImGui windows and widgets.
    // Clients are responsible for rendering the View. This should be called on every frame,
    // regardless of whether the Renderer wants to skip or not.
    void render(float timeStepInSeconds, Callback imguiCommands);

    // Low-level alternative to render() that consumes an ImGui command list and translates it into
    // various Filament calls. This includes updating the vertex buffer, setting up material
    // instances, and rebuilding the Renderable component that encompasses the entire UI. Since this
    // makes Filament calls, it must be called from the main thread.
    void processImGuiCommands(ImDrawData* commands);

    // Helper method called after resolving fontPath; public so fonts can be added by caller.
    void createAtlasTexture();

    // Returns the client-owned view, useful for drawing 2D overlays.
    filament::View* getView() const { return mView; }

private:
    void                                     createBuffers(int numRequiredBuffers);
    void                                     populateVertexData(size_t bufferIndex, size_t vbSizeInBytes, void* vbData, size_t ibSizeInBytes, void* ibData);
    void                                     createVertexBuffer(size_t bufferIndex, size_t capacity);
    void                                     createIndexBuffer(size_t bufferIndex, size_t capacity);
    void                                     syncThreads();
    
    filament::Engine*                        mEngine;
    filament::View*                          mView; // The view is owned by the client.
    filament::Scene*                         mScene;
    filament::Material*                      mMaterial = nullptr;
    filament::Camera*                        mCamera   = nullptr;
    std::vector<filament::VertexBuffer*>     mVertexBuffers;
    std::vector<filament::IndexBuffer*>      mIndexBuffers;
    std::vector<filament::MaterialInstance*> mMaterialInstances;
    utils::Entity                            mRenderable;
    utils::Entity                            mCameraEntity;
    filament::Texture*                       mTexture   = nullptr;
    bool                                     mHasSynced = false;
    ImGuiContext*                            mImGuiContext;
    filament::TextureSampler                 mSampler;
    bool                                     mFlipVertical = false;
    utils::Path                              mSettingsPath;
};



#endif // __IMGUIHELPER_H__
