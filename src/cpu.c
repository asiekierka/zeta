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

#ifndef NO_MEMSET
#include <string.h>
#endif
#include "cpu.h"
//#define DBG1

#ifdef DBG1
#include <stdio.h>
#endif

static int cpu_run_one(cpu_state* cpu, u8 no_interrupting, u8 pr_state);

#define SEG(s, v) ( ((cpu->seg[(s)]<<4)+(v)) & 0xFFFFF )
#define SEGMD(s, v) ( ((cpu->seg[cpu->segmod ? ((cpu->segmod)-1) : (s)]<<4)+(v)) & 0xFFFFF )
#define FLAG(f) ((cpu->flags & (f)) != 0)
#define FLAG_CLEAR(f) cpu->flags &= ~(f)
#define FLAG_SET(f) cpu->flags |= (f)
#define FLAG_WRITE(f, v) if (v) { FLAG_SET(f); } else { FLAG_CLEAR(f); }
#define FLAG_COMPLEMENT(f) cpu->flags ^= (f)

static u8 ram_u8(cpu_state* cpu, u32 addr) {
	return *((u8*) (cpu->ram + addr));
}

static u16 ram_u16(cpu_state* cpu, u32 addr) {
#if defined(UNALIGNED_OK) && !defined(BIG_ENDIAN)
	return *((u16*) (cpu->ram + addr));
#else
	return ((u16) ram_u8(cpu, addr + 1) << 8) | ram_u8(cpu, addr);
#endif
}

/* static s8 ram_s8(cpu_state* cpu, u32 addr) {
	return *((s8*) (cpu->ram + addr));
}

static s16 ram_s16(cpu_state* cpu, u32 addr) {
#if defined(UNALIGNED_OK) && !defined(BIG_ENDIAN)
	return *((s16*) (cpu->ram + addr));
#else
	return (s16) ram_u16(cpu, addr);
#endif
} */

static void ram_w8(cpu_state* cpu, u32 addr, u8 v) {
	*((u8*) (cpu->ram + addr)) = v;
}

static void ram_w16(cpu_state* cpu, u32 addr, u16 v) {
#if defined(UNALIGNED_OK) && !defined(BIG_ENDIAN)
	*((u16*) (cpu->ram + addr)) = v;
#else
	ram_w8(cpu, addr, (u8) v);
	ram_w8(cpu, addr + 1, (u8) (v >> 8));
#endif
}

static u8 cpu_advance_ip(cpu_state* cpu) {
	u32 ip = SEG(SEG_CS, cpu->ip);
	cpu->ip++;
	return ram_u8(cpu, ip);
}

static u16 cpu_advance_ip16(cpu_state* cpu) {
	u32 ip = SEG(SEG_CS, cpu->ip);
	cpu->ip += 2;
	return ram_u16(cpu, ip);
}

// src/dst format:
// 0-7 = ax,cx,dx,bx,sp,bp,si,di
// 8-15 = bx+si+disp, bx+di+disp, bp+si+disp, bp+di+disp, si+disp, di+disp, bp+disp, bx+disp
// 16-23 = al,cl,dl,bl,ah,ch,dh,bh
// 24-25 = address8(disp), address16(disp)
// 26-31 = es,cs,ss,ds,fs,gs
// 32-39 = 8-15(8-bit)
// 40 = imm8, 41 = imm16
typedef struct {
	u8 src, dst;
	u16 imm;
	int disp;
} mrm_entry;

static u8 parity_table[256];
static mrm_entry mrm_table[2048];
static mrm_entry mrm6_4, mrm6_5;

static void generate_mrm_table(void) {
	int i;
	u8 is_seg, mod, reg, rm, d, w;
	u8 op1, op2;
	mrm_entry* e = mrm_table;

	for (i = 0; i < 2048; i++, e++) {
		is_seg = (i >> 10) & 0x1;
		mod = (i >> 6) & 0x3;
		reg = (i >> 3) & 0x7;
		rm = i & 0x7;
		d = (i >> 9) & 0x1;
		w = is_seg ? 1 : ((i >> 8) & 0x1);

		op1 = reg;
		op2 = rm;
		if (is_seg) op1 = (op1 % 6) + 26;
		else if (w == 0) op1 += 16;

		if (mod == 0 && rm == 6) op2 = 24 + w;
		else if (mod != 3) op2 += (w ? 8 : 32); // do not treat rm as reg field
		else if (w == 0) op2 += 16; // treat rm as reg field

		if (d == 0) { e->src = op1; e->dst = op2; }
		else { e->src = op2; e->dst = op1; }

		if (mod == 2) e->disp = 2;
		else if (mod == 1) e->disp = 1;
		else if (mod == 0 && rm == 6) e->disp = 3;
		else e->disp = 0;

		e->imm = 0;
	}

	mrm6_4.src = 40;
	mrm6_4.dst = 16;
	mrm6_4.disp = 0;

	mrm6_5.src = 41;
	mrm6_5.dst = 0;
	mrm6_5.disp = 0;
}

static u16 incdec_dir(cpu_state* cpu, u16 reg, u16 amount) {
	if (FLAG(FLAG_DIRECTION)) {
		return reg - amount;
	} else {
		return reg + amount;
	}
}

void cpu_set_ip(cpu_state* cpu, u16 cs, u16 ip) {
	cpu->seg[SEG_CS] = cs;
	cpu->ip = ip;
}

void cpu_init_globals(void) {
	int i;
	u8 p, v;

	generate_mrm_table();
	for (i = 0; i < 256; i++) {
		p = 0;
		v = i;
		while (v != 0) {
			p = p + (v & 1);
			v >>= 1;
		}
		parity_table[i] = (p & 1) ? 0 : FLAG_PARITY;
	}
}

#define FLAG_WRITE_PARITY(val) cpu->flags = (cpu->flags & 0xFFFB) | parity_table[((val) & 0xFF)];

static u8 cpu_seg_rm(int v) {
	switch (v) {
		case 10:
		case 11:
		case 14:
		case 34:
		case 35:
		case 38:
			return SEG_SS;
		default:
			return SEG_DS;
	}
}

static u16 cpu_addr_rm(cpu_state* cpu, mrm_entry* data, u8 v) {
	switch (v) {
		case 24: case 25: return data->disp;
		case 8:  case 32: return cpu->bx + cpu->si + data->disp;
		case 9:  case 33: return cpu->bx + cpu->di + data->disp;
		case 10: case 34: return cpu->bp + cpu->si + data->disp;
		case 11: case 35: return cpu->bp + cpu->di + data->disp;
		case 12: case 36: return cpu->si + data->disp;
		case 13: case 37: return cpu->di + data->disp;
		case 14: case 38: return cpu->bp + data->disp;
		case 15: case 39: return cpu->bx + data->disp;
		default: return 0;
	}
}

static u16 cpu_read_rm(cpu_state* cpu, mrm_entry* data, u8 v) {
	switch (v) {
		case 0: return cpu->ax;
		case 1: return cpu->cx;
		case 2: return cpu->dx;
		case 3: return cpu->bx;
		case 4: return cpu->sp;
		case 5: return cpu->bp;
		case 6: return cpu->si;
		case 7: return cpu->di;
		case 8: case 9: case 12: case 13: case 15:
			return ram_u16(cpu, SEGMD(SEG_DS, cpu_addr_rm(cpu,data,v)));
		case 10: case 11: case 14:
			return ram_u16(cpu, SEGMD(SEG_SS, cpu_addr_rm(cpu,data,v)));
		case 16: return cpu->al;
		case 17: return cpu->cl;
		case 18: return cpu->dl;
		case 19: return cpu->bl;
		case 20: return cpu->ah;
		case 21: return cpu->ch;
		case 22: return cpu->dh;
		case 23: return cpu->bh;
		case 24: return ram_u8(cpu, SEGMD(SEG_DS, data->disp));
		case 25: return ram_u16(cpu, SEGMD(SEG_DS, data->disp));
		case 26: return cpu->seg[0];
		case 27: return cpu->seg[1];
		case 28: return cpu->seg[2];
		case 29: return cpu->seg[3];
		case 32: case 33: case 36: case 37: case 39:
			return ram_u8(cpu, SEGMD(SEG_DS, cpu_addr_rm(cpu,data,v)));
		case 34: case 35: case 38:
			return ram_u8(cpu, SEGMD(SEG_SS, cpu_addr_rm(cpu,data,v)));
		case 40: return data->imm & 0xFF;
		case 41: return data->imm;
		default: return 0;
	}
}

