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

static int force_update;
static int last_blink_mode;

static int sdl_render_software_init(const char *window_name) {
	window = SDL_CreateWindow(window_name, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
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

	force_update = 1;
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

    force_update = 1;
}

static void sdl_render_software_update_palette(u32 *data_arg) {
    palette_update_data = data_arg;
    force_update = 1;
}

static void sdl_render_software_draw(u8 *vram, int blink_mode) {
	SDL_Rect dest;
	void *buffer;
	int w, h, pitch;

	int swidth = (zzt_video_mode() & 2) ? 80 : 40;
	int sflags = 0;
	int should_render = force_update || (blink_mode != last_blink_mode);

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

	if (should_render) {
		SDL_LockTexture(playfieldtex, NULL, &buffer, &pitch);
		render_software_rgb(
			buffer,
			swidth, pitch / 4, sflags,
			vram, charset_update_data,
			charw, charh,
			palette_update_data
		);
		SDL_UnlockTexture(playfieldtex);
	}

	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_RenderClear(renderer);
	SDL_RenderCopy(renderer, playfieldtex, NULL, &dest);

	SDL_RenderPresent(renderer);

	last_blink_mode = blink_mode;
	force_update = 0;
}

static SDL_Window *sdl_render_software_get_window(void) {
	return window;
}

static void sdl_render_software_update_vram(u8 *vram) {
	force_update = 1;
}

static sdl_render_size sdl_render_software_get_render_size(void) {
	sdl_render_size s;
	SDL_GetRendererOutputSize(renderer, &s.w, &s.h);
	return s;
}

sdl_renderer sdl_renderer_software = {
    sdl_render_software_init,
    sdl_render_software_deinit,
    sdl_render_software_update_charset,
    sdl_render_software_update_palette,
    sdl_render_software_update_vram,
    sdl_render_software_draw,
    sdl_render_software_get_window,
    sdl_render_software_get_render_size
};
