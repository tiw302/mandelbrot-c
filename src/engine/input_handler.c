/* input_handler.c
 *
 * maps keyboard and mouse events to application actions.
 * translates platform-specific keycodes to unified internal inputs.
 */

#include "input_handler.h"
#include <stdio.h>
#include "config.h"

InputAction app_handle_input(AppCommonState* state, const AppInputEvent* event, uint32_t now) {
    if (!state || !event) return ACTION_NONE;

    if (event->type == INPUT_MOUSE_SCROLL) {
        camera_handle_wheel(&state->cam, event->scroll_y, state->cam.mouse_x, state->cam.mouse_y);
        state->needs_redraw = 1;
        return ACTION_NONE;
    } else if (event->type == INPUT_MOUSE_DOWN) {
        camera_handle_mouse_down(&state->cam, event->mouse_btn, event->mouse_x, event->mouse_y);
        return ACTION_NONE;
    } else if (event->type == INPUT_MOUSE_MOVE) {
        camera_handle_mouse_motion(&state->cam, event->mouse_x, event->mouse_y);
        if (state->cam.is_panning) {
            state->needs_redraw = 1;
        } else if (state->julia_mode && !state->cam.is_zooming) {
            // tracking mouse cursor for real-time julia set update
            app_state_get_mouse_coord(state, state->cam.mouse_x, state->cam.mouse_y,
                                      &state->julia_c.re, &state->julia_c.im);
            state->needs_redraw = 1;
        }
        return ACTION_NONE;
    } else if (event->type == INPUT_MOUSE_UP) {
        if (camera_handle_mouse_up(&state->cam, event->mouse_btn)) {
            state->needs_redraw = 1;
        }
        return ACTION_NONE;
    } else if (event->type == INPUT_KEY_DOWN) {
        switch (event->key) {
            case KEY_ESCAPE:
            case KEY_Q:
                return ACTION_QUIT;

            case KEY_H:
                state->show_help = !state->show_help;
                return ACTION_NONE;

            case KEY_Z:
                if (event->mod_ctrl) {
                    if (camera_pop_history(&state->cam)) {
                        state->needs_redraw = 1;
                        app_state_push_notification(state, "undo camera action", now);
                    }
                }
                return ACTION_NONE;

            case KEY_R:
                app_state_reset(state, NULL);
                app_state_push_notification(state, "view reset!", now);
                return ACTION_NONE;

            case KEY_P:
                app_state_cycle_palette(state);
                {
                    char buf[64];
                    snprintf(buf, sizeof(buf), "palette: %s", get_palette_name(state->palette_idx % get_palette_count()));
                    app_state_push_notification(state, buf, now);
                }
                return ACTION_NONE;

            case KEY_0: case KEY_1: case KEY_2: case KEY_3: case KEY_4:
            case KEY_5: case KEY_6: case KEY_7: case KEY_8: case KEY_9:
                state->palette_idx = event->key - KEY_0;
                state->needs_redraw = 1;
                {
                    char buf[64];
                    snprintf(buf, sizeof(buf), "palette: %s", get_palette_name(state->palette_idx % get_palette_count()));
                    app_state_push_notification(state, buf, now);
                }
                return ACTION_NONE;

            case KEY_UP:
            case KEY_DOWN: {
                int step = state->max_iterations / 10;
                if (step < 10) step = 10;
                if (event->mod_shift) step *= 10;
                state->max_iterations += (event->key == KEY_UP) ? step : -step;
                if (state->max_iterations < 10) state->max_iterations = 10;
                if (state->max_iterations > MAX_ITERATIONS_LIMIT)
                    state->max_iterations = MAX_ITERATIONS_LIMIT;
                init_color_palette(state->max_iterations, state->palette_idx);
                state->needs_redraw = 1;
                return ACTION_NONE;
            }

            case KEY_N:
                return ACTION_TOGGLE_PERTURBATION;

            case KEY_E:
                return ACTION_TOGGLE_PRECISION;

            case KEY_G:
                return ACTION_TOGGLE_GPU;

            case KEY_J:
                app_state_toggle_julia(state, NULL);
                app_state_push_notification(state, state->julia_mode ? "julia mode: active" : "julia mode: inactive", now);
                return ACTION_NONE;

            case KEY_F:
                app_state_cycle_fractal(state, NULL);
                app_state_push_notification(state, state->base_fractal == RENDER_BURNING_SHIP ? "burning ship mode" : (state->base_fractal == RENDER_TRICORN ? "tricorn mode" : (state->base_fractal == RENDER_CELTIC ? "celtic mode" : (state->base_fractal == RENDER_BUFFALO ? "buffalo mode" : "mandelbrot mode"))), now);
                return ACTION_NONE;

            case KEY_S:
                state->needs_redraw = 1;
                return ACTION_NONE; // the backend sets screenshot_requested itself, wait... we should let backend do it.

            case KEY_X:
                return ACTION_MEGA_SCREENSHOT;

            case KEY_V:
                return ACTION_TOGGLE_VIDEO;

            case KEY_M:
                app_state_save_bookmark(state);
                app_state_push_notification(state, "bookmark saved!", now);
                return ACTION_NONE;

            case KEY_L:
                app_state_load_next_bookmark(state);
                app_state_push_notification(state, "bookmark loaded!", now);
                return ACTION_NONE;

            case KEY_T:
                app_state_toggle_tour(state, now, NULL);
                if (state->julia_mode) {
                    app_state_push_notification(state, state->j_tour.phase != JULIA_TOUR_IDLE ? "julia tour started" : "tour stopped", now);
                } else {
                    app_state_push_notification(state, state->m_tour.phase != TOUR_IDLE ? (state->base_fractal == RENDER_BURNING_SHIP ? "burning ship tour started" : (state->base_fractal == RENDER_TRICORN ? "tricorn tour started" : (state->base_fractal == RENDER_CELTIC ? "celtic tour started" : (state->base_fractal == RENDER_BUFFALO ? "buffalo tour started" : "mandelbrot tour started")))) : "tour stopped", now);
                }
                return ACTION_NONE;

            case KEY_LEFT_BRACKET:
                return ACTION_RESIZE_THREADS_DOWN;

            case KEY_RIGHT_BRACKET:
                return ACTION_RESIZE_THREADS_UP;

            case KEY_F5:
                return ACTION_RELOAD_SHADERS;

            case KEY_I:
                return ACTION_TOGGLE_SETTINGS;

            default:
                break;
        }
    }

    return ACTION_NONE;
}