static void cpu_write_rm(cpu_state* cpu, mrm_entry* data, u8 v, u16 val) {
	switch (v) {
		case 0: cpu->ax = val; break;
		case 1: cpu->cx = val; break;
		case 2: cpu->dx = val; break;
		case 3: cpu->bx = val; break;
		case 4: cpu->sp = val; break;
		case 5: cpu->bp = val; break;
		case 6: cpu->si = val; break;
		case 7: cpu->di = val; break;
		case 8: case 9: case 12: case 13: case 15:
			ram_w16(cpu, SEGMD(SEG_DS, cpu_addr_rm(cpu,data,v)), val);
			break;
		case 10: case 11: case 14:
			ram_w16(cpu, SEGMD(SEG_SS, cpu_addr_rm(cpu,data,v)), val);
			break;
		case 16: cpu->al = (u8) val; break;
		case 17: cpu->cl = (u8) val; break;
		case 18: cpu->dl = (u8) val; break;
		case 19: cpu->bl = (u8) val; break;
		case 20: cpu->ah = (u8) val; break;
		case 21: cpu->ch = (u8) val; break;
		case 22: cpu->dh = (u8) val; break;
		case 23: cpu->bh = (u8) val; break;
		case 24: ram_w8(cpu, SEGMD(SEG_DS, data->disp), val); break;
		case 25: ram_w16(cpu, SEGMD(SEG_DS, data->disp), val); break;
		case 26: cpu->seg[0] = val; break;
		case 27: cpu->seg[1] = val; break;
		case 28: cpu->seg[2] = val; break;
		case 29: cpu->seg[3] = val; break;
		case 32: case 33: case 36: case 37: case 39:
			ram_w8(cpu, SEGMD(SEG_DS, cpu_addr_rm(cpu,data,v)), val);
			break;
		case 34: case 35: case 38:
			ram_w8(cpu, SEGMD(SEG_SS, cpu_addr_rm(cpu,data,v)), val);
			break;
	}
}

static mrm_entry cpu_mod_rm(cpu_state* cpu, u8 opcode, u16 is_seg) {
	int entry_id = cpu_advance_ip(cpu) | ((opcode & 3) << 8) | is_seg;
	mrm_entry e = mrm_table[entry_id];

	switch (e.disp) {
		case 1:
			e.disp = (s8) cpu_advance_ip(cpu);
			break;
		case 2:
			e.disp = (s16) cpu_advance_ip16(cpu);
			break;
		case 3:
			e.disp = cpu_advance_ip16(cpu);
			break;
	}

	return e;
}

static mrm_entry cpu_mod_rm6(cpu_state* cpu, u8 opcode) {
	switch (opcode & 0x07) {
		case 4:
			mrm6_4.imm = cpu_advance_ip(cpu);
			return mrm6_4;
		case 5:
			mrm6_5.imm = cpu_advance_ip16(cpu);
			return mrm6_5;
		default:
			return cpu_mod_rm(cpu, opcode, 0);
	}
}

void cpu_push16(cpu_state* cpu, u16 v) {
	cpu->sp -= 2;
	ram_w16(cpu, SEG(SEG_SS,cpu->sp), v);
}

u16 cpu_pop16(cpu_state* cpu) {
	u16 sp = cpu->sp;
	cpu->sp += 2;
	return ram_u16(cpu, SEG(SEG_SS,sp));
}

static inline void cpu_mov(cpu_state* cpu, mrm_entry e) {
	u16 v1 = cpu_read_rm(cpu, &e, e.src);
	cpu_write_rm(cpu, &e, e.dst, v1);
}

static void cpu_uf_zsp(cpu_state* cpu, u16 vr, u8 opc) {
	if (opc & 0x01) {
		FLAG_WRITE(FLAG_ZERO, (vr & 0xFFFF) == 0);
		FLAG_WRITE(FLAG_SIGN, (vr & 0x8000) != 0);
	} else {
		FLAG_WRITE(FLAG_ZERO, (vr & 0xFF) == 0);
		FLAG_WRITE(FLAG_SIGN, (vr & 0x80) != 0);
	}
	FLAG_WRITE_PARITY(vr);
}

static void cpu_uf_inc(cpu_state* cpu, u16 vr, u8 opc) {
	cpu_uf_zsp(cpu, vr, opc);
	FLAG_WRITE(FLAG_ADJUST, (vr & 0xF) == 0) // 15 + 1 = 0
	FLAG_WRITE(FLAG_OVERFLOW, vr == ((opc & 0x01) ? 0x8000 : 0x80));
}

static void cpu_uf_dec(cpu_state* cpu, u16 vr, u8 opc) {
	cpu_uf_zsp(cpu, vr, opc);
	FLAG_WRITE(FLAG_ADJUST, (vr & 0xF) == 0xF) // 0 - 1 = 15
	FLAG_WRITE(FLAG_OVERFLOW, vr == ((opc & 0x01) ? 0x7FFF : 0x7F));
}

static void cpu_uf_co_add(cpu_state* cpu, u16 v1, u16 v2, u8 vc, u32 vr, u8 opc) {
	FLAG_WRITE(FLAG_ADJUST, ((v1 & 0xF) + (v2 & 0xF) + vc) >= 0x10);
	if (opc & 0x01) {
		FLAG_WRITE(FLAG_CARRY, (vr & 0xFFFF) != vr);
		FLAG_WRITE(FLAG_OVERFLOW, ((v1 & 0x8000) == (v2 & 0x8000)) && ((vr & 0x8000) != (v1 & 0x8000)));
	} else {
		FLAG_WRITE(FLAG_CARRY, (vr & 0xFF) != vr);
		FLAG_WRITE(FLAG_OVERFLOW, ((v1 & 0x80) == (v2 & 0x80)) && ((vr & 0x80) != (v1 & 0x80)));
	}
}

static void cpu_uf_co_sub(cpu_state* cpu, u16 v1, u16 v2, u8 vb, u32 vr, u8 opc) {
	FLAG_WRITE(FLAG_ADJUST, ((v2 & 0xF) - (v1 & 0xF) - vb) < 0);
	if (opc & 0x01) {
		FLAG_WRITE(FLAG_CARRY, (vr & 0xFFFF) != vr);
		FLAG_WRITE(FLAG_OVERFLOW, ((v1 & 0x8000) != (v2 & 0x8000)) && ((vr & 0x8000) == (v1 & 0x8000)));
	} else {
		FLAG_WRITE(FLAG_CARRY, (vr & 0xFF) != vr);
		FLAG_WRITE(FLAG_OVERFLOW, ((v1 & 0x80) != (v2 & 0x80)) && ((vr & 0x80) == (v1 & 0x80)));
	}
}

static void cpu_uf_bit(cpu_state* cpu, u16 vr, u8 opc) {
	cpu->flags &= ~0x0801; // clear carry (0) and overflow (11)
	cpu_uf_zsp(cpu, vr, opc);
}

// 8086: 0xFF, 80186: 0x1F
#define CPU_SHIFT_MASK 0x1F

static void cpu_shl(cpu_state* cpu, mrm_entry e, u8 opcode) {
	u16 mask = (opcode & 0x01) ? 0xFFFF : 0xFF;
	u16 msb = (opcode & 0x01) ? 0x8000 : 0x80;
	u32 v2;
	u8 v1 = cpu_read_rm(cpu, &e, e.src);
#ifdef CPU_SHIFT_MASK
	v1 &= CPU_SHIFT_MASK;
#endif
	if (v1 >= 1) {
		v2 = cpu_read_rm(cpu, &e, e.dst);
		v2 <<= v1;
		cpu_write_rm(cpu, &e, e.dst, v2 & mask);
		FLAG_WRITE(FLAG_CARRY, (v2 & (mask + 1)) != 0);
		cpu_uf_zsp(cpu, v2, opcode);
		if (v1 == 1) {
			FLAG_WRITE(FLAG_OVERFLOW, ((v2 ^ (v2 >> 1)) & msb) != 0);
		}
	}
}

static void cpu_shr(cpu_state* cpu, mrm_entry e, u8 opcode, u8 arith) {
	u16 msb = (opcode & 0x01) ? 0x8000 : 0x80;
	u8 v1 = cpu_read_rm(cpu, &e, e.src);
#ifdef CPU_SHIFT_MASK
	v1 &= CPU_SHIFT_MASK;
#endif
	u16 v2 = cpu_read_rm(cpu, &e, e.dst);
	u16 vr;
	if (arith) {
		vr = (u16) (((s16) v2) >> v1);
	} else {
		vr = v2 >> v1;
	}
	cpu_write_rm(cpu, &e, e.dst, vr);
	cpu_uf_zsp(cpu, vr, opcode);
	if ((1 << (v1 - 1)) > msb) {
		FLAG_WRITE(FLAG_CARRY, arith && ((v2 & msb) != 0));
	} else {
		FLAG_WRITE(FLAG_CARRY, (v2 & (1 << (v1 - 1))) != 0);
	}
	if (v1 == 1) {
		FLAG_WRITE(FLAG_OVERFLOW, !arith && ((v2 & msb) != 0));
	}
}

#define ROTATE_MODE_ROL 0
#define ROTATE_MODE_ROR 1
#define ROTATE_MODE_RCL 2
#define ROTATE_MODE_RCR 3

