#pragma once
// A GLES 3 context with no window: for measuring the renderer and reading frames back, not for showing them.
// Tries, in order: Mesa's surfaceless platform, a GBM device on a DRM node (the RK3326 Mali blob and Panfrost),
// then the default display. Uses a surfaceless context when EGL_KHR_surfaceless_context exists, else a pbuffer.
#include <string>

namespace gles
{
    struct HeadlessContext
    {
        bool Create(std::string &error);
        void Destroy();
        std::string description;
        void *display = nullptr;
        void *context = nullptr;
        void *surface = nullptr;
        void *gbmDevice = nullptr;
        int drmFd = -1;
    };
}
