
#include "gamedriver/BaseLibs.h"
#include "gamedriver/GameDriver.h"

// ------------------------------------------------------------------------------------------------

CView::CView(filament::Engine& engine, std::string name) :
    mRenderEngine(engine), mName(name)
{
    mView = mRenderEngine.createView();
    mView->setName(name.c_str());
}

CView::~CView() {
    mRenderEngine.destroy(mView);
}

void CView::setViewport(filament::Viewport const& viewport)
{
    mViewport = viewport;
    mView->setViewport(viewport);
    if (mCameraManipulator) {
        mCameraManipulator->setViewport(viewport.width, viewport.height);
    }
}

void CView::tick(double dt)
{
    filament::math::float3 eye, center, up;
    if (mCameraManipulator)
    {
        mCameraManipulator->update(dt);
        mCameraManipulator->getLookAt(&eye, &center, &up);
    }

    if (mCamera)
    {
        mCamera->lookAt(eye,center,up);
    }

    if (mGodCamera)
    {
        mGodCamera->lookAt(eye, center, up);
    }
}

void CView::mouseDown(int button, ssize_t x, ssize_t y) {
    if (mCameraManipulator) {
        mCameraManipulator->grabBegin(x, y, button == 3);
    }
}

void CView::mouseUp(ssize_t x, ssize_t y) {
    if (mCameraManipulator) {
        mCameraManipulator->grabEnd();
    }
}

void CView::mouseMoved(ssize_t x, ssize_t y) {
    if (mCameraManipulator) {
        mCameraManipulator->grabUpdate(x, y);
    }
}

void CView::mouseWheel(ssize_t x) {
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

void CView::keyUp(SDL_Scancode scancode) {
    if (mCameraManipulator) {
        CameraManipulator::Key key;
        if (GameDriver::manipulatorKeyFromKeycode(scancode, key))
        {
            mCameraManipulator->keyUp(key);
        }
    }
}

void CView::keyDown(SDL_Scancode scancode) {
    if (mCameraManipulator) {
        CameraManipulator::Key key;
        if (GameDriver::manipulatorKeyFromKeycode(scancode, key))
        {
            mCameraManipulator->keyDown(key);
        }
    }
}

bool CView::intersects(ssize_t x, ssize_t y) {
    if (x >= mViewport.left && x < mViewport.left + mViewport.width)
        if (y >= mViewport.bottom && y < mViewport.bottom + mViewport.height)
            return true;

    return false;
}

void CView::setCameraManipulator(CameraManipulator* cm) {
    mCameraManipulator = cm;
}

void CView::setCamera(filament::Camera* camera)
{
    mCamera = camera;
    mView->setCamera(camera);
}

void CView::setGodCamera(filament::Camera* camera)
{
    mGodCamera = camera;
    mView->setDebugCamera(camera);
}

