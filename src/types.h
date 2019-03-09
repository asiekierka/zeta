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

#ifndef __TYPES_H__
#define __TYPES_H__

#ifdef EMSCRIPTEN
#define USER_FUNCTION EMSCRIPTEN_KEEPALIVE
#define IMPLEMENT_FUNCTION
#include <emscripten.h>
#else
# ifdef __cplusplus
#define USER_FUNCTION extern "C"
#define IMPLEMENT_FUNCTION extern "C"
# else
#define USER_FUNCTION
#define IMPLEMENT_FUNCTION
# endif
#endif

typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

#endif /* __TYPES_H__ */
