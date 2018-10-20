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

#include "zzt.h"

#ifdef EMSCRIPTEN
#define fprintf(...)
#else
#include <stdio.h>
#endif

typedef struct {
	cpu_state cpu;

	// keyboard
	int qch, qke, kmod;

	// joystick
	u8 joy_xstrobe_val, joy_ystrobe_val;
	u8 joy_xstrobes, joy_ystrobes;

	// mouse
	u16 mouse_buttons;
	u16 mouse_x, mouse_y;
	s16 mouse_xd, mouse_yd;

	// I/O
	u8 cga_status;
	u8 port_42_latch;
	u16 port_42;
	// u8 port_43[3]; (not actually used)
	u8 port_61;
	u8 port_201;

	// DOS
	u32 dos_dta;
} zzt_state;

zzt_state zzt;

void zzt_kmod_set(int mod) {
	zzt.kmod |= mod;
}

void zzt_kmod_clear(int mod) {
	zzt.kmod &= ~mod;
}

void zzt_key(int c, int k) {
	zzt.qch = c >= 0 && c < 128 ? c : 0;
	zzt.qke = k;
	fprintf(stderr, "kbget %d %d\n", zzt.qch, zzt.qke);
}

static void cpu_func_intr_0x10(cpu_state* cpu);
static void cpu_func_intr_0x13(cpu_state* cpu);
static void cpu_func_intr_0x16(cpu_state* cpu);
static void cpu_func_intr_0x21(cpu_state* cpu);

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
			if (!vfs_has_feature(FEATURE_JOY_CONNECTED))
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
		default:
			fprintf(stderr, "port out %04X = %04X\n", addr, val);
	}
}

