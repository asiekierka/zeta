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

#include <string.h>
#include "zzt.h"

#ifdef EMSCRIPTEN
#define fprintf(...)
#else
# ifdef ANDROID
#include <android/log.h>
#define fprintf(f, ...) __android_log_print(ANDROID_LOG_INFO, "ZetaNative", __VA_ARGS__)
# else
#include <stdio.h>
# endif
#endif

#if defined(ANDROID) || defined(HAS_OSK)
#define KEYBUF_SIZE 192
#else
#define KEYBUF_SIZE 8
#endif

// #define DEBUG_FS_ACCESS
// #define DEBUG_INTERRUPTS
// #define DEBUG_KEYSTROKES

#define STR_DS_DX (char*)(&cpu->ram[cpu->seg[SEG_DS]*16 + cpu->dx])
#define U8_ES_BP (u8*)(&cpu->ram[cpu->seg[SEG_ES]*16 + cpu->bp])

typedef struct {
	int qch;
	int qke;
} zzt_keybuf_entry;

typedef struct {
	int qch;
	int qke;
	long time;
	u8 repeat;
} zzt_key_entry;

extern unsigned char res_8x14_bin[];

static u32 def_palette[] = {
	0xff000000, 0xff0000aa, 0xff00aa00, 0xff00aaaa,
	0xffaa0000, 0xffaa00aa, 0xffaa5500, 0xffaaaaaa,
	0xff555555, 0xff5555ff, 0xff55ff55, 0xff55ffff,
	0xffff5555, 0xffff55ff, 0xffffff55, 0xffffffff
};

typedef struct {
	cpu_state cpu;
	long real_time;
	long timer_time_offset;
	double timer_time;

	// keyboard
	int key_delay, key_repeat_delay;
	zzt_key_entry key;
	zzt_keybuf_entry keybuf[KEYBUF_SIZE];
	int kmod;

	// joystick
	u8 joy_xstrobe_val, joy_ystrobe_val;
	u8 joy_xstrobes, joy_ystrobes;

	// mouse
	u16 mouse_buttons;
	u16 mouse_x, mouse_y;
	s16 mouse_xd, mouse_yd;

	// I/O
	u8 cga_status, cga_palette, cga_crt_index;
	u8 port_42_latch;
	u16 port_42;
	// u8 port_43[3]; (not actually used)
	u8 port_61;
	u8 port_201;

	// DOS
	u32 dos_dta;

	u8 charset[256*16];
	u32 palette[16];
} zzt_state;

zzt_state zzt;

void zzt_kmod_set(int mod) {
	zzt.kmod |= mod;
}

void zzt_kmod_clear(int mod) {
	zzt.kmod &= ~mod;
}

static long zzt_internal_time(void) {
	return zzt.timer_time_offset + ((long) zzt.timer_time);
//	return (long) (zzt.timer_time + (zeta_time_ms() - zzt.real_time));
}

static int zzt_key_append(int qch, int qke) {
	for (int j = 0; j < KEYBUF_SIZE; j++) {
		if (zzt.keybuf[j].qke == -1 || zzt.keybuf[j].qke == qke) {
#ifdef DEBUG_KEYSTROKES
			fprintf(stderr, "key appended %d @ %d\n", qke, j);
#endif
			zzt.keybuf[j].qch = qch;
			zzt.keybuf[j].qke = qke;
			return 1;
		}
	}
	fprintf(stderr, "key not appended %d\n", qke);
	return 0;
}

int zzt_key_get_delay(void) {
	return zzt.key_delay;
}

int zzt_key_get_repeat_delay(void) {
	return zzt.key_repeat_delay;
}

void zzt_key_set_delay(int ms, int repeat_ms) {
	zzt.key_delay = ms;
	zzt.key_repeat_delay = repeat_ms;
}

void zzt_key(int c, int k) {
	// cull repeat presses
	if (zzt.key.qke == k) {
		return;
	}

	zzt.key.qch = ((c & 0x7F) == c) ? c : 0;
	zzt.key.qke = k;
	zzt.key.time = zeta_time_ms();
	zzt.key.repeat = 0;
#ifdef DEBUG_KEYSTROKES
	fprintf(stderr, "key down %d %d\n", zzt.key.qch, zzt.key.qke);
#endif
	zzt_key_append(zzt.key.qch, zzt.key.qke);
}

void zzt_keyup(int k) {
	int changed = 0;

	if (zzt.key.qke == k) {
#ifdef DEBUG_KEYSTROKES
		fprintf(stderr, "key up %d %d\n", zzt.key.qch, zzt.key.qke);
#endif
		zzt.key.qke = -1;
		changed |= zzt.key.repeat;
	}

	if (changed) {
		// if the key was in repeat mode, cull existing occurences to clear up the queue
		for (int i = 0; i < KEYBUF_SIZE; i++) {
			if (zzt.keybuf[i].qke == k) {
				for (int j = i+1; j < KEYBUF_SIZE; j++) {
					zzt.keybuf[j-1] = zzt.keybuf[j];
				}
				zzt.keybuf[KEYBUF_SIZE-1].qke = -1;
				i--;
			}
		}
	}
}

