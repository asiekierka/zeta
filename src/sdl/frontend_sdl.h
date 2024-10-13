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

#ifndef __FRONTEND_SDL_H__
#define __FRONTEND_SDL_H__

#include "../types.h"
#include <SDL3/SDL.h>
#ifdef USE_OPENGL
#ifdef USE_OPENGL_ES
#include <SDL3/SDL_opengles.h>
#else
#include <SDL3/SDL_opengl.h>
#endif
#endif

enum {
    BLINK_MODE_NONE = 0,
    BLINK_MODE_1,
    BLINK_MODE_2
};

typedef struct {
	int w, h;
} sdl_render_size;

typedef struct {
    int (*init)(const char*, int, int);
    void (*deinit)(void);
    void (*update_charset)(int, int, u8*);
    void (*update_palette)(u32*);
    void (*update_vram)(u8*);
    void (*draw)(u8*, int);
    SDL_Window *(*get_window)(void);
    sdl_render_size (*get_render_size)(void);
} sdl_renderer;

// provided by frontend.c

#define AREA_WITHOUT_SCALE 1

void calc_render_area(SDL_FRect *rect, int w, int h, int *scale_out, int flags);

#endif /* __FRONTEND_SDL_H__ */
