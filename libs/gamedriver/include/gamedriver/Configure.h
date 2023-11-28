
#ifndef __CONFIGURE_H__
#define __CONFIGURE_H__ 1

#include "gamedriver/BaseLibs.h"

struct Configure
{
    bool resizeable = true;
    bool splitView  = false;

    int width  = 1280; //window 宽高
    int height = 768;

    float scale = 1.0f;

    std::string projectURI = "E:/1/filament/third_party/models/";
    std::string title      = "GameBox";
    std::string iblDirectory;
    std::string dirt;
    std::string filename = "/sdcard/models/TreeHouse.glb"; //"/sdcard/model/TreeHouse.glb";
    //std::string filename = "E:/1/filament/third_party/models/Breakdancer.glb";

    filament::Engine::Backend       backend      = filament::Engine::Backend::OPENGL;
    filament::backend::FeatureLevel featureLevel = filament::backend::FeatureLevel::FEATURE_LEVEL_1;
    filament::camutils::Mode        cameraMode   = filament::camutils::Mode::ORBIT;
};

extern Configure                  gConfigure;
extern filament::viewer::Settings gSettings;




#endif // __CONFIG_H
