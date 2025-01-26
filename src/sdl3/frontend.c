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

#include "../config.h"
#include <SDL3/SDL_messagebox.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef USE_GETOPT
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 2
#endif
#include <unistd.h>
#endif

#include <ctype.h>
#include <math.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <SDL3/SDL_main.h>
#include "../util.h"
#include "../zzt.h"
#include "../audio_shared.h"
#include "../audio_stream.h"
#include "../posix_vfs.h"
#include "../render_software.h"
#include "../ui.h"
#ifdef ENABLE_AUDIO_WRITER
#include "../audio_writer.h"
#endif
#ifdef ENABLE_GIF_WRITER
#include "../gif_writer.h"
#endif
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

static const u8 text_char_to_scode_32[] = {
	0x39, // 32
	0x02, 0x03, 0x04, 0x05, 0x06, 0x08, 0x28, 0x0A, 0x0B, 0x09, 0x0D, 0x33, 0x0C, 0x34, // 46
	0x35, 0x0B, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, // 57
	0x27, 0x27, 0x33, 0x0D, 0x34, 0x35, // 63
	0x03, // 64
	0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, /* A-I */
	0x24, 0x25, 0x26, 0x32, 0x31, 0x18, 0x19, 0x10, 0x13, /* J-R */
	0x1F, 0x14, 0x16, 0x2F, 0x11, 0x2D, 0x15, 0x2C,       /* S-Z */
	0x1A, 0x2B, 0x1B, 0x07, 0x0C, 0x29, // 96
	0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, /* A-I */
	0x24, 0x25, 0x26, 0x32, 0x31, 0x18, 0x19, 0x10, 0x13, /* J-R */
	0x1F, 0x14, 0x16, 0x2F, 0x11, 0x2D, 0x15, 0x2C,       /* S-Z */
	0x1A, 0x2B, 0x1B, 0x29 // 126
};
#define TEXT_CHAR_TO_SCANCODE(c) (((c) >= 32 && (c) <= 126) ? text_char_to_scode_32[(c) - 32] : 0)

static const int sdl_to_pc_scancode_max = sizeof(sdl_to_pc_scancode) - 1;

long zeta_time_ms(void) {
	return SDL_GetTicks();
}

void cpu_ext_log(const char *s) {
	fprintf(stderr, "%s\n", s);
}

void zeta_show_developer_warning(const char *format, ...) {
	char debug_message[4097];
	char debug_title[129];
	va_list val;
	
	if (!developer_mode) return;
	debug_title[128] = 0;
	snprintf(debug_title, 128, "Developer Warning @ %04X:%04X", zzt_get_ip() >> 16, zzt_get_ip() & 0xFFFF);
	debug_message[4096] = 0;
	va_start(val, format);
	vsnprintf(debug_message, 4096, format, val);
	va_end(val);

	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, debug_title, debug_message, NULL);
}

int zeta_has_feature(int feature) {
	return 1;
}

static SDL_AudioStream *audio_stream;
static SDL_Mutex *audio_mutex;

static double audio_time;
#ifdef ENABLE_AUDIO_WRITER
static audio_writer_state *audio_writer_s = NULL;
#endif
#ifdef ENABLE_GIF_WRITER
static gif_writer_state *gif_writer_s = NULL;
static u32 gif_writer_ticks;
#endif

static void audio_callback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount) {
	if (additional_amount) {
		uint8_t *data = SDL_stack_alloc(uint8_t, additional_amount);
		if (data) {
			SDL_LockMutex(audio_mutex);
			audio_stream_generate(zeta_time_ms(), data, additional_amount);
			SDL_UnlockMutex(audio_mutex);
			SDL_PutAudioStreamData(stream, data, additional_amount);
			SDL_stack_free(data);
		}
	}
}

void speaker_on(int cycles, double freq) {
	SDL_LockMutex(audio_mutex);
	audio_stream_append_on(audio_time, cycles, freq);
	SDL_UnlockMutex(audio_mutex);
#ifdef ENABLE_AUDIO_WRITER
	if (audio_writer_s != NULL) {
		audio_writer_speaker_on(audio_writer_s, audio_time, cycles, freq);
	}
#endif
}

void speaker_off(int cycles) {
	SDL_LockMutex(audio_mutex);
	audio_stream_append_off(audio_time, cycles);
	SDL_UnlockMutex(audio_mutex);
#ifdef ENABLE_AUDIO_WRITER
	if (audio_writer_s != NULL) {
		audio_writer_speaker_off(audio_writer_s, audio_time, cycles);
	}
#endif
}

