
#ifndef __CONFIG_H__
#define __CONFIG_H__ 1

#include "gamedriver/BaseLibs.h"
#include "Config_generated.h"

struct Config {

    bool resizeable = true;
    bool headless   = false;
    bool splitView  = false;

    float scale = 1.0f;
    
    std::string title = "23";
    std::string iblDirectory;
    std::string dirt;

    filament::Engine::Backend backend               = filament::Engine::Backend::DEFAULT;
    filament::backend::FeatureLevel featureLevel    = filament::backend::FeatureLevel::FEATURE_LEVEL_1;
    filament::camutils::Mode cameraMode             = filament::camutils::Mode::ORBIT;
    
};



#endif // __CONFIG_H
