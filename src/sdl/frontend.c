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

#include "../config.h"

#ifdef USE_GETOPT
#define _POSIX_C_SOURCE 2
#include <unistd.h>
#endif

#include <ctype.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_main.h>
#include "../util.h"
#include "../zzt.h"
#include "../audio_shared.h"
#include "../audio_stream.h"
#include "../posix_vfs.h"
#include "../render_software.h"
#include "../audio_writer.h"
#ifdef ENABLE_SCREENSHOTS
#include "../screenshot_writer.h"
#endif
#include "frontend_sdl.h"

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

long zeta_time_ms(void) {
	return SDL_GetTicks();
}

void cpu_ext_log(const char *s) {
	fprintf(stderr, "%s\n", s);
}

int zeta_has_feature(int feature) {
	return 1;
}

static SDL_AudioDeviceID audio_device;
static SDL_AudioSpec audio_spec;
static SDL_mutex *audio_mutex;

static double audio_time;
static u8 audio_writer_enabled = 0;

static void audio_callback(void *userdata, Uint8 *stream, int len) {
	SDL_LockMutex(audio_mutex);
	audio_stream_generate_u8(zeta_time_ms(), stream, len);
	SDL_UnlockMutex(audio_mutex);
}

void speaker_on(int cycles, double freq) {
	SDL_LockMutex(audio_mutex);
	audio_stream_append_on(audio_time, cycles, freq);
	SDL_UnlockMutex(audio_mutex);
	if (audio_writer_enabled) {
		audio_writer_speaker_on(audio_time, cycles, freq);
	}
}

void speaker_off(int cycles) {
	SDL_LockMutex(audio_mutex);
	audio_stream_append_off(audio_time, cycles);
	SDL_UnlockMutex(audio_mutex);
	if (audio_writer_enabled) {
		audio_writer_speaker_off(audio_time, cycles);
	}
}

static SDL_mutex *zzt_thread_lock;
static SDL_cond *zzt_thread_cond;
static u8 zzt_vram_copy[80*25*2];
static u8 zzt_thread_running;
static atomic_int zzt_renderer_waiting = 0;
static u8 video_blink = 1;

static long first_timer_tick;
static double timer_time;

static Uint32 sdl_timer_thread(Uint32 interval, void *param) {
	if (!zzt_thread_running) return 0;
	long curr_timer_tick = zeta_time_ms();

	atomic_fetch_add(&zzt_renderer_waiting, 1);
	SDL_LockMutex(zzt_thread_lock);
	atomic_fetch_sub(&zzt_renderer_waiting, 1);
	zzt_mark_timer();

	audio_time = zeta_time_ms();
	timer_time += SYS_TIMER_TIME;
	long duration = curr_timer_tick - first_timer_tick;
	long tick_time = ((long) (timer_time + SYS_TIMER_TIME)) - duration;

	while (tick_time <= 0) {
		zzt_mark_timer();
		timer_time += SYS_TIMER_TIME;
		tick_time = ((long) (timer_time + SYS_TIMER_TIME)) - duration;
	}

	SDL_CondBroadcast(zzt_thread_cond);
	SDL_UnlockMutex(zzt_thread_lock);
	return tick_time;
}

static void sdl_timer_init(void) {
	first_timer_tick = zeta_time_ms();
	timer_time = 0;
	SDL_AddTimer((int) SYS_TIMER_TIME, sdl_timer_thread, (void*)NULL);
}

static int sdl_is_blink_phase(long curr_time) {
	return ((curr_time % (BLINK_TOGGLE_DURATION_MS*2)) >= BLINK_TOGGLE_DURATION_MS);
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
			int rcode = zzt_execute(opcodes);
			duration = zeta_time_ms() - duration;
			if (rcode == STATE_CONTINUE) {
				if (duration < 2) {
					opcodes = (opcodes * 20 / 19);
				} else if (duration > 4) {
					opcodes = (opcodes * 19 / 20);
				}
			}
			SDL_CondBroadcast(zzt_thread_cond);
			if (rcode == STATE_WAIT) {
				SDL_CondWaitTimeout(zzt_thread_cond, zzt_thread_lock, 20);
			} else if (rcode == STATE_END) {
				zzt_thread_running = 0;
			}
			SDL_UnlockMutex(zzt_thread_lock);
		}
	}

	return 0;
}

#define KEYMOD_ALT(keymod) ((keymod) & (KMOD_LALT | KMOD_RALT))
#define KEYMOD_CTRL(keymod) ((keymod) & (KMOD_LCTRL | KMOD_RCTRL))
#define KEYMOD_SHIFT(keymod) ((keymod) & (KMOD_LSHIFT | KMOD_RSHIFT))

static void update_keymod(SDL_Keymod keymod) {
	if (KEYMOD_SHIFT(keymod)) zzt_kmod_set(0x01); else zzt_kmod_clear(0x01);
	if (KEYMOD_CTRL(keymod)) zzt_kmod_set(0x04); else zzt_kmod_clear(0x04);
	if (KEYMOD_ALT(keymod)) zzt_kmod_set(0x08); else zzt_kmod_clear(0x08);
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
}