// used for marking render data updates
static SDL_Mutex *render_data_update_mutex;

static SDL_Mutex *zzt_thread_lock;
static SDL_Condition *zzt_thread_cond;
static u8 zzt_vram_copy[80*25*2];
static u8 zzt_thread_running;
static atomic_int zzt_renderer_waiting = 0;
static u8 zzt_turbo = 0;

static Uint64 first_timer_tick;
static double timer_time;

static void sdl_pit_tick(void) {
#ifdef ENABLE_GIF_WRITER
	if (gif_writer_s != NULL) {
		SDL_LockMutex(render_data_update_mutex);
		gif_writer_frame(gif_writer_s, gif_writer_ticks++);
		SDL_UnlockMutex(render_data_update_mutex);
	}
#endif
	zzt_mark_timer();
}

static Uint64 sdl_timer_thread(void *param, SDL_TimerID timerID, Uint64 interval) {
	if (!zzt_thread_running) return 0;
        Uint64 curr_timer_tick = SDL_GetTicksNS();

	atomic_fetch_add(&zzt_renderer_waiting, 1);
	SDL_LockMutex(zzt_thread_lock);
	atomic_fetch_sub(&zzt_renderer_waiting, 1);
	sdl_pit_tick();

	audio_time = zeta_time_ms();
	timer_time += SYS_TIMER_TIME;
	Sint64 duration = curr_timer_tick - first_timer_tick;
	Sint64 tick_time = ((Sint64) ((timer_time + SYS_TIMER_TIME) * 1000000)) - duration;

	while (tick_time <= 0) {
		sdl_pit_tick();
		timer_time += SYS_TIMER_TIME;
		tick_time = ((Sint64) ((timer_time + SYS_TIMER_TIME) * 1000000)) - duration;
	}

	SDL_BroadcastCondition(zzt_thread_cond);
	SDL_UnlockMutex(zzt_thread_lock);
	return tick_time;
}

static void sdl_timer_init(void) {
	first_timer_tick = SDL_GetTicksNS();
	timer_time = 0;
	SDL_AddTimerNS((Uint64) (SYS_TIMER_TIME * 1000000), sdl_timer_thread, (void*)NULL);
}

// try to keep a budget of ~5ms per call

static int zzt_thread_func(void *ptr) {
	int opcodes = 1000;

	while (zzt_thread_running) {
		SDL_LockMutex(zzt_thread_lock);
		while (zzt_renderer_waiting > 0) {
			SDL_WaitCondition(zzt_thread_cond, zzt_thread_lock);
		}
		long duration = zeta_time_ms();
		int rcode = zzt_execute(opcodes);
		duration = zeta_time_ms() - duration;
		if (rcode == STATE_CONTINUE) {
			if (duration < 2) {
				opcodes = (opcodes * 20 / 19);
			} else if ((duration > 4) && (opcodes >= 525)) {
				opcodes = (opcodes * 19 / 20);
			}
		}
		SDL_BroadcastCondition(zzt_thread_cond);
		if (rcode >= STATE_WAIT_FRAME) {
			// TODO: Make the counting code more accurate.
			int timeout_time = STATE_WAIT_PIT ? 20 : 10;
			if (rcode >= STATE_WAIT_TIMER) timeout_time = rcode - STATE_WAIT_TIMER;
			if (zzt_turbo) zzt_mark_timer_turbo();
			else SDL_WaitConditionTimeout(zzt_thread_cond, zzt_thread_lock, timeout_time);
		} else if (rcode == STATE_END) {
			zzt_thread_running = 0;
		}
		SDL_UnlockMutex(zzt_thread_lock);
	}

	return 0;
}

#define KEYMOD_ALT(keymod) ((keymod) & (SDL_KMOD_LALT | SDL_KMOD_RALT))
#define KEYMOD_CTRL(keymod) ((keymod) & (SDL_KMOD_LCTRL | SDL_KMOD_RCTRL))
#define KEYMOD_RSHIFT(keymod) ((keymod) & (SDL_KMOD_RSHIFT))
#define KEYMOD_LSHIFT(keymod) ((keymod) & (SDL_KMOD_LSHIFT))
#define KEYMOD_SHIFT(keymod) ((keymod) & (SDL_KMOD_RSHIFT | SDL_KMOD_LSHIFT))

