#include "egl_headless.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <gbm.h>

#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

namespace gles
{
    namespace
    {
        bool hasExtension(const char *list, const char *name)
        {
            if (!list)
                return false;
            const size_t n = std::strlen(name);
            for (const char *p = std::strstr(list, name); p; p = std::strstr(p + 1, name))
                if ((p == list || p[-1] == ' ') && (p[n] == ' ' || p[n] == '\0'))
                    return true;
            return false;
        }
    }

    bool HeadlessContext::Create(std::string &error)
    {
        const char *clientExtensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
        auto getPlatformDisplay = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
        EGLDisplay dpy = EGL_NO_DISPLAY;
        if (getPlatformDisplay && hasExtension(clientExtensions, "EGL_MESA_platform_surfaceless"))
        {
            dpy = getPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);
            if (dpy != EGL_NO_DISPLAY && eglInitialize(dpy, nullptr, nullptr))
                description = "surfaceless (Mesa)";
            else
                dpy = EGL_NO_DISPLAY;
        }
        if (dpy == EGL_NO_DISPLAY)
        {
            // Never renderD128 by default: on the RK3326 it is rockchip-drm's display-only render node, and
            // eglInitialize with EGL_PLATFORM_GBM_KHR on it NULL-dereferences in the 4.4 kernel (Oops 96000005) and hangs
            // the device until a hard power-off (found by the Celosia R36S port, docs/r36s_port.md). SDL2 KMSDRM and
            // raylib DRM use the card nodes, which is what this tries. GLES_EGL_NODE=<path> forces one node.
            const char *forced = std::getenv("GLES_EGL_NODE");
            const bool useForced = forced && *forced;
            const char *nodes[2] = {useForced ? forced : "/dev/dri/card0", useForced ? nullptr : "/dev/dri/card1"};
            for (const char *node : nodes)
            {
                if (!node)
                    continue;
                drmFd = open(node, O_RDWR | O_CLOEXEC);
                if (drmFd < 0)
                    continue;
                gbmDevice = gbm_create_device(drmFd);
                if (gbmDevice)
                {
                    if (getPlatformDisplay && hasExtension(clientExtensions, "EGL_KHR_platform_gbm"))
                        dpy = getPlatformDisplay(EGL_PLATFORM_GBM_KHR, gbmDevice, nullptr);
                    else
                        dpy = eglGetDisplay(reinterpret_cast<EGLNativeDisplayType>(gbmDevice));
                    if (dpy != EGL_NO_DISPLAY && eglInitialize(dpy, nullptr, nullptr))
                    {
                        description = std::string("GBM on ") + node;
                        break;
                    }
                    dpy = EGL_NO_DISPLAY;
                    gbm_device_destroy(static_cast<gbm_device *>(gbmDevice));
                    gbmDevice = nullptr;
                }
                close(drmFd);
                drmFd = -1;
            }
        }
        if (dpy == EGL_NO_DISPLAY)
        {
            dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
            if (dpy == EGL_NO_DISPLAY || !eglInitialize(dpy, nullptr, nullptr))
            {
                error = "no EGL display (surfaceless, GBM and default all failed)";
                return false;
            }
            description = "default display";
        }
        display = dpy;
        if (!eglBindAPI(EGL_OPENGL_ES_API))
        {
            error = "eglBindAPI(GLES) failed";
            return false;
        }
        const bool surfaceless = hasExtension(eglQueryString(dpy, EGL_EXTENSIONS), "EGL_KHR_surfaceless_context");
        const EGLint configAttribs[] = {EGL_RENDERABLE_TYPE, 0x40 /* EGL_OPENGL_ES3_BIT */, EGL_SURFACE_TYPE,
                                        surfaceless ? 0 : EGL_PBUFFER_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8,
                                        EGL_BLUE_SIZE, 8, EGL_NONE};
        EGLConfig config = nullptr;
        EGLint count = 0;
        if (!eglChooseConfig(dpy, configAttribs, &config, 1, &count) || count == 0)
        {
            error = "no GLES 3 EGL config";
            return false;
        }
        const EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
        context = eglCreateContext(dpy, config, EGL_NO_CONTEXT, contextAttribs);
        if (context == EGL_NO_CONTEXT)
        {
            error = "eglCreateContext(GLES 3) failed";
            return false;
        }
        if (!surfaceless)
        {
            const EGLint pbufferAttribs[] = {EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE};
            surface = eglCreatePbufferSurface(dpy, config, pbufferAttribs);
            if (surface == EGL_NO_SURFACE)
            {
                error = "no surfaceless context and no pbuffer";
                return false;
            }
        }
        if (!eglMakeCurrent(dpy, surface ? surface : EGL_NO_SURFACE, surface ? surface : EGL_NO_SURFACE, context))
        {
            error = "eglMakeCurrent failed";
            return false;
        }
        description += surfaceless ? ", surfaceless context" : ", pbuffer";
        description += std::string(", ") + reinterpret_cast<const char *>(glGetString(GL_RENDERER)) + ", " +
                       reinterpret_cast<const char *>(glGetString(GL_VERSION));
        return true;
    }

    void HeadlessContext::Destroy()
    {
        if (display)
        {
            eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            if (surface)
                eglDestroySurface(display, surface);
            if (context)
                eglDestroyContext(display, context);
            eglTerminate(display);
        }
        if (gbmDevice)
            gbm_device_destroy(static_cast<gbm_device *>(gbmDevice));
        if (drmFd >= 0)
            close(drmFd);
        display = context = surface = gbmDevice = nullptr;
        drmFd = -1;
    }
}