static void cpu_func_intr_0x10(cpu_state* cpu);
static void cpu_func_intr_0x13(cpu_state* cpu);
static int cpu_func_intr_0x16(cpu_state* cpu);
static int cpu_func_intr_0x21(cpu_state* cpu);

void zzt_mark_frame(void) {
	zzt.cga_status |= 0x8;
}

#define JOY_MIN 3
#define JOY_MAX 51
#define JOY_MID (JOY_MIN+JOY_MAX/2)
#define JOY_RANGE (JOY_MAX-JOY_MIN)

void zzt_joy_set(int button) {
	if (button < 2) zzt.port_201 &= ~(1 << (button + 4));
}

void zzt_joy_clear(int button) {
	if (button < 2) zzt.port_201 |= (1 << (button + 4));
}

void zzt_joy_axis(int axis, int value) {
	if (axis >= 2) return;

	value = ((value + 127) * JOY_RANGE / 254) + JOY_MIN;
	switch (axis) {
		case 0: zzt.joy_xstrobe_val = value; break;
		case 1: zzt.joy_ystrobe_val = value; break;
	}
}

static void zzt_joy_strobe(zzt_state* zzt) {
	if (zzt->joy_xstrobes == 0) zzt->cpu.keep_going++;
	if (zzt->joy_ystrobes == 0) zzt->cpu.keep_going++;
	zzt->joy_xstrobes = zzt->joy_xstrobe_val;
	zzt->joy_ystrobes = zzt->joy_ystrobe_val;
}

void zzt_mouse_set(int button) {
	zzt.mouse_buttons |= 1 << button;
}

void zzt_mouse_clear(int button) {
	zzt.mouse_buttons &= ~(1 << button);
}

static inline int m_clamp(int v, int min, int max) {
	return (v > min) ? ((v < max) ? v : max) : min;
}

void zzt_mouse_axis(int axis, int value) {
	switch (axis) {
		case 0:
			zzt.mouse_xd += value;
			zzt.mouse_x = m_clamp(zzt.mouse_x + value, 0, 639);
			break;
		case 1:
			zzt.mouse_yd += value;
			zzt.mouse_y = m_clamp(zzt.mouse_y + value, 0, 349);
			break;
	}
}

static u16 cpu_func_port_in_main(cpu_state* cpu, u16 addr) {
	zzt_state* zzt = (zzt_state*) cpu;

	switch (addr) {
		case 0x61:
			return zzt->port_61;
		case 0x201:
			if (!zeta_has_feature(FEATURE_JOY_CONNECTED))
				return 0xF0;
			zzt->port_201 &= 0xF0;
			if (zzt->joy_xstrobes > 0) {
				zzt->joy_xstrobes--;
				if (zzt->joy_xstrobes == 0) cpu->keep_going--;
				zzt->port_201 |= 1;
			}
			if (zzt->joy_ystrobes > 0) {
				zzt->joy_ystrobes--;
				if (zzt->joy_ystrobes == 0) cpu->keep_going--;
				zzt->port_201 |= 2;
			}
			return zzt->port_201;
		case 0x3D4: return zzt->cga_crt_index;
		case 0x3D5:
			fprintf(stderr, "CRT port in %02X\n", zzt->cga_crt_index);
			return 0;
		case 0x3D9: return zzt->cga_palette;
		case 0x3DA: {
			int old_status = zzt->cga_status;
			zzt->cga_status = (old_status & (~0x8)) ^ 0x1;
			return old_status;
		}
		default:
			fprintf(stderr, "port in %04X\n", addr);
			return 0;
	}
}

static void cpu_func_port_out_main(cpu_state* cpu, u16 addr, u16 val) {
	zzt_state* zzt = (zzt_state*) cpu;

	switch (addr) {
		case 0x42:
			if (zzt->port_42_latch) zzt->port_42 = (zzt->port_42 & 0xFF) | ((val << 8) & 0xFF00);
			else zzt->port_42 = (zzt->port_42 & 0xFF00) | (val & 0xFF);
			zzt->port_42_latch ^= 1;
//			if (!port_42_latch && (port_43[2] & 0x04) == 0x04 && (port_61 & 3) == 3) {
			if (!(zzt->port_42_latch) && (zzt->port_61 & 3) == 3) {
				speaker_on(1193182.0 / zzt->port_42);
			}
			return;
		case 0x43: {
/*			int addr = (val >> 6) & 0x3;
			if (addr == 3) return;
			if ((val & 0x30) == 0) {
				zzt->port_43[addr] = val;
			} */
		} return;
		case 0x61:
			zzt->port_61 = val;
			if ((val & 3) != 3) {
				speaker_off();
			}
			return;
		case 0x201:
			zzt_joy_strobe(zzt);
			return;
		case 0x3D4: zzt->cga_crt_index = val; return;
		case 0x3D5:
			switch (zzt->cga_crt_index) {
				case 0x0E: case 0x0F:
					// cursor location high, low
					// we do not render the cursor, so we do not need those values
					break;
				default:
					fprintf(stderr, "CRT port out %02X = %02X\n", zzt->cga_crt_index, val);
			}
			return;
		case 0x3D9: zzt->cga_palette = val; return;
		default:
			fprintf(stderr, "port out %04X = %04X\n", addr, val);
	}
}

