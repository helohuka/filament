
#ifndef __CONFIGURE_H__
#define __CONFIGURE_H__ 1

#include "gamedriver/BaseLibs.h"
#include "Configure_generated.h"

struct Config {

    bool resizeable = true;
    bool splitView  = true;


    int width = 1280;   //window 宽高
    int height = 768;

    float scale = 1.0f;
    
    std::string projectURI = "E:/1/filament/third_party/models/";
    std::string title = "23";
    std::string iblDirectory;
    std::string dirt;
    std::string filename = "E:/1/filament/third_party/models/Breakdancer.glb";

    filament::Engine::Backend backend               = filament::Engine::Backend::DEFAULT;
    filament::backend::FeatureLevel featureLevel    = filament::backend::FeatureLevel::FEATURE_LEVEL_1;
    filament::camutils::Mode cameraMode             = filament::camutils::Mode::ORBIT;
    
    
};



#endif // __CONFIG_H