static void cpu_rotate(cpu_state* cpu, mrm_entry e, u8 opcode, u8 mode) {
	u8 shift = (opcode & 0x01) ? 15 : 7;
	u8 v1 = cpu_read_rm(cpu, &e, e.src);
#ifdef CPU_SHIFT_MASK
	v1 &= CPU_SHIFT_MASK;
#endif
	u16 v2 = cpu_read_rm(cpu, &e, e.dst);
	u32 vr = v2;
	u8 cf = cpu->flags & 0x01;
	u8 of;
	u32 shiftmask;

	if (v1 > 0) {
		switch(mode) {
			case ROTATE_MODE_ROR:
				v1 &= shift;
				shiftmask = (1 << v1) - 1;
				cf = (vr >> ((v1 - 1) & shift)) & 0x01;
				vr = (vr >> v1) | ((vr & shiftmask) << ((shift - v1 + 1) & shift));
				of = ((vr >> shift) ^ (vr >> (shift - 1))) & 0x01;
				break;
			case ROTATE_MODE_ROL:
				v1 &= shift;
				cf = (vr >> ((shift - v1 + 1) & shift)) & 0x01;
				vr = ((vr << v1) & ((1 << (shift + 1)) - 1)) | (vr >> ((shift - v1 + 1) & shift));
				of = ((vr >> shift) ^ cf) & 0x01;
				break;
			case ROTATE_MODE_RCR:
				for (u8 shifts = 0; shifts < (v1 % (shift + 2)); shifts++) {
					u8 newcf = (vr & 0x01);
					vr = (vr >> 1) | (cf << shift);
					cf = newcf;
				}
				of = ((vr >> shift) ^ (vr >> (shift - 1))) & 0x01;
				break;
			case ROTATE_MODE_RCL:
				for (u8 shifts = 0; shifts < (v1 % (shift + 2)); shifts++) {
					u8 newcf = (vr >> shift) & 0x01;
					vr = ((vr << 1) & ((1 << (shift + 1)) - 1)) | cf;
					cf = newcf;
				}
				of = ((vr >> shift) ^ cf) & 0x01;
				break;
		}

		cpu_write_rm(cpu, &e, e.dst, vr);
		FLAG_WRITE(FLAG_CARRY, cf);
		if (v1 == 1) {
			FLAG_WRITE(FLAG_OVERFLOW, of);
		}
	}
}

static void cpu_mul(cpu_state* cpu, mrm_entry e, u8 opcode) {
	u16 v1 = cpu_read_rm(cpu, &e, e.src);
	u16 v2 = cpu_read_rm(cpu, &e, e.dst);
	u32 vr = (u32)(v1) * (u32)(v2);
	u16 vrf;
	if (opcode & 0x01) {
		cpu->dx = (u16) (vr >> 16);
		cpu->ax = (u16) vr;
		vrf = vr >> 16;
	} else {
		cpu->ax = (u16) vr;
		vrf = vr >> 8;
	}

	FLAG_WRITE(FLAG_CARRY | FLAG_OVERFLOW, vrf != 0);
}

static void cpu_imul(cpu_state* cpu, mrm_entry e, u8 opcode) {
	u16 v1 = cpu_read_rm(cpu, &e, e.src);
	u16 v2 = cpu_read_rm(cpu, &e, e.dst);

	if (opcode & 0x01) {
		s32 vr = ((s16) v1) * ((s16) v2);
		cpu->dx = (u16) (vr >> 16);
		cpu->ax = (u16) vr;

		FLAG_WRITE(FLAG_CARRY | FLAG_OVERFLOW, (vr < -0x8000) || (vr >= 0x8000));
	} else {
		s16 vr = ((s8) v1) * ((s8) v2);
		cpu->ax = (u16) vr;

		FLAG_WRITE(FLAG_CARRY | FLAG_OVERFLOW, (vr < -0x80) || (vr >= 0x80));
	}
}

static void cpu_div(cpu_state* cpu, mrm_entry e, u8 opcode) {
	u16 v2 = cpu_read_rm(cpu, &e, e.dst);
	if (v2 == 0) {
		cpu_ext_log("division by zero");
		cpu_emit_interrupt(cpu, 0);
		return;
	}

	if (opcode & 0x01) {
		u32 v = ((u32) cpu->dx << 16) | cpu->ax;
		u32 vd = v / v2;
		u16 vr = v % v2;
		if (vd >= 0x10000) {
			cpu_emit_interrupt(cpu, 0);
			return;
		}

		cpu->dx = vr;
		cpu->ax = (u16) vd;
	} else {
		u16 v = cpu->ax;
		u16 vd = v / v2;
		u8 vr = v % v2;
		if (vd >= 0x100) {
			cpu_emit_interrupt(cpu, 0);
			return;
		}

		cpu->ah = vr;
		cpu->al = vd;
	}
}

static void cpu_idiv(cpu_state* cpu, mrm_entry e, u8 opcode) {
	s16 v2 = cpu_read_rm(cpu, &e, e.dst);
	if (v2 == 0) {
		cpu_ext_log("division by zero");
		cpu_emit_interrupt(cpu, 0);
		return;
	}

	if (opcode & 0x01) {
		s32 v = (s32) (((u32) cpu->dx << 16) | cpu->ax);
		s32 vd = v / v2;
		s16 vr = v % v2;
		if (vd >= 0x8000 || vd < -0x8000) {
			cpu_emit_interrupt(cpu, 0);
			return;
		}

		cpu->dx = (u16) vr;
		cpu->ax = (u16) vd;
	} else {
		v2 = (s16) ((s8) v2);

		s16 v = (s16) cpu->ax;
		s16 vd = v / v2;
		s8 vr = v % v2;
		if (vd >= 0x80 || vd < -0x80) {
			cpu_emit_interrupt(cpu, 0);
			return;
		}

		cpu->ah = vr;
		cpu->al = vd;
	}
}

static void cpu_add(cpu_state* cpu, mrm_entry e, u8 opcode, u8 carry) {
	u16 v1 = cpu_read_rm(cpu, &e, e.src);
	u16 v2 = cpu_read_rm(cpu, &e, e.dst);

	carry &= cpu->flags; // FLAG_CARRY = 1
	u32 vr = v1 + v2 + carry;

	cpu_write_rm(cpu, &e, e.dst, (opcode & 0x01) ? (vr & 0xFFFF) : (vr & 0xFF));
	cpu_uf_zsp(cpu, vr, opcode);
	cpu_uf_co_add(cpu, v1, v2, carry, vr, opcode);
}

static void cpu_cmp(cpu_state* cpu, u16 v1, u16 v2, u8 opcode) {
	s32 vr = v1 - v2;
	cpu_uf_zsp(cpu, vr, opcode);
	cpu_uf_co_sub(cpu, v2, v1, 0, vr, opcode);
}

static void cpu_cmp_mrm(cpu_state* cpu, mrm_entry e, u8 opcode) {
	cpu_cmp(cpu, cpu_read_rm(cpu, &e, e.dst), cpu_read_rm(cpu, &e, e.src), opcode);
}

static void cpu_sub(cpu_state* cpu, mrm_entry e, u8 opcode, u8 borrow) {
	u16 v1 = cpu_read_rm(cpu, &e, e.src);
	u16 v2 = cpu_read_rm(cpu, &e, e.dst);
	borrow &= cpu->flags; // FLAG_CARRY = 1
	s32 vr = v2 - v1 - borrow;
	cpu_write_rm(cpu, &e, e.dst, (opcode & 0x01) ? (vr & 0xFFFF) : (vr & 0xFF));
	cpu_uf_zsp(cpu, vr, opcode);
	cpu_uf_co_sub(cpu, v1, v2, borrow, vr, opcode);
}

static void cpu_xor(cpu_state* cpu, mrm_entry e, u8 opcode) {
	u16 vr = cpu_read_rm(cpu, &e, e.src) ^ cpu_read_rm(cpu, &e, e.dst);
	cpu_write_rm(cpu, &e, e.dst, vr);
	cpu_uf_bit(cpu, vr, opcode);
}

static void cpu_and(cpu_state* cpu, mrm_entry e, u8 opcode) {
	u16 vr = cpu_read_rm(cpu, &e, e.src) & cpu_read_rm(cpu, &e, e.dst);
	cpu_write_rm(cpu, &e, e.dst, vr);
	cpu_uf_bit(cpu, vr, opcode);
}

static void cpu_test(cpu_state* cpu, mrm_entry e, u8 opcode) {
	u16 vr = cpu_read_rm(cpu, &e, e.src) & cpu_read_rm(cpu, &e, e.dst);
	cpu_uf_bit(cpu, vr, opcode);
}

static void cpu_or(cpu_state* cpu, mrm_entry e, u8 opcode) {
	u16 vr = cpu_read_rm(cpu, &e, e.src) | cpu_read_rm(cpu, &e, e.dst);
	cpu_write_rm(cpu, &e, e.dst, vr);
	cpu_uf_bit(cpu, vr, opcode);
}

