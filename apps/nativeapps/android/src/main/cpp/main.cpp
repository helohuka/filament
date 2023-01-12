/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

// BEGIN_INCLUDE(all)
#include <EGL/egl.h>
#include <GLES/gl.h>
#include <android/log.h>
#include <android/sensor.h>
#include <android_native_app_glue.h>
#include <jni.h>

#include <cassert>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <memory>

#include <filament/Camera.h>
#include <filament/Engine.h>
#include <filament/Renderer.h>
#include <filament/IndexBuffer.h>
#include <filament/Material.h>
#include <filament/MaterialInstance.h>
#include <filament/RenderableManager.h>
#include <filament/Scene.h>
#include <filament/Skybox.h>
#include <filament/TransformManager.h>
#include <filament/VertexBuffer.h>
#include <filament/View.h>
#include <filament/Viewport.h>
#include <utils/EntityManager.h>
#include <camutils/Manipulator.h>

#include "generated/resources/resources.h"

#include <cmath>
#include <vector>
#define LOGI(...) \
  ((void)__android_log_print(ANDROID_LOG_INFO, "native-activity", __VA_ARGS__))
#define LOGW(...) \
  ((void)__android_log_print(ANDROID_LOG_WARN, "native-activity", __VA_ARGS__))

/**
 * Our saved state data.
 */
struct saved_state {
  float angle;
  int32_t x;
  int32_t y;
};

/**
 * Shared state for our app.
 */

using CameraManipulator = filament::camutils::Manipulator<float>;
class CView {

public:
    CView(filament::Renderer& renderer, std::string name);
    virtual ~CView();

    void setCameraManipulator(CameraManipulator* cm);
    void setViewport(filament::Viewport const& viewport);
    void setCamera(filament::Camera* camera);
    bool intersects(ssize_t x, ssize_t y);

    virtual void mouseDown(int button, ssize_t x, ssize_t y);
    virtual void mouseUp(ssize_t x, ssize_t y);
    virtual void mouseMoved(ssize_t x, ssize_t y);
    virtual void mouseWheel(ssize_t x);

    filament::View const* getView() const { return view; }
    filament::View* getView() { return view; }
    CameraManipulator* getCameraManipulator() { return mCameraManipulator; }

private:
    enum class Mode : uint8_t {
        NONE, ROTATE, TRACK
    };

    filament::Engine& engine;
    filament::Viewport mViewport;
    filament::View* view = nullptr;
    CameraManipulator* mCameraManipulator = nullptr;
    std::string mName;
};


// ------------------------------------------------------------------------------------------------

CView::CView(filament::Renderer& renderer, std::string name)
        : engine(*renderer.getEngine()), mName(name) {
    view = engine.createView();
    view->setName(name.c_str());
}

CView::~CView() {
    engine.destroy(view);
}

