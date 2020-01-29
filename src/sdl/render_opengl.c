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

#ifdef USE_OPENGL

#ifdef DEBUG_OPENGL
#define OGL_GUARD() oglguard()

static void oglguard(void) {
	GLenum err;

	while ((err = glGetError()) != GL_NO_ERROR) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "OpenGL error: %d", err);
	}
}
#else
#define OGL_GUARD() {}
#endif

static SDL_Texture *chartex = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Window *window = NULL;
static SDL_GLContext gl_context;
static int charw, charh;

static int force_update;
static int last_blink_mode;

static SDL_Texture *create_texture_from_array(SDL_Renderer *renderer, int access, unsigned char *data, int height) {
	SDL_Texture *texture;
	SDL_Rect rect;
	Uint32 texdata[8 * height];
	Uint32* tptr;
	int ch, cx, cy, ctmp;

	rect.w = 8;
	rect.h = height;
	texture = SDL_CreateTexture(renderer,
		SDL_PIXELFORMAT_RGBA32,
		access,
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

static void prepare_render_opengl(void) {
	SDL_Rect rect;
	int w, h, scale;
	SDL_GL_GetDrawableSize(window, &w, &h);
	calc_render_area(&rect, w, h, &scale, AREA_WITHOUT_SCALE);

	glViewport(0, 0, scale * (rect.w + (rect.x * 2)), scale * (rect.h + (rect.y * 2)));
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
#ifdef USE_OPENGL_ES
	glOrthof(-(rect.x), rect.w + (rect.x), rect.h + (rect.y), -(rect.y), -1, 1);
#else
	glOrtho(-(rect.x), rect.w + (rect.x), rect.h + (rect.y), -(rect.y), -1, 1);
#endif
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

#define GLVX(i,width) ((i)*charw*(80/width))
#define GLVY(i) ((i)*charh)
#define GLTX(chr,i) ( ( ((chr)&0xF)+(i) )/16.0*1.0 )
#define GLTY(chr,i) ( ( ((chr)>>4)+(i) )/16.0*1.0 )

static unsigned short *ogl_buf_pos;
static unsigned short *ogl_buf_pos40;
static unsigned char *ogl_buf_col;
static float *ogl_buf_tex;
static unsigned char *ogl_buf_colcache;
static float *ogl_buf_texcache;

#ifdef USE_OPENGL_ES
#define GL_COMPONENT_POINTS 6
#else
#define GL_COMPONENT_POINTS 4
#endif

static void update_opengl_colcache(u32* pal) {
	int size = GL_COMPONENT_POINTS * 4;
	for (int i = 0; i < size; i++) {
		for (int bpos = i*size; bpos < (i+1)*size; bpos+=4) {
			ogl_buf_colcache[bpos] = (pal[i] >> 16) & 0xFF;
			ogl_buf_colcache[bpos + 1] = (pal[i] >> 8) & 0xFF;
			ogl_buf_colcache[bpos + 2] = (pal[i] >> 0) & 0xFF;
			ogl_buf_colcache[bpos + 3] = 0xFF;
		}
	}
}

static void update_opengl_tables(void) {
#ifndef USE_OPENGL_ES
	for (int tpos = 0; tpos < 256 * 8; tpos += 8) {
		u8 chr = tpos >> 3;
		ogl_buf_texcache[tpos] = GLTX(chr,0);
		ogl_buf_texcache[tpos+1] = GLTY(chr,0);
		ogl_buf_texcache[tpos+2] = GLTX(chr,1);
		ogl_buf_texcache[tpos+3] = GLTY(chr,0);
		ogl_buf_texcache[tpos+4] = GLTX(chr,1);
		ogl_buf_texcache[tpos+5] = GLTY(chr,1);
		ogl_buf_texcache[tpos+6] = GLTX(chr,0);
		ogl_buf_texcache[tpos+7] = GLTY(chr,1);
	}

	for (int i = 0; i < 2000; i++) {
		int x = i % 80;
		int y = i / 80;
		ogl_buf_pos[i * 8 + 0] = GLVX(x,80);
		ogl_buf_pos[i * 8 + 1] = GLVY(y);
		ogl_buf_pos[i * 8 + 2] = GLVX(x+1,80);
		ogl_buf_pos[i * 8 + 3] = GLVY(y);
		ogl_buf_pos[i * 8 + 4] = GLVX(x+1,80);
		ogl_buf_pos[i * 8 + 5] = GLVY(y+1);
		ogl_buf_pos[i * 8 + 6] = GLVX(x,80);
		ogl_buf_pos[i * 8 + 7] = GLVY(y+1);
	}

	for (int i = 0; i < 1000; i++) {
		int x = i % 40;
		int y = i / 40;
		ogl_buf_pos40[i * 8 + 0] = GLVX(x,40);
		ogl_buf_pos40[i * 8 + 1] = GLVY(y);
		ogl_buf_pos40[i * 8 + 2] = GLVX(x+1,40);
		ogl_buf_pos40[i * 8 + 3] = GLVY(y);
		ogl_buf_pos40[i * 8 + 4] = GLVX(x+1,40);
		ogl_buf_pos40[i * 8 + 5] = GLVY(y+1);
		ogl_buf_pos40[i * 8 + 6] = GLVX(x,40);
		ogl_buf_pos40[i * 8 + 7] = GLVY(y+1);
	}
#else
	for (int tpos = 0; tpos < 256 * 12; tpos += 12) {
		u8 chr = tpos / 12;
		ogl_buf_texcache[tpos] = GLTX(chr,0);
		ogl_buf_texcache[tpos+1] = GLTY(chr,0);
		ogl_buf_texcache[tpos+2] = GLTX(chr,1);
		ogl_buf_texcache[tpos+3] = GLTY(chr,0);
		ogl_buf_texcache[tpos+4] = GLTX(chr,1);
		ogl_buf_texcache[tpos+5] = GLTY(chr,1);
		ogl_buf_texcache[tpos+6] = GLTX(chr,0);
		ogl_buf_texcache[tpos+7] = GLTY(chr,0);
		ogl_buf_texcache[tpos+8] = GLTX(chr,1);
		ogl_buf_texcache[tpos+9] = GLTY(chr,1);
		ogl_buf_texcache[tpos+10] = GLTX(chr,0);
		ogl_buf_texcache[tpos+11] = GLTY(chr,1);
	}

	for (int i = 0; i < 2000; i++) {
		int x = i % 80;
		int y = i / 80;
		ogl_buf_pos[i * 12 + 0] = GLVX(x,80);
		ogl_buf_pos[i * 12 + 1] = GLVY(y);
		ogl_buf_pos[i * 12 + 2] = GLVX(x+1,80);
		ogl_buf_pos[i * 12 + 3] = GLVY(y);
		ogl_buf_pos[i * 12 + 4] = GLVX(x+1,80);
		ogl_buf_pos[i * 12 + 5] = GLVY(y+1);
		ogl_buf_pos[i * 12 + 6] = GLVX(x,80);
		ogl_buf_pos[i * 12 + 7] = GLVY(y);
		ogl_buf_pos[i * 12 + 8] = GLVX(x+1,80);
		ogl_buf_pos[i * 12 + 9] = GLVY(y+1);
		ogl_buf_pos[i * 12 + 10] = GLVX(x,80);
		ogl_buf_pos[i * 12 + 11] = GLVY(y+1);
	}

	for (int i = 0; i < 1000; i++) {
		int x = i % 40;
		int y = i / 40;
		ogl_buf_pos40[i * 12 + 0] = GLVX(x,40);
		ogl_buf_pos40[i * 12 + 1] = GLVY(y);
		ogl_buf_pos40[i * 12 + 2] = GLVX(x+1,40);
		ogl_buf_pos40[i * 12 + 3] = GLVY(y);
		ogl_buf_pos40[i * 12 + 4] = GLVX(x+1,40);
		ogl_buf_pos40[i * 12 + 5] = GLVY(y+1);
		ogl_buf_pos40[i * 12 + 6] = GLVX(x,40);
		ogl_buf_pos40[i * 12 + 7] = GLVY(y);
		ogl_buf_pos40[i * 12 + 8] = GLVX(x+1,40);
		ogl_buf_pos40[i * 12 + 9] = GLVY(y+1);
		ogl_buf_pos40[i * 12 + 10] = GLVX(x,40);
		ogl_buf_pos40[i * 12 + 11] = GLVY(y+1);
	}
#endif
}

static void init_opengl_tables(void) {
	ogl_buf_pos = malloc((80 * 25) * GL_COMPONENT_POINTS * 2 * sizeof(short));
	ogl_buf_pos40 = malloc((40 * 25) * GL_COMPONENT_POINTS * 2 * sizeof(short));
	ogl_buf_col = malloc(2 * (80 * 25) * GL_COMPONENT_POINTS * 4 * sizeof(char));
	ogl_buf_tex = malloc((80 * 25) * GL_COMPONENT_POINTS * 2 * sizeof(float));
	ogl_buf_colcache = malloc(16 * 4 * GL_COMPONENT_POINTS * sizeof(char));
	ogl_buf_texcache = malloc(256 * 2 * GL_COMPONENT_POINTS * sizeof(float));

	memset(ogl_buf_colcache, 0, 16 * 4 * GL_COMPONENT_POINTS * sizeof(char));
}

static void free_opengl_tables(void) {
	free(ogl_buf_texcache);
	free(ogl_buf_colcache);
	free(ogl_buf_tex);
	free(ogl_buf_col);
	free(ogl_buf_pos40);
	free(ogl_buf_pos);
}

static void render_opengl(u8 *vram, int regen_visuals, int blink_mode) {
	float texw, texh;
	int width = (zzt_video_mode() & 2) ? 80 : 40;

	prepare_render_opengl();

	// generate visual data
	int vpos = 0;
	if (regen_visuals) for (int y = 0; y < 25; y++) {
		for (int x = 0; x < width; x++, vpos += 2) {
			u8 chr = vram[vpos];
			u8 col = vram[vpos+1];
			u8 bgcol = col >> 4;
			u8 fgcol = col & 0xF;

			if (blink_mode != BLINK_MODE_NONE) {
				if (bgcol >= 0x8) {
					bgcol &= 0x7;
					if (blink_mode == BLINK_MODE_2) fgcol = bgcol;
				}
			}

			int bpos_s = vpos * 2 * GL_COMPONENT_POINTS;
			memcpy(ogl_buf_col + bpos_s, ogl_buf_colcache + (4*GL_COMPONENT_POINTS*bgcol), 4*GL_COMPONENT_POINTS*sizeof(char));
			memcpy(ogl_buf_col + bpos_s + (8000 * GL_COMPONENT_POINTS), ogl_buf_colcache + (4*GL_COMPONENT_POINTS*fgcol), 4*GL_COMPONENT_POINTS*sizeof(char));

			int tpos_s = bpos_s >> 1;
			memcpy(ogl_buf_tex + tpos_s, ogl_buf_texcache + chr*2*GL_COMPONENT_POINTS, 2*GL_COMPONENT_POINTS*sizeof(float));
		}
	}

	// pass 1: background colors
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_CULL_FACE);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);

	glVertexPointer(2, GL_SHORT, 0, width == 40 ? ogl_buf_pos40 : ogl_buf_pos);
	glColorPointer(4, GL_UNSIGNED_BYTE, 0, ogl_buf_col);

#ifdef USE_OPENGL_ES
	glDrawArrays(GL_TRIANGLES, 0, width * 25 * 6);
#else
	glDrawArrays(GL_QUADS, 0, width * 25 * 4);
#endif

	OGL_GUARD();

	// pass 2: foreground colors
	if (chartex != NULL) {
		if (SDL_GL_BindTexture(chartex, &texw, &texh)) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not bind OpenGL texture! %s", SDL_GetError());
		}
		glAlphaFunc(GL_GREATER, 0.5);
		glEnable(GL_ALPHA_TEST);
		glEnable(GL_TEXTURE_2D);

		glEnableClientState(GL_TEXTURE_COORD_ARRAY);

		glColorPointer(4, GL_UNSIGNED_BYTE, 0, ogl_buf_col + (80 * 25 * 4 * GL_COMPONENT_POINTS * sizeof(char)));
		glTexCoordPointer(2, GL_FLOAT, 0, ogl_buf_tex);

#ifdef USE_OPENGL_ES
		glDrawArrays(GL_TRIANGLES, 0, width * 25 * 6);
#else
		glDrawArrays(GL_QUADS, 0, width * 25 * 4);
#endif

		OGL_GUARD();

		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);

		SDL_GL_UnbindTexture(chartex);
	}
}

