/* headless_egl.h
 *
 * provides an offscreen/headless opengl context via egl for linux environments.
 * this allows sokol_gfx to render without a physical window display.
 */

#pragma once

#ifdef __linux__
// initializes a hidden egl context (pbuffer) with the specified dimensions
int init_headless_egl(int width, int height);
#else
// stub for non-linux platforms
static inline int init_headless_egl(int width, int height) {
    return 0;
}
#endif