void cpu_emit_interrupt(cpu_state* cpu, u8 intr) {
	if (cpu->intq_pos == MAX_INTQUEUE_SIZE) {
		return;
	}

	cpu->intq[cpu->intq_pos++] = intr;
}

static int cpu_pop_interrupt(cpu_state* cpu) {
	if (cpu->intq_pos == 0) return -1;
	u8 intr = cpu->intq[0];

	for (int i = 1; i < cpu->intq_pos; i++) {
		cpu->intq[i-1] = cpu->intq[i];
	}

	cpu->intq_pos--;
	return intr;
}

#define REP_COND_NZ 0
#define REP_COND_Z 1

static int cpu_rep(cpu_state* cpu, int cond) {
	u16 old_ip = cpu->ip;
	u8 opcode = cpu_advance_ip(cpu);

	// if length zero, skip
	// TODO: this relies on the opcode after REP always being 1 long...
	if (cpu->cx == 0) {
		return STATE_CONTINUE;
	}

	cpu->ip = old_ip;
	u8 pr_state = 1;
	u8 skip_conds = opcode != 0xA6 && opcode != 0xA7 && opcode != 0xAE && opcode != 0xAF;

	while (cpu->cx != 0) {
		int result = cpu_run_one(cpu, 1, pr_state);
		switch (result) {
			case STATE_END:
			case STATE_BLOCK:
				return STATE_END;
		}

		cpu->cx--;
		if (cpu->cx != 0) {
			u8 cond_result = skip_conds;
			if (!cond_result) {
				switch (cond) {
					case REP_COND_NZ:
						cond_result = !FLAG(FLAG_ZERO);
						break;
					case REP_COND_Z:
						cond_result = FLAG(FLAG_ZERO);
						break;
				}
			}
			if (cond_result) {
				cpu->ip = old_ip;
				pr_state = 0;
			} else break;
		}
	}

	return STATE_CONTINUE;
}

static void cpu_int(cpu_state* cpu, u8 intr) {
	u16 addr = ram_u16(cpu, intr * 4);
	u16 seg = ram_u16(cpu, intr * 4 + 2);

	cpu_push16(cpu, cpu->flags);
	cpu_push16(cpu, cpu->seg[SEG_CS]);
	cpu_push16(cpu, cpu->ip);

	cpu->ip = addr;
	cpu->seg[SEG_CS] = seg;
	cpu->halted = 0;

	FLAG_CLEAR(FLAG_INTERRUPT);
}

#ifdef USE_OPCODES_DECIMAL
static inline void cpu_daa(cpu_state* cpu) {
	u8 old_al = cpu->al;
	u8 old_cf = cpu->flags & 0x01;
	cpu->flags &= 0xFFFE;
	if (((cpu->al & 0x0F) > 0x9) || FLAG(4)) {
		cpu->al += 6;
		FLAG_SET(FLAG_ADJUST);
		if (old_cf || (old_al > 0xFF-6)) cpu->flags |= FLAG_CARRY;
	} else {
		FLAG_CLEAR(FLAG_ADJUST);
	}
	if ((old_al > 0x99) || old_cf) {
		cpu->al = cpu->al + 0x60;
		FLAG_SET(FLAG_CARRY);
	}
	cpu_uf_zsp(cpu, cpu->al, 0);
}

static inline void cpu_das(cpu_state* cpu) {
	u8 old_al = cpu->al;
	u8 old_cf = cpu->flags & 0x01;
	cpu->flags &= 0xFFFE;
	if (((cpu->al & 0x0F) > 0x9) || FLAG(4)) {
		cpu->al -= 6;
		FLAG_SET(FLAG_ADJUST);
		if (old_cf || (old_al < 6)) cpu->flags |= FLAG_CARRY;
	} else {
		FLAG_CLEAR(FLAG_ADJUST);
	}
	if ((old_al > 0x99) || old_cf) {
		cpu->al = cpu->al - 0x60;
		FLAG_SET(FLAG_CARRY);
	}
	cpu_uf_zsp(cpu, cpu->al, 0);
}

static inline void cpu_aaa(cpu_state* cpu) {
	if (((cpu->al & 0x0F) > 0x09) || FLAG(4)) {
		cpu->ax += 0x106;
		FLAG_SET(FLAG_CARRY | FLAG_ADJUST);
	} else {
		FLAG_CLEAR(FLAG_CARRY | FLAG_ADJUST);
	}
	cpu->ax &= 0xFF0F;
}

static inline void cpu_aas(cpu_state* cpu) {
	if (((cpu->al & 0x0F) > 0x09) || FLAG(4)) {
		cpu->ax -= 0x6;
		cpu->ah--;
		FLAG_SET(FLAG_CARRY | FLAG_ADJUST);
	} else {
		FLAG_CLEAR(FLAG_CARRY | FLAG_ADJUST);
	}
	cpu->ax &= 0xFF0F;
}
#endif

#define CPU_JMP(cond) { \
	s8 offset = cpu_advance_ip(cpu); \
	if ((cond)) cpu->ip += offset; \
	break; \
}

#define CPU_JMP_TABLE(offs) \
	case (offs)+0:  CPU_JMP(FLAG(FLAG_OVERFLOW)) \
	case (offs)+1:  CPU_JMP(!FLAG(FLAG_OVERFLOW)) \
	case (offs)+2:  CPU_JMP(FLAG(FLAG_CARRY)) \
	case (offs)+3:  CPU_JMP(!FLAG(FLAG_CARRY)) \
	case (offs)+4:  CPU_JMP(FLAG(FLAG_ZERO)) \
	case (offs)+5:  CPU_JMP(!FLAG(FLAG_ZERO)) \
	case (offs)+6:  CPU_JMP(FLAG(FLAG_CARRY | FLAG_ZERO)) \
	case (offs)+7:  CPU_JMP(!FLAG(FLAG_CARRY | FLAG_ZERO)) \
	case (offs)+8:  CPU_JMP(FLAG(FLAG_SIGN)) \
	case (offs)+9:  CPU_JMP(!FLAG(FLAG_SIGN)) \
	case (offs)+10: CPU_JMP(FLAG(FLAG_PARITY)) \
	case (offs)+11: CPU_JMP(!FLAG(FLAG_PARITY)) \
	case (offs)+12: CPU_JMP(FLAG(FLAG_OVERFLOW) != FLAG(FLAG_SIGN)) \
	case (offs)+13: CPU_JMP(FLAG(FLAG_OVERFLOW) == FLAG(FLAG_SIGN)) \
	case (offs)+14: CPU_JMP((FLAG(FLAG_OVERFLOW) != FLAG(FLAG_SIGN)) || FLAG(FLAG_ZERO)) \
	case (offs)+15: CPU_JMP(!((FLAG(FLAG_OVERFLOW) != FLAG(FLAG_SIGN)) || FLAG(FLAG_ZERO)))

static void cpu_grp1(cpu_state* cpu, u8 v, mrm_entry e, u8 opcode) {
	switch (v) {
		case 0: cpu_add(cpu, e, opcode, 0); break;
		case 1: cpu_or(cpu, e, opcode); break;
		case 2: cpu_add(cpu, e, opcode, 1); break;
		case 3: cpu_sub(cpu, e, opcode, 1); break;
		case 4: cpu_and(cpu, e, opcode); break;
		case 5: cpu_sub(cpu, e, opcode, 0); break;
		case 6: cpu_xor(cpu, e, opcode); break;
		case 7: cpu_cmp_mrm(cpu, e, opcode); break;
	}
}

static inline void cpu_grp1_u8(cpu_state* cpu) {
	mrm_entry e = cpu_mod_rm(cpu, 0, 0);
	u8 v = e.src & 0x7;
	e.src = 40; e.imm = cpu_advance_ip(cpu);
	cpu_grp1(cpu, v, e, 0);
}

static inline void cpu_grp1_u16(cpu_state* cpu) {
	mrm_entry e = cpu_mod_rm(cpu, 1, 0);
	u8 v = e.src & 0x7;
	e.src = 41; e.imm = cpu_advance_ip16(cpu);
	cpu_grp1(cpu, v, e, 1);
}

static inline void cpu_grp1_s8(cpu_state* cpu) {
	mrm_entry e = cpu_mod_rm(cpu, 1, 0);
	u8 v = e.src & 0x7;
	e.src = 41; e.imm = (s16) ((s8) cpu_advance_ip(cpu));
	cpu_grp1(cpu, v, e, 1);
}

#define CPU_XCHG(reg) { u16 v = reg; reg = cpu->ax; cpu->ax = v; break; }

#define CPU_S(amt, cmd) { \
	u32 addr_src = SEGMD(SEG_DS, cpu->si); \
	u32 addr_dst = SEG(SEG_ES, cpu->di); \
	cmd \
	cpu->si = incdec_dir(cpu, cpu->si, (amt)); \
	cpu->di = incdec_dir(cpu, cpu->di, (amt)); \
break; }