static int sdl_render_opengl_init(void) {
	init_opengl_tables();

#ifdef USE_OPENGL_ES
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
#else
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 4);
#endif
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);

	window = SDL_CreateWindow("Zeta", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		640, 350, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
	if (window == NULL) {
		return -1;
	} else if ((gl_context = SDL_GL_CreateContext(window)) == NULL) {
		SDL_DestroyWindow(window);
		return -1;
	}

	SDL_GL_SetSwapInterval(1);
	SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL) {
		SDL_DestroyWindow(window);
        return -1;
    }

	return 0;
}

static void sdl_render_opengl_deinit(void) {
	if (chartex != NULL) {
		SDL_DestroyTexture(chartex);
	}
	
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);

	free_opengl_tables();
}

static void sdl_render_opengl_update_charset(int charw_arg, int charh_arg, u8 *data_arg) {
	charw = charw_arg;
	charh = charh_arg;

	if (chartex != NULL) SDL_DestroyTexture(chartex);
	chartex = create_texture_from_array(renderer, SDL_TEXTUREACCESS_STATIC, data_arg, charh_arg);
	SDL_SetTextureBlendMode(chartex, SDL_BLENDMODE_BLEND);

	update_opengl_tables();
	force_update = 1;
}

static void sdl_render_opengl_update_palette(u32 *data_arg) {
    update_opengl_colcache(data_arg);
	force_update = 1;
}

static void sdl_render_opengl_update_vram(u8 *vram) {
	force_update = 1;
}

static void sdl_render_opengl_draw(u8 *vram, int blink_mode) {
	// TODO: reimplement should_render flag
	int should_render = force_update || (blink_mode != last_blink_mode);
	render_opengl(vram, should_render, blink_mode);
	SDL_GL_SwapWindow(window);
	last_blink_mode = blink_mode;
	force_update = 0;
}

static SDL_Window *sdl_render_opengl_get_window(void) {
	return window;
}

static sdl_render_size sdl_render_opengl_get_render_size(void) {
	sdl_render_size s;
	SDL_GL_GetDrawableSize(window, &s.w, &s.h);
	return s;
}

sdl_renderer sdl_renderer_opengl = {
	sdl_render_opengl_init,
	sdl_render_opengl_deinit,
	sdl_render_opengl_update_charset,
	sdl_render_opengl_update_palette,
	sdl_render_opengl_update_vram,
	sdl_render_opengl_draw,
	sdl_render_opengl_get_window,
	sdl_render_opengl_get_render_size
};

#endif /* USE_OPENGL */