static void update_keymod(SDL_Keymod keymod) {
	if (KEYMOD_RSHIFT(keymod)) zzt_kmod_set(ZZT_KMOD_RSHIFT); else zzt_kmod_clear(ZZT_KMOD_RSHIFT);
	if (KEYMOD_LSHIFT(keymod)) zzt_kmod_set(ZZT_KMOD_LSHIFT); else zzt_kmod_clear(ZZT_KMOD_LSHIFT);
	if (KEYMOD_CTRL(keymod)) zzt_kmod_set(ZZT_KMOD_CTRL); else zzt_kmod_clear(ZZT_KMOD_CTRL);
	if (KEYMOD_ALT(keymod)) zzt_kmod_set(ZZT_KMOD_ALT); else zzt_kmod_clear(ZZT_KMOD_ALT);
}

static SDL_Window *window;
static int charw = 8, charh = 14;
static int last_window_scale = 1;

static int charset_update_requested = 0;
static u8* charset_update_data = NULL;

static int palette_update_requested = 0;
static u32* palette_update_data = NULL;
static u8 windowed = 1;
static int windowed_old_w, windowed_old_h;

void calc_render_area(SDL_FRect *rect, int w, int h, int *scale_out, int flags) {
	int iw = 80*charw;
	int ih = 25*charh;

	int scale = 1;
	while (((scale+1)*iw <= w) && ((scale+1)*ih <= h)) scale++;
	if (scale_out != NULL) *scale_out = scale;

	if (rect != NULL) {
		w /= scale;
		h /= scale;

		if (flags & AREA_WITHOUT_SCALE) scale = 1;

		rect->x = ((w - iw) * scale) / 2;
		rect->y = ((h - ih) * scale) / 2;
		rect->w = iw * scale;
		rect->h = ih * scale;
	}
}

static void sdl_resize_window(int delta, bool only_if_too_small, bool delta_is_scale) {
	int iw = 80*charw;
	int ih = 25*charh;
	int w, h, scale;

	if (window == NULL) return;

	SDL_SetWindowMinimumSize(window, iw, ih);

	SDL_GetWindowSize(window, &w, &h);
	calc_render_area(NULL, w, h, &scale, 0);
	if (delta_is_scale) {
		scale = delta;
	} else {
		scale += delta;
	}
	if (scale < 1) scale = 1;

	last_window_scale = scale;
	iw *= scale;
	ih *= scale;

	if (windowed) {
		if (!only_if_too_small || ((iw > w) || (ih > h))) {
			SDL_SetWindowSize(window, iw, ih);
			if (delta_is_scale) {
				sdl_resize_window(0, only_if_too_small, false);
			}
		}
	}
}

static int blink_duration_ms = 0;

static bool sdl_is_blink_phase(long curr_time) {
	if (blink_duration_ms <= 0) {
		return false;
	} else {
		return ((curr_time % (blink_duration_ms*2)) >= blink_duration_ms);
	}
}

void zeta_update_blink(int blink) {
	blink_duration_ms = blink;
}

void zeta_update_charset(int width, int height, u8* data) {
	SDL_LockMutex(render_data_update_mutex);
	charw = width;
	charh = height;
	charset_update_data = data;
	charset_update_requested = 1;
#ifdef ENABLE_GIF_WRITER
	if (gif_writer_s != NULL) {
		gif_writer_on_charset_change(gif_writer_s);
	}
#endif
	SDL_UnlockMutex(render_data_update_mutex);
}

void zeta_update_palette(u32* data) {
	SDL_LockMutex(render_data_update_mutex);
	palette_update_data = data;
	palette_update_requested = 1;
#ifdef ENABLE_GIF_WRITER
	if (gif_writer_s != NULL) {
		gif_writer_on_palette_change(gif_writer_s);
	}
#endif
	SDL_UnlockMutex(render_data_update_mutex);
}

#include "../asset_loader.h"
#include "../frontend_posix.c"

#ifdef USE_OPENGL
extern sdl_renderer sdl_renderer_opengl;
#endif
extern sdl_renderer sdl_renderer_software;

static char window_name[65];

