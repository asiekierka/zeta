/**
 * Copyright (c) 2018, 2019 Adrian Siekierka
 *
 * This file is part of Zeta.
 *
 * Zeta is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Zeta is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Zeta.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __CONFIG_H__
#define __CONFIG_H__

// General configuration

//#define BIG_ENDIAN
#define UNALIGNED_OK

// SDL port configuration

#define ENABLE_SCREENSHOTS
#define USE_GETOPT
#define USE_OPENGL
// #define USE_OPENGL_ES /* GLES 1.1, not production ready */

// Zeta-preconfigured CPU core settings - do not touch!

#define MAX_INTQUEUE_SIZE 256

// required for accurate Zeta joystick support
#define CPU_CHECK_KEEP_GOING

//#define USE_8086_PUSH_SP_BUG
//#define USE_OPCODES_8086_ALIASED
//#define USE_OPCODES_8086_UNDOCUMENTED
//#define USE_OPCODES_80186
//#define USE_OPCODES_DECIMAL
//#define USE_OPCODES_SALC

#endif /* __CONFIG_H__ */