static void cpu_func_intr_0x33(cpu_state* cpu) {
	zzt_state* zzt = (zzt_state*) cpu;

	switch (cpu->ax) {
		case 0:
			if (zeta_has_feature(FEATURE_MOUSE_CONNECTED)) {
				cpu->ax = 0xFFFF;
				cpu->bx = 0xFFFF;
			}
			break;
		case 3:
			cpu->bx = zzt->mouse_buttons;
			cpu->cx = zzt->mouse_x / 8;
			cpu->dx = zzt->mouse_y / 14;
			break;
		case 0xB:
			cpu->cx = zzt->mouse_xd;
			cpu->dx = zzt->mouse_yd;
			zzt->mouse_xd = 0;
			zzt->mouse_yd = 0;
			break;
		default:
			fprintf(stderr, "mouse %04X\n", cpu->ax);
			break;
	}
}

void zzt_set_timer_offset(long time) {
	zzt.timer_time_offset = time;
}

static int cpu_func_interrupt_main(cpu_state* cpu, u8 intr) {
#ifdef DEBUG_INTERRUPTS
	fprintf(stderr, "dbg: interrupt %02X %04X\n", intr, cpu->ax);
#endif
	switch (intr) {
		case 0x11:
			cpu->al = cpu->ram[0x410];
			cpu->ah = cpu->ram[0x411];
			break;
		case 0x12:
			cpu->al = cpu->ram[0x413];
			cpu->ah = cpu->ram[0x414];
			break;
		case 0x08: {
			u32 time = cpu->ram[0x46c] | (cpu->ram[0x46d] << 8)
				| (cpu->ram[0x46e] << 16) | (cpu->ram[0x46f] << 24);
			time++;
			cpu->ram[0x46c] = time & 0xFF;
			cpu->ram[0x46d] = (time>>8) & 0xFF;
			cpu->ram[0x46e] = (time>>16) & 0xFF;
			cpu->ram[0x46f] = (time>>24) & 0xFF;
			cpu_emit_interrupt(cpu, 0x1C);
		} break;
		case 0x1C: break;
		case 0x10: cpu_func_intr_0x10(cpu); break;
		case 0x13: cpu_func_intr_0x13(cpu); break;
		case 0x16: return cpu_func_intr_0x16(cpu);
		case 0x20: return STATE_END;
		case 0x21: return cpu_func_intr_0x21(cpu);
		case 0x33: cpu_func_intr_0x33(cpu); break;
		case 0x15:
			fprintf(stderr, "sysconf %04X\n", cpu->ax);
			cpu->ah = 0x86;
			cpu->flags |= FLAG_CARRY;
			break;
		default:
			fprintf(stderr, "unknown interrupt %02X %04X\n", intr, cpu->ax);
			break;
	}

	return STATE_CONTINUE;
}

static void video_scroll_up(cpu_state* cpu, int lines, u8 empty_attr, int y1, int x1, int y2, int x2) {
	if (lines <= 0) {
		for (int y = y1; y <= y2; y++)
		for (int x = x1; x <= x2; x++) {
			cpu->ram[TEXT_ADDR(x,y)] = 0;
			cpu->ram[TEXT_ADDR(x,y)+1] = empty_attr;
		}
	} else {
		for (int y = y1+lines; y <= y2; y++)
		for (int x = x1; x <= x2; x++) {
			cpu->ram[TEXT_ADDR(x,y-lines)] = cpu->ram[TEXT_ADDR(x,y)];
			cpu->ram[TEXT_ADDR(x,y-lines)+1] = cpu->ram[TEXT_ADDR(x,y)+1];
		}

		for (int y = y2-lines+1; y <= y2; y++)
		for (int x = x1; x <= x2; x++) {
			cpu->ram[TEXT_ADDR(x,y)] = 0;
			cpu->ram[TEXT_ADDR(x,y)+1] = empty_attr;
		}
	}
}

