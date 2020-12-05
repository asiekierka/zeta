/**
 * Copyright (c) 2018, 2019, 2020 Adrian Siekierka
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

#ifndef __TYPES_H__
#define __TYPES_H__

#include <stdbool.h>

#ifdef __EMSCRIPTEN__
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

#if !defined(_3DS)
typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef signed long long s64;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
#else
#include <3ds.h>
#endif

#endif /* __TYPES_H__ */
