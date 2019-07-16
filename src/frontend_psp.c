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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "zzt.h"
#include "posix_vfs.h"
#include "audio_stream.h"

#include <pspkernel.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspaudio.h>
#include <pspaudiolib.h>
#include <psppower.h>
#include <psputility.h>

static u32 __attribute__((aligned(16))) gu_clut4[16];
static u32 __attribute__((aligned(16))) gu_list[262144];
static u32 palette_colors[16];

#include "frontend_curses_tables.c"
#include "../obj/6x10_psp.c"

PSP_MODULE_INFO("Zeta", 0, 1, 1);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);

long zeta_time_ms(void) {
	clock_t c = clock();
	return c / (CLOCKS_PER_SEC/1000);
}

void cpu_ext_log(const char* s) {
	fprintf(stderr, "%s\n", s);
}

int zeta_has_feature(int feature) {
	return 1;
}

void zeta_update_charset(int width, int height, u8* data) {
}

void zeta_update_palette(u32* data) {
	for (int i = 0; i < 16; i++) {
		palette_colors[i] = (data[i] & 0xFF00) | ((data[i] >> 16) & 0xFF) | ((data[i] & 0xFF) << 16) | 0xFF000000;
	}
}

typedef struct {
	u32 color;
	u16 x, y, z, v_pad;
} point_bg;

typedef struct {
	u16 u, v;
	u32 color;
	u16 x, y, z, v_pad;
} point_fg;

#define FRAME_Y_OFFSET ((272-250)/2)

int psp_exit_cb(int a1, int a2, void *a3) {
	sceKernelExitGame();
	return 0;
}

int psp_exit_thread(SceSize args, void *argp) {
	int cbid = sceKernelCreateCallback("exit callback", (void*) psp_exit_cb, NULL);
	sceKernelRegisterExitCallback(cbid);
	sceKernelSleepThreadCB();
	return 0;
}

static int zzt_thread_running = 1;
static int psp_osk_open = 0;
static long timer_last_ms;

int psp_zzt_thread(SceSize args, void *argp) {
	int opcodes = 1000;

	timer_last_ms = zeta_time_ms();
	while (zzt_thread_running) {
		if (psp_osk_open) {
			sceKernelDelayThreadCB(20 * 1000);
			continue;
		}

		long duration = zeta_time_ms();
		int rcode = zzt_execute(opcodes);
		long timer_curr_ms = zeta_time_ms();
		duration = timer_curr_ms - duration;
		if (rcode == STATE_CONTINUE) {
			if (duration < 2) {
				opcodes = (opcodes * 20 / 19);
			} else if (duration > 4) {
				opcodes = (opcodes * 19 / 20);
			}
		}
		if (rcode == STATE_END) {
			zzt_thread_running = 0;
		}

		sceKernelDelayThreadCB(1);
	}

	return 0;
}

static long first_timer_tick;
static double timer_time;

static u16 osk_text[65];
static int osk_text_pos = 0;
static int osk_counter = 0;

int psp_timer_thread(SceSize args, void *argp) {
	while (zzt_thread_running) {
		long curr_timer_tick = zeta_time_ms();
		if (psp_osk_open) {
			timer_time = curr_timer_tick - first_timer_tick;
			sceKernelDelayThreadCB(20 * 1000);
			continue;
		}

		zzt_mark_timer();

		timer_time += SYS_TIMER_TIME;
		long duration = curr_timer_tick - first_timer_tick;
		long tick_time = ((long) (timer_time + SYS_TIMER_TIME)) - duration;

		while (tick_time <= 0) {
			zzt_mark_timer();
			timer_time += SYS_TIMER_TIME;
			tick_time = ((long) (timer_time + SYS_TIMER_TIME)) - duration;
		}

		osk_counter = (osk_counter + 1) % 3;
		if (osk_counter == 0 && osk_text[0] != 0) {
			u16 chr = osk_text[osk_text_pos++];
			if (chr == 0) {
				osk_text[0] = 0;
				osk_text_pos = 0;
				zzt_key(13, 0x1C);
				zzt_keyup(0x1C);
			} else {
				if (chr >= 32 && chr <= 127) {
					zzt_key(chr, map_char_to_key[chr]);
					zzt_keyup(map_char_to_key[chr]);
				}
			}
		}

		sceKernelDelayThreadCB(tick_time * 1000);
	}

	return 0;
}

