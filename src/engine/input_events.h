// Minimal GLFW-compatible definitions used by ldt so ldt sources
// don't need to include the real <GLFW/glfw3.h>.
// The top-level application (examples) should still use real GLFW and
// pass the original GLFW integer codes into ldt event APIs.
#pragma once

// actions
#ifndef GLFW_PRESS
#define GLFW_PRESS 1
#endif
#ifndef GLFW_RELEASE
#define GLFW_RELEASE 0
#endif
#ifndef GLFW_REPEAT
#define GLFW_REPEAT 2
#endif

// common keycodes used by ldt (match GLFW values)
#ifndef GLFW_KEY_A
#define GLFW_KEY_A 65
#endif
#ifndef GLFW_KEY_BACKSPACE
#define GLFW_KEY_BACKSPACE 259
#endif
#ifndef GLFW_KEY_DELETE
#define GLFW_KEY_DELETE 261
#endif
#ifndef GLFW_KEY_LEFT
#define GLFW_KEY_LEFT 263
#endif
#ifndef GLFW_KEY_RIGHT
#define GLFW_KEY_RIGHT 262
#endif
#ifndef GLFW_KEY_UP
#define GLFW_KEY_UP 265
#endif
#ifndef GLFW_KEY_DOWN
#define GLFW_KEY_DOWN 264
#endif
#ifndef GLFW_KEY_ENTER
#define GLFW_KEY_ENTER 257
#endif

// standard keys
#ifndef GLFW_KEY_SPACE
#define GLFW_KEY_SPACE 32
#endif
#ifndef GLFW_KEY_UNKNOWN
#define GLFW_KEY_UNKNOWN -1
#endif
#ifndef GLFW_KEY_ESCAPE
#define GLFW_KEY_ESCAPE 256
#endif
#ifndef GLFW_KEY_TAB
#define GLFW_KEY_TAB 258
#endif
#ifndef GLFW_KEY_INSERT
#define GLFW_KEY_INSERT 260
#endif
#ifndef GLFW_KEY_PAGE_UP
#define GLFW_KEY_PAGE_UP 266
#endif
#ifndef GLFW_KEY_PAGE_DOWN
#define GLFW_KEY_PAGE_DOWN 267
#endif
#ifndef GLFW_KEY_HOME
#define GLFW_KEY_HOME 268
#endif
#ifndef GLFW_KEY_END
#define GLFW_KEY_END 269
#endif
#ifndef GLFW_KEY_CAPS_LOCK
#define GLFW_KEY_CAPS_LOCK 280
#endif
#ifndef GLFW_KEY_SCROLL_LOCK
#define GLFW_KEY_SCROLL_LOCK 281
#endif
#ifndef GLFW_KEY_NUM_LOCK
#define GLFW_KEY_NUM_LOCK 282
#endif
#ifndef GLFW_KEY_PRINT_SCREEN
#define GLFW_KEY_PRINT_SCREEN 283
#endif
#ifndef GLFW_KEY_PAUSE
#define GLFW_KEY_PAUSE 284
#endif

// letter keys commonly used
#ifndef GLFW_KEY_A
#define GLFW_KEY_A 65
#endif
#ifndef GLFW_KEY_B
#define GLFW_KEY_B 66
#endif
#ifndef GLFW_KEY_C
#define GLFW_KEY_C 67
#endif
#ifndef GLFW_KEY_D
#define GLFW_KEY_D 68
#endif
#ifndef GLFW_KEY_E
#define GLFW_KEY_E 69
#endif
#ifndef GLFW_KEY_F
#define GLFW_KEY_F 70
#endif
#ifndef GLFW_KEY_G
#define GLFW_KEY_G 71
#endif
#ifndef GLFW_KEY_H
#define GLFW_KEY_H 72
#endif
#ifndef GLFW_KEY_I
#define GLFW_KEY_I 73
#endif
#ifndef GLFW_KEY_J
#define GLFW_KEY_J 74
#endif
#ifndef GLFW_KEY_K
#define GLFW_KEY_K 75
#endif
#ifndef GLFW_KEY_L
#define GLFW_KEY_L 76
#endif
#ifndef GLFW_KEY_M
#define GLFW_KEY_M 77
#endif
#ifndef GLFW_KEY_N
#define GLFW_KEY_N 78
#endif
#ifndef GLFW_KEY_O
#define GLFW_KEY_O 79
#endif
#ifndef GLFW_KEY_P
#define GLFW_KEY_P 80
#endif
#ifndef GLFW_KEY_Q
#define GLFW_KEY_Q 81
#endif
#ifndef GLFW_KEY_R
#define GLFW_KEY_R 82
#endif
#ifndef GLFW_KEY_S
#define GLFW_KEY_S 83
#endif
#ifndef GLFW_KEY_T
#define GLFW_KEY_T 84
#endif
#ifndef GLFW_KEY_U
#define GLFW_KEY_U 85
#endif
#ifndef GLFW_KEY_V
#define GLFW_KEY_V 86
#endif
#ifndef GLFW_KEY_W
#define GLFW_KEY_W 87
#endif
#ifndef GLFW_KEY_X
#define GLFW_KEY_X 88
#endif
#ifndef GLFW_KEY_Y
#define GLFW_KEY_Y 89
#endif
#ifndef GLFW_KEY_Z
#define GLFW_KEY_Z 90
#endif

