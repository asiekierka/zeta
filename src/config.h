/**
 * Copyright (c) 2018, 2019, 2020, 2021 Adrian Siekierka
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __CONFIG_H__
#define __CONFIG_H__

// SDL port configuration

// 60/16 Hz (blink every 16 frames at 60 Hz)
#define ENABLE_AUDIO_WRITER
#define ENABLE_GIF_WRITER
#define ENABLE_SCREENSHOTS
#define USE_GETOPT
#define POSIX_VFS_SORTED_DIRS

#ifndef __EMSCRIPTEN__
#define HAS_DEVELOPER_MODE
#endif

// #define DEBUG_OPENGL

// Zeta emulator core configuration

#define IDLEHACK_MAX_PLAYER_CLONES 8
#define USE_EMS_EMULATION
#ifndef __EMSCRIPTEN__
#define USE_EMS_REALLOC
#endif
#define USE_ZETA_INTERRUPT_EXTENSIONS

// Zeta-preconfigured CPU core settings - do not touch!

#define MAX_INTQUEUE_SIZE 256

// required for accurate Zeta joystick support
#define CPU_CHECK_KEEP_GOING

//#define USE_8086_PUSH_SP_BUG
//#define USE_OPCODES_8086_ALIASED
//#define USE_OPCODES_8086_UNDOCUMENTED
//#define USE_OPCODES_80186
#define USE_OPCODES_DECIMAL
#define USE_OPCODES_SALC
#define USE_CPU_PARITY_FLAG

#include "config_build.h"

#endif /* __CONFIG_H__ */
