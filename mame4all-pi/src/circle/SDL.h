/* SDL.h - minimal SDL 1.2 compatibility for the Circle (bare-metal) port of MAME4ALL.
   Only the key codes, types and the few joystick calls used by the original Raspberry Pi
   code are provided; events come from circle_glue.cpp instead of SDL. */
#ifndef CIRCLE_SDL_SHIM_H
#define CIRCLE_SDL_SHIM_H

#include <stdint.h>

typedef uint8_t  Uint8;
typedef uint16_t Uint16;
typedef uint32_t Uint32;
typedef int16_t  Sint16;
typedef enum { SDL_FALSE = 0, SDL_TRUE = 1 } SDL_bool;

/* SDL 1.2 key symbols (same numeric values, so mame.cfg key settings keep working) */
typedef enum {
	SDLK_UNKNOWN = 0, SDLK_BACKSPACE = 8, SDLK_TAB = 9, SDLK_RETURN = 13, SDLK_PAUSE = 19,
	SDLK_ESCAPE = 27, SDLK_SPACE = 32, SDLK_QUOTE = 39, SDLK_LEFTPAREN = 40, SDLK_RIGHTPAREN = 41,
	SDLK_COMMA = 44, SDLK_MINUS = 45, SDLK_PERIOD = 46, SDLK_SLASH = 47,
	SDLK_0 = 48, SDLK_1, SDLK_2, SDLK_3, SDLK_4, SDLK_5, SDLK_6, SDLK_7, SDLK_8, SDLK_9,
	SDLK_COLON = 58, SDLK_SEMICOLON = 59, SDLK_EQUALS = 61,
	SDLK_LEFTBRACKET = 91, SDLK_BACKSLASH = 92, SDLK_RIGHTBRACKET = 93, SDLK_BACKQUOTE = 96,
	SDLK_a = 97, SDLK_b, SDLK_c, SDLK_d, SDLK_e, SDLK_f, SDLK_g, SDLK_h, SDLK_i, SDLK_j,
	SDLK_k, SDLK_l, SDLK_m, SDLK_n, SDLK_o, SDLK_p, SDLK_q, SDLK_r, SDLK_s, SDLK_t,
	SDLK_u, SDLK_v, SDLK_w, SDLK_x, SDLK_y, SDLK_z,
	SDLK_DELETE = 127,
	SDLK_KP0 = 256, SDLK_KP1, SDLK_KP2, SDLK_KP3, SDLK_KP4, SDLK_KP5, SDLK_KP6, SDLK_KP7,
	SDLK_KP8, SDLK_KP9, SDLK_KP_PERIOD, SDLK_KP_DIVIDE, SDLK_KP_MULTIPLY, SDLK_KP_MINUS,
	SDLK_KP_PLUS, SDLK_KP_ENTER, SDLK_KP_EQUALS,
	SDLK_UP = 273, SDLK_DOWN, SDLK_RIGHT, SDLK_LEFT, SDLK_INSERT, SDLK_HOME, SDLK_END,
	SDLK_PAGEUP, SDLK_PAGEDOWN,
	SDLK_F1 = 282, SDLK_F2, SDLK_F3, SDLK_F4, SDLK_F5, SDLK_F6, SDLK_F7, SDLK_F8, SDLK_F9,
	SDLK_F10, SDLK_F11, SDLK_F12,
	SDLK_NUMLOCK = 300, SDLK_CAPSLOCK, SDLK_SCROLLOCK, SDLK_RSHIFT, SDLK_LSHIFT, SDLK_RCTRL,
	SDLK_LCTRL, SDLK_RALT, SDLK_LALT,
	SDLK_LAST = 323
} SDLKey;

#define SDL_BUTTON_LEFT   1
#define SDL_BUTTON_MIDDLE 2
#define SDL_BUTTON_RIGHT  3

/* joysticks: index into the gamepads handled by the kernel */
typedef struct { int index; } SDL_Joystick;

#ifdef __cplusplus
extern "C" {
#endif
int cg_joy_axis(int joy, int axis);	/* -32768..32767 */
#ifdef __cplusplus
}
#endif

static inline Sint16 SDL_JoystickGetAxis(SDL_Joystick *j, int axis)
{
	return j ? (Sint16) cg_joy_axis(j->index, axis) : 0;
}

#endif