static void cpu_grp2(cpu_state* cpu, u8 v, mrm_entry e, u8 opcode) {
	switch (v) {
		case 0: case 1: case 2: case 3: cpu_rotate(cpu, e, opcode, v); break;
		case 4: cpu_shl(cpu, e, opcode); break;
		case 5: cpu_shr(cpu, e, opcode, 0); break;
		case 6: cpu_shl(cpu, e, opcode); break;
		case 7: cpu_shr(cpu, e, opcode, 1); break;
	}
}

#if defined(USE_OPCODES_80186)
static inline void cpu_grp2_u8(cpu_state* cpu, u8 opcode) {
	mrm_entry e = cpu_mod_rm(cpu, opcode & 0x01, 0);
	u8 v = e.src & 0x7;
	e.src = 40; e.imm = cpu_advance_ip(cpu);
	cpu_grp2(cpu, v, e, opcode);
}
#endif

static inline void cpu_grp2_1(cpu_state* cpu, u8 opcode) {
	mrm_entry e = cpu_mod_rm(cpu, opcode & 0x01, 0);
	u8 v = e.src & 0x7;
	e.src = 40; e.imm = 1;
	cpu_grp2(cpu, v, e, opcode);
}

static inline void cpu_grp2_cl(cpu_state* cpu, u8 opcode) {
	mrm_entry e = cpu_mod_rm(cpu, opcode & 0x01, 0);
	u8 v = e.src & 0x7;
	e.src = 17;
	cpu_grp2(cpu, v, e, opcode);
}

static void cpu_grp3(cpu_state* cpu, u8 opcode) {
	mrm_entry e = cpu_mod_rm(cpu, opcode & 1, 0);
	switch (e.src & 0x07) {
		case 0:
			e.src = 40 + (opcode & 1);
			e.imm = (opcode & 0x01) ? cpu_advance_ip16(cpu) : cpu_advance_ip(cpu);
			cpu_test(cpu, e, opcode);
			break;
		case 1:
			cpu_ext_log("invalid opcode: GRP3/1");
			break;
		case 2: /* NOT */
			cpu_write_rm(cpu, &e, e.dst, cpu_read_rm(cpu, &e, e.dst) ^ ((opcode & 0x01) ? 0xFFFF : 0xFF));
			break;
		case 3: /* NEG */ {
			u16 src = cpu_read_rm(cpu, &e, e.dst);
			u16 result = 0;
			if (opcode & 1) {
				result = ((src ^ 0xFFFF) + 1);
				FLAG_WRITE(FLAG_OVERFLOW, src == 0x8000);
			} else {
				result = ((src ^ 0xFF) + 1) & 0xFF;
				FLAG_WRITE(FLAG_OVERFLOW, src == 0x80);
			}
			cpu_write_rm(cpu, &e, e.dst, result);
			FLAG_WRITE(FLAG_CARRY, src != 0);
			FLAG_WRITE(FLAG_ADJUST, ((src ^ result) & 0x10) != 0);
			cpu_uf_zsp(cpu, result, opcode);
		} break;
		case 4:
			e.src = opcode == 0xF7 ? 0 : 16;
			cpu_mul(cpu, e, opcode);
			break;
		case 5:
			e.src = opcode == 0xF7 ? 0 : 16;
			cpu_imul(cpu, e, opcode);
			break;
		case 6:
			e.src = opcode == 0xF7 ? 0 : 16;
			cpu_div(cpu, e, opcode);
			break;
		case 7:
			e.src = opcode == 0xF7 ? 0 : 16;
			cpu_idiv(cpu, e, opcode);
			break;
	}
}

static void cpu_grp4(cpu_state* cpu, u8 opcode) {
	mrm_entry e = cpu_mod_rm(cpu, 0, 0);
	switch (e.src & 0x07) {
		case 0: {
			u8 v = cpu_read_rm(cpu, &e, e.dst) + 1;
			cpu_write_rm(cpu, &e, e.dst, v);
			cpu_uf_inc(cpu, v, 0);
		} break;
		case 1: {
			u8 v = cpu_read_rm(cpu, &e, e.dst) - 1;
			cpu_write_rm(cpu, &e, e.dst, v);
			cpu_uf_dec(cpu, v, 0);
		} break;
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		case 7:
			cpu_ext_log("invalid grp4 opcode");
			break;
	}
}

static void cpu_grp5(cpu_state* cpu, u8 opcode) {
	mrm_entry e = cpu_mod_rm(cpu, 1, 0);
	switch (e.src & 0x07) {
		case 0: {
			u16 v = cpu_read_rm(cpu, &e, e.dst) + 1;
			cpu_write_rm(cpu, &e, e.dst, v);
			cpu_uf_inc(cpu, v, 1);
		} break;
		case 1: {
			u16 v = cpu_read_rm(cpu, &e, e.dst) - 1;
			cpu_write_rm(cpu, &e, e.dst, v);
			cpu_uf_dec(cpu, v, 1);
		} break;
		case 2: { // CALL near abs
			u16 new_ip = cpu_read_rm(cpu, &e, e.dst);
			cpu_push16(cpu, cpu->ip);
			cpu->ip = new_ip;
		} break;
		case 3: { // CALL far abs
			u32 addr = SEGMD(cpu_seg_rm(e.dst), cpu_addr_rm(cpu, &e, e.dst));
			u16 new_ip = ram_u16(cpu, addr);
			u16 new_cs = ram_u16(cpu, addr + 2);
			cpu_push16(cpu, cpu->seg[SEG_CS]);
			cpu_push16(cpu, cpu->ip);
			cpu->seg[SEG_CS] = new_cs;
			cpu->ip = new_ip;
		} break;
		case 4: { // JMP near abs
			cpu->ip = cpu_read_rm(cpu, &e, e.dst);
		} break;
		case 5: { // JMP far abs
			u32 addr = SEGMD(cpu_seg_rm(e.dst), cpu_addr_rm(cpu, &e, e.dst));
			u16 new_ip = ram_u16(cpu, addr);
			u16 new_cs = ram_u16(cpu, addr + 2);
			cpu->seg[SEG_CS] = new_cs;
			cpu->ip = new_ip;
		} break;
		case 6: cpu_push16(cpu, cpu_read_rm(cpu, &e, e.dst)); break;
		case 7:
			cpu_ext_log("invalid grp5 opcode");
			break;
	}
}

#ifdef USE_OPCODES_DECIMAL
static inline void cpu_aam(cpu_state* cpu) {
	u8 base = cpu_advance_ip(cpu);
	u8 old_al = cpu->al;
	cpu->ah = old_al / base;
	cpu->al = old_al % base;
	cpu_uf_zsp(cpu, cpu->al, 0);
}

static inline void cpu_aad(cpu_state* cpu) {
	u8 base = cpu_advance_ip(cpu);
	u8 old_al = cpu->al;
	u8 old_ah = cpu->ah;
	cpu->ax = (old_al + (old_ah * base)) & 0xFF;
	cpu_uf_zsp(cpu, cpu->ax, 0);
}
#endif

