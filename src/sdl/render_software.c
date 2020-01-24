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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../zzt.h"
#include "../render_software.h"
#include "frontend_sdl.h"

static u8* charset_update_data = NULL;
static u32* palette_update_data = NULL;

static SDL_Texture *playfieldtex = NULL;
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static int charw, charh;
#ifdef BIG_ENDIAN
static int pformat = SDL_PIXELFORMAT_ARGB32;
#else
static int pformat = SDL_PIXELFORMAT_BGRA32;
#endif

static int sdl_render_software_init(void) { 
	window = SDL_CreateWindow("Zeta", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		640, 350, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (window == NULL) {
        return -1;
    }

	SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");

	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL) {
        return -1;
    }

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    return 0;
}

static void sdl_render_software_deinit(void) {
    if (playfieldtex != NULL) {
        SDL_DestroyTexture(playfieldtex);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
}

static void sdl_render_software_update_charset(int charw_arg, int charh_arg, u8 *data_arg) {
    charset_update_data = data_arg;

    if (charw != charw_arg || charh != charh_arg) {
        charw = charw_arg;
        charh = charh_arg;

        if (playfieldtex != NULL) {
            SDL_DestroyTexture(playfieldtex);
        }
        
        playfieldtex = SDL_CreateTexture(renderer, pformat, SDL_TEXTUREACCESS_STREAMING, 80*charw, 25*charh);
    }
}

static void sdl_render_software_update_palette(u32 *data_arg) {
    palette_update_data = data_arg;
}

static void sdl_render_software_draw(u8 *vram, int blink_mode) {
	SDL_Rect dest;
	void *buffer;
	int w, h, pitch;

	int swidth = (zzt_video_mode() & 2) ? 80 : 40;
	int sflags = 0;

	if (palette_update_data == NULL || charset_update_data == NULL) {
		return;
	}

    switch (blink_mode) {
        case BLINK_MODE_NONE:
            sflags |= RENDER_BLINK_OFF;
            break;
        case BLINK_MODE_2:
            sflags |= RENDER_BLINK_PHASE;
            break;
    }

	SDL_GetRendererOutputSize(renderer, &w, &h);
	calc_render_area(&dest, w, h, NULL, 0);

	SDL_LockTexture(playfieldtex, NULL, &buffer, &pitch);
	render_software_rgb(
		buffer,
		swidth, pitch / 4, sflags,
		vram, charset_update_data,
		charw, charh,
		palette_update_data
	);
	SDL_UnlockTexture(playfieldtex);

	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, playfieldtex, NULL, &dest);

    SDL_RenderPresent(renderer);
}

static SDL_Window *sdl_render_software_get_window(void) {
    return window;
}

static void sdl_render_software_update_vram(u8 *vram) {
    // pass
}

sdl_renderer sdl_renderer_software = {
    sdl_render_software_init,
    sdl_render_software_deinit,
    sdl_render_software_update_charset,
    sdl_render_software_update_palette,
    sdl_render_software_update_vram,
    sdl_render_software_draw,
    sdl_render_software_get_window
};