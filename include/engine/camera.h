/* camera.h
 *
 * viewport and camera movement logic.
 */

#ifndef CAMERA_H
#define CAMERA_H

#include <stdbool.h>

#include "config.h"    // for MAX_HISTORY_SIZE
#include "renderer.h"  // for ViewState

// rect structure avoiding SDL dependency
typedef struct {
    int x, y, w, h;
} CamRect;

typedef struct {
    ViewState view;
    ViewState history[MAX_HISTORY_SIZE];
    int history_count;

    int is_panning;
    int is_zooming;
    int last_mouse_x, last_mouse_y;
    int mouse_x, mouse_y;
    CamRect zoom_rect;

    int win_w, win_h;
} Camera;

// initialize a camera with given viewport dimensions
void camera_init(Camera* cam, int win_w, int win_h);

// update viewport dimensions
void camera_resize(Camera* cam, int win_w, int win_h);

// get complex coordinates for given screen coordinates
void camera_screen_to_complex(const Camera* cam, int screen_x, int screen_y, precise_float* re,
                              precise_float* im);

// mouse events
void camera_handle_wheel(Camera* cam, double y_delta, int mouse_x, int mouse_y);
void camera_handle_mouse_down(Camera* cam, int button, int x, int y);
void camera_handle_mouse_motion(Camera* cam, int x, int y);
bool camera_handle_mouse_up(Camera* cam,
                            int button);  // returns true if view changed (needs redraw)

// history management
void camera_push_history(Camera* cam);
bool camera_pop_history(Camera* cam);

// reset view to initial state
void camera_reset(Camera* cam);

#endif  // CAMERA_H