// number keys
#ifndef GLFW_KEY_0
#define GLFW_KEY_0 48
#endif
#ifndef GLFW_KEY_1
#define GLFW_KEY_1 49
#endif
#ifndef GLFW_KEY_2
#define GLFW_KEY_2 50
#endif
#ifndef GLFW_KEY_3
#define GLFW_KEY_3 51
#endif
#ifndef GLFW_KEY_4
#define GLFW_KEY_4 52
#endif
#ifndef GLFW_KEY_5
#define GLFW_KEY_5 53
#endif
#ifndef GLFW_KEY_6
#define GLFW_KEY_6 54
#endif
#ifndef GLFW_KEY_7
#define GLFW_KEY_7 55
#endif
#ifndef GLFW_KEY_8
#define GLFW_KEY_8 56
#endif
#ifndef GLFW_KEY_9
#define GLFW_KEY_9 57
#endif

// function keys
#ifndef GLFW_KEY_F1
#define GLFW_KEY_F1 290
#endif
#ifndef GLFW_KEY_F2
#define GLFW_KEY_F2 291
#endif
#ifndef GLFW_KEY_F3
#define GLFW_KEY_F3 292
#endif
#ifndef GLFW_KEY_F4
#define GLFW_KEY_F4 293
#endif
#ifndef GLFW_KEY_F5
#define GLFW_KEY_F5 294
#endif
#ifndef GLFW_KEY_F6
#define GLFW_KEY_F6 295
#endif
#ifndef GLFW_KEY_F7
#define GLFW_KEY_F7 296
#endif
#ifndef GLFW_KEY_F8
#define GLFW_KEY_F8 297
#endif
#ifndef GLFW_KEY_F9
#define GLFW_KEY_F9 298
#endif
#ifndef GLFW_KEY_F10
#define GLFW_KEY_F10 299
#endif
#ifndef GLFW_KEY_F11
#define GLFW_KEY_F11 300
#endif
#ifndef GLFW_KEY_F12
#define GLFW_KEY_F12 301
#endif

// modifier keys
#ifndef GLFW_KEY_LEFT_SHIFT
#define GLFW_KEY_LEFT_SHIFT 340
#endif
#ifndef GLFW_KEY_LEFT_CONTROL
#define GLFW_KEY_LEFT_CONTROL 341
#endif
#ifndef GLFW_KEY_LEFT_ALT
#define GLFW_KEY_LEFT_ALT 342
#endif
#ifndef GLFW_KEY_LEFT_SUPER
#define GLFW_KEY_LEFT_SUPER 343
#endif
#ifndef GLFW_KEY_RIGHT_SHIFT
#define GLFW_KEY_RIGHT_SHIFT 344
#endif
#ifndef GLFW_KEY_RIGHT_CONTROL
#define GLFW_KEY_RIGHT_CONTROL 345
#endif
#ifndef GLFW_KEY_RIGHT_ALT
#define GLFW_KEY_RIGHT_ALT 346
#endif
#ifndef GLFW_KEY_RIGHT_SUPER
#define GLFW_KEY_RIGHT_SUPER 347
#endif
#ifndef GLFW_KEY_MENU
#define GLFW_KEY_MENU 348
#endif

#ifndef GLFW_KEY_X
#define GLFW_KEY_X 88
#endif
#ifndef GLFW_KEY_Z
#define GLFW_KEY_Z 90
#endif
#ifndef GLFW_KEY_Y
#define GLFW_KEY_Y 89
#endif

// modifiers
#ifndef GLFW_MOD_SHIFT
#define GLFW_MOD_SHIFT 0x0001
#endif
#ifndef GLFW_MOD_CONTROL
#define GLFW_MOD_CONTROL 0x0002
#endif
#ifndef GLFW_MOD_ALT
#define GLFW_MOD_ALT 0x0004
#endif
#ifndef GLFW_MOD_SUPER
#define GLFW_MOD_SUPER 0x0008
#endif
#ifndef GLFW_MOD_CAPS_LOCK
#define GLFW_MOD_CAPS_LOCK 0x0010
#endif
#ifndef GLFW_MOD_NUM_LOCK
#define GLFW_MOD_NUM_LOCK 0x0020
#endif
