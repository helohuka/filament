#ifndef __SPHERE_H__
#define __SPHERE_H__ 1

/*!
 * \file Sphere.h
 * \date 2023.08.20
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

#include <utils/Entity.h>
#include <math/vec3.h>

namespace filament
{
class Engine;
class IndexBuffer;
class Material;
class MaterialInstance;
class VertexBuffer;
} // namespace filament

class Sphere
{
public:
    Sphere(filament::Engine&         engine,
           filament::Material const* material,
           bool                      culling = true);
    ~Sphere();

    Sphere(Sphere const&)            = delete;
    Sphere& operator=(Sphere const&) = delete;

    Sphere(Sphere&& rhs) noexcept
        :
        mEngine(rhs.mEngine),
        mMaterialInstance(rhs.mMaterialInstance),
        mRenderable(rhs.mRenderable)
    {
        rhs.mMaterialInstance = {};
        rhs.mRenderable       = {};
    }

    utils::Entity getSolidRenderable() const
    {
        return mRenderable;
    }

    filament::MaterialInstance* getMaterialInstance()
    {
        return mMaterialInstance;
    }

    Sphere& setPosition(filament::math::float3 const& position) noexcept;
    Sphere& setRadius(float radius) noexcept;

private:
    filament::Engine&           mEngine;
    filament::MaterialInstance* mMaterialInstance = nullptr;
    utils::Entity               mRenderable;
};

#endif // __SPHERE_H__