static SDL_Window *window;
static int charw, charh;

// used for marking render data updates
static SDL_mutex *render_data_update_mutex;

static int charset_update_requested = 0;
static u8* charset_update_data = NULL;

static int palette_update_requested = 0;
static u32* palette_update_data = NULL;

void calc_render_area(SDL_Rect *rect, int w, int h, int *scale_out, int flags) {
	int iw = 80*charw;
	int ih = 25*charh;

	int scale = 1;
	while (((scale+1)*iw <= w) && ((scale+1)*ih <= h)) scale++;
	if (scale_out != NULL) *scale_out = scale;

	w /= scale;
	h /= scale;

	if (flags & AREA_WITHOUT_SCALE) scale = 1;

	rect->x = ((w - iw) * scale) / 2;
	rect->y = ((h - ih) * scale) / 2;
	rect->w = iw * scale;
	rect->h = ih * scale;
}

void zeta_update_charset(int width, int height, u8* data) {
	SDL_LockMutex(render_data_update_mutex);
	charw = width;
	charh = height;
	charset_update_data = data;
	charset_update_requested = 1;
	SDL_UnlockMutex(render_data_update_mutex);
}

void zeta_update_palette(u32* data) {
	SDL_LockMutex(render_data_update_mutex);
	palette_update_data = data;
	palette_update_requested = 1;
	SDL_UnlockMutex(render_data_update_mutex);
}

#include "../asset_loader.h"
#include "../frontend_posix.c"

#ifdef USE_OPENGL
extern sdl_renderer sdl_renderer_opengl;
#endif
extern sdl_renderer sdl_renderer_software;

int main(int argc, char **argv) {
	int scancodes_lifted[sdl_to_pc_scancode_max + 1];
	int slc = 0;

	SDL_AudioSpec requested_audio_spec;

	SDL_Event event;
	int scode, kcode;

	SDL_Thread* zzt_thread;
	u8 windowed = 1;

	init_posix_vfs("");

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed! %s", SDL_GetError());
		return 1;
	}

	render_data_update_mutex = SDL_CreateMutex();
	zzt_thread_lock = SDL_CreateMutex();
	zzt_thread_cond = SDL_CreateCond();
	audio_mutex = SDL_CreateMutex();

	if (posix_zzt_init(argc, argv) < 0) {
		fprintf(stderr, "Could not load ZZT!\n");
		SDL_Quit();
		return 1;
	}

	sdl_renderer *renderer = NULL;
#ifdef USE_OPENGL
	renderer = &sdl_renderer_opengl;
	if (renderer->init() < 0) {
		fprintf(stderr, "Could not initialize OpenGL (%s), using software renderer...", SDL_GetError());
		renderer = NULL;
	}