static void cpu_0x10_output(cpu_state* cpu, u8 chr) {
	u8 cursor_width = cpu->ram[0x44A];
	u8 cursor_height = 25;

	switch (chr) {
		case 0x0D:
			cpu->ram[0x450] = 0;
			break;
		case 0x0A:
			if (cpu->ram[0x451] < (cursor_height - 1)) {
				cpu->ram[0x451]++;
			} else {
				video_scroll_up(cpu, 1, 0x07, 0, 0, cursor_height - 1, cursor_width - 1);
			}
			break;
		case 0x08:
			if (cpu->ram[0x450] > 0)
				cpu->ram[0x450]--;
			cpu->ram[TEXT_ADDR(cpu->ram[0x450], cpu->ram[0x451])] = 0;
			break;
		case 0x07:
			break;
		default:
			cpu->ram[TEXT_ADDR(cpu->ram[0x450], cpu->ram[0x451])] = chr;
			cpu->ram[0x450]++;
			if (cpu->ram[0x450] >= cursor_width) {
				cpu->ram[0x450] = 0;
				if (cpu->ram[0x451] < (cursor_height - 1)) {
					cpu->ram[0x451]++;
				} else {
					video_scroll_up(cpu, 1, 0x07, 0, 0, cursor_height - 1, cursor_width - 1);
				}
			}
			break;
	}
}

static int video_mode = 3;
int zzt_video_mode(void) {
	return video_mode;
}

static void cpu_func_intr_0x10(cpu_state* cpu) {
	switch (cpu->ah) {
		case 0x00: // set video mode
			video_mode = cpu->al & 0x7F;
			return;
		case 0x01: // cursor shape
			// fprintf(stderr, "int 0x10 set cursor shape %04X\n", cpu->cx);
			return;
		case 0x02:
			cpu->ram[0x451] = cpu->dh;
			cpu->ram[0x450] = cpu->dl;
			return;
		case 0x03:
			cpu->dh = cpu->ram[0x451];
			cpu->dl = cpu->ram[0x450];
			return;
		case 0x04: // light pen position
			cpu->ah = 0;
			return;
		case 0x05: // set active page
			// BQ installer changes the active page to zero.
			// In general, we only support zero. So ignore.
			return;
		case 0x06: // scroll up
			video_scroll_up(cpu, cpu->al, cpu->bh, cpu->ch, cpu->cl, cpu->dh, cpu->dl);
			return;
		case 0x08: {
			u32 addr = TEXT_ADDR(cpu->bl, cpu->bh);
			cpu->ah = 0x08;
			cpu->al = cpu->ram[addr];
			cpu->bh = cpu->ram[addr+1];
		} return;
		case 0x09:
		case 0x0A: {
			u32 addr = TEXT_ADDR(cpu->bl, cpu->bh);
			for (int i = 0; i < cpu->cx && addr < 160*25; i++, addr+=2) {
				cpu->ram[addr] = cpu->al;
				if (cpu->ah == 0x09) cpu->ram[addr + 1] = cpu->bl;
			}
		} return;
		case 0x0E:
			cpu_0x10_output(cpu, cpu->al);
			return;
		case 0x0F: // query
			cpu->ah = cpu->ram[0x44A];
			cpu->al = video_mode;
			cpu->bh = 0; // active page
			return;
		case 0x11:
			switch (cpu->al) {
				case 0x00:
				case 0x10: {
					if (cpu->bh != 14) {
						fprintf(stderr, "int 0x10: character loading failed - non-8x14 character sizes unsupported!\n");
						return;
					}
#ifdef DEBUG_INTERRUPTS
					fprintf(stderr, "int 0x10: load %d characters from %d (%d bytes each), block %d\n", cpu->cx, cpu->dx, cpu->bh, cpu->bl);
#endif
					int size = cpu->bh * cpu->cx;
					int outpos = cpu->dx * cpu->bh;
					u8* buffer = U8_ES_BP;
					for (int i = 0; i < size; i++) {
						if ((outpos + i) >= (256*14)) break;
						zzt.charset[outpos + i] = buffer[i];
					}

					zeta_update_charset(8, 14, zzt.charset);
				} return;
			}
			break;
		case 0x12:
			switch (cpu->bl) {
				case 0x30: {
					// set vertical resolution
					// some font installers like to change it, but ZZT always changes it back to 1.
					fprintf(stderr, "int 0x10: set vertical resolution = %d (ignored)\n", cpu->al);
				} return;
			}
			break;
	}

	fprintf(stderr, "int 0x10 AX=%04X AH=%02X AL=%02X BL=%02X\n",
		cpu->ax, cpu->ah, cpu->al, cpu->bl);
	cpu->flags |= FLAG_CARRY;
}

static void cpu_func_intr_0x13(cpu_state* cpu) {
	fprintf(stderr, "int 0x13 AX=%04X\n", cpu->ax);
}

