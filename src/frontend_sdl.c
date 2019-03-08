/**
 * Copyright (c) 2018 Adrian Siekierka
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
#include <SDL2/SDL.h>
#include "zzt.h"

static int def_palette[] = {
	0x000000, 0x0000aa, 0x00aa00, 0x00aaaa,
	0xaa0000, 0xaa00aa, 0xaa5500, 0xaaaaaa,
	0x555555, 0x5555ff, 0x55ff55, 0x55ffff,
	0xff5555, 0xff55ff, 0xffff55, 0xffffff
};

static const u8 sdl_to_pc_scancode[] = {
/*  0*/	0,
/*  1*/	0, 0, 0,
/*  4*/	0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, /* A-I */
/* 13*/	0x24, 0x25, 0x26, 0x32, 0x31, 0x18, 0x19, 0x10, 0x13, /* J-R */
/* 22*/	0x1F, 0x14, 0x16, 0x2F, 0x11, 0x2D, 0x15, 0x2C,       /* S-Z */
/* 30*/	2, 3, 4, 5, 6, 7, 8, 9, 10, 11, /* 1-0 */
/* 40*/	0x1C, 0x01, 0x0E, 0x0F, 0x39,
/* 45*/	0x0C, 0x0D, 0x1A, 0x1B, 0x2B,
/* 50*/	0x2B, 0x27, 0x28, 0x29,
/* 54*/	0x33, 0x34, 0x35, 0x3A,
	0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44, 0x57, 0x58,
	0x37, 0x46, 0, 0x52, 0x47, 0x49, 0x53, 0x4F, 0x51,
	0x4D, 0x4B, 0x50, 0x48, 0x45
};

static const int sdl_to_pc_scancode_max = sizeof(sdl_to_pc_scancode) - 1;

extern unsigned char res_8x14_bin[];

static SDL_Texture *create_texture_from_array(SDL_Renderer *renderer, unsigned char *data, int height) {
	SDL_Texture *texture;
	SDL_Rect rect;
	Uint32 texdata[8 * height];
	Uint32* tptr;
	int ch, cx, cy, ctmp;

	rect.w = 8;
	rect.h = height;
	texture = SDL_CreateTexture(renderer,
		SDL_PIXELFORMAT_RGBA32,
		SDL_TEXTUREACCESS_STATIC,
		16*rect.w, 16*rect.h);

	for (ch = 0; ch < 256; ch++) {
		rect.x = (ch & 0x0F) * rect.w;
		rect.y = (ch >> 4) * rect.h;
		tptr = texdata;
		for (cy = 0; cy < height; cy++, data++) {
			ctmp = data[0];
			for (cx = 0; cx < 8; cx++, tptr++, ctmp <<= 1) {
				*tptr = ((ctmp >> 7) & 1) * 0xFFFFFFFF;
			}
		}
		SDL_UpdateTexture(texture, &rect, texdata, 32);
	}

	return texture;
}

long zeta_time_ms() {
	return SDL_GetTicks();
}

void cpu_ext_log(const char *s) {
	fprintf(stderr, "%s\n", s);
}

void init_posix_vfs(const char *path);

void speaker_on(double freq) {}
void speaker_off() {}

int zeta_has_feature(int feature) {
	return 1;
}

static SDL_mutex *zzt_thread_lock;
static u8 zzt_vram_copy[80*25*2];
static u8 zzt_thread_running;
static u8 video_blink = 1;

static int zzt_thread_func(void *ptr) {
	while (zzt_thread_running) {
		if (SDL_LockMutex(zzt_thread_lock) == 0) {
			if (!zzt_execute(40000)) zzt_thread_running = 0;
			SDL_UnlockMutex(zzt_thread_lock);
		}
		SDL_Delay(1);
	}

	return 0;
}