static int cpu_run_one(cpu_state* cpu, u8 no_interrupting, u8 pr_state) {
	if (cpu->intq_pos > 0 && !no_interrupting) {
		u8 intr = cpu->intq[0];
		if (intr == 2 || FLAG(FLAG_INTERRUPT)) {
			cpu_pop_interrupt(cpu);
			cpu_int(cpu, intr);
		}
	}

	if (((cpu->ip & 0xFF00) == 0x1100) && (cpu->seg[SEG_CS] == 0xF000)) {
		FLAG_SET(FLAG_INTERRUPT);
		int res = cpu->func_interrupt(cpu, (cpu->ip & 0xFF));
		if (res != STATE_BLOCK) {
			cpu->ip = cpu_pop16(cpu);
			cpu->seg[SEG_CS] = cpu_pop16(cpu);
			u16 old_flags = cpu_pop16(cpu);
			u16 old_flag_mask = FLAG_INTERRUPT;
			cpu->flags &= ~(old_flag_mask);
			cpu->flags |= (old_flags & old_flag_mask);
			return res;
		} else {
			return STATE_BLOCK;
		}
	}

	u8 opcode = cpu_advance_ip(cpu);

	switch (opcode) {
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
		case 0x04:
		case 0x05:
			cpu_add(cpu, cpu_mod_rm6(cpu, opcode), opcode, 0);
			break;
		case 0x06:
			cpu_push16(cpu, cpu->seg[SEG_ES]);
			break;
		case 0x07:
			cpu->seg[SEG_ES] = cpu_pop16(cpu);
			break;
		case 0x08:
		case 0x09:
		case 0x0A:
		case 0x0B:
		case 0x0C:
		case 0x0D:
			cpu_or(cpu, cpu_mod_rm6(cpu, opcode), opcode);
			break;
		case 0x0E:
			cpu_push16(cpu, cpu->seg[SEG_CS]);
			break;
#if defined(USE_OPCODES_8086_UNDOCUMENTED)
		case 0x0F:
			cpu->seg[SEG_CS] = cpu_pop16(cpu);
			break;
#endif
		case 0x10:
		case 0x11:
		case 0x12:
		case 0x13:
		case 0x14:
		case 0x15:
			cpu_add(cpu, cpu_mod_rm6(cpu, opcode), opcode, 1);
			break;
		case 0x16:
			cpu_push16(cpu, cpu->seg[SEG_SS]);
			break;
		case 0x17:
			cpu->seg[SEG_SS] = cpu_pop16(cpu);
			break;
		case 0x18:
		case 0x19:
		case 0x1A:
		case 0x1B:
		case 0x1C:
		case 0x1D:
			cpu_sub(cpu, cpu_mod_rm6(cpu, opcode), opcode, 1);
			break;
		case 0x1E:
			cpu_push16(cpu, cpu->seg[SEG_DS]);
			break;
		case 0x1F:
			cpu->seg[SEG_DS] = cpu_pop16(cpu);
			break;
		case 0x20:
		case 0x21:
		case 0x22:
		case 0x23:
		case 0x24:
		case 0x25:
			cpu_and(cpu, cpu_mod_rm6(cpu, opcode), opcode);
			break;
		case 0x26:
			cpu->segmod = SEG_ES+1;
			int r26 = cpu_run_one(cpu, 1, 1);
			cpu->segmod = 0;
			return r26;
#ifdef USE_OPCODES_DECIMAL
		case 0x27:
			cpu_daa(cpu);
			break;
#endif
		case 0x28:
		case 0x29:
		case 0x2A:
		case 0x2B:
		case 0x2C:
		case 0x2D:
			cpu_sub(cpu, cpu_mod_rm6(cpu, opcode), opcode, 0);
			break;
		case 0x2E:
			cpu->segmod = SEG_CS+1;
			int r2e = cpu_run_one(cpu, 1, 1);
			cpu->segmod = 0;
			return r2e;
#ifdef USE_OPCODES_DECIMAL
		case 0x2F:
			cpu_das(cpu);
			break;
#endif
		case 0x30:
		case 0x31:
		case 0x32:
		case 0x33:
		case 0x34:
		case 0x35:
			cpu_xor(cpu, cpu_mod_rm6(cpu, opcode), opcode);
			break;
		case 0x36:
			cpu->segmod = SEG_SS+1;
			int r36 = cpu_run_one(cpu, 1, 1);
			cpu->segmod = 0;
			return r36;
#ifdef USE_OPCODES_DECIMAL
		case 0x37:
			cpu_aaa(cpu);
			break;
#endif
		case 0x38:
		case 0x39:
		case 0x3A:
		case 0x3B:
		case 0x3C:
		case 0x3D:
			cpu_cmp_mrm(cpu, cpu_mod_rm6(cpu, opcode), opcode);
			break;
		case 0x3E:
			cpu->segmod = SEG_DS+1;
			int r3e = cpu_run_one(cpu, 1, 1);
			cpu->segmod = 0;
			return r3e;
#ifdef USE_OPCODES_DECIMAL
		case 0x3F:
			cpu_aas(cpu);
			break;
#endif
		case 0x40: cpu->ax++; cpu_uf_inc(cpu, cpu->ax, 1); break;
		case 0x41: cpu->cx++; cpu_uf_inc(cpu, cpu->cx, 1); break;
		case 0x42: cpu->dx++; cpu_uf_inc(cpu, cpu->dx, 1); break;
		case 0x43: cpu->bx++; cpu_uf_inc(cpu, cpu->bx, 1); break;
		case 0x44: cpu->sp++; cpu_uf_inc(cpu, cpu->sp, 1); break;
		case 0x45: cpu->bp++; cpu_uf_inc(cpu, cpu->bp, 1); break;
		case 0x46: cpu->si++; cpu_uf_inc(cpu, cpu->si, 1); break;
		case 0x47: cpu->di++; cpu_uf_inc(cpu, cpu->di, 1); break;
		case 0x48: cpu->ax--; cpu_uf_dec(cpu, cpu->ax, 1); break;
		case 0x49: cpu->cx--; cpu_uf_dec(cpu, cpu->cx, 1); break;
		case 0x4A: cpu->dx--; cpu_uf_dec(cpu, cpu->dx, 1); break;
		case 0x4B: cpu->bx--; cpu_uf_dec(cpu, cpu->bx, 1); break;
		case 0x4C: cpu->sp--; cpu_uf_dec(cpu, cpu->sp, 1); break;
		case 0x4D: cpu->bp--; cpu_uf_dec(cpu, cpu->bp, 1); break;
		case 0x4E: cpu->si--; cpu_uf_dec(cpu, cpu->si, 1); break;
		case 0x4F: cpu->di--; cpu_uf_dec(cpu, cpu->di, 1); break;
		case 0x50: cpu_push16(cpu, cpu->ax); break;
		case 0x51: cpu_push16(cpu, cpu->cx); break;
		case 0x52: cpu_push16(cpu, cpu->dx); break;
		case 0x53: cpu_push16(cpu, cpu->bx); break;
#ifdef USE_8086_PUSH_SP_BUG
		case 0x54: cpu_push16(cpu, cpu->sp - 2); break;
#else
		case 0x54: cpu_push16(cpu, cpu->sp); break;
#endif
		case 0x55: cpu_push16(cpu, cpu->bp); break;
		case 0x56: cpu_push16(cpu, cpu->si); break;
		case 0x57: cpu_push16(cpu, cpu->di); break;
		case 0x58: cpu->ax = cpu_pop16(cpu); break;
		case 0x59: cpu->cx = cpu_pop16(cpu); break;
		case 0x5A: cpu->dx = cpu_pop16(cpu); break;
		case 0x5B: cpu->bx = cpu_pop16(cpu); break;
		case 0x5C: cpu->sp = cpu_pop16(cpu); break;
		case 0x5D: cpu->bp = cpu_pop16(cpu); break;
		case 0x5E: cpu->si = cpu_pop16(cpu); break;
		case 0x5F: cpu->di = cpu_pop16(cpu); break;
#if defined(USE_OPCODES_80186)
		case 0x60: { // PUSHA
			u16 tmp = cpu->sp;
			cpu_push16(cpu, cpu->ax);
			cpu_push16(cpu, cpu->cx);
			cpu_push16(cpu, cpu->dx);
			cpu_push16(cpu, cpu->bx);
			cpu_push16(cpu, tmp);
			cpu_push16(cpu, cpu->bp);
			cpu_push16(cpu, cpu->si);
			cpu_push16(cpu, cpu->di);
		} break;
		case 0x61: { // POPA
			cpu->di = cpu_pop16(cpu);
			cpu->si = cpu_pop16(cpu);
			cpu->bp = cpu_pop16(cpu);
			/*cpu->sp*/cpu_pop16(cpu);
			cpu->bx = cpu_pop16(cpu);
			cpu->dx = cpu_pop16(cpu);
			cpu->cx = cpu_pop16(cpu);
			cpu->ax = cpu_pop16(cpu);
		} break;
		case 0x68: cpu_push16(cpu, cpu_advance_ip16(cpu)); break;
		case 0x6A: cpu_push16(cpu, cpu_advance_ip(cpu)); break;
		// TODO: further 80186 opcodes
#elif defined(USE_OPCODES_8086_ALIASED)
		CPU_JMP_TABLE(0x60)
#endif
		CPU_JMP_TABLE(0x70)
		case 0x80: case 0x82: cpu_grp1_u8(cpu); break;
		case 0x81: cpu_grp1_u16(cpu); break;
		case 0x83: cpu_grp1_s8(cpu); break;
		case 0x84: cpu_test(cpu, cpu_mod_rm(cpu, 0, 0), opcode); break;
		case 0x85: cpu_test(cpu, cpu_mod_rm(cpu, 1, 0), opcode); break;
		case 0x86:
		case 0x87: {
			mrm_entry e = cpu_mod_rm(cpu, opcode & 0x01, 0);
			u16 t = cpu_read_rm(cpu, &e, e.src);
			cpu_write_rm(cpu, &e, e.src, cpu_read_rm(cpu, &e, e.dst));
			cpu_write_rm(cpu, &e, e.dst, t);
		} break;
		case 0x88: cpu_mov(cpu, cpu_mod_rm(cpu, 0, 0)); break;
		case 0x89: cpu_mov(cpu, cpu_mod_rm(cpu, 1, 0)); break;
		case 0x8A: cpu_mov(cpu, cpu_mod_rm(cpu, 2, 0)); break;
		case 0x8B: cpu_mov(cpu, cpu_mod_rm(cpu, 3, 0)); break;
		case 0x8C: /* MOV segment */
		case 0x8E: {
			mrm_entry e = cpu_mod_rm(cpu, opcode, 1024);
			if (e.dst == 26+SEG_CS) {
				cpu_ext_log("Tried writing to CS segment!");
				return STATE_END;
			}
			cpu_mov(cpu, e);
			// Loading the SS register with a MOV inhibits all interrupts until
			// after the next instruction, so let's just call an extra, non-interruptible
			// instruction here
			if (e.dst == 26+SEG_SS) {
				return cpu_run_one(cpu, 1, 1);
			}
		} break;
		case 0x8D: /* LEA */ {
			mrm_entry e = cpu_mod_rm(cpu, 3, 0);
			cpu_write_rm(cpu, &e, e.dst, cpu_addr_rm(cpu, &e, e.src));
		} break;
		case 0x8F: /* POP m16 */ {
			mrm_entry e = cpu_mod_rm(cpu, 1, 0);
			cpu_write_rm(cpu, &e, e.dst, cpu_pop16(cpu));
		} break;
		case 0x90: /* XCHG AX, AX == NOP */ break;
		case 0x91: CPU_XCHG(cpu->cx);
		case 0x92: CPU_XCHG(cpu->dx);
		case 0x93: CPU_XCHG(cpu->bx);
		case 0x94: CPU_XCHG(cpu->sp);
		case 0x95: CPU_XCHG(cpu->bp);
		case 0x96: CPU_XCHG(cpu->si);
		case 0x97: CPU_XCHG(cpu->di);
		case 0x98: /* CBW */ {
			cpu->ax = (s16) ((s8) cpu->al);
		} break;
		case 0x99: /* CWD */ {
			cpu->dx = (cpu->ax >= 0x8000) ? 0xFFFF : 0x0000;
		} break;
		case 0x9A: /* CALL far */ {
			u16 new_ip = cpu_advance_ip16(cpu);
			u16 new_cs = cpu_advance_ip16(cpu);
			cpu_push16(cpu, cpu->seg[SEG_CS]);
			cpu_push16(cpu, cpu->ip);
			cpu->seg[SEG_CS] = new_cs;
			cpu->ip = new_ip;
		} break;
		case 0x9C: cpu_push16(cpu, cpu->flags); break;
		// ARCH: The 286 clears bits 12-15 in real mode.
		case 0x9D: cpu->flags = cpu_pop16(cpu) | 0xF002; break;
		case 0x9E: /* SAHF */ cpu->flags = (cpu->flags & 0xFF00) | cpu->ah; break;
		case 0x9F: /* LAHF */ cpu->ah = (u8) cpu->flags; break;
		case 0xA0: /* MOV offs->AL */ {
			u16 addr = cpu_advance_ip16(cpu);
			cpu->al = ram_u8(cpu, SEGMD(SEG_DS, addr));
		} break;
		case 0xA1: /* MOV offs->AX */ {
			u16 addr = cpu_advance_ip16(cpu);
			cpu->ax = ram_u16(cpu, SEGMD(SEG_DS, addr));
		} break;
		case 0xA2: /* MOV AL->offs */ {
			u16 addr = cpu_advance_ip16(cpu);
			ram_w8(cpu, SEGMD(SEG_DS, addr), cpu->al);
		} break;
		case 0xA3: /* MOV AX->offs */ {
			u16 addr = cpu_advance_ip16(cpu);
			ram_w16(cpu, SEGMD(SEG_DS, addr), cpu->ax);
		} break;
		case 0xA4: CPU_S(1, {
			ram_w8(cpu, addr_dst, ram_u8(cpu, addr_src));
		}); /* MOVSB */
		case 0xA5: CPU_S(2, {
			ram_w16(cpu, addr_dst, ram_u16(cpu, addr_src));
		}); /* MOVSW */
		case 0xA6: CPU_S(1, {
			cpu_cmp(cpu, ram_u8(cpu, addr_src), ram_u8(cpu, addr_dst), 0);
		}); /* CMPSB */
		case 0xA7: CPU_S(2, {
			cpu_cmp(cpu, ram_u16(cpu, addr_src), ram_u16(cpu, addr_dst), 1);
		}); /* CMPSW */
		case 0xA8: cpu_uf_bit(cpu, cpu->al & cpu_advance_ip(cpu), 0); break;
		case 0xA9: cpu_uf_bit(cpu, cpu->ax & cpu_advance_ip16(cpu), 1); break;
		case 0xAA: {
			u32 addr_dst = SEG(SEG_ES, cpu->di);
			ram_w8(cpu, addr_dst, cpu->al);
			cpu->di = incdec_dir(cpu, cpu->di, 1);
		} break; /* STOSB */
		case 0xAB: {
			u32 addr_dst = SEG(SEG_ES, cpu->di);
			ram_w16(cpu, addr_dst, cpu->ax);
			cpu->di = incdec_dir(cpu, cpu->di, 2);
		} break; /* STOSW */
		case 0xAC: {
			u32 addr_src = SEGMD(SEG_DS, cpu->si);
			cpu->al = ram_u8(cpu, addr_src);
			cpu->si = incdec_dir(cpu, cpu->si, 1);
		} break; /* LODSB */
		case 0xAD: {
			u32 addr_src = SEGMD(SEG_DS, cpu->si);
			cpu->ax = ram_u16(cpu, addr_src);
			cpu->si = incdec_dir(cpu, cpu->si, 2);
		} break; /* LODSW */
		case 0xAE: {
			u32 addr_dst = SEG(SEG_ES, cpu->di);
			cpu_cmp(cpu, cpu->al, ram_u8(cpu, addr_dst), opcode);
			cpu->di = incdec_dir(cpu, cpu->di, 1);
		} break; /* SCASB */
		case 0xAF: {
			u32 addr_dst = SEG(SEG_ES, cpu->di);
			cpu_cmp(cpu, cpu->ax, ram_u16(cpu, addr_dst), opcode);
			cpu->di = incdec_dir(cpu, cpu->di, 2);
		} break; /* SCASW */
		case 0xB0: cpu->al = cpu_advance_ip(cpu); break;
		case 0xB1: cpu->cl = cpu_advance_ip(cpu); break;
		case 0xB2: cpu->dl = cpu_advance_ip(cpu); break;
		case 0xB3: cpu->bl = cpu_advance_ip(cpu); break;
		case 0xB4: cpu->ah = cpu_advance_ip(cpu); break;
		case 0xB5: cpu->ch = cpu_advance_ip(cpu); break;
		case 0xB6: cpu->dh = cpu_advance_ip(cpu); break;
		case 0xB7: cpu->bh = cpu_advance_ip(cpu); break;
		case 0xB8: cpu->ax = cpu_advance_ip16(cpu); break;
		case 0xB9: cpu->cx = cpu_advance_ip16(cpu); break;
		case 0xBA: cpu->dx = cpu_advance_ip16(cpu); break;
		case 0xBB: cpu->bx = cpu_advance_ip16(cpu); break;
		case 0xBC: cpu->sp = cpu_advance_ip16(cpu); break;
		case 0xBD: cpu->bp = cpu_advance_ip16(cpu); break;
		case 0xBE: cpu->si = cpu_advance_ip16(cpu); break;
		case 0xBF: cpu->di = cpu_advance_ip16(cpu); break;
#if defined(USE_OPCODES_8086_ALIASED)
		case 0xC0:
#endif
		case 0xC2: /* RET near + pop */ {
			u16 btp = cpu_advance_ip16(cpu);
			cpu->ip = cpu_pop16(cpu);
			cpu->sp += btp;
		} break;
#if defined(USE_OPCODES_8086_ALIASED)
		case 0xC1:
#endif
		case 0xC3: /* RET near */ {
			cpu->ip = cpu_pop16(cpu);
		} break;
		case 0xC4: case 0xC5: /* LES, LDS */ {
			mrm_entry e = cpu_mod_rm(cpu, 3, 0);
			u16 addr = cpu_addr_rm(cpu, &e, e.src);
			u8 defseg = cpu_seg_rm(e.src);
			cpu_write_rm(cpu, &e, e.dst, ram_u16(cpu, SEGMD(defseg, addr)));
			cpu->seg[opcode == 0xC5 ? SEG_DS : SEG_ES] = ram_u16(cpu, SEGMD(defseg, addr + 2));
		} break;
		case 0xC6: {
			mrm_entry e = cpu_mod_rm(cpu, 0, 0);
			cpu_write_rm(cpu, &e, e.dst, cpu_advance_ip(cpu));
		} break;
		case 0xC7: {
			mrm_entry e = cpu_mod_rm(cpu, 1, 0);
			cpu_write_rm(cpu, &e, e.dst, cpu_advance_ip16(cpu));
		} break;
#if defined(USE_OPCODES_8086_ALIASED)
		case 0xC8:
#endif
		case 0xCA: /* RET far * pop */ {
			u16 btp = cpu_advance_ip16(cpu);
			cpu->ip = cpu_pop16(cpu);
			cpu->seg[SEG_CS] = cpu_pop16(cpu);
			cpu->sp += btp;
		} break;
#if defined(USE_OPCODES_8086_ALIASED)
		case 0xC9:
#endif
		case 0xCB: /* RET far */ {
			cpu->ip = cpu_pop16(cpu);
			cpu->seg[SEG_CS] = cpu_pop16(cpu);
		} break;
		case 0xCC: /* INT 3 */ {
			cpu_ext_log("Breakpoint");
			return STATE_END;
		} break;
		case 0xCD: cpu_int(cpu, cpu_advance_ip(cpu)); break;
		case 0xCE: if (FLAG(FLAG_OVERFLOW)) cpu_int(cpu, 4); break;
		case 0xCF: /* IRET far */ {
			cpu->ip = cpu_pop16(cpu);
			cpu->seg[SEG_CS] = cpu_pop16(cpu);
			cpu->flags = cpu_pop16(cpu);
		} break;
#if defined(USE_OPCODES_80186)
		case 0xC0: case 0xC1: cpu_grp2_u8(cpu, opcode); break;
#endif
		case 0xD0: case 0xD1: cpu_grp2_1(cpu, opcode); break;
		case 0xD2: case 0xD3: cpu_grp2_cl(cpu, opcode); break;
#ifdef USE_OPCODES_DECIMAL
		case 0xD4: cpu_aam(cpu); break;
		case 0xD5: cpu_aad(cpu); break;
#endif
#if defined(USE_OPCODES_SALC)
		case 0xD6: /* SALC */ cpu->al = (cpu->flags & 0x01) * 0xFF; break;
#endif
		case 0xD7: /* XLAT */ {
			u16 addr = cpu->bx + cpu->al;
			cpu->al = ram_u8(cpu, SEGMD(SEG_DS, addr));
		} break;
		case 0xE0: /* LOOPNZ r8 */ {
			s8 offset = (s8) cpu_advance_ip(cpu);
			cpu->cx--;
			if (cpu->cx != 0 && !FLAG(FLAG_ZERO))
				cpu->ip += offset;
		} break;
		case 0xE1: /* LOOPZ r8 */ {
			s8 offset = (s8) cpu_advance_ip(cpu);
			cpu->cx--;
			if (cpu->cx != 0 && FLAG(FLAG_ZERO))
				cpu->ip += offset;
		} break;
		case 0xE2: /* LOOP r8 */ {
			s8 offset = (s8) cpu_advance_ip(cpu);
			cpu->cx--;
			if (cpu->cx != 0)
				cpu->ip += offset;
		} break;
		case 0xE3: /* JCXZ r8 */ {
			s8 offset = (s8) cpu_advance_ip(cpu);
			if (cpu->cx == 0)
				cpu->ip += offset;
		} break;
		case 0xE4: cpu->al = cpu->func_port_in(cpu, cpu_advance_ip(cpu)); break;
		case 0xE5: cpu->ax = cpu->func_port_in(cpu, cpu_advance_ip(cpu)); break;
		case 0xE6: cpu->func_port_out(cpu, cpu_advance_ip(cpu), cpu->al); break;
		case 0xE7: cpu->func_port_out(cpu, cpu_advance_ip(cpu), cpu->ax); break;
		case 0xE8: /* CALL rel16 */ {
			s16 offset = (s16) cpu_advance_ip16(cpu);
			cpu_push16(cpu, cpu->ip);
			cpu->ip += offset;
		} break;
		case 0xE9: /* JMP rel16 */ {
			s16 offset = (s16) cpu_advance_ip16(cpu);
			cpu->ip += offset;
		} break;
		case 0xEA: /* JMP ptr */ {
			u16 new_ip = cpu_advance_ip16(cpu);
			u16 new_cs = cpu_advance_ip16(cpu);
			cpu->ip = new_ip;
			cpu->seg[SEG_CS] = new_cs;
		} break;
		case 0xEB: /* JMP rel8 */ {
			s8 offset = (s8) cpu_advance_ip(cpu);
			cpu->ip += offset;
		} break;
		case 0xEC: cpu->al = cpu->func_port_in(cpu, cpu->dx); break;
		case 0xED: cpu->ax = cpu->func_port_in(cpu, cpu->dx); break;
		case 0xEE: cpu->func_port_out(cpu, cpu->dx, cpu->al); break;
		case 0xEF: cpu->func_port_out(cpu, cpu->dx, cpu->ax); break;
		case 0xF0: /* LOCK */ break;
#if defined(USE_OPCODES_8086_ALIASED)
		case 0xF1: /* LOCK */ break;
#endif
		case 0xF2: if (!cpu_rep(cpu, REP_COND_NZ)) return STATE_END; break;
		case 0xF3: if (!cpu_rep(cpu, REP_COND_Z)) return STATE_END; break;
		case 0xF4: cpu->halted = 1; return STATE_BLOCK;
		case 0xF5: /* CMC */ FLAG_COMPLEMENT(FLAG_CARRY); break;
		case 0xF6: case 0xF7: cpu_grp3(cpu, opcode); break;
		case 0xF8: FLAG_CLEAR(FLAG_CARRY); break;
		case 0xF9: FLAG_SET(FLAG_CARRY); break;
		case 0xFA: FLAG_CLEAR(FLAG_INTERRUPT); break;
		case 0xFB: FLAG_SET(FLAG_INTERRUPT); break;
		case 0xFC: FLAG_CLEAR(FLAG_DIRECTION); break;
		case 0xFD: FLAG_SET(FLAG_DIRECTION); break;
		case 0xFE: cpu_grp4(cpu, opcode); break;
		case 0xFF: cpu_grp5(cpu, opcode); break;

		/* FPU stubs */
		case 0x9B: break;
		case 0xD8:
		case 0xD9:
		case 0xDA:
		case 0xDB:
		case 0xDD:
		case 0xDC:
		case 0xDE:
		case 0xDF:
			cpu_mod_rm(cpu, 1, 0);
			break;
		default:
			cpu_ext_log("Invalid opcode!");
			cpu_emit_interrupt(cpu, 6);
			return STATE_CONTINUE;
	}

	return STATE_CONTINUE;
}

