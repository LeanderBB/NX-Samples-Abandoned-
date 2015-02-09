
#include "gltutapp.h"
#include <nx/media/nx3dmodel.h>
#include <nx/ogl/nxoglprogram.h>
#include <nx/ogl/nxoglinternal.h>
#include <nx/gpu/nxgpuprogramsource.h>
#include <nx/io/nxiobase.h>
#include <nx/sys/nxinputctx.h>
#include <nx/util/nxbitarray.h>
#include <nx/math/nx3dmath.h>
#include <nx/math/nxtransform.h>
#include <nx/sys/nxwindow.h>
#include <nx/resource/nxgpuprogramresource.h>
#include <nx/resource/nx3dmodelresource.h>
#include <nx/resource/nxgpumeshresource.h>

using namespace nx;


#define USE_NX_TRANSFORM
struct CameraState
{
    float near = 0.1f;
    float far = 100.0f;
    float fovy = 67.0f;
    float aspect = 0.0f;
    float cam_speed = 3.0f;
    float cam_heading_speed = 50.0f;
    float cam_heading = 0.0f;
#if !defined(USE_NX_TRANSFORM)
    glm::quat quaternion;
    glm::vec4 fwd = glm::vec4 (0.0f, 0.0f, -1.0f, 0.0f);
    glm::vec4 rgt = glm::vec4 (1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec4 up = glm::vec4 (0.0f, 1.0f, 0.0f, 0.0f);
#endif
    glm::mat4 modelMats[4];
};

class GLTutCamera : public GLTutApp, public NXInputCtx
{
public:
    GLTutCamera():
        GLTutApp("camera", "glcamera"),
        NXInputCtx("camera_input_ctx", 0),
        _viewMat(),
        _projMat(),
    #if !defined(USE_NX_TRANSFORM)
        _camPos(0.0f, 0.0f, 5.0f),
    #endif
        _speherePosWorld(),
        _spehereColor(),
        _hdlModel(),
        _program()

    {
        _speherePosWorld[0] = glm::vec3(-2.0f, 0.0f, 0.0f);
        _speherePosWorld[1] = glm::vec3(2.0f, 0.0f, 0.0f);
        _speherePosWorld[2] = glm::vec3(2.0f, 0.0f, -2.0f);
        _speherePosWorld[3] = glm::vec3(1.5f, 1.0f, -1.0f);

        _spehereColor[0] = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
        _spehereColor[1] = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f);
        _spehereColor[2] = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
        _spehereColor[3] = glm::vec4(1.0f, 0.0f, 1.0f, 1.0f);

#if defined(USE_NX_TRANSFORM)
        _transform.translate(glm::vec3(0.0f, 0.0f, 5.0f));
#endif
    }

    void onResize(const int w, const int h) NX_CPP_OVERRIDE
    {
        nx::NXLog("Resize %dx%d", w, h);

        _camState.aspect = (float)w/(float)h;
        _projMat = glm::perspective(_camState.fovy, _camState.aspect, _camState.near, _camState.far);
        glUniformMatrix4fv (_uniformProjLoc, 1, GL_FALSE, glm::value_ptr(_projMat));
        glViewport (0, 0, w, h);
    }


