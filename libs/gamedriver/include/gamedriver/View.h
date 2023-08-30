#ifndef __VIEW_H__
#define __VIEW_H__ 1

/*!
 * \file View.h
 * \date 2023.08.21
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

/*!
 * \class CView
 *
 * \ingroup Views
 *
 * \brief 
 *
 * TODO: long description
 *
 * \note 
 *
 * \author Helohuka
 *
 * \version 1.0
 *
 * \date 2023.08.15
 *
 * Contact: helohuka@outlook.com
 *
 */


using CameraManipulator = filament::camutils::Manipulator<float>;

class CView
{
public:
    CView(filament::Renderer& renderer, std::string name);
    virtual ~CView();

    void setCameraManipulator(CameraManipulator* cm);
    void setViewport(filament::Viewport const& viewport);
    void setCamera(filament::Camera* camera);
    void setGodCamera(filament::Camera* camera);
    bool intersects(ssize_t x, ssize_t y);

    void mouseDown(int button, ssize_t x, ssize_t y);
    void mouseUp(ssize_t x, ssize_t y);
    void mouseMoved(ssize_t x, ssize_t y);
    void mouseWheel(ssize_t x);
    void keyDown(SDL_Scancode scancode);
    void keyUp(SDL_Scancode scancode);

    filament::View const* getView() const { return mView; }
    filament::View*       getView() { return mView; }
    CameraManipulator*    getCameraManipulator() { return mCameraManipulator; }

private:
    enum class Mode : uint8_t
    {
        NONE,
        ROTATE,
        TRACK
    };

    filament::Engine&  mEngine;
    filament::Viewport mViewport;
    filament::View*    mView               = nullptr;
    CameraManipulator* mCameraManipulator = nullptr;
    std::string        mName;
};

#endif // __VIEW_H__