static void psp_timer_init(void) {
	int thid = sceKernelCreateThread("timer", psp_timer_thread, 0x11, 0xFA0, PSP_THREAD_ATTR_USER, NULL);
	if (thid >= 0) {
		sceKernelStartThread(thid, 0, 0);
	}
}

static int psp_is_blink_phase(long curr_time) {
	return ((curr_time % (BLINK_TOGGLE_DURATION_MS*2)) >= BLINK_TOGGLE_DURATION_MS);
}

static void psp_draw_frame(void) {
	sceGuStart(GU_DIRECT, gu_list);

	// draw 2000 BG cells
	point_bg *bg_cells = sceGuGetMemory(sizeof(point_bg) * 4000);
	// draw 2000 FG cells
	point_fg *fg_cells = sceGuGetMemory(sizeof(point_fg) * 4000);
	point_bg *bg_cells_origin = bg_cells;
	point_fg *fg_cells_origin = fg_cells;

	u8* ram = zzt_get_ram();
	int i = 0;
	int should_blink = psp_is_blink_phase(zeta_time_ms());

	for (int y = 0; y < 25; y++) {
		u16 cy0 = y*10+FRAME_Y_OFFSET;
		u16 cy1 = (y+1)*10+FRAME_Y_OFFSET;
		for (int x = 0; x < 80; x++, i+=2) {
			u8 chr = ram[TEXT_ADDR(x,y)];
			u8 col = ram[TEXT_ADDR(x,y)+1];
			int should_blink_now = 0;
			if (col >= 0x80) {
				col &= 0x7F;
				should_blink_now = should_blink;
			}
			u32 bg_col = palette_colors[col >> 4];
			u32 fg_col = palette_colors[col & 0xF];
			u16 cx0 = x*6;
			u16 cx1 = (x+1)*6;
			u32 cu = (chr & 31)<<3;
			u32 cv = (chr >> 5)<<4;

			bg_cells[0].color = bg_col;
			bg_cells[0].x = cx0;
			bg_cells[0].y = cy0;
			bg_cells[0].z = 0;
			bg_cells[1].color = bg_col;
			bg_cells[1].x = cx1;
			bg_cells[1].y = cy1;
			bg_cells[1].z = 0;
			bg_cells += 2;

			if (!should_blink_now && ((col ^ (col >> 4)) & 0x0F) && chr != 0 && chr != 32) {
				fg_cells[0].u = cu;
				fg_cells[0].v = cv;
				fg_cells[0].color = fg_col;
				fg_cells[0].x = cx0;
				fg_cells[0].y = cy0;
				fg_cells[0].z = 0;
				fg_cells[1].u = cu+6;
				fg_cells[1].v = cv+10;
				fg_cells[1].color = fg_col;
				fg_cells[1].x = cx1;
				fg_cells[1].y = cy1;
				fg_cells[1].z = 0;
				fg_cells += 2;
			}
		}
	}

	sceGuDisable(GU_TEXTURE_2D);
 	sceGumDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, (bg_cells - bg_cells_origin), 0, bg_cells_origin);
	sceGuEnable(GU_TEXTURE_2D);
	sceGumDrawArray(GU_SPRITES, GU_TEXTURE_16BIT | GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D, (fg_cells - fg_cells_origin), 0, fg_cells_origin);

	sceGuFinish();
	sceGuSync(0, 0);

	sceDisplayWaitVblankStartCB();
	sceGuSwapBuffers();

	zzt_mark_frame();
}

void psp_audio_callback(void *stream, unsigned int len, void *userdata) {
	if (psp_osk_open) {
		memset(stream, 0, len*sizeof(s16)*2);
		return;
	}

	u8 *stream_u8 = ((u8*) stream) + (len * 3);
	s16 *stream_s16 = ((s16*) stream);

	audio_stream_generate_u8(zeta_time_ms(), stream_u8, len);
	for (int i = 0; i < len; i++, stream_u8++, stream_s16+=2) {
		s8 sample_s8 = (s8) (stream_u8[0] ^ 0x80);
		s16 val = ((s16) sample_s8) << 8;
		stream_s16[0] = (s16) val;
		stream_s16[1] = (s16) val;
	}
}

void speaker_on(double freq) {
	audio_stream_append_on(zeta_time_ms(), freq);
}

void speaker_off(void) {
	audio_stream_append_off(zeta_time_ms());
}

