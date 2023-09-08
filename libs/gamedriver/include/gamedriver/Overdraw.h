#ifndef __OVERDRAW_H__
#define __OVERDRAW_H__ 1

/*!
 * \file Overdraw.h
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

class OverdrawVisualizer
{
public:
    // use layer 7 because 0, 1 and 2 are used by GameDriver
    static constexpr auto OVERDRAW_VISIBILITY_LAYER = 7u; // overdraw renderables View layer
    static constexpr auto OVERDRAW_LAYERS           = 4u; // unique overdraw colors

    OverdrawVisualizer(filament::Engine& engine, filament::Scene* scene, filament::Material const* material);

    ~OverdrawVisualizer();

private:
    filament::Engine&                                        mRenderEngine;
    std::array<utils::Entity, OVERDRAW_LAYERS>               mRenderableArray;
    std::array<filament::MaterialInstance*, OVERDRAW_LAYERS> mMIArray;
    filament::VertexBuffer*                                  mVertexBuffer = nullptr;
    filament::IndexBuffer*                                   mIndexBuffer = nullptr;
};

#endif // __OVERDRAW_H__
