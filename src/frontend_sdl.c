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

#include <ctype.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
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

int zeta_has_feature(int feature) {
	return 1;
}

static SDL_AudioDeviceID audio_device;
static SDL_AudioSpec audio_spec;
static double audio_prev_time;

typedef struct {
	u8 enabled;
	double freq;
	long ms;
} speaker_entry;

#define AUDIO_VOLUME_MAX 127
#define SPEAKER_ENTRY_LEN 64
static int speaker_entry_pos = 0;
static speaker_entry speaker_entries[SPEAKER_ENTRY_LEN];
static long speaker_freq_ctr = 0;
static u8 audio_volume = AUDIO_VOLUME_MAX;
static SDL_mutex *speaker_mutex;

static void audio_callback(void *userdata, Uint8 *stream, int len) {
	int i; long j;
	float freq_samples;
//	double audio_curr_time = audio_prev_time + (len / (double) audio_spec.freq * 1000);
	double audio_curr_time = zeta_time_ms();
	long audio_res = (audio_curr_time - audio_prev_time);
	float res_to_samples = len / (float) audio_res;
	long audio_from, audio_to;

	SDL_LockMutex(speaker_mutex);
	if (speaker_entry_pos == 0) {
		memset(stream, 128, len);
	} else for (i = 0; i < speaker_entry_pos; i++) {
		audio_from = speaker_entries[i].ms - audio_prev_time;

		if (i == speaker_entry_pos - 1) audio_to = audio_res;
		else audio_to = speaker_entries[i+1].ms - audio_prev_time;

		// convert
		audio_from = (long) (audio_from * res_to_samples);
		audio_to = (long) (audio_to * res_to_samples);

		if (audio_from < 0) audio_from = 0;
		else if (audio_from >= len) continue;
		if (audio_to < 0) audio_to = 0;
		else if (audio_to > len) audio_to = len;

		// emit
		if (audio_to > audio_from) {
			if (speaker_entries[i].enabled) {
				freq_samples = (float) (audio_spec.freq / (speaker_entries[i].freq * 2));
				for (j = audio_from; j < audio_to; j++) {
					stream[j] = (((long) ((speaker_freq_ctr + j - audio_from) / freq_samples)) & 1) ? (128-audio_volume) : (128+audio_volume);
				}
				speaker_freq_ctr += audio_to - audio_from;
			} else {
				speaker_freq_ctr = 0;
				memset(stream + audio_from, 128, audio_to - audio_from);
			}
		}
	}

	/* if (speaker_entry_pos > 0) {
		if (i >= speaker_entry_pos) i = speaker_entry_pos - 1;
		k = i;
		for (; i < speaker_entry_pos; i++) {
			speaker_entries[i - k] = speaker_entries[i];
		}
		speaker_entry_pos = i - k;
	} */
	if (speaker_entry_pos > 0) {
		speaker_entries[0] = speaker_entries[speaker_entry_pos - 1];
		speaker_entry_pos = 1;
	}

	audio_prev_time = audio_curr_time;
	SDL_UnlockMutex(speaker_mutex);
}

void speaker_on(double freq) {
	SDL_LockMutex(speaker_mutex);
	if (speaker_entry_pos >= SPEAKER_ENTRY_LEN) {
		cpu_ext_log("speaker buffer overrun");
		SDL_UnlockMutex(speaker_mutex);
		return;
	}
	speaker_entries[speaker_entry_pos].ms = zeta_time_ms();
	speaker_entries[speaker_entry_pos].freq = freq;
	speaker_entries[speaker_entry_pos++].enabled = 1;
	SDL_UnlockMutex(speaker_mutex);
}
void speaker_off() {
	SDL_LockMutex(speaker_mutex);
	if (speaker_entry_pos >= SPEAKER_ENTRY_LEN) {
		cpu_ext_log("speaker buffer overrun");
		SDL_UnlockMutex(speaker_mutex);
		return;
	}
	speaker_entries[speaker_entry_pos].ms = zeta_time_ms();
	speaker_entries[speaker_entry_pos++].enabled = 0;
	SDL_UnlockMutex(speaker_mutex);
}

