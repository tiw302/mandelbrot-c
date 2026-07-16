/* headless_egl.c
 *
 * implementation of headless egl context setup.
 * configures an opengl 3.3 core profile via an egl pbuffer surface.
 */

#include "headless_egl.h"
#include <stdio.h>

#ifdef __linux__
#include <EGL/egl.h>

// initializes egl and sets it up as the current rendering context
int init_headless_egl(int width, int height) {
    EGLDisplay egl_dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (!egl_dpy || !eglInitialize(egl_dpy, NULL, NULL)) {
        fprintf(stderr, "error: egl initialization failed\n");
        return 0;
    }
    
    // request opengl api
    eglBindAPI(EGL_OPENGL_API);
    
    // configure rgb8 surface with depth 8 (offscreen pbuffer)
    EGLint config_attribs[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8, EGL_DEPTH_SIZE, 8, EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, EGL_NONE };
    EGLConfig egl_cfg; 
    EGLint num_configs;
    if (!eglChooseConfig(egl_dpy, config_attribs, &egl_cfg, 1, &num_configs)) return 0;
    
    // create the offscreen surface
    EGLint pbuffer_attribs[] = { EGL_WIDTH, width, EGL_HEIGHT, height, EGL_NONE };
    EGLSurface egl_surf = eglCreatePbufferSurface(egl_dpy, egl_cfg, pbuffer_attribs);
    
    // request opengl 3.3 core profile
    EGLint ctx_attribs[] = { EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 3, EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE };
    EGLContext egl_ctx = eglCreateContext(egl_dpy, egl_cfg, EGL_NO_CONTEXT, ctx_attribs);
    
    // make the context current
    if (!eglMakeCurrent(egl_dpy, egl_surf, egl_surf, egl_ctx)) return 0;
    
    return 1;
}
#endif