// ZZT calls this once a "frame". But let's give it a bit of a buffer,
// in case this doesn't always hold true.
static long kbd_call_time = 0;
static int kbd_call_count = 0;

static int cpu_func_intr_0x16(cpu_state* cpu) {
	zzt_state* zzt = (zzt_state*) cpu;

	if (cpu->ah == 0x00) {
		if (zzt->keybuf[0].qke >= 0) {
			cpu->flags &= ~FLAG_ZERO;
			cpu->ah = zzt->keybuf[0].qke;
			cpu->al = zzt->keybuf[0].qch;

			// rotate keybuf
			for (int i = 1; i < KEYBUF_SIZE; i++) {
				zzt->keybuf[i-1] = zzt->keybuf[i];
			}
			zzt->keybuf[KEYBUF_SIZE-1].qke = -1;
		} else {
			return STATE_BLOCK;
		}
		return STATE_CONTINUE;
	} else if (cpu->ah == 0x01) {
		if (zzt->keybuf[0].qke >= 0) {
			cpu->flags &= ~FLAG_ZERO;
			cpu->ah = zzt->keybuf[0].qke;
			cpu->al = zzt->keybuf[0].qch;
		} else {
			cpu->flags |= FLAG_ZERO;
			if (kbd_call_time != zzt_internal_time()) {
				kbd_call_time = zzt_internal_time();
				kbd_call_count = 0;
			}
			if ((++kbd_call_count) >= 4) {
				kbd_call_count = 0;
				return STATE_WAIT;
			}
		}
		return STATE_CONTINUE;
	} else if (cpu->ah == 0x02) {
		cpu->al = zzt->kmod;
		return STATE_CONTINUE;
	} else if (cpu->ah == 0x03) {
		// typematic control; not used by ZZT,
		// but can be used by external tools
		switch (cpu->al) {
			case 0x00:
				zzt->key_delay = 500;
				zzt->key_repeat_delay = 100;
				break;
			case 0x04:
				zzt->key_repeat_delay = 0;
				break;
			case 0x05:
				zzt->key_delay = 250 * (cpu->bh & 0x03);
				double krd_tmp = 1.0 / ((8 + (cpu->bl & 7)) * (1 << ((cpu->bl >> 3) & 3)) * 0.00417);
				zzt->key_repeat_delay = (int) (1000 / krd_tmp);
				break;
			default:
				fprintf(stderr, "int 0x16:0x03 subfunction=%02X\n", cpu->al);
				break;
		}
		return STATE_CONTINUE;
	} else if (cpu->ah == 0x09) {
		// check subfunctions; see 0x03
		cpu->al = 0x07;
		return STATE_CONTINUE;
	}
	fprintf(stderr, "int 0x16 AX=%04X\n", cpu->ax);
	return STATE_CONTINUE;
}