static SDL_mutex *zzt_thread_lock;
static SDL_cond *zzt_thread_cond;
static u8 zzt_vram_copy[80*25*2];
static u8 zzt_thread_running;
static atomic_int zzt_renderer_waiting = 0;
static u8 video_blink = 1;

#define SDL_TIMER_MS 54.9451
static long first_timer_tick;
static double timer_time;

static Uint32 sdl_timer_thread(Uint32 interval, void *param) {
	if (!zzt_thread_running) return 0;
	long curr_timer_tick = zeta_time_ms();

	atomic_fetch_add(&zzt_renderer_waiting, 1);
	SDL_LockMutex(zzt_thread_lock);
	atomic_fetch_sub(&zzt_renderer_waiting, 1);
	zzt_mark_timer();

	timer_time += SDL_TIMER_MS;
	long duration = curr_timer_tick - first_timer_tick;
	long tick_time = ((long) (timer_time + SDL_TIMER_MS)) - duration;

	while (tick_time <= 0) {
		zzt_mark_timer();
		timer_time += SDL_TIMER_MS;
		tick_time = ((long) (timer_time + SDL_TIMER_MS)) - duration;
	}

	SDL_CondBroadcast(zzt_thread_cond);
	SDL_UnlockMutex(zzt_thread_lock);
	return tick_time;
}

static void sdl_timer_init() {
	first_timer_tick = zeta_time_ms();
	timer_time = 0;
	SDL_AddTimer((int) SDL_TIMER_MS, sdl_timer_thread, (void*)NULL);
}

// try to keep a budget of ~5ms per call

static int zzt_thread_func(void *ptr) {
	int opcodes = 1000;
	while (zzt_thread_running) {
		if (SDL_LockMutex(zzt_thread_lock) == 0) {
			while (zzt_renderer_waiting > 0) {
				SDL_CondWait(zzt_thread_cond, zzt_thread_lock);
			}
			long duration = zeta_time_ms();
			if (!zzt_execute(opcodes)) zzt_thread_running = 0;
			duration = zeta_time_ms() - duration;
			if (duration < 2) {
				opcodes = (opcodes * 20 / 19);
			} else if (duration > 4) {
				opcodes = (opcodes * 19 / 20);
			}
			SDL_CondBroadcast(zzt_thread_cond);
			SDL_UnlockMutex(zzt_thread_lock);
		}
	}

	return 0;
}

static void update_keymod(SDL_Keymod keymod) {
	if (keymod & (KMOD_LSHIFT | KMOD_RSHIFT)) zzt_kmod_set(0x01); else zzt_kmod_clear(0x01);
	if (keymod & (KMOD_LCTRL | KMOD_RCTRL)) zzt_kmod_set(0x04); else zzt_kmod_clear(0x04);
	if (keymod & (KMOD_LALT | KMOD_RALT)) zzt_kmod_set(0x08); else zzt_kmod_clear(0x08);
}

#define is_shift(kmod) ((kmod) & (KMOD_LSHIFT | KMOD_RSHIFT))

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *chartex;
static SDL_GLContext gl_context;
static int charw, charh;

static void init_opengl() {
	glViewport(0, 0, 80 * charw, 25 * charh);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, 80 * charw, 0, 25 * charh, -1, 1);
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

/* #define GLVX(i) (((i) * 2.0f / 80) - 1)
#define GLVY(i) (((i) * 2.0f / 25) - 1) */

#define GLVX(i) ((i)*charw*(80/width))
#define GLVY(i) ((i)*charh)
#define GLTX(chr,i) ( ( ((chr)&0xF)+(i) )/16.0*texw )
#define GLTY(chr,i) ( ( ((chr)>>4)+(i) )/16.0*texh )

static void oglguard() {
	GLenum err;

	if ((err = glGetError()) != GL_NO_ERROR) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "OpenGL error: %d", err);
	}
}

static char as_shifted(char kcode) {
	if (kcode >= 'a' && kcode <= 'z') {
		return kcode - 32;
	} else switch(kcode) {
		case '1': return '!';
		case '2': return '@';
		case '3': return '#';
		case '4': return '$';
		case '5': return '%';
		case '6': return '^';
		case '7': return '&';
		case '8': return '*';
		case '9': return '(';
		case '0': return ')';
		case '-': return '_';
		case '=': return '+';
		case '[': return '{';
		case ']': return '}';
		case ';': return ':';
		case '\'': return '"';
		case '\\': return '|';
		case ',': return '<';
		case '.': return '>';
		case '/': return '?';
		case '`': return '~';
		default: return kcode;
	}

if (kcode == '2') return '@';
	else if (kcode == '6') return '^';
}