#endif
	if (renderer == NULL) {
		renderer = &sdl_renderer_software;
		if (renderer->init() < 0) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not open video device!");
			return 1;
		}
	}
	window = renderer->get_window();

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

	u8 cont_loop = 1;

	zzt_thread_running = 1;
	zzt_thread = SDL_CreateThread(zzt_thread_func, "ZZT Executor", (void*)NULL);
	if (zzt_thread == NULL) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not create ZZT thread! %s", SDL_GetError());
		return 1;
	}

	if (audio_device != 0) {
		audio_stream_init(zeta_time_ms(), audio_spec.freq);
		if (posix_zzt_arg_note_delay >= 0.0) {
			audio_set_note_delay(posix_zzt_arg_note_delay);
		}
		audio_stream_set_volume(audio_stream_get_max_volume() >> 1);
		SDL_PauseAudioDevice(audio_device, 0);
	}

	sdl_timer_init();

	int should_render = 1;

	while (cont_loop) {
		if (!zzt_thread_running) { cont_loop = 0; break; }

		atomic_fetch_add(&zzt_renderer_waiting, 1);
		SDL_LockMutex(zzt_thread_lock);
		atomic_fetch_sub(&zzt_renderer_waiting, 1);

		u8* ram = zzt_get_ram();
		should_render = memcmp(ram + 0xB8000, zzt_vram_copy, 80*25*2);
		if (should_render) {
			memcpy(zzt_vram_copy, ram + 0xB8000, 80*25*2);
			renderer->update_vram(zzt_vram_copy);
		}

		zzt_mark_frame();

		// do KEYUPs before KEYDOWNS - fixes key loss issues w/ Windows
		while (slc > 0) {
			zzt_keyup(scancodes_lifted[--slc]);
		}

		while (SDL_PollEvent(&event)) {
			switch (event.type) {
				case SDL_KEYDOWN:
					if (windowed && (event.key.keysym.sym == 'q' || event.key.keysym.scancode == SDL_SCANCODE_ESCAPE)) {
						if (SDL_GetRelativeMouseMode() != 0) {
							SDL_SetRelativeMouseMode(0);
							break;
						}
					}
#ifdef ENABLE_SCREENSHOTS
					if (event.key.keysym.sym == SDLK_F12) {
						FILE *file;
						char filename[24];

						int sflags = 0;
						int swidth = (zzt_video_mode() & 2) ? 80 : 40;

						if (charset_update_data == NULL || palette_update_data == NULL) {
							break;
						}

						if (!video_blink) sflags |= RENDER_BLINK_OFF;
						else if (sdl_is_blink_phase(zeta_time_ms())) sflags |= RENDER_BLINK_PHASE;

#ifdef USE_LIBPNG
						int stype = SCREENSHOT_TYPE_PNG;
						file = create_inc_file(filename, 23, "screen%d.png", "wb");
#else
						int stype = SCREENSHOT_TYPE_BMP;
						file = create_inc_file(filename, 23, "screen%d.bmp", "wb");
#endif
						if (file != NULL) {
							if (write_screenshot(
								file, stype,
								swidth, sflags,
								zzt_get_ram() + 0xB8000, charset_update_data,
								charw, charh,
								palette_update_data
							) < 0) {
								fprintf(stderr, "Could not write screenshot!\n");
							}
							fclose(file);
						}
						break;
					}
#endif
					if (event.key.keysym.sym == SDLK_F6 && KEYMOD_CTRL(event.key.keysym.mod)) {
						// audio writer
						if (!audio_writer_enabled) {
							FILE *file;
							char filename[24];
							file = create_inc_file(filename, 23, "audio%d.wav", "wb");
							if (file != NULL) {
								fclose(file);
								if (audio_writer_start(filename, audio_time, 48000) >= 0) {
									audio_writer_enabled = 1;
									fprintf(stderr, "Audio writing started [%s].\n", filename);
								} else {
									fprintf(stderr, "Could not start audio writing - internal error!\n");
								}
							}
						} else {
							audio_writer_enabled = 0;
							audio_writer_stop(audio_time, 0);
							fprintf(stderr, "Audio writing stopped.\n");
						}
						break;
					}

					if (event.key.keysym.scancode == SDL_SCANCODE_RETURN && KEYMOD_ALT(event.key.keysym.mod)) {
						// Alt+ENTER
						if (windowed) {
							SDL_DisplayMode mode;
							SDL_GetDesktopDisplayMode(SDL_GetWindowDisplayIndex(window), &mode);
							SDL_SetWindowSize(window, mode.w, mode.h);
							SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);
							// force focus
							SDL_SetRelativeMouseMode(1);
						} else {
							SDL_SetWindowFullscreen(window, 0);
							SDL_SetWindowSize(window, 80*charw, 25*charh);
							// drop focus
							SDL_SetRelativeMouseMode(0);
						}
						windowed = 1 - windowed;
						break;
					}
					update_keymod(event.key.keysym.mod);
					scode = event.key.keysym.scancode;
					kcode = event.key.keysym.sym;
					if (kcode < 0 || kcode >= 127) kcode = 0;
					if (scode >= 0 && scode <= sdl_to_pc_scancode_max) {
						if (KEYMOD_SHIFT(event.key.keysym.mod)) kcode = as_shifted(kcode);
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
				case SDL_MOUSEBUTTONDOWN:
					if (SDL_GetRelativeMouseMode() == 0) {
						if (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) {
							SDL_SetRelativeMouseMode(1);
						}
					} else {
						zzt_mouse_set(event.button.button);
					}
					break;
				case SDL_MOUSEBUTTONUP:
					zzt_mouse_clear(event.button.button);
					break;
				case SDL_MOUSEMOTION:
					if (SDL_GetRelativeMouseMode() != 0) {
						zzt_mouse_axis(0, event.motion.xrel);
						zzt_mouse_axis(1, event.motion.yrel);
					}
					break;
				case SDL_QUIT:
					cont_loop = 0;
					break;
			}
		}

		SDL_CondBroadcast(zzt_thread_cond);
		SDL_UnlockMutex(zzt_thread_lock);

		SDL_LockMutex(render_data_update_mutex);

		if (charset_update_requested) {
			renderer->update_charset(charw, charh, charset_update_data);
			charset_update_requested = 0;
		}

		if (palette_update_requested) {
			renderer->update_palette(palette_update_data);
			palette_update_requested = 0;
		}

		SDL_UnlockMutex(render_data_update_mutex);

		long curr_time = zeta_time_ms();
		int blink_mode = video_blink ? (sdl_is_blink_phase(curr_time) ? BLINK_MODE_2 : BLINK_MODE_1) : BLINK_MODE_NONE;
		renderer->draw(zzt_vram_copy, blink_mode);
	}

	if (audio_writer_enabled) {
		audio_writer_enabled = 0;
		audio_writer_stop(audio_time, 0);
	}

	zzt_thread_running = 0;
	if (audio_device != 0) {
		SDL_CloseAudioDevice(audio_device);
	}
	renderer->deinit();
	SDL_Quit();
	return 0;
}