static int cpu_func_intr_0x21(cpu_state* cpu) {
	zzt_state* zzt = (zzt_state*) cpu;

	switch (cpu->ah) {
		case 0x48: { // allocation (unsupported)
			fprintf(stderr, "DOS allocation not implemented!\n");
		} return STATE_END;
		case 0x49: { // freeing (faked; Banana Quest installer)
			cpu->flags &= ~FLAG_CARRY;
		} return STATE_CONTINUE;
		case 0x33: { // ext break checking (ZZT init)
			cpu->dl = 0x00;
		} return STATE_CONTINUE;
		case 0x09: { // write string (Banana Quest installer)
			u8* ptr = cpu->ram + (cpu->seg[SEG_DS]*16 + cpu->dx);
			while (*(ptr) != '$') {
				cpu_0x10_output(cpu, *(ptr++));
			}
			cpu->al = 0x24;
		} return STATE_CONTINUE;
		case 0x30: // DOS version
			cpu->al = 3;
			cpu->ah = 0;
			cpu->bh = 0;
			return STATE_CONTINUE;
		case 0x06: // d.c.output
			cpu_0x10_output(cpu, cpu->dl);
			cpu->al = cpu->dl;
			return STATE_CONTINUE;
		case 0x1A: // set dta
			zzt->dos_dta = (cpu->seg[SEG_DS]*16 + cpu->dx);
			return STATE_CONTINUE;
		case 0x25: // set ivt
			cpu->ram[cpu->al * 4] = cpu->dl;
			cpu->ram[cpu->al * 4 + 1] = cpu->dh;
			cpu->ram[cpu->al * 4 + 2] = cpu->seg[SEG_DS] & 0xFF;
			cpu->ram[cpu->al * 4 + 3] = cpu->seg[SEG_DS] >> 8;
			return STATE_CONTINUE;
		case 0x2C: { // systime
			long ms = zzt_internal_time();
			cpu->ch = (ms / 3600000) % 24;
			cpu->cl = (ms / 60000) % 60;
			cpu->dh = (ms / 1000) % 60;
			cpu->dl = (ms / 10) % 100;
		} return STATE_WAIT;
		case 0x2D: { // systime set
			long ms = cpu->dl * 10 + cpu->dh * 1000 + cpu->cl * 60000 + cpu->ch * 3600000;
			zzt->timer_time_offset = ms - zzt->timer_time;
			cpu->al = 0x00;
		} return STATE_CONTINUE;
		case 0x35: // get ivt
			cpu->bl = cpu->ram[cpu->al * 4];
			cpu->bh = cpu->ram[cpu->al * 4 + 1];
			cpu->seg[SEG_ES] = cpu->ram[cpu->al * 4 + 2]
				| (cpu->ram[cpu->al * 4 + 3] << 8);
			return STATE_CONTINUE;
		case 0x3C: { // creat
			int handle = vfs_open(STR_DS_DX, 0x10001);
			if (handle < 0) {
				fprintf(stderr, "creat: file not found: %s\n", STR_DS_DX);
				cpu->ax = 0x02;
				cpu->flags |= FLAG_CARRY;
			} else {
				cpu->ax = handle;
				cpu->flags &= ~FLAG_CARRY;
			}
		} return STATE_CONTINUE;
		case 0x3D: { // open
#ifdef DEBUG_FS_ACCESS
			fprintf(stderr, "open %02X %s\n", cpu->al, STR_DS_DX);
#endif
			int handle = vfs_open(STR_DS_DX, cpu->al);
			if (handle < 0) {
				fprintf(stderr, "open: file not found: %s\n", STR_DS_DX);
				cpu->ax = 0x02;
				cpu->flags |= FLAG_CARRY;
			} else {
				cpu->ax = handle;
				cpu->flags &= ~FLAG_CARRY;
			}
		} return STATE_CONTINUE;
		case 0x3E: { // close
			int res = vfs_close(cpu->bx);
			if (res < 0) {
				cpu->ax = 0x06;
				cpu->flags |= FLAG_CARRY;
			} else {
				cpu->flags &= ~FLAG_CARRY;
			}
		} return STATE_CONTINUE;
		case 0x3F: { // read
#ifdef DEBUG_FS_ACCESS
			fprintf(stderr, "read %04X\n", cpu->cx);
#endif
			int res = vfs_read(cpu->bx, (u8*)STR_DS_DX, cpu->cx);
			if (res < 0) {
				cpu->ax = 0x05;
				cpu->flags |= FLAG_CARRY;
			} else {
				cpu->ax = res;
				cpu->flags &= ~FLAG_CARRY;
			}
		} return STATE_CONTINUE;
		case 0x40: { // write
#ifdef DEBUG_FS_ACCESS
			fprintf(stderr, "write %04X\n", cpu->cx);
#endif
			if (cpu->cx == 0) {
				// we don't implement the special case
				cpu->ax = 0x05;
				cpu->flags |= FLAG_CARRY;
				return STATE_CONTINUE;
			}

			int res = vfs_write(cpu->bx, (u8*)STR_DS_DX, cpu->cx);
			if (res < 0) {
				cpu->ax = 0x05;
				cpu->flags |= FLAG_CARRY;
			} else {
				cpu->ax = res;
				cpu->flags &= ~FLAG_CARRY;
			}
		} return STATE_CONTINUE;
		case 0x42: { // lseek
			int res = vfs_seek(cpu->bx, (cpu->cx << 16) | cpu->dx, cpu->al);
			if (res < 0) {
				cpu->ax = 0x05;
				cpu->flags |= FLAG_CARRY;
			} else {
				cpu->ax = res;
				cpu->flags &= ~FLAG_CARRY;
			}
		} return STATE_CONTINUE;
		case 0x44: // 0x4400
			if (cpu->ax == 0x4400) {
				cpu->ax = 0x01;
				cpu->flags |= FLAG_CARRY;
				return STATE_CONTINUE;
			}
			break;
		case 0x00:
		case 0x4C:
			return STATE_END;
		case 0x4E: { // findfirst
			int res = vfs_findfirst(cpu->ram + zzt->dos_dta, cpu->cx, STR_DS_DX);
			if (res < 0) {
				cpu->ax = 0x12;
				cpu->flags |= FLAG_CARRY;
			} else {
				cpu->flags &= ~FLAG_CARRY;
			}
			break;
		};
		case 0x4F: { // findnext
			int res = vfs_findnext(cpu->ram + zzt->dos_dta);
			if (res < 0) {
				cpu->ax = 0x12;
				cpu->flags |= FLAG_CARRY;
			} else {
				cpu->flags &= ~FLAG_CARRY;
			}
			break;
		};
		default:
			fprintf(stderr, "int 0x21 AX=%04X\n", cpu->ax);
			break;
	}

	return STATE_CONTINUE;
}