static void psp_init_vfs(void) {
	char buf[MAXPATHLEN+1];

	if (getcwd(buf, MAXPATHLEN) == NULL || strlen(buf) <= 0) {
		fprintf(stderr, "psp_init_vfs: getcwd failed, using fallback\n");
		if (chdir("ms0:/PSP/GAME/ZETA/") != 0) {
			fprintf(stderr, "psp_init_vfs: chdir failed, using fallback\n");
			init_posix_vfs("ms0:/PSP/GAME/ZETA/");
			return;
		}
	}

	init_posix_vfs("");
}

int main(int argc, char** argv) {
	SceCtrlData pad;
	u32 last_buttons = 0;

	{
		psp_init_vfs();
		zzt_init();

		int exeh = vfs_open("zzt.exe", 0);
		if (exeh < 0) return -1;
		zzt_load_binary(exeh, "");
		vfs_close(exeh);

		zzt_set_timer_offset((time(NULL) % 86400) * 1000L);
		zzt_key('k', 0x25);
		zzt_key('c', 0x2E);
		zzt_key(13, 0x1C);

		init_map_char_to_key();
	}

	{
		for(int i = 0; i < 15; i++)
			gu_clut4[i] = 0;
		gu_clut4[15] = 0xFFFFFFFF;

		sceGuInit();
		sceGuStart(GU_DIRECT, gu_list);
		sceGuDrawBuffer(GU_PSM_8888, NULL, 512);
		sceGuDispBuffer(480, 272, (void*) 0x88000, 512);
		sceGuDepthBuffer((void*) 0x110000, 512);
		sceGuOffset(2048 - 240, 2048 - 136);
		sceGuViewport(2048, 2048, 480, 272);
		sceGuDepthRange(0xC350, 0x2710);
		sceGuScissor(0, 0, 480, 272);
		sceGuEnable(GU_SCISSOR_TEST);
		sceGuDisable(GU_DEPTH_TEST);
		sceGuShadeModel(GU_FLAT);
		sceGuAlphaFunc(GU_GREATER, 0, 0xFF);
		sceGuEnable(GU_ALPHA_TEST);
		sceGuEnable(GU_TEXTURE_2D);
		sceGuClutMode(GU_PSM_8888, 0, 0x0F, 0);
		sceGuClutLoad(2, gu_clut4);
		sceGuTexMode(GU_PSM_T4, 0, 0, 0);
		sceGuTexImage(0, 256, 128, 256, obj_6x10_psp_bin);
		sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
		sceGuTexEnvColor(0x0);
		sceGuTexOffset(0.0f, 0.0f);
		sceGuTexScale(1.0f / 256.0f, 1.0f / 128.0f);
		sceGuTexWrap(GU_REPEAT, GU_REPEAT);
		sceGuTexFilter(GU_NEAREST, GU_NEAREST);
		sceGuFinish();
		sceGuSync(0, 0);
		sceGuDisplay(GU_TRUE);
	}

	psp_timer_init();

	// TODO: Can we control this dynamically?
	scePowerSetClockFrequency(300, 300, 150);

	{
		int thid = sceKernelCreateThread("zzt", psp_zzt_thread, 0x1C, 0xFA0, PSP_THREAD_ATTR_USER, NULL);
		if (thid >= 0) {
			sceKernelStartThread(thid, 0, 0);
		} else {
			return 1;
		}
	}

	{
		int thid = sceKernelCreateThread("exit handler", psp_exit_thread, 0x1E, 0xFA0, PSP_THREAD_ATTR_USER, NULL);
		if (thid >= 0) {
			sceKernelStartThread(thid, 0, 0);
		} else {
			return 1;
		}
	}

	audio_stream_init(zeta_time_ms(), 44100);
	audio_stream_set_volume(audio_stream_get_max_volume() >> 1);

	pspAudioInit();
	pspAudioSetChannelCallback(0, psp_audio_callback, NULL);

	clock_t last = clock();
	clock_t curr = last;

	sceCtrlSetSamplingCycle(0);
	sceCtrlSetSamplingMode(PSP_CTRL_MODE_DIGITAL);

	while (1) {
		curr = clock();
		last = curr;

		sceCtrlReadBufferPositive(&pad, 1);

		u32 bp = (~last_buttons) & (pad.Buttons);
		u32 br = (~(pad.Buttons)) & last_buttons;
		last_buttons = pad.Buttons;
		if (bp != 0) {
			if (bp & PSP_CTRL_UP) zzt_key(0, 0x48);
			if (bp & PSP_CTRL_LEFT) zzt_key(0, 0x4B);
			if (bp & PSP_CTRL_DOWN) zzt_key(0, 0x50);
			if (bp & PSP_CTRL_RIGHT) zzt_key(0, 0x4D);
			if (bp & PSP_CTRL_CROSS) zzt_kmod_set(0x01);
			if (bp & PSP_CTRL_CIRCLE) zzt_key(13, 0x1C);
			if (bp & PSP_CTRL_SELECT) zzt_key('w', 16);
			if (bp & PSP_CTRL_START) {
				if (pad.Buttons & PSP_CTRL_LTRIGGER) {
					zzt_key('q', 15);
					zzt_keyup(15);
				} else {
					zzt_key('p', 24);
					zzt_keyup(24);
				}
			}
			if (bp & PSP_CTRL_SQUARE) {
				if (pad.Buttons & PSP_CTRL_LTRIGGER) {
					zzt_key('r', 30);
					zzt_keyup(30);
				} else {
					zzt_key('t', 19);
					zzt_keyup(19);
				}
			}
			if (bp & PSP_CTRL_TRIANGLE) {
				if (pad.Buttons & PSP_CTRL_LTRIGGER) {
					zzt_key('s', 30);
					zzt_keyup(30);
				} else {
					zzt_key(0x1C, 1);
					zzt_keyup(1);
				}
			}
		}

		if (br != 0) {
			if (br & PSP_CTRL_UP) zzt_keyup(0x48);
			if (br & PSP_CTRL_LEFT) zzt_keyup(0x4B);
			if (br & PSP_CTRL_DOWN) zzt_keyup(0x50);
			if (br & PSP_CTRL_RIGHT) zzt_keyup(0x4D);
			if (br & PSP_CTRL_CROSS) zzt_kmod_clear(0x01);
			if (br & PSP_CTRL_CIRCLE) zzt_keyup(0x1C);
			if (br & PSP_CTRL_SELECT) zzt_keyup(16);
			if (br & PSP_CTRL_RTRIGGER && osk_text[0] == 0) {
				psp_osk_open = 1;
				unsigned short desctext[33];
				unsigned short intext[65];
				SceUtilityOskData data[1];
				SceUtilityOskParams params;

				desctext[0] = 'K';
				desctext[1] = 'e';
				desctext[2] = 'y';
				desctext[3] = 'b';
				desctext[4] = 'o';
				desctext[5] = 'a';
				desctext[6] = 'r';
				desctext[7] = 'd';
				desctext[8] = ':';
				intext[0] = 0;
				memset(osk_text, 0, sizeof(osk_text));

				data[0].language = PSP_UTILITY_OSK_LANGUAGE_ENGLISH;
				data[0].lines = 1;
				data[0].unk_24 =  1;
				data[0].inputtype = PSP_UTILITY_OSK_INPUTTYPE_ALL;
				data[0].desc = desctext;
				data[0].intext = intext;
				data[0].outtextlength = 64;
				data[0].outtextlimit = 64;
				data[0].outtext = osk_text;

				memset(&params, 0, sizeof(params));
				params.base.size = sizeof(params);
				sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &params.base.language);
				sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_UNKNOWN, &params.base.buttonSwap);
				params.base.graphicsThread = 18;
				params.base.accessThread = 20;
				params.base.fontThread = 19;
				params.base.soundThread = 17;
				params.datacount = 1;
				params.data = data;

				sceUtilityOskInitStart(&params);
				while (psp_osk_open) {
					sceGuStart(GU_DIRECT, gu_list);
					sceGuClearColor(0);
					sceGuClearDepth(0);
					sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
					sceGuFinish();
					sceGuSync(0, 0);
					switch (sceUtilityOskGetStatus()) {
						case PSP_UTILITY_DIALOG_VISIBLE:
							sceUtilityOskUpdate(1);
							break;
						case PSP_UTILITY_DIALOG_QUIT:
							sceUtilityOskShutdownStart();
							break;
						case PSP_UTILITY_DIALOG_NONE:
							psp_osk_open = 0;
							break;
					}
					sceDisplayWaitVblankStartCB();
					sceGuSwapBuffers();
				}

				if (data[0].result != PSP_UTILITY_OSK_RESULT_CHANGED) {
					osk_text[0] = 0;
				}
			}
		}

		psp_draw_frame();
	}

	sceGuDisplay(GU_FALSE);
	sceGuTerm();

	return 0;
}
