
#ifndef __CONFIG_H
#define __CONFIG_H

#include <string>

#include <filament/Engine.h>

#include <camutils/Manipulator.h>

struct Config {
    std::string title;
    std::string iblDirectory;
    std::string dirt;
    float scale = 1.0f;
    bool splitView = false;
    mutable filament::Engine::Backend backend = filament::Engine::Backend::DEFAULT;
    mutable filament::backend::FeatureLevel featureLevel = filament::backend::FeatureLevel::FEATURE_LEVEL_1;
    filament::camutils::Mode cameraMode = filament::camutils::Mode::ORBIT;
    bool resizeable = true;
    bool headless = false;
};

#endif // __CONFIG_H