/* #define MAX_ALLOC (639*64)
static u32 seg_allocs[MAX_ALLOC];

static int mem_alloc(u32 paragraphs, u8 simulate) {
	int offset = 0;
	while (offset < MAX_ALLOC) {
		fprintf(stderr, "offset = %d\n", offset);
		if (seg_allocs[offset] > 0) {
			offset += seg_allocs[offset];
		} else {
			int size = 0;
			while (seg_allocs[offset+size] == 0 && size < paragraphs) {
				size++;
			}
			fprintf(stderr, "size = %d/%d\n", size, paragraphs);
			if (size == paragraphs) {
				if (!simulate) {
					for (int i = 0; i < size; i++) {
						seg_allocs[offset+i] = (size-i);
					}
				}
				fprintf(stderr, "offset >= %d\n", offset);
				return offset;
			} else {
				offset += size;
			}
		}
	}

	return -1;
}

static int mem_free(u32 addr) {
	int v = seg_allocs[addr];
	if (v <= 0) return 0;
	if (addr > 0 && seg_allocs[addr - 1] > v) return 0;

	for (int i = 0; i < v; i++) {
		seg_allocs[addr+i] = 0;
	}
	return 1;
} */

static u8 vfs_read8(int handle, int pos) {
	u8 v;
	vfs_seek(handle, pos, VFS_SEEK_SET);
	vfs_read(handle, &v, 1);
	return v;
}

static u16 vfs_read16(int handle, int pos) {
	u8 v1, v2;
	vfs_seek(handle, pos, VFS_SEEK_SET);
	vfs_read(handle, &v1, 1);
	vfs_read(handle, &v2, 1);
	return v1 | (v2 << 8);
}

static void zzt_load_build_psp(int first_seg, int last_seg, const char *arg) {
	int psp = first_seg * 16;

	zzt.cpu.ram[psp + 0x02] = last_seg & 0xFF;
	zzt.cpu.ram[psp + 0x03] = last_seg >> 8;

	if (arg != NULL) {
		int arglen = strlen(arg);
		if (arglen > 126) arglen = 126;
		zzt.cpu.ram[psp + 0x80] = 1+arglen;
		strncpy((char*) (zzt.cpu.ram + psp + 0x81), arg, arglen);
		zzt.cpu.ram[psp + 0x81 + arglen] = 0x0D;
	} else {
		zzt.cpu.ram[psp + 0x80] = 1;
		zzt.cpu.ram[psp + 0x81] = 0x0D;
	}
}

static void zzt_load_exe(int handle, const char *arg) {
	int last_page_size = vfs_read16(handle, 2);
	int pages = vfs_read16(handle, 4);
	int hdr_offset = vfs_read16(handle, 8);
//	int minalloc = vfs_read16(handle, 0xA);
//	int maxalloc = vfs_read16(handle, 0xC);

	int filesize = (pages * 512) - ((last_page_size > 0) ? (512 - last_page_size) : 0) - (hdr_offset * 16);
/*	int size_pars = (filesize + 15) / 16;
	size_pars += 16; // PSP */
//	int size_pars = 40400;
//	int offset_pars = mem_alloc(size_pars, 0);
	int offset_pars = 0x100;
	int size_pars = 0xA000 - 0x100;

	// location
	zzt.cpu.seg[SEG_CS] = vfs_read16(handle, 0x16) + offset_pars + 0x10;
	zzt.cpu.seg[SEG_SS] = vfs_read16(handle, 0xE) + offset_pars + 0x10;
	zzt.cpu.seg[SEG_DS] = offset_pars;
	zzt.cpu.seg[SEG_ES] = offset_pars;
	zzt.cpu.ip = vfs_read16(handle, 0x14);
	zzt.cpu.sp = vfs_read16(handle, 0x10);

	zzt_load_build_psp(offset_pars, offset_pars + size_pars, arg);

	// load file into memory
	vfs_seek(handle, hdr_offset * 16, VFS_SEEK_SET);
	vfs_read(handle, &(zzt.cpu.ram[(offset_pars * 16) + 256]), filesize);
#ifdef DEBUG_FS_ACCESS
	fprintf(stderr, "wrote %d bytes to %05X\n", filesize, (offset_pars * 16 + 256));
#endif

	// relocation
	int pos_reloc = vfs_read16(handle, 0x18);
	int size_reloc = vfs_read16(handle, 0x06);
	if (size_reloc > 0) {
		for (int i = 0; i < size_reloc; i++) {
			int offset_seg = vfs_read16(handle, pos_reloc + i*4 + 2);
			int offset = (offset_seg + offset_pars + 16) * 16 + vfs_read16(handle, pos_reloc + i*4);

			// read word at offset
			u16 word = zzt.cpu.ram[offset] | (zzt.cpu.ram[offset + 1] << 8);
			word += (offset_pars + 16);
			zzt.cpu.ram[offset] = (word & 0xFF);
			zzt.cpu.ram[offset + 1] = ((word >> 8) & 0xFF);
		}
		fprintf(stderr, "relocated %d exe entries\n", size_reloc);
	}
}