int main(int argc, char **argv) {
	int scancodes_lifted[sdl_to_pc_scancode_max + 1];
	int slc = 0;

	SDL_Event event;
	int scode, kcode;

	SDL_Thread* zzt_thread;

        SDL_SetAppMetadata("Zeta", VERSION, "pl.asie.zeta");
	if (!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed! %s", SDL_GetError());
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Could not initialize SDL!", NULL);
		return 1;
	}

	render_data_update_mutex = SDL_CreateMutex();
	zzt_thread_lock = SDL_CreateMutex();
	zzt_thread_cond = SDL_CreateCondition();
	audio_mutex = SDL_CreateMutex();

	int posix_init_result = posix_zzt_init(argc, argv);
	if (posix_init_result < 0) {
		char cwd[1025];
		char message[1281];

		getcwd(cwd, 1024);
		cwd[1024] = 0;
		fprintf(stderr, "Could not load ZZT from '%s'!\n", cwd);

		switch (posix_init_result) {
			case INIT_ERR_NO_EXECUTABLE: {
				snprintf(message, 1280, "Requested executable not found. Is your downloaded game package complete?\n\n%s", cwd);
				message[1280] = 0;
				SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Missing executable", message, NULL);
			} break;
			case INIT_ERR_NO_EXECUTABLE_ZZT: {
				snprintf(message, 1280, "ZZT.EXE not found. Is your downloaded game package complete?\n\n%s", cwd);
				message[1280] = 0;
				SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "ZZT.EXE not found", message, NULL);
			} break;
		}
		SDL_Quit();
		return 1;
	}

	snprintf(window_name, sizeof(window_name), "Zeta %s", VERSION);
	window_name[64] = 0;

	sdl_renderer *renderer = NULL;
#ifdef USE_OPENGL
	renderer = &sdl_renderer_opengl;
	if (renderer->init(window_name, charw, charh) < 0) {
		fprintf(stderr, "Could not initialize OpenGL (%s), using software renderer...", SDL_GetError());
		renderer = NULL;
	}
#endif
	if (renderer == NULL) {
		renderer = &sdl_renderer_software;
		if (renderer->init(window_name, charw, charh) < 0) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not open video device!");
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Could not open video device!", NULL);
			return 1;
		}
	}
	window = renderer->get_window();
	sdl_resize_window(0, false, false);
	SDL_StartTextInput(window);

	{
		float scale = SDL_GetWindowDisplayScale(window);
		if (scale > 1.0f) {
			sdl_resize_window((int) ceil(scale / 1.0f), false, true);
			SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
		}
	}

	const SDL_AudioSpec audio_spec = { SDL_AUDIO_S16LE, 1, 48000 };
	audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &audio_spec, audio_callback, NULL);
	if (audio_stream == 0) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not open audio device! %s", SDL_GetError());
	}

	u8 cont_loop = 1;

	zzt_thread_running = 1;
	zzt_thread = SDL_CreateThread(zzt_thread_func, "ZZT Executor", (void*)NULL);
	if (zzt_thread == NULL) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not create ZZT thread! %s", SDL_GetError());
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Could not spawn ZZT thread!", NULL);
		return 1;
	}

	audio_generate_init();
	if (audio_stream != 0) {
		// TODO: Does SDL3 allow picking a more preferable format?
		/*	audio_spec.format == SDL_AUDIO_S8 || audio_spec.format == SDL_AUDIO_S16LE,
			audio_spec.format == AUDIO_U16 || audio_spec.format == SDL_AUDIO_S16LE); */
		audio_stream_init(zeta_time_ms(), audio_spec.freq, true, true);
		if (posix_zzt_arg_note_delay >= 0.0) {
			audio_set_note_delay(posix_zzt_arg_note_delay);
		}
		SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(audio_stream));
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
				case SDL_EVENT_TEXT_INPUT:
					kcode = event.text.text[0];
					if (kcode >= 32 && kcode < 127) {
						zzt_key(kcode, TEXT_CHAR_TO_SCANCODE(kcode));
						zzt_keyup(TEXT_CHAR_TO_SCANCODE(kcode));
					}
					break;
				case SDL_EVENT_KEY_DOWN:
					if (windowed && (event.key.key == 'q' || event.key.scancode == SDL_SCANCODE_ESCAPE)) {
						if (SDL_GetWindowRelativeMouseMode(window) != 0) {
							SDL_SetWindowRelativeMouseMode(window, 0);
							break;
						}
					}
					if (event.key.key == SDLK_F11) {
						ui_activate();
					}
#ifdef ENABLE_SCREENSHOTS
					if (event.key.key == SDLK_F12) {
						FILE *file;
						char filename[24];

						int sflags = 0;
						int swidth;
						zzt_get_screen_size(&swidth, NULL);

						if (blink_duration_ms < 0) sflags |= RENDER_BLINK_OFF;
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
								zzt_get_ram() + 0xB8000, zzt_get_charset(NULL, NULL),
								charw, charh,
								zzt_get_palette()
							) < 0) {
								fprintf(stderr, "Could not write screenshot!\n");
							}
							fclose(file);
						}
						break;
					}