static void render_opengl(long curr_time) {
	u8 blink_local = video_blink && ((curr_time % 466) >= 233);
	float texw, texh;
	int width = (zzt_video_mode() & 2) ? 80 : 40;

	// pass 1: background colors
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_TEXTURE_2D);
	glBegin(GL_QUADS);
	int vpos = 1;
	for (int y = 0; y < 25; y++) {
		for (int x = 0; x < width; x++, vpos += 2) {
			u8 col = zzt_vram_copy[vpos] >> 4;
			if (video_blink) col &= 0x7;
			glColor3ub(
				(def_palette[col] >> 16) & 0xFF,
				(def_palette[col] >> 8) & 0xFF,
				(def_palette[col] >> 0) & 0xFF
			);
			glVertex2i(GLVX(x), GLVY(y));
			glVertex2i(GLVX(x+1), GLVY(y));
			glVertex2i(GLVX(x+1), GLVY(y+1));
			glVertex2i(GLVX(x), GLVY(y+1));
		}
	}
	glEnd();

	// pass 2: foreground colors
	if (SDL_GL_BindTexture(chartex, &texw, &texh)) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not bind OpenGL texture! %s", SDL_GetError());
	}
	glAlphaFunc(GL_GREATER, 0.5);
	glEnable(GL_ALPHA_TEST);
	glEnable(GL_TEXTURE_2D);
	glBegin(GL_QUADS);
	vpos = 0;
	for (int y = 0; y < 25; y++) {
		for (int x = 0; x < width; x++, vpos += 2) {
			u8 chr = zzt_vram_copy[vpos];
			u8 col = zzt_vram_copy[vpos+1];
			if (blink_local && col >= 0x80) continue;
			else col = col & 0xF;
			glColor3ub(
				(def_palette[col] >> 16) & 0xFF,
				(def_palette[col] >> 8) & 0xFF,
				(def_palette[col] >> 0) & 0xFF
			);
			glTexCoord2f(GLTX(chr,0), GLTY(chr,0));
			glVertex2i(GLVX(x), GLVY(y));
			glTexCoord2f(GLTX(chr,1), GLTY(chr,0));
			glVertex2i(GLVX(x+1), GLVY(y));
			glTexCoord2f(GLTX(chr,1), GLTY(chr,1));
			glVertex2i(GLVX(x+1), GLVY(y+1));
			glTexCoord2f(GLTX(chr,0), GLTY(chr,1));
			glVertex2i(GLVX(x), GLVY(y+1));
		}
	}
	glEnd();
	SDL_GL_UnbindTexture(chartex);
}