static u16 cpu_func_port_in_default(cpu_state* cpu, u16 addr) { return 0; }
static void cpu_func_port_out_default(cpu_state* cpu, u16 addr, u16 val) {}
static int cpu_func_interrupt_default(cpu_state* cpu, u8 intr) { return STATE_CONTINUE; }

int cpu_execute(cpu_state* cpu, int cycles) {
	int last_state = STATE_CONTINUE;
	int max_cycles = cpu->cycles + cycles;
	if (cpu->halted && cpu->intq_pos == 0) return STATE_BLOCK;

#ifdef CPU_CHECK_KEEP_GOING
	while (last_state == STATE_CONTINUE && ((((cpu->cycles)++) < max_cycles) || cpu->keep_going)) {
#else
	while (last_state == STATE_CONTINUE && (((cpu->cycles)++) < max_cycles)) {
#endif
#ifdef DBG1
		fprintf(stderr,
"[%04X %04X] AX:%04X CX:%04X DX:%04X BX:%04X SP:%04X BP:%04X SI:%04X DI:%04X | %04X %04X %04X %04X | %04X | opc %02X\n", 
cpu->seg[SEG_CS], cpu->ip, cpu->ax, cpu->cx, cpu->dx, cpu->bx, cpu->sp, cpu->bp, cpu->si, cpu->di, cpu->seg[0], cpu->seg[1],
cpu->seg[2], cpu->seg[3], cpu->flags, ram_u8(cpu, SEG(SEG_CS, cpu->ip)));
#endif
		last_state = cpu_run_one(cpu, 0, 1);
	}

	if (last_state >= STATE_WAIT_FRAME) {
		// try to avoid overflow
		cpu->cycles = 0;
	}
	return last_state;
}

void cpu_init(cpu_state* cpu) {
	int i;

	cpu->ax = 0;
	cpu->cx = 0;
	cpu->dx = 0;
	cpu->bx = 0;
	cpu->sp = 0;
	cpu->bp = 0;
	cpu->si = 0;
	cpu->di = 0;
	cpu->seg[0] = 0;
	cpu->seg[1] = 0;
	cpu->seg[2] = 0;
	cpu->seg[3] = 0;
	cpu->flags = 0x0202;
	cpu->halted = 0;
	cpu->segmod = 0;
	cpu->intq_pos = 0;
        cpu->keep_going = 0;
	cpu->cycles = 0;

	cpu->func_port_in = cpu_func_port_in_default;
	cpu->func_port_out = cpu_func_port_out_default;
	cpu->func_interrupt = cpu_func_interrupt_default;

	// clear
#ifdef NO_MEMSET
	for (i = 1024; i < 1048576; i++)
		cpu->ram[i] = 0;
#else
	memset(cpu->ram + 1024, 0, 1048576 - 1024);
#endif

	// ivt
	for (i = 0; i < 256; i++) {
		ram_w16(cpu, i * 4, 0x1100 | i);
		ram_w16(cpu, i * 4 + 2, 0xF000);
	}

	// iret
	// TODO: remove IRET from implemented ivts
	for (i = 0xF1100; i < 0xF1200; i++)
		cpu->ram[i] = 0xCF; /* IRET */
}