#endif
					if (event.key.key == SDLK_F9) {
						zzt_turbo = 1;
						break;
					}

#ifdef ENABLE_AUDIO_WRITER
					if (event.key.key == SDLK_F6 && KEYMOD_CTRL(event.key.mod)) {
						// audio writer
						if (audio_writer_s == NULL) {
							FILE *file;
							char filename[24];
							file = create_inc_file(filename, 23, "audio%d.wav", "wb");
							if (file != NULL) {
								fclose(file);
								if ((audio_writer_s = audio_writer_start(filename, audio_time, 48000)) != NULL) {
									fprintf(stderr, "Audio writing started [%s].\n", filename);
								} else {
									fprintf(stderr, "Could not start audio writing - internal error!\n");
								}
							}
						} else {
							audio_writer_stop(audio_writer_s, audio_time, 0);
							audio_writer_s = NULL;
							fprintf(stderr, "Audio writing stopped.\n");
						}
						break;
					}
#endif

#ifdef ENABLE_GIF_WRITER
					if (event.key.key == SDLK_F5 && KEYMOD_CTRL(event.key.mod)) {
						// gif writer
						if (gif_writer_s == NULL) {
							FILE *file;
							char filename[24];
							file = create_inc_file(filename, 23, "screen%d.gif", "wb");
							if (file != NULL) {
								fclose(file);
								bool optimized = !KEYMOD_SHIFT(event.key.mod);
								gif_writer_ticks = 0;
								if ((gif_writer_s = gif_writer_start(filename, optimized, true)) != NULL) {
									fprintf(stderr, "GIF writing started [%s, %s].\n", filename, optimized ? "optimized" : "unoptimized");
								} else {
									fprintf(stderr, "Could not start GIF writing - internal error!\n");
								}
							}
						} else {
							gif_writer_stop(gif_writer_s);
							gif_writer_s = NULL;
							fprintf(stderr, "GIF writing stopped.\n");
						}
						break;
					}
