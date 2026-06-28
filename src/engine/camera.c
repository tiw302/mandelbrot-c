/* camera.c
 *
 * viewport navigation, coordinate mapping, and zoom history.
 * tracks mouse movement to adjust center coordinates and zoom levels.
 */

#include "camera.h"

#include <math.h>

void camera_init(Camera* cam, int win_w, int win_h) {
    cam->view.center_re = -0.5;
    cam->view.center_im = 0.0;
    cam->view.zoom = 3.0;
    cam->history_count = 0;
    cam->is_panning = 0;
    cam->is_zooming = 0;
    cam->last_mouse_x = 0;
    cam->last_mouse_y = 0;
    cam->mouse_x = 0;
    cam->mouse_y = 0;
    cam->zoom_rect = (CamRect){0, 0, 0, 0};
    // clamp window size to prevent division by zero
    cam->win_w = win_w < 1 ? 1 : win_w;
    cam->win_h = win_h < 1 ? 1 : win_h;
}

void camera_resize(Camera* cam, int win_w, int win_h) {
    // clamp window size to prevent division by zero
    cam->win_w = win_w < 1 ? 1 : win_w;
    cam->win_h = win_h < 1 ? 1 : win_h;
}

void camera_screen_to_complex(const Camera* cam, int screen_x, int screen_y, precise_float* re,
                              precise_float* im) {
    precise_float aspect = (precise_float)cam->win_w / (precise_float)cam->win_h;
    precise_float im_min = cam->view.center_im - cam->view.zoom / 2.0;
    precise_float im_max = cam->view.center_im + cam->view.zoom / 2.0;
    precise_float re_min = cam->view.center_re - (cam->view.zoom * aspect) / 2.0;
    precise_float re_max = cam->view.center_re + (cam->view.zoom * aspect) / 2.0;

    *re = re_min + (precise_float)screen_x * (re_max - re_min) / cam->win_w;
    *im = im_max - (precise_float)screen_y * (im_max - im_min) / cam->win_h;
}

void camera_push_history(Camera* cam) {
    if (cam->history_count < MAX_HISTORY_SIZE) {
        cam->history[cam->history_count++] = cam->view;
    }
}

bool camera_pop_history(Camera* cam) {
    if (cam->history_count > 0) {
        cam->view = cam->history[--cam->history_count];
        return true;
    }
    return false;
}

void camera_reset(Camera* cam) {
    cam->view.center_re = -0.5;
    cam->view.center_im = 0.0;
    cam->view.zoom = 3.0;
    cam->history_count = 0;
}

void camera_handle_wheel(Camera* cam, double y_delta, int mouse_x, int mouse_y) {
    if (y_delta == 0.0) return;

    // zoom factor based on wheel delta
    double factor = pow(0.9, y_delta);

    camera_push_history(cam);

    precise_float aspect = (precise_float)cam->win_w / (precise_float)cam->win_h;
    precise_float offset_re = ((precise_float)mouse_x / cam->win_w - 0.5) * cam->view.zoom * aspect;
    precise_float offset_im = (0.5 - (precise_float)mouse_y / cam->win_h) * cam->view.zoom;

    cam->view.zoom *= factor;
    cam->view.center_re += offset_re * (1.0 - factor);
    cam->view.center_im += offset_im * (1.0 - factor);
}

// button 1 = left, 3 = right
void camera_handle_mouse_down(Camera* cam, int button, int x, int y) {
    if (button == 3) {
        cam->is_panning = 1;
        cam->last_mouse_x = x;
        cam->last_mouse_y = y;
    } else if (button == 1) {
        cam->is_zooming = 1;
        cam->zoom_rect = (CamRect){x, y, 0, 0};
    }
}

void camera_handle_mouse_motion(Camera* cam, int x, int y) {
    cam->mouse_x = x;
    cam->mouse_y = y;

    if (cam->is_panning) {
        precise_float aspect = (precise_float)cam->win_w / cam->win_h;
        cam->view.center_re -= (x - cam->last_mouse_x) * cam->view.zoom * aspect / cam->win_w;
        cam->view.center_im += (y - cam->last_mouse_y) * cam->view.zoom / cam->win_h;
        cam->last_mouse_x = x;
        cam->last_mouse_y = y;
    } else if (cam->is_zooming) {
        cam->zoom_rect.w = x - cam->zoom_rect.x;
        cam->zoom_rect.h = y - cam->zoom_rect.y;
    }
}

bool camera_handle_mouse_up(Camera* cam, int button) {
    if (button == 3) {
        cam->is_panning = 0;
        return true;
    } else if (button == 1) {
        if (cam->is_zooming && cam->zoom_rect.w != 0 && cam->zoom_rect.h != 0) {
            camera_push_history(cam);

            precise_float center_offset_x =
                ((precise_float)cam->zoom_rect.x + (precise_float)cam->zoom_rect.w / 2.0) /
                    cam->win_w -
                0.5;
            precise_float center_offset_y =
                0.5 - ((precise_float)cam->zoom_rect.y + (precise_float)cam->zoom_rect.h / 2.0) /
                          cam->win_h;

            precise_float aspect = (precise_float)cam->win_w / cam->win_h;
            cam->view.center_re += center_offset_x * cam->view.zoom * aspect;
            cam->view.center_im += center_offset_y * cam->view.zoom;

            cam->view.zoom =
                fmax(fabs((double)cam->zoom_rect.w) / cam->win_w * cam->view.zoom * aspect,
                     fabs((double)cam->zoom_rect.h) / cam->win_h * cam->view.zoom);
            cam->is_zooming = 0;
            return 1;
        }
        cam->is_zooming = 0;
    }
    return false;
}