int main(int argc, char **argv) {
	SDL_Window *window;
	SDL_Renderer *renderer;
	SDL_Rect rectSrc, rectDst;

	SDL_Texture *chartex;
	int charw, charh;

	SDL_Event event;
	int scode, kcode;

	SDL_Thread* zzt_thread;

	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed! %s", SDL_GetError());
		return 1;
	}

	charw = 8;
	charh = 14;

	window = SDL_CreateWindow("Zeta", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		80*charw, 25*charh, 0);
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, 0);

	chartex = create_texture_from_array(renderer, res_8x14_bin, charh);
	rectSrc.w = rectDst.w = charw;
	rectSrc.h = rectDst.h = charh;

	SDL_SetTextureBlendMode(chartex, SDL_BLENDMODE_BLEND);

	init_posix_vfs("");
	zzt_init();

	zzt_thread_lock = SDL_CreateMutex();
	if (!zzt_thread_lock) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not create ZZT thread mutex! %s", SDL_GetError());
		return 1;
	}

	long last_timer_call = zeta_time_ms();
	u8 cont_loop = 1;

	zzt_thread_running = 1;
	zzt_thread = SDL_CreateThread(zzt_thread_func, "ZZT Executor", (void*)NULL);
	if (zzt_thread == NULL) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not create ZZT thread! %s", SDL_GetError());
		return 1;
	}

	while (cont_loop) {
		if (SDL_LockMutex(zzt_thread_lock) != 0) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not lock ZZT thread mutex! %s", SDL_GetError());
			cont_loop = 0;
			break;
		}

		u8* ram = zzt_get_ram();
		memcpy(zzt_vram_copy, ram + 0xB8000, 80*25*2);
		zzt_mark_frame();

		while (SDL_PollEvent(&event)) {
			switch (event.type) {
				case SDL_KEYDOWN:
					scode = event.key.keysym.scancode;
					kcode = event.key.keysym.sym;
					if (kcode < 0 || kcode >= 127) kcode = 0;
					if (scode >= 0 && scode <= sdl_to_pc_scancode_max) {
						zzt_key(kcode, sdl_to_pc_scancode[scode]);
					}
					break;
				case SDL_KEYUP:
					scode = event.key.keysym.scancode;
					if (scode >= 0 && scode <= sdl_to_pc_scancode_max) {
						zzt_keyup(sdl_to_pc_scancode[scode]);
					}
					break;
				case SDL_QUIT:
					cont_loop = 0;
					break;
			}
		}

		long curr_timer_call = zeta_time_ms();
		if ((curr_timer_call - last_timer_call) >= 55) {
			zzt_mark_timer();
			last_timer_call = curr_timer_call;
		}
		SDL_UnlockMutex(zzt_thread_lock);

		int vpos = 0;
		for (int y = 0; y < 25; y++) {
			for (int x = 0; x < 80; x++, vpos += 2) {
				u8 chr = zzt_vram_copy[vpos];
				u8 col = zzt_vram_copy[vpos + 1];
				rectSrc.x = (chr & 0xF)*charw;
				rectSrc.y = (chr >> 4)*charh;
				rectDst.x = x*charw;
				rectDst.y = y*charh;

				if (video_blink && col >= 0x80) {
					col &= 0x7f;
					if ((curr_timer_call % 466) >= 233) {
						col = (col >> 4) * 0x11;
					}
				}

				u8 render_fg = ((col >> 4) ^ (col & 0xF));

				SDL_SetRenderDrawColor(renderer,
					(def_palette[col >> 4] >> 16) & 0xFF,
					(def_palette[col >> 4] >> 8) & 0xFF,
					(def_palette[col >> 4] >> 0) & 0xFF,
					255);

				SDL_RenderFillRect(renderer, &rectDst);

				if (render_fg) {
					SDL_SetTextureColorMod(chartex,
						(def_palette[col & 0xF] >> 16) & 0xFF,
						(def_palette[col & 0xF] >> 8) & 0xFF,
						(def_palette[col & 0xF] >> 0) & 0xFF);
					SDL_RenderCopy(renderer, chartex, &rectSrc, &rectDst);
				}
			}
		}

		SDL_RenderPresent(renderer);
	}

	zzt_thread_running = 0;

	SDL_DestroyRenderer(renderer);
	SDL_Quit();
}
