#ifndef __GROUND_H__
#define __GROUND_H__ 1

/*!
 * \file Ground.h
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

class Ground
{
public:
    Ground(filament::Engine& engine, filament::Scene* scene, filament::Material const* material);

    ~Ground();

    utils::Entity getSolidRenderable()
    {
        return mRenderable;
    }
    
    filament::MaterialInstance* getMI()
    {
        return mMaterialInstance;
    }

    //utils::Entity getWireFrameRenderable()
    //{
    //    return mWireFrameRenderable;
    //}



private:
    filament::Engine&           mRenderEngine;
    utils::Entity               mRenderable;
    filament::VertexBuffer*     mVertexBuffer;
    filament::IndexBuffer*      mIndexBuffer;
    filament::MaterialInstance* mMaterialInstance;
};

#endif // __GROUND_H__