void CView::setViewport(filament::Viewport const& viewport) {
    mViewport = viewport;
    view->setViewport(viewport);
    if (mCameraManipulator) {
        mCameraManipulator->setViewport(viewport.width, viewport.height);
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



bool CView::intersects(ssize_t x, ssize_t y) {
    if (x >= mViewport.left && x < mViewport.left + mViewport.width)
        if (y >= mViewport.bottom && y < mViewport.bottom + mViewport.height)
            return true;

    return false;
}

void CView::setCameraManipulator(CameraManipulator* cm) {
    mCameraManipulator = cm;
}

void CView::setCamera(filament::Camera* camera) {
    view->setCamera(camera);
}


class GodView : public CView {
public:
    using CView::CView;
    void setGodCamera(filament::Camera* camera);
};

void GodView::setGodCamera(filament::Camera* camera) {
    getView()->setDebugCamera(camera);
}

struct App {
    filament::VertexBuffer* vb;
    filament::IndexBuffer* ib;
    filament::Material* mat;
    filament::Camera* cam;
    utils::Entity camera;
    filament::Skybox* skybox;
    utils::Entity renderable;
} app;


struct GameDriver {
  struct android_app* app;

  ASensorManager* sensorManager;
  const ASensor* accelerometerSensor;
  ASensorEventQueue* sensorEventQueue;

  filament::Engine* mEngine = nullptr;
  filament::Scene* mScene = nullptr;
  filament::SwapChain* mSwapChain = nullptr;
  filament::Renderer* mRenderer = nullptr;

  CameraManipulator* mMainCameraMan;
  CameraManipulator* mDebugCameraMan;

  utils::Entity mCameraEntities[3];
  filament::Camera* mCameras[3] = { nullptr };
  filament::Camera* mMainCamera;
  filament::Camera* mDebugCamera;
  filament::Camera* mOrthoCamera;

  std::vector<std::unique_ptr<CView>> mViews;
  CView* mMainView;
  CView* mUiView;
  CView* mDepthView;
  GodView* mGodView;
  CView* mOrthoView;

  filament::Skybox* mSkyBox;

  int32_t width;
  int32_t height;
  struct saved_state state;
};


struct Vertex {
    filament::math::float2 position;
    uint32_t color;
};

static const Vertex TRIANGLE_VERTICES[3] = {
        {{1, 0}, 0xffff0000u},
        {{cos(M_PI * 2 / 3), sin(M_PI * 2 / 3)}, 0xff00ff00u},
        {{cos(M_PI * 4 / 3), sin(M_PI * 4 / 3)}, 0xff0000ffu},
};

static constexpr uint16_t TRIANGLE_INDICES[3] = { 0, 1, 2 };

/**
 * Initialize an EGL context for the current display.
 */
static int engine_init_display(struct GameDriver* engine) {
  engine->mEngine = filament::Engine::create();
  engine->mSwapChain = engine->mEngine->createSwapChain(engine->app->window);
  engine->mRenderer = engine->mEngine->createRenderer();

//  filament::Renderer::ClearOptions opt{};
//  opt.clearColor =  filament::math::float4(1,3,0,0.5);
//  opt.clear = true;
//  engine->mRenderer->setClearOptions(opt);

    // create cameras
    utils::EntityManager& em = utils::EntityManager::get();
    em.create(3,  engine->mCameraEntities);
    engine->mCameras[0] =  engine->mMainCamera =  engine->mEngine->createCamera( engine->mCameraEntities[0]);
    engine->mCameras[1] =  engine->mDebugCamera =  engine->mEngine->createCamera( engine->mCameraEntities[1]);
    engine->mCameras[2] =  engine->mOrthoCamera =  engine->mEngine->createCamera( engine->mCameraEntities[2]);

    // set exposure
    for (auto camera :  engine->mCameras) {
        camera->setExposure(16.0f, 1 / 125.0f, 100.0f);
    }

    // create views
    engine->mViews.emplace_back( engine->mMainView = new CView(* engine->mRenderer, "Main View"));

    engine->mViews.emplace_back( engine->mUiView = new CView(* engine->mRenderer, "UI View"));

    const uint32_t width = ANativeWindow_getWidth(engine->app->window);
    const uint32_t height = ANativeWindow_getHeight(engine->app->window);

    const filament::math::float3 at(0, 0, -4);
    const double ratio = double(height) / double(width);
    //const int sidebar = mFilamentApp->mSidebarWidth * dpiScaleX;

    //const bool splitview = mViews.size() > 2;

    // To trigger a floating-point exception, users could shrink the window to be smaller than
    // the sidebar. To prevent this we simply clamp the width of the main viewport.
    const uint32_t mainWidth = width;//splitview ? width : std::max(1, (int) width - sidebar);

    double near = 0.1;
    double far = 100;
  engine->mMainCamera->setLensProjection(10, double(mainWidth) / height, near, far);
  engine->mDebugCamera->setProjection(45.0, double(width) / height, 0.0625, 4096, filament::Camera::Fov::VERTICAL);


  engine->mMainView->setViewport({ 0, 0, mainWidth, height });

  engine->mUiView->setViewport({ 0, 0, width, height });

  engine->mScene = engine->mEngine->createScene();

  engine->mSkyBox = filament::Skybox::Builder().color({0.1, 0.125, 0.25, 1.0}).build(*engine->mEngine);

  engine->mScene->setSkybox(engine->mSkyBox);

  engine->mMainView->getView()->setPostProcessingEnabled(false);

  app.skybox =  filament::Skybox::Builder().color({0.1, 0.125, 0.25, 1.0}).build(*engine->mEngine);
  engine->mScene->setSkybox(app.skybox);
  engine->mMainView->getView()->setPostProcessingEnabled(false);
  static_assert(sizeof(Vertex) == 12, "Strange vertex size.");
  app.vb =  filament::VertexBuffer::Builder()
          .vertexCount(3)
          .bufferCount(1)
          .attribute( filament::VertexAttribute::POSITION, 0,  filament::VertexBuffer::AttributeType::FLOAT2, 0, 12)
          .attribute( filament::VertexAttribute::COLOR, 0,  filament::VertexBuffer::AttributeType::UBYTE4, 8, 12)
          .normalized( filament::VertexAttribute::COLOR)
          .build(*engine->mEngine);
  app.vb->setBufferAt(*engine->mEngine, 0,
                      filament::VertexBuffer::BufferDescriptor(TRIANGLE_VERTICES, 36, nullptr));
  app.ib =  filament::IndexBuffer::Builder()
          .indexCount(3)
          .bufferType( filament::IndexBuffer::IndexType::USHORT)
          .build(*engine->mEngine);
  app.ib->setBuffer(*engine->mEngine,
                    filament::IndexBuffer::BufferDescriptor(TRIANGLE_INDICES, 6, nullptr));
  app.mat =  filament::Material::Builder()
          .package(RESOURCES_BAKEDCOLOR_DATA, RESOURCES_BAKEDCOLOR_SIZE)
          .build(*engine->mEngine);
  app.renderable = utils::EntityManager::get().create();
  filament::RenderableManager::Builder(1)
          .boundingBox({{ -1, -1, -1 }, { 1, 1, 1 }})
          .material(0, app.mat->getDefaultInstance())
          .geometry(0,  filament::RenderableManager::PrimitiveType::TRIANGLES, app.vb, app.ib, 0, 3)
          .culling(false)
          .receiveShadows(false)
          .castShadows(false)
          .build(*engine->mEngine, app.renderable);
  engine->mScene->addEntity(app.renderable);
  app.camera = utils::EntityManager::get().create();
  app.cam = engine->mEngine->createCamera(app.camera);
  engine->mMainView->setCamera(app.cam);

  return 0;
}

/**
 * Just the current frame in the display.
 */
static void engine_draw_frame(struct GameDriver* engine) {

  if (engine->mRenderer->beginFrame( engine->mSwapChain )) {
    LOGI("static void engine_draw_frame(struct GameDriver* engine) {");
    engine->mRenderer->render(engine->mMainView->getView());
    engine->mRenderer->endFrame();
  }
}

/**
 * Tear down the EGL context currently associated with the display.
 */
static void engine_term_display(struct GameDriver* engine) {

}

/**
 * Process the next input event.
 */
static int32_t engine_handle_input(struct android_app* app,
                                   AInputEvent* event) {
  auto* engine = (struct GameDriver*)app->userData;
  if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
    engine_draw_frame(engine);
    engine->state.x = AMotionEvent_getX(event, 0);
    engine->state.y = AMotionEvent_getY(event, 0);
    return 1;
  }
  return 0;
}

/**
 * Process the next main command.
 */
static void engine_handle_cmd(struct android_app* app, int32_t cmd) {
  auto* engine = (struct GameDriver*)app->userData;
  switch (cmd) {
    case APP_CMD_SAVE_STATE:
      // The system has asked us to save our current state.  Do so.
      engine->app->savedState = malloc(sizeof(struct saved_state));
      *((struct saved_state*)engine->app->savedState) = engine->state;
      engine->app->savedStateSize = sizeof(struct saved_state);
      break;
    case APP_CMD_INIT_WINDOW:
      // The window is being shown, get it ready.
      if (engine->app->window != nullptr) {
        engine_init_display(engine);
        engine_draw_frame(engine);
      }
      break;
    case APP_CMD_TERM_WINDOW:
      // The window is being hidden or closed, clean it up.
      engine_term_display(engine);
      break;
    case APP_CMD_GAINED_FOCUS:
      // When our app gains focus, we start monitoring the accelerometer.
      if (engine->accelerometerSensor != nullptr) {
        ASensorEventQueue_enableSensor(engine->sensorEventQueue,
                                       engine->accelerometerSensor);
        // We'd like to get 60 events per second (in us).
        ASensorEventQueue_setEventRate(engine->sensorEventQueue,
                                       engine->accelerometerSensor,
                                       (1000L / 60) * 1000);
      }
      break;
    case APP_CMD_LOST_FOCUS:
      // When our app loses focus, we stop monitoring the accelerometer.
      // This is to avoid consuming battery while not being used.
      if (engine->accelerometerSensor != nullptr) {
        ASensorEventQueue_disableSensor(engine->sensorEventQueue,
                                        engine->accelerometerSensor);
      }
      // Also stop animating.

      engine_draw_frame(engine);
      break;
    default:
      break;
  }
}

/*
 * AcquireASensorManagerInstance(void)
 *    Workaround ASensorManager_getInstance() deprecation false alarm
 *    for Android-N and before, when compiling with NDK-r15
 */
#include <dlfcn.h>
ASensorManager* AcquireASensorManagerInstance(android_app* app) {
  if (!app) return nullptr;

  typedef ASensorManager* (*PF_GETINSTANCEFORPACKAGE)(const char* name);
  void* androidHandle = dlopen("libandroid.so", RTLD_NOW);
  auto getInstanceForPackageFunc = (PF_GETINSTANCEFORPACKAGE)dlsym(
      androidHandle, "ASensorManager_getInstanceForPackage");
  if (getInstanceForPackageFunc) {
    JNIEnv* env = nullptr;
    app->activity->vm->AttachCurrentThread(&env, nullptr);

    jclass android_content_Context = env->GetObjectClass(app->activity->clazz);
    jmethodID midGetPackageName = env->GetMethodID(
        android_content_Context, "getPackageName", "()Ljava/lang/String;");
    auto packageName =
        (jstring)env->CallObjectMethod(app->activity->clazz, midGetPackageName);

    const char* nativePackageName =
        env->GetStringUTFChars(packageName, nullptr);
    ASensorManager* mgr = getInstanceForPackageFunc(nativePackageName);
    env->ReleaseStringUTFChars(packageName, nativePackageName);
    app->activity->vm->DetachCurrentThread();
    if (mgr) {
      dlclose(androidHandle);
      return mgr;
    }
  }

  typedef ASensorManager* (*PF_GETINSTANCE)();
  auto getInstanceFunc =
      (PF_GETINSTANCE)dlsym(androidHandle, "ASensorManager_getInstance");
  // by all means at this point, ASensorManager_getInstance should be available
  assert(getInstanceFunc);
  dlclose(androidHandle);

  return getInstanceFunc();
}

/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, with its own
 * event loop for receiving input events and doing other things.
 */
extern "C" void android_main(struct android_app* state) {
  struct GameDriver engine {};

  memset(&engine, 0, sizeof(engine));
  state->userData = &engine;
  state->onAppCmd = engine_handle_cmd;
  state->onInputEvent = engine_handle_input;
  engine.app = state;

  // Prepare to monitor accelerometer
  engine.sensorManager = AcquireASensorManagerInstance(state);
  engine.accelerometerSensor = ASensorManager_getDefaultSensor(
      engine.sensorManager, ASENSOR_TYPE_ACCELEROMETER);
  engine.sensorEventQueue = ASensorManager_createEventQueue(
      engine.sensorManager, state->looper, LOOPER_ID_USER, nullptr, nullptr);

  if (state->savedState != nullptr) {
    // We are starting with a previous saved state; restore from it.
    engine.state = *(struct saved_state*)state->savedState;
  }

  // loop waiting for stuff to do.

  while (true) {
    // Read all pending events.
    int ident;
    int events;
    struct android_poll_source* source;

    // If not animating, we will block forever waiting for events.
    // If animating, we loop until all events are read, then continue
    // to draw the next frame of animation.
    while ((ident = ALooper_pollAll( 0 , nullptr, &events,
                                    (void**)&source)) >= 0) {
      // Process this event.
      if (source != nullptr) {
        source->process(state, source);
      }

      // If a sensor has data, process it now.
      if (ident == LOOPER_ID_USER) {
        if (engine.accelerometerSensor != nullptr) {
          ASensorEvent event;
          while (ASensorEventQueue_getEvents(engine.sensorEventQueue, &event,
                                             1) > 0) {

          }
        }
      }

      // Check if we are exiting.
      if (state->destroyRequested != 0) {
        engine_term_display(&engine);
        return;
      }
    }



      // Drawing is throttled to the screen update rate, so there
      // is no need to do timing here.
      //engine_draw_frame(&engine);

  }
}
// END_INCLUDE(all)
