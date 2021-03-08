#ifndef __NBL_C_OPENGLES_LOGICAL_DEVICE_H_INCLUDED__
#define __NBL_C_OPENGLES_LOGICAL_DEVICE_H_INCLUDED__

#include "nbl/video/COpenGL_LogicalDevice.h"
#include "nbl/video/COpenGLESQueue.h"
#include "nbl/video/COpenGLESSwapchain.h"

namespace nbl {
namespace video
{

class COpenGLESLogicalDevice final : public COpenGL_LogicalDevice<COpenGLESQueue, COpenGLESSwapchain>
{
    using base_t = COpenGL_LogicalDevice<COpenGLESQueue, COpenGLESSwapchain>;

public:
    COpenGLESLogicalDevice(const egl::CEGL* _egl, FeaturesType* _features, EGLConfig config, EGLint major, EGLint minor, const SCreationParams& params) :
        base_t(_egl, EAT_OPENGL_ES, _features, config, major, minor, params)
    {

    }
};

}
}

#endif