
#ifndef __CUBE_H
#define __CUBE_H

#include <vector>

#include <filament/Engine.h>
#include <filament/Box.h>
#include <filament/Camera.h>
#include <filament/Material.h>
#include <filament/MaterialInstance.h>
#include <utils/Entity.h>

class Cube {
public:
    Cube(filament::Engine& engine, filament::Material const* material, filament::math::float3 linearColor, bool culling = true);

    utils::Entity getSolidRenderable() {
        return mSolidRenderable;
    }

    utils::Entity getWireFrameRenderable() {
        return mWireFrameRenderable;
    }

    ~Cube();

    void mapFrustum(filament::Engine& engine, filament::Camera const* camera);
    void mapFrustum(filament::Engine& engine, filament::math::mat4 const& transform);
    void mapAabb(filament::Engine& engine, filament::Box const& box);

private:
    static constexpr size_t WIREFRAME_OFFSET = 3*2*6;
    static const uint32_t mIndices[];
    static const filament::math::float3 mVertices[];

    filament::Engine& mEngine;
    filament::VertexBuffer* mVertexBuffer = nullptr;
    filament::IndexBuffer* mIndexBuffer = nullptr;
    filament::Material const* mMaterial = nullptr;
    filament::MaterialInstance* mMaterialInstanceSolid = nullptr;
    filament::MaterialInstance* mMaterialInstanceWireFrame = nullptr;
    utils::Entity mSolidRenderable;
    utils::Entity mWireFrameRenderable;
};


#endif // __CUBE_H