static void render_software_copy(long curr_time) {
	SDL_Rect rectSrc, rectDst;
	rectSrc.w = rectDst.w = charw;
	rectSrc.h = rectDst.h = charh;

	int width = (zzt_video_mode() & 2) ? 80 : 40;
	int vpos = 0;
	for (int y = 0; y < 25; y++) {
		for (int x = 0; x < width; x++, vpos += 2) {
			u8 chr = zzt_vram_copy[vpos];
			u8 col = zzt_vram_copy[vpos + 1];
			rectSrc.x = (chr & 0xF)*charw;
			rectSrc.y = (chr >> 4)*charh;
			rectDst.x = x*charw;
			rectDst.y = y*charh;

			if (video_blink && col >= 0x80) {
				col &= 0x7f;
				if ((curr_time % 466) >= 233) {
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
}

int main(int argc, char **argv) {
	int scancodes_lifted[128];
	int slc = 0;
	int use_opengl = 0;

	SDL_AudioSpec requested_audio_spec;

	SDL_Event event;
	int scode, kcode;

	SDL_Thread* zzt_thread;

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed! %s", SDL_GetError());
		return 1;
	}

	SDL_zero(requested_audio_spec);
	requested_audio_spec.freq = 48000;
	requested_audio_spec.format = AUDIO_U8;
	requested_audio_spec.channels = 1;
	requested_audio_spec.samples = 4096;
	requested_audio_spec.callback = audio_callback;

	audio_device = SDL_OpenAudioDevice(NULL, 0, &requested_audio_spec, &audio_spec, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
	if (audio_device == 0) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not open audio device! %s", SDL_GetError());
	}

	charw = 8;
	charh = 14;

	use_opengl = 1;
	if (use_opengl) {
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);

		window = SDL_CreateWindow("Zeta", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
			80*charw, 25*charh, SDL_WINDOW_OPENGL);
		if (window == NULL) {
			use_opengl = 0;
		} else if ((gl_context = SDL_GL_CreateContext(window)) == NULL) {
			SDL_DestroyWindow(window);
			use_opengl = 0;
		} else {
			SDL_GL_SetSwapInterval(1);
		}
	}

	if (!use_opengl) {
		fprintf(stderr, "Could not initialize OpenGL (%s), using fallback renderer...", SDL_GetError());
		window = SDL_CreateWindow("Zeta", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
			80*charw, 25*charh, 0);
		SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");
	} else {
		init_opengl();
		SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
	}

	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

	chartex = create_texture_from_array(renderer, res_8x14_bin, charh);
	SDL_SetTextureBlendMode(chartex, SDL_BLENDMODE_BLEND);

	init_posix_vfs("");
	zzt_init();

	zzt_thread_lock = SDL_CreateMutex();
	zzt_thread_cond = SDL_CreateCond();
	speaker_mutex = SDL_CreateMutex();

	long curr_time;
	u8 cont_loop = 1;

	zzt_thread_running = 1;
	zzt_thread = SDL_CreateThread(zzt_thread_func, "ZZT Executor", (void*)NULL);
	if (zzt_thread == NULL) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not create ZZT thread! %s", SDL_GetError());
		return 1;
	}

	if (audio_device != 0) {
		audio_prev_time = zeta_time_ms();
		SDL_PauseAudioDevice(audio_device, 0);
	}

	sdl_timer_init();

	while (cont_loop) {
		if (!zzt_thread_running) { cont_loop = 0; break; }

		atomic_fetch_add(&zzt_renderer_waiting, 1);
		SDL_LockMutex(zzt_thread_lock);
		atomic_fetch_sub(&zzt_renderer_waiting, 1);
		u8* ram = zzt_get_ram();
		memcpy(zzt_vram_copy, ram + 0xB8000, 80*25*2);
		zzt_mark_frame();

		// do KEYUPs before KEYDOWNS - fixes key loss issues w/ Windows
		while (slc > 0) {
			zzt_keyup(scancodes_lifted[--slc]);
		}

		while (SDL_PollEvent(&event)) {
			switch (event.type) {
				case SDL_KEYDOWN:
					update_keymod(event.key.keysym.mod);
					scode = event.key.keysym.scancode;
					kcode = event.key.keysym.sym;
					if (kcode < 0 || kcode >= 127) kcode = 0;
					if (scode >= 0 && scode <= sdl_to_pc_scancode_max) {
						if (is_shift(event.key.keysym.mod)) kcode = as_shifted(kcode);
						zzt_key(kcode, sdl_to_pc_scancode[scode]);
					}
					break;
				case SDL_KEYUP:
					update_keymod(event.key.keysym.mod);
					scode = event.key.keysym.scancode;
					if (scode >= 0 && scode <= sdl_to_pc_scancode_max) {
						scancodes_lifted[slc++] = sdl_to_pc_scancode[scode];
					}
					break;
				case SDL_QUIT:
					cont_loop = 0;
					break;
			}
		}

		SDL_CondBroadcast(zzt_thread_cond);
		SDL_UnlockMutex(zzt_thread_lock);

		curr_time = zeta_time_ms();
		if (use_opengl) {
			render_opengl(curr_time);
			SDL_GL_SwapWindow(window);
		} else {
			render_software_copy(curr_time);
			SDL_RenderPresent(renderer);
		}
	}

	zzt_thread_running = 0;

	SDL_DestroyRenderer(renderer);
	if (audio_device != 0) {
		SDL_CloseAudioDevice(audio_device);
	}
	SDL_Quit();
	return 0;
}