static void cpu_func_intr_0x33(cpu_state* cpu) {
	zzt_state* zzt = (zzt_state*) cpu;

	switch (cpu->ax) {
		case 0:
			if (vfs_has_feature(FEATURE_MOUSE_CONNECTED)) {
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

static int cpu_func_interrupt_main(cpu_state* cpu, u8 intr) {
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
		case 0x16: cpu_func_intr_0x16(cpu); break;
		case 0x21: cpu_func_intr_0x21(cpu); break;
		case 0x33: cpu_func_intr_0x33(cpu); break;
		case 0x15:
			fprintf(stderr, "sysconf %04X\n", cpu->ax);
			cpu->ah = 0x86;
			cpu->flags |= FLAG_CARRY;
			break;
		default:
			fprintf(stderr, "interrupt %02X\n", intr);
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
	if (cpu->ah == 0x02) {
		cpu->ram[0x451] = cpu->dh;
		cpu->ram[0x450] = cpu->dl;
	} else if (cpu->ah == 0x03) {
		cpu->dh = cpu->ram[0x451];
		cpu->dl = cpu->ram[0x450];
	} else if (cpu->ah == 0x04) {
		cpu->ah = 0;
	} else if (cpu->ah == 0x06) {
		video_scroll_up(cpu, cpu->al, cpu->bh, cpu->ch, cpu->cl, cpu->dh, cpu->dl);
	} else if (cpu->ah == 0x08) {
		u32 addr = TEXT_ADDR(cpu->bl, cpu->bh);
		cpu->ah = 0x08;
		cpu->al = cpu->ram[addr];
		cpu->bh = cpu->ram[addr+1];
	} else if (cpu->ah == 0x09 || cpu->ah == 0x0A) {
		u32 addr = TEXT_ADDR(cpu->bl, cpu->bh);
		for (int i = 0; i < cpu->cx && addr < 160*25; i++, addr+=2) {
			cpu->ram[addr] = cpu->al;
			if (cpu->ah == 0x09) cpu->ram[addr + 1] = cpu->bl;
		}
	} else if (cpu->ah == 0x0E) {
		cpu_0x10_output(cpu, cpu->al);
	} else if (cpu->ah == 0x00) {
		video_mode = cpu->al & 0x7F;
		fprintf(stderr, "set video mode to %d\n", cpu->al);
	} else if (cpu->ah == 0x0F) {
		cpu->ah = cpu->ram[0x44A];
		cpu->al = video_mode;
		cpu->bh = 0;
	} else {
		fprintf(stderr, "int 0x10 AX=%04X AH=%02X AL=%02X BL=%02X\n",
			cpu->ax, cpu->ah, cpu->al, cpu->bl);
		cpu->flags |= FLAG_CARRY;
	}
}

static void cpu_func_intr_0x13(cpu_state* cpu) {
	fprintf(stderr, "int 0x13 AX=%04X\n", cpu->ax);
}

static void cpu_func_intr_0x16(cpu_state* cpu) {
	zzt_state* zzt = (zzt_state*) cpu;

	if (cpu->ah == 0x00) {
		if (zzt->qke >= 0) {
			cpu->flags &= ~FLAG_ZERO;
			cpu->ah = zzt->qke;
			cpu->al = zzt->qch;
			zzt->qch = -1;
			zzt->qke = -1;
		} else {
			cpu->flags |= FLAG_ZERO;
		}
		return;
	} else if (cpu->ah == 0x01) {
		if (zzt->qke >= 0) {
			cpu->flags &= ~FLAG_ZERO;
			cpu->ah = zzt->qke;
			cpu->al = zzt->qch;
		} else {
			cpu->flags |= FLAG_ZERO;
		}
		return;
	} else if (cpu->ah == 0x02) {
		cpu->al = zzt->kmod;
		return;
	}
	fprintf(stderr, "int 0x16 AX=%04X\n", cpu->ax);
}

#define STR_DS_DX (char*)(&cpu->ram[cpu->seg[SEG_DS]*16 + cpu->dx])

static void cpu_func_intr_0x21(cpu_state* cpu) {
	zzt_state* zzt = (zzt_state*) cpu;

	switch (cpu->ah) {
		case 0x30: // DOS version
			cpu->al = 3;
			cpu->ah = 0;
			cpu->bh = 0;
			return;
		case 0x06: // d.c.output
			cpu_0x10_output(cpu, cpu->dl);
			cpu->al = cpu->dl;
			return;
		case 0x1A: // set dta
			zzt->dos_dta = (cpu->seg[SEG_DS]*16 + cpu->dx);
			return;
		case 0x25: // set ivt
			cpu->ram[cpu->al * 4] = cpu->dl;
			cpu->ram[cpu->al * 4 + 1] = cpu->dh;
			cpu->ram[cpu->al * 4 + 2] = cpu->seg[SEG_DS] & 0xFF;
			cpu->ram[cpu->al * 4 + 3] = cpu->seg[SEG_DS] >> 8;
			return;
		case 0x2C: { // systime
			long ms = vfs_time_ms();
			cpu->ch = (ms / 3600000) % 24;
			cpu->cl = (ms / 60000) % 60;
			cpu->dh = (ms / 1000) % 60;
			cpu->dl = (ms / 10) % 100;
		} return;
		case 0x35: // get ivt
			cpu->bl = cpu->ram[cpu->al * 4];
			cpu->bh = cpu->ram[cpu->al * 4 + 1];
			cpu->seg[SEG_ES] = cpu->ram[cpu->al * 4 + 2]
				| (cpu->ram[cpu->al * 4 + 3] << 8);
			return;
		case 0x3C: { // creat
			int handle = vfs_open(STR_DS_DX, 0x10001);
			if (handle < 0) {
				fprintf(stderr, "not found\n");
				cpu->ax = 0x02;
				cpu->flags |= FLAG_CARRY;
			} else {
				cpu->ax = handle;
				cpu->flags &= ~FLAG_CARRY;
			}
		} return;
		case 0x3D: { // open
			fprintf(stderr, "open %02X %s\n", cpu->al, STR_DS_DX);
			int handle = vfs_open(STR_DS_DX, cpu->al);
			if (handle < 0) {
				fprintf(stderr, "not found\n");
				cpu->ax = 0x02;
				cpu->flags |= FLAG_CARRY;
			} else {
				cpu->ax = handle;
				cpu->flags &= ~FLAG_CARRY;
			}
		} return;
		case 0x3E: { // close
			int res = vfs_close(cpu->bx);
			if (res < 0) {
				cpu->ax = 0x06;
				cpu->flags |= FLAG_CARRY;
			} else {
				cpu->flags &= ~FLAG_CARRY;
			}
		} return;
		case 0x3F: { // read
			fprintf(stderr, "read %04X\n", cpu->cx);
			int res = vfs_read(cpu->bx, (u8*)STR_DS_DX, cpu->cx);
			if (res < 0) {
				cpu->ax = 0x05;
				cpu->flags |= FLAG_CARRY;
			} else {
				cpu->ax = res;
				cpu->flags &= ~FLAG_CARRY;
			}
		} return;
		case 0x40: { // write
			fprintf(stderr, "write %04X\n", cpu->cx);
			if (cpu->cx == 0) {
				// we don't implement the special case
				cpu->ax = 0x05;
				cpu->flags |= FLAG_CARRY;
				return;
			}

			int res = vfs_write(cpu->bx, (u8*)STR_DS_DX, cpu->cx);
			if (res < 0) {
				cpu->ax = 0x05;
				cpu->flags |= FLAG_CARRY;
			} else {
				cpu->ax = res;
				cpu->flags &= ~FLAG_CARRY;
			}
		} return;
		case 0x42: { // lseek
			int res = vfs_seek(cpu->bx, (cpu->cx << 16) | cpu->dx, cpu->al);
			if (res < 0) {
				cpu->ax = 0x05;
				cpu->flags |= FLAG_CARRY;
			} else {
				cpu->ax = res;
				cpu->flags &= ~FLAG_CARRY;
			}
		} return;
		case 0x44: // 0x4400
			if (cpu->ax == 0x4400) {
				cpu->ax = 0x01;
				cpu->flags |= FLAG_CARRY;
				return;
			}
			break;
		case 0x4C:
			cpu->terminated = 1;
			break;
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
		};
		default:
			fprintf(stderr, "int 0x21 AX=%04X\n", cpu->ax);
			break;
	}
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

u32 zzt_init(void) {
/*	for (int i = 0; i < MAX_ALLOC; i++)
		seg_allocs[i] = (i < 256) ? (256-i) : 0; */

	zzt.qch = -1;
	zzt.qke = -1;
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

	// load exe
	// NOTE: relocation items are not handled, as the ZZT
	// .EXE does not need them

	int handle = vfs_open("zzt.exe", 0);
	if (handle < 0)
		handle = vfs_open("superz.exe", 0);

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

	// build faux psp
	int psp = offset_pars * 16;
	int seg_first = offset_pars + size_pars;
	fprintf(stderr, "seg_first %04X\n", seg_first);
	zzt.cpu.ram[psp + 0x02] = seg_first & 0xFF;
	zzt.cpu.ram[psp + 0x03] = seg_first >> 8;
	zzt.cpu.ram[psp + 0x80] = 1;
	zzt.cpu.ram[psp + 0x81] = 0x0D;

	// load file into memory
	vfs_seek(handle, hdr_offset * 16, VFS_SEEK_SET);
	vfs_read(handle, &(zzt.cpu.ram[(offset_pars * 16) + 256]), filesize);
	fprintf(stderr, "wrote %d bytes to %d\n", filesize, (offset_pars * 16 + 256));

	vfs_close(handle);
	return psp;
}

int zzt_execute(int opcodes) {
	return cpu_execute(&(zzt.cpu), opcodes);
}

void zzt_mark_timer(void) {
	cpu_emit_interrupt(&(zzt.cpu), 0x08);
}

u8* zzt_get_ram(void) {
	return zzt.cpu.ram;
}