#endif

					if (event.key.scancode == SDL_SCANCODE_RETURN && KEYMOD_ALT(event.key.mod)) {
						// Alt+ENTER
						if (windowed) {
							SDL_GetWindowSize(window, &windowed_old_w, &windowed_old_h);
							const SDL_DisplayMode *mode = SDL_GetDesktopDisplayMode(SDL_GetDisplayForWindow(window));
							if (mode) {
								SDL_SetWindowSize(window, mode->w, mode->h);
							}
							SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);
							// force focus
							SDL_SetWindowRelativeMouseMode(window, 1);
						} else {
							SDL_SetWindowFullscreen(window, 0);
							SDL_SetWindowSize(window, windowed_old_w, windowed_old_h);
							sdl_resize_window(0, true, false);
							// drop focus
							SDL_SetWindowRelativeMouseMode(window, 0);
						}
						windowed = 1 - windowed;
						break;
					}
					update_keymod(event.key.mod);
					scode = event.key.scancode;
					kcode = event.key.key;
					if (zzt_kmod_get() & ZZT_KMOD_CTRL) {
						if (!((kcode >= 97 && kcode <= 122) || (kcode >= 65 && kcode <= 90))) break;
						kcode &= 0x1F;
						scode = 0;
					}

					// 32-126 characters are handled via SDL_EVENT_TEXT_INPUT
					if (kcode >= 32 && kcode <= 126) break;
					if (kcode < 0 || kcode >= 127) kcode = 0;
					if (scode >= 0 && scode <= sdl_to_pc_scancode_max) {
						zzt_key(kcode, sdl_to_pc_scancode[scode]);
					}
					break;
				case SDL_EVENT_KEY_UP:
					if (KEYMOD_CTRL(event.key.mod)) {
						kcode = event.key.key;
						if (kcode == '-' || kcode == SDLK_KP_MINUS) {
							sdl_resize_window(-1, false, false);
							break;
						} else if (kcode == '+' || kcode == '=' || kcode == SDLK_KP_PLUS) {
							sdl_resize_window(1, false, false);
							break;
						}
					}
					if (event.key.key == SDLK_F9) {
						zzt_turbo = 0;
						break;
					}
					update_keymod(event.key.mod);
					scode = event.key.scancode;
					kcode = event.key.key;
					if (zzt_kmod_get() & ZZT_KMOD_CTRL) {
						if (!((kcode >= 97 && kcode <= 122) || (kcode >= 65 && kcode <= 90))) break;
						kcode &= 0x1F;
						scode = 0;
					}

					if (scode >= 0 && scode <= sdl_to_pc_scancode_max) {
						// 32-126 characters are handled via SDL_EVENT_TEXT_INPUT
						if (!(kcode >= 32 && kcode <= 126)) {
							scancodes_lifted[slc++] = sdl_to_pc_scancode[scode];
						}
					}
					break;
				case SDL_EVENT_MOUSE_BUTTON_DOWN:
					if (SDL_GetWindowRelativeMouseMode(window) == 0) {
						if (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) {
							SDL_SetWindowRelativeMouseMode(window, 1);
						}
					} else {
						if (event.button.button == SDL_BUTTON_LEFT) {
							zzt_mouse_set(0);
						} else if (event.button.button == SDL_BUTTON_RIGHT) {
							zzt_mouse_set(1);
						} else if (event.button.button == SDL_BUTTON_MIDDLE) {
							zzt_mouse_set(2);
						}
					}
					break;
				case SDL_EVENT_MOUSE_BUTTON_UP:
					if (event.button.button == SDL_BUTTON_LEFT) {
						zzt_mouse_clear(0);
					} else if (event.button.button == SDL_BUTTON_RIGHT) {
						zzt_mouse_clear(1);
					} else if (event.button.button == SDL_BUTTON_MIDDLE) {
						zzt_mouse_clear(2);
					}
					break;
				case SDL_EVENT_MOUSE_MOTION:
					if (SDL_GetWindowRelativeMouseMode(window) != 0) {
						zzt_mouse_axis(0, event.motion.xrel);
						zzt_mouse_axis(1, event.motion.yrel);
					}
					break;
				case SDL_EVENT_QUIT:
					cont_loop = 0;
					break;
			}
		}

		SDL_BroadcastCondition(zzt_thread_cond);
		SDL_UnlockMutex(zzt_thread_lock);

		SDL_LockMutex(render_data_update_mutex);

		if (charset_update_requested) {
			sdl_resize_window(last_window_scale, true, true);
			renderer->update_charset(charw, charh, charset_update_data);
			charset_update_requested = 0;
		}

		if (palette_update_requested) {
			renderer->update_palette(palette_update_data);
			palette_update_requested = 0;
		}

		SDL_UnlockMutex(render_data_update_mutex);

		long curr_time = zeta_time_ms();
		int blink_mode = blink_duration_ms >= 0 ? (sdl_is_blink_phase(curr_time) ? BLINK_MODE_2 : BLINK_MODE_1) : BLINK_MODE_NONE;
		renderer->draw(zzt_vram_copy, blink_mode);
	}

#ifdef ENABLE_AUDIO_WRITER
	if (audio_writer_s != NULL) {
		audio_writer_stop(audio_writer_s, audio_time, 0);
		audio_writer_s = NULL;
	}
#endif

#ifdef ENABLE_GIF_WRITER
	if (gif_writer_s != NULL) {
		gif_writer_stop(gif_writer_s);
		gif_writer_s = NULL;
	}
#endif

	zzt_thread_running = 0;
	SDL_StopTextInput(window);
	if (audio_stream != 0) {
		SDL_CloseAudioDevice(SDL_GetAudioStreamDevice(audio_stream));
	}
	SDL_DetachThread(zzt_thread);
	renderer->deinit();

	if (developer_mode) {
		char *debug_line = malloc(1025);
		char *debug_text = malloc(131073);
		if (debug_line != NULL && debug_text != NULL) {
			debug_line[1024] = 0;
			debug_text[131072] = 0;

			// Print open file informations.
			int unclosed_files = 0;
			strcpy(debug_text, "The following files were opened, but not closed:\n");
			for (int i = 0; i < vfs_posix_get_file_pointer_count(); i++) {
				char *name = vfs_posix_get_file_pointer_name(i);
				if (name != NULL) {
					unclosed_files++;
					snprintf(debug_line, 1024, "- %s\n", name);
					strcat(debug_text, debug_line);
				}
			}
			if (unclosed_files > 0) {
				SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Developer Warning", debug_text, NULL);
			}
		}
	}
	exit_posix_vfs();

	SDL_Quit();
	return 0;
}
