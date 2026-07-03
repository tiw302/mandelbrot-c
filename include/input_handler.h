#ifndef INPUT_HANDLER_H
#define INPUT_HANDLER_H

#include <stdint.h>
#include "app_state.h"

// unified event types to bridge SDL and Sokol
typedef enum {
    INPUT_KEY_DOWN,
    INPUT_KEY_UP,
    INPUT_MOUSE_DOWN,
    INPUT_MOUSE_UP,
    INPUT_MOUSE_MOVE,
    INPUT_MOUSE_SCROLL
} InputEventType;

// common key codes
typedef enum {
    KEY_UNKNOWN,
    KEY_ESCAPE,
    KEY_Q,
    KEY_H,
    KEY_Z,
    KEY_R,
    KEY_P,
    KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9,
    KEY_UP,
    KEY_DOWN,
    KEY_N,
    KEY_E,
    KEY_G,
    KEY_J,
    KEY_B, KEY_F,
    KEY_S,
    KEY_X,
    KEY_V,
    KEY_M,
    KEY_L,
    KEY_T,
    KEY_LEFT_BRACKET,
    KEY_RIGHT_BRACKET,
    KEY_F5
} InputKey;

typedef struct {
    InputEventType type;
    InputKey key;
    int mod_shift;
    int mod_ctrl;
    int mouse_x, mouse_y;
    int mouse_btn; // 1 = left, 3 = right
    double scroll_y;
} AppInputEvent;

typedef enum {
    ACTION_NONE = 0,
    ACTION_QUIT = 1,
    ACTION_RELOAD_SHADERS = 2,
    ACTION_MEGA_SCREENSHOT = 3,
    ACTION_TOGGLE_VIDEO = 4,
    ACTION_RESIZE_THREADS_UP = 5,
    ACTION_RESIZE_THREADS_DOWN = 6,
    ACTION_TOGGLE_GPU = 7,
    ACTION_TOGGLE_PRECISION = 8,
    ACTION_TOGGLE_PERTURBATION = 9
} InputAction;

// handles the common event, modifies AppCommonState, and returns any specific action the backend needs to perform
InputAction app_handle_input(AppCommonState* state, const AppInputEvent* event, uint32_t now);

#endif // INPUT_HANDLER_H
