
#include <memory>

#include <gamedriver/GameDriver.h>

#if defined(WIN32)
#    include <SDL_syswm.h>
#    include <utils/unwindows.h>
#endif

#include <iostream>

#include <imgui.h>

#include <utils/EntityManager.h>
#include <utils/Panic.h>
#include <utils/Path.h>

#include <filament/Camera.h>
#include <filament/Material.h>
#include <filament/MaterialInstance.h>
#include <filament/Renderer.h>
#include <filament/RenderableManager.h>
#include <filament/Scene.h>
#include <filament/Skybox.h>
#include <filament/View.h>

#ifndef NDEBUG
#include <filament/DebugRegistry.h>
#endif

#include <filagui/ImGuiHelper.h>

#include <gamedriver/Cube.h>
#include <gamedriver/NativeWindowHelper.h>

#include <stb_image.h>

#include "generated/resources/gamedriver.h"


using namespace filament;
using namespace filagui;
using namespace filament::math;
using namespace utils;
// ------------------------------------------------------------------------------------------------

GameDriver::CView::CView(Renderer& renderer, std::string name)
        : engine(*renderer.getEngine()), mName(name) {
    view = engine.createView();
    view->setName(name.c_str());
}

GameDriver::CView::~CView() {
    engine.destroy(view);
}

void GameDriver::CView::setViewport(Viewport const& viewport) {
    mViewport = viewport;
    view->setViewport(viewport);
    if (mCameraManipulator) {
        mCameraManipulator->setViewport(viewport.width, viewport.height);
    }
}

void GameDriver::CView::mouseDown(int button, ssize_t x, ssize_t y) {
    if (mCameraManipulator) {
        mCameraManipulator->grabBegin(x, y, button == 3);
    }
}

void GameDriver::CView::mouseUp(ssize_t x, ssize_t y) {
    if (mCameraManipulator) {
        mCameraManipulator->grabEnd();
    }
}

void GameDriver::CView::mouseMoved(ssize_t x, ssize_t y) {
    if (mCameraManipulator) {
        mCameraManipulator->grabUpdate(x, y);
    }
}

void GameDriver::CView::mouseWheel(ssize_t x) {
    if (mCameraManipulator) {
        mCameraManipulator->scroll(0, 0, x);
    }
}

bool GameDriver::manipulatorKeyFromKeycode(SDL_Scancode scancode, CameraManipulator::Key& key) {
    switch (scancode) {
        case SDL_SCANCODE_W:
            key = CameraManipulator::Key::FORWARD;
            return true;
        case SDL_SCANCODE_A:
            key = CameraManipulator::Key::LEFT;
            return true;
        case SDL_SCANCODE_S:
            key = CameraManipulator::Key::BACKWARD;
            return true;
        case SDL_SCANCODE_D:
            key = CameraManipulator::Key::RIGHT;
            return true;
        case SDL_SCANCODE_E:
            key = CameraManipulator::Key::UP;
            return true;
        case SDL_SCANCODE_Q:
            key = CameraManipulator::Key::DOWN;
            return true;
        default:
            return false;
    }
}

void GameDriver::CView::keyUp(SDL_Scancode scancode) {
    if (mCameraManipulator) {
        CameraManipulator::Key key;
        if (manipulatorKeyFromKeycode(scancode, key)) {
            mCameraManipulator->keyUp(key);
        }
    }
}

void GameDriver::CView::keyDown(SDL_Scancode scancode) {
    if (mCameraManipulator) {
        CameraManipulator::Key key;
        if (manipulatorKeyFromKeycode(scancode, key)) {
            mCameraManipulator->keyDown(key);
        }
    }
}

bool GameDriver::CView::intersects(ssize_t x, ssize_t y) {
    if (x >= mViewport.left && x < mViewport.left + mViewport.width)
        if (y >= mViewport.bottom && y < mViewport.bottom + mViewport.height)
            return true;

    return false;
}

void GameDriver::CView::setCameraManipulator(CameraManipulator* cm) {
    mCameraManipulator = cm;
}

void GameDriver::CView::setCamera(Camera* camera) {
    view->setCamera(camera);
}

void GameDriver::GodView::setGodCamera(Camera* camera) {
    getView()->setDebugCamera(camera);
}