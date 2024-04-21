
#ifndef __CONFIGURE_H__
#define __CONFIGURE_H__ 1

#include "gamedriver/BaseLibs.h"

//std::string filename = "/sdcard/models/Breakdancer.glb"; //"/sdcard/model/TreeHouse.glb";
struct Configure
{
    bool                            resizeable      = true;
    bool                            splitView       = false;
    int                             width           = 1280; //window 宽高
    int                             height          = 768;
    float                           scale           = 1.0f;
    std::string                     title           = "GameBox";
    std::string                     assetsDirectory = "E:/1/filament/apps/windows/build/apps/launcher/assets";
    std::string                     iblDirectory    = assetsDirectory + "/ibl/noon_grass_2k";
    std::string                     fontPath        = assetsDirectory + "/fonts/Roboto-Medium.ttf";
    std::string                     dirt;
    std::string                     filename     = assetsDirectory + "/models/jet.glb";
    filament::Engine::Backend       backend      = filament::Engine::Backend::OPENGL;
    filament::backend::FeatureLevel featureLevel = filament::backend::FeatureLevel::FEATURE_LEVEL_3;
    filament::camutils::Mode        cameraMode   = filament::camutils::Mode::ORBIT;
};

extern Configure                  gConfigure;
extern filament::viewer::Settings gSettings;




#endif // __CONFIG_H