    void doInit() NX_CPP_OVERRIDE
    {
        // load shaders

        NXHdl hdl_prog = _mediaManager.create("prog","programs/camera.nxprog");
        if (!hdl_prog.valid())
        {
            quit();
            return;
        }

        _mediaManager.load(hdl_prog);
        if (!_mediaManager.isLoaded(hdl_prog))
        {
            quit();
            return;
        }

        _program = _mediaManager.get(hdl_prog);

        _hdlModel = _mediaManager.create("model","models/sphere.nx3d");
        if (!_hdlModel.valid())
        {
            quit();
            return;
        }

        _mediaManager.load(_hdlModel);
        if (!_mediaManager.isLoaded(_hdlModel))
        {
            quit();
            return;
        }

        NXHdl gpuhdl = _program->gpuHdl();

        // locate uniforms
        _uniformColorLoc = _pGPUInterface->uniformLocation(gpuhdl, "sphere_color");
        _uniformModelLoc = _pGPUInterface->uniformLocation(gpuhdl, "model");
        _uniformViewLoc = _pGPUInterface->uniformLocation(gpuhdl, "view");
        _uniformProjLoc = _pGPUInterface->uniformLocation(gpuhdl, "proj");


        // setup camera info
        _camState.aspect = (float)windowWidth()/(float)windowHeight();
        _projMat = glm::perspective(glm::radians(_camState.fovy), _camState.aspect, _camState.near, _camState.far);

        const glm::mat4 tmp;

#if !defined(USE_NX_TRANSFORM)

        const glm::quat qtmp;
        glm::mat4 tran = glm::translate(tmp, -_camPos);

        // make a quaternion representing negated initial camera orientation

        _camState.quaternion = glm::rotate(qtmp, glm::radians(-_camState.cam_heading), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 rot;
        rot = glm::mat4_cast(_camState.quaternion);
        _viewMat = rot * tran;
#else

        glm::quat qtmp2 = glm::angleAxis(glm::radians(_camState.cam_heading), glm::vec3(0.0f, 1.0f, 0.0f));
        _transform.rotate(qtmp2);
        _viewMat = _transform.toMatixInv();
#endif

        for (int i = 0; i < 4; i++) {
            _camState.modelMats[i] = glm::translate (tmp, _speherePosWorld[i]);
        }

        _pGPUInterface->bindProgram(gpuhdl);
        glUniformMatrix4fv (_uniformViewLoc, 1, GL_FALSE, glm::value_ptr(_viewMat));
        glUniformMatrix4fv (_uniformProjLoc, 1, GL_FALSE, glm::value_ptr(_projMat));

        glEnable (GL_DEPTH_TEST); // enable depth-testing
        glDepthFunc (GL_LESS); // depth-testing interprets a smaller value as "closer"
        glEnable (GL_CULL_FACE); // cull face
        glCullFace (GL_BACK); // cull back face
        glFrontFace (GL_CCW); // set counter-clock-wise vertex order to mean the front
        glClearColor (0.2, 0.2, 0.2, 1.0); // grey background to help spot mistakes
        glViewport (0, 0, windowWidth(), windowHeight());

        memset(_keysPressed, 0, sizeof(_keysPressed));
        inputManager()->addInputCtx(this);

        system()->window()->setCaptureInput(true);
    }

    void doTerm() NX_CPP_OVERRIDE
    {
        inputManager()->remInputCtx(this);
        _program.reset();
    }



    void doRun(const double elapsedSec) NX_CPP_OVERRIDE
    {
        updateInput(elapsedSec);
        // wipe the drawing surface clear
        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        NXGPUMeshResourcePtr_t mesh = _mediaManager.get(_hdlModel);
        NXGPUSubMeshPtr_t sub_mesh = mesh->mesh()->submesh(0);
        _pGPUInterface->bindShaderInput(sub_mesh->gpuHdl());
        for (int i = 0; i < 4; i++) {
            glUniform4fv(_uniformColorLoc, 1, glm::value_ptr(_spehereColor[i]));
            glUniformMatrix4fv (_uniformModelLoc, 1, GL_FALSE, glm::value_ptr(_camState.modelMats[i]));

            glDrawElements(GL_TRIANGLES, sub_mesh->indexCount(), GL_UNSIGNED_INT, 0);
        }
    }



    virtual bool handleInputEvent(const NXInputEvent& pEvtData) NX_CPP_OVERRIDE
    {

        _deltax = _deltay = 0.0f;

        if (pEvtData.type == kInputEventTypeKey)
        {
            InputKey key = NXInputKey(pEvtData.evt.keyboard);
            if (NXInputKeyDown(pEvtData.evt.keyboard))
            {
                _keysPressed[key] = true;
            }
            else
            {
                _keysPressed[key] = false;
            }
        }

        if (pEvtData.type == kInputEventTypeMouseMove)
        {
            _deltax = pEvtData.evt.mouseMove.deltax;
            _deltay = pEvtData.evt.mouseMove.deltay;
        }
        return false;
    }

    void updateInput(const double elapsedSeconds)
    {
        // control keys
        bool cam_moved = false;
        glm::vec3 move (0.0, 0.0, 0.0);
        float cam_yaw = 0.0f; // y-rotation in degrees
        float cam_pitch = 0.0f;
        float cam_roll = 0.0;

        if (_keysPressed[kInputKeyA])
        {
            move.x -= _camState.cam_speed * elapsedSeconds;
            cam_moved = true;
        }
        if (_keysPressed[kInputKeyD])
        {
            move.x += _camState.cam_speed * elapsedSeconds;
            cam_moved = true;
        }
        if (_keysPressed[kInputKeyQ])
        {
            move.y += _camState.cam_speed * elapsedSeconds;
            cam_moved = true;
        }
        if (_keysPressed[kInputKeyE])
        {
            move.y -= _camState.cam_speed * elapsedSeconds;
            cam_moved = true;
        }
        if (_keysPressed[kInputKeyW])
        {
            move.z -= _camState.cam_speed * elapsedSeconds;
            cam_moved = true;
        }
        if (_keysPressed[kInputKeyS])
        {
            move.z += _camState.cam_speed * elapsedSeconds;
            cam_moved = true;
        }


        if (_keysPressed[kInputKeyLeft])
        {
            cam_yaw += _camState.cam_heading_speed * elapsedSeconds;
            cam_moved = true;

            // create a quaternion representing change in heading (the yaw)
#if !defined(USE_NX_TRANSFORM)
            _camState.quaternion = glm::rotate(_camState.quaternion,glm::radians(cam_yaw), glm::vec3(_camState.up.x, _camState.up.y, _camState.up.z));
#else
            _transform.rotate(glm::angleAxis(glm::radians(cam_yaw), _transform.up()));
#endif
        }
        if (_keysPressed[kInputKeyRight])
        {
            cam_yaw -= _camState.cam_heading_speed * elapsedSeconds;
            cam_moved = true;
#if !defined(USE_NX_TRANSFORM)
            _camState.quaternion = glm::rotate(_camState.quaternion, glm::radians(cam_yaw), glm::vec3(_camState.up.x, _camState.up.y, _camState.up.z));
#else
            _transform.rotate(glm::angleAxis(glm::radians(cam_yaw), _transform.up()));
#endif
        }
        if (_keysPressed[kInputKeyUp]) {
            cam_pitch += _camState.cam_heading_speed * elapsedSeconds;
            cam_moved = true;
#if !defined(USE_NX_TRANSFORM)
            _camState.quaternion = glm::rotate(_camState.quaternion, glm::radians(cam_pitch), glm::vec3(_camState.rgt.x, _camState.rgt.y, _camState.rgt.z));
#else
            _transform.rotate(glm::angleAxis(glm::radians(cam_pitch), _transform.right()));
#endif
        }
        if (_keysPressed[kInputKeyDown]) {
            cam_pitch -= _camState.cam_heading_speed * elapsedSeconds;
            cam_moved = true;
#if !defined(USE_NX_TRANSFORM)
            _camState.quaternion = glm::rotate(_camState.quaternion, glm::radians(cam_pitch), glm::vec3(_camState.rgt.x, _camState.rgt.y, _camState.rgt.z));
#else
            _transform.rotate(glm::angleAxis(glm::radians(cam_pitch), _transform.right()));
#endif
        }
        if (_keysPressed[kInputKeyZ]) {
            cam_roll -= _camState.cam_heading_speed * elapsedSeconds;
            cam_moved = true;
#if !defined(USE_NX_TRANSFORM)
            _camState.quaternion = glm::rotate(_camState.quaternion, glm::radians(cam_roll), glm::vec3(_camState.fwd.x, _camState.fwd.y, _camState.fwd.z));
#else
            _transform.rotate(glm::angleAxis(glm::radians(cam_roll), _transform.forward()));
#endif
        }
        if (_keysPressed[kInputKeyC])
        {
            cam_roll += _camState.cam_heading_speed * elapsedSeconds;
            cam_moved = true;
#if !defined(USE_NX_TRANSFORM)
            _camState.quaternion = glm::rotate(_camState.quaternion, glm::radians(cam_roll), glm::vec3(_camState.fwd.x, _camState.fwd.y, _camState.fwd.z));
#else
            _transform.rotate(glm::angleAxis(glm::radians(cam_roll), _transform.forward()));
#endif
        }

        // update mouse
        if (_deltax != 0.0f || _deltay != 0.0f)
        {
            cam_moved = true;
            cam_yaw -= 5.0f * elapsedSeconds * _deltax;
            cam_pitch += 5.0f * elapsedSeconds * _deltay;

            glm::quat cam_mouse_rot;
#if !defined(USE_NX_TRANSFORM)
            cam_mouse_rot = glm::angleAxis(glm::radians(cam_yaw), glm::vec3(_camState.up.x, _camState.up.y, _camState.up.z));
            cam_mouse_rot *= glm::angleAxis(glm::radians(cam_pitch), glm::vec3(_camState.rgt.x, _camState.rgt.y, _camState.rgt.z));
            _camState.quaternion *= cam_mouse_rot;
#else
            cam_mouse_rot = glm::angleAxis(glm::radians(cam_pitch), _transform.right());
            cam_mouse_rot *= glm::angleAxis(glm::radians(cam_yaw), _transform.up());

            _transform.rotate(cam_mouse_rot);
#endif
            _deltax = 0.0f;
            _deltay = 0.0f;
        }

        // update view matrix
        if (cam_moved) {
            // re-calculate local axes so can move fwd in dir cam is pointing
#if !defined(USE_NX_TRANSFORM)
            glm::mat4 R;
            R = glm::mat4_cast(_camState.quaternion);
            _camState.fwd = R * glm::vec4 (0.0, 0.0, -1.0, 0.0);
            _camState.rgt = R * glm::vec4 (1.0, 0.0, 0.0, 0.0);
            _camState.up = R * glm::vec4 (0.0, 1.0, 0.0, 0.0);

            _camPos = _camPos + glm::vec3 (_camState.fwd) * -move.z;
            _camPos = _camPos + glm::vec3 (_camState.up) * move.y;
            _camPos = _camPos + glm::vec3 (_camState.rgt) * move.x;
            glm::mat4 tmp;
            glm::mat4 T = glm::translate (tmp, glm::vec3 (_camPos));

            _viewMat = glm::inverse (R) * glm::inverse (T);
#else
            glm::vec3 up = _transform.up();
            glm::vec3 right = _transform.right();
            glm::vec3 forward = _transform.forward();

            _transform.translate(forward * -move.z);
            _transform.translate(right * move.x);
            _transform.translate(up * move.y);

            _viewMat = _transform.toMatixInv();
#endif
            glUniformMatrix4fv (_uniformViewLoc, 1, GL_FALSE, glm::value_ptr(_viewMat));
        }
    }



protected:
    glm::mat4 _viewMat;
    glm::mat4 _projMat;
#if !defined(USE_NX_TRANSFORM)
    glm::vec3 _camPos;
#endif
    glm::vec3 _speherePosWorld[4];
    glm::vec4 _spehereColor[4];
    NXHdl _hdlModel;
    NXGPUProgramResourcePtr_t _program;

    int _uniformColorLoc;
    int _uniformModelLoc;
    int _uniformProjLoc;
    int _uniformViewLoc;

    CameraState _camState;
    bool _keysPressed[kInputKeyTotal];
    float _deltax, _deltay;
#if defined(USE_NX_TRANSFORM)
    NXTransform _transform;
#endif

};



GLTUTAPP(GLTutCamera)
