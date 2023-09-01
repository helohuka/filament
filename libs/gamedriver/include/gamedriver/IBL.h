#ifndef __IBL_H__
#define __IBL_H__ 1

/*!
 * \file IBL.h
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


#include "gamedriver/BaseLibs.h"

class IBL
{
public:
    explicit IBL(filament::Engine& engine);
    ~IBL();

    bool loadFromEquirect(const utils::Path& path);
    bool loadFromDirectory(const utils::Path& path);
    bool loadFromKtx(const std::string& prefix);

    filament::IndirectLight* getIndirectLight() const noexcept
    {
        return mIndirectLight;
    }

    filament::Skybox* getSkybox() const noexcept
    {
        return mSkybox;
    }

    filament::math::float3 const* getSphericalHarmonics() const { return mBands; }

private:
    bool loadCubemapLevel(filament::Texture** texture, const utils::Path& path, size_t level = 0, std::string const& levelPrefix = "") const;

    bool loadCubemapLevel(filament::Texture**                       texture,
                          filament::Texture::PixelBufferDescriptor* outBuffer,
                          uint32_t*                                 dim,
                          const utils::Path&                        path,
                          size_t                                    level       = 0,
                          std::string const&                        levelPrefix = "") const;

    filament::Engine&        mRenderEngine;
    filament::math::float3   mBands[9]      = {};
    filament::Texture*       mTexture       = nullptr;
    filament::IndirectLight* mIndirectLight = nullptr;
    filament::Texture*       mSkyboxTexture = nullptr;
    filament::Skybox*        mSkybox        = nullptr;
};

#endif // __IBL_H__
