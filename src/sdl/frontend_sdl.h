/**
 * Copyright (c) 2018, 2019, 2020 Adrian Siekierka
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

#ifndef __FRONTEND_SDL_H__
#define __FRONTEND_SDL_H__

#include "../types.h"
#include <SDL2/SDL.h>
#ifdef USE_OPENGL
#ifdef USE_OPENGL_ES
#include <SDL2/SDL_opengles.h>
#else
#include <SDL2/SDL_opengl.h>
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
    int (*init)(void);
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

void calc_render_area(SDL_Rect *rect, int w, int h, int *scale_out, int flags);

#endif /* __FRONTEND_SDL_H__ */