void zzt_load_binary(int handle, const char *arg) {
	if (vfs_read8(handle, 0) == 0x4D && vfs_read8(handle, 1) == 0x5A) {
		// MZ, is exe file
		zzt_load_exe(handle, arg);
		return;
	}

	// assume com file
	int offset_pars = 0x100;
	int size_pars = 0xA000 - 0x100;
	zzt_load_build_psp(offset_pars, offset_pars + size_pars, arg);

	zzt.cpu.seg[SEG_CS] = offset_pars;
	zzt.cpu.seg[SEG_SS] = offset_pars;
	zzt.cpu.seg[SEG_DS] = offset_pars;
	zzt.cpu.seg[SEG_ES] = offset_pars;
	zzt.cpu.ip = 0x100;
	zzt.cpu.sp = 0xFFFE;

	vfs_seek(handle, 0, VFS_SEEK_SET);
	u8 *data_ptr = &(zzt.cpu.ram[(offset_pars * 16) + 256]);
	int bytes_read = vfs_read(handle, data_ptr, 65536 - 256);
	fprintf(stderr, "wrote %d bytes to %d\n", bytes_read, (offset_pars * 16 + 256));
}

int zzt_load_charset(int width, int height, u8 *data) {
	if (width != 8 || height <= 0 || height > 16) return -1;

	for (int i = 0; i < 256*height; i++) {
		zzt.charset[i] = data[i];
	}

	zeta_update_charset(8, 14, zzt.charset);
	return 0;
}

int zzt_load_palette(u32 *colors) {
	for (int i = 0; i < 16; i++) {
		zzt.palette[i] = colors[i];
	}

	zeta_update_palette(zzt.palette);
	return 0;
}

void zzt_init(void) {
/*	for (int i = 0; i < MAX_ALLOC; i++)
		seg_allocs[i] = (i < 256) ? (256-i) : 0; */

	zzt.key.qke = -1;
	for (int i = 0; i < KEYBUF_SIZE; i++) {
		zzt.keybuf[i].qke = -1;
	}

	zzt.key_delay = 500;
	zzt.key_repeat_delay = 100;

	zzt.real_time = 0;
	zzt.timer_time = 0;
	zzt.joy_xstrobe_val = -1;
	zzt.joy_ystrobe_val = -1;
	zzt.joy_xstrobes = 0;
	zzt.joy_ystrobes = 0;
	zzt.mouse_buttons = 0;
	zzt.mouse_x = 640 / 2;
	zzt.mouse_y = 350 / 2;
	zzt.port_201 = 0xF0;

	cpu_init_globals();
	cpu_init(&(zzt.cpu));

	// sysconf constants

	zzt.cpu.ram[0x410] = 0x61;
	zzt.cpu.ram[0x411] = 0x00;
	zzt.cpu.ram[0x413] = (640) & 255;
	zzt.cpu.ram[0x414] = (640) >> 8;
	zzt.cpu.ram[0xFFFFE] = 0xFB;

	// video constants

	zzt.cpu.ram[0x44A] = 80;
	zzt.cpu.ram[0x463] = 0xD4;
	zzt.cpu.ram[0x464] = 0x03;

	zzt.cpu.func_port_in = cpu_func_port_in_main;
	zzt.cpu.func_port_out = cpu_func_port_out_main;
	zzt.cpu.func_interrupt = cpu_func_interrupt_main;

	// default assets

	zzt_load_charset(8, 14, res_8x14_bin);
	zzt_load_palette(def_palette);
}

int zzt_execute(int opcodes) {
	return cpu_execute(&(zzt.cpu), opcodes);
}

static void zzt_update_keys(void) {
	long ctime = zeta_time_ms();
	zzt_key_entry* key = &(zzt.key);

	if (key->qke == -1) return;
	long dtime = ctime - key->time;
	if (dtime >= (key->repeat ? zzt.key_repeat_delay : zzt.key_delay)) {
		if (key->repeat && zzt.key_repeat_delay <= 0) {
			return;
		}

		if (zzt_key_append(key->qch, key->qke)) {
			key->time = dtime;
			key->repeat = 1;
		}
	}
}

void zzt_mark_timer(void) {
	zzt.real_time = zeta_time_ms();
	zzt.timer_time += SYS_TIMER_TIME;
	zzt_update_keys();
	cpu_emit_interrupt(&(zzt.cpu), 0x08);
}

u8* zzt_get_ram(void) {
	return zzt.cpu.ram;
}
