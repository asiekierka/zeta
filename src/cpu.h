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

#ifndef __CPU_H__
#define __CPU_H__
#define MAX_INTQUEUE_SIZE 256

#include "config.h"
#include "types.h"

#define FLAG_CARRY 1
#define FLAG_PARITY 4
#define FLAG_ADJUST 16
#define FLAG_ZERO 64
#define FLAG_SIGN 128
#define FLAG_TRAP 256
#define FLAG_INTERRUPT 512
#define FLAG_DIRECTION 1024
#define FLAG_OVERFLOW 2048

#define SEG_ES 0
#define SEG_CS 1
#define SEG_SS 2
#define SEG_DS 3

struct s_cpu_state {
	u8 ram[1048576];

	struct {
		union {
			struct {
				#ifdef BIG_ENDIAN
					u8 ah, al;
				#else
					u8 al, ah;
				#endif
			};
			u16 ax;
		};
	};
	struct {
		union {
			struct {
				#ifdef BIG_ENDIAN
					u8 ch, cl;
				#else
					u8 cl, ch;
				#endif
			};
			u16 cx;
		};
	};
	struct {
		union {
			struct {
				#ifdef BIG_ENDIAN
					u8 dh, dl;
				#else
					u8 dl, dh;
				#endif
			};
			u16 dx;
		};
	};
	struct {
		union {
			struct {
				#ifdef BIG_ENDIAN
					u8 bh, bl;
				#else
					u8 bl, bh;
				#endif
			};
			u16 bx;
		};
	};
	u16 sp, bp, si, di;
	u16 seg[4];
	u16 ip, flags;
	u8 segmod, halted, terminated;
	u32 keep_going;

	void* port_obj;
	void* intr_obj;

	u16 (*func_port_in)(struct s_cpu_state* cpu, u16 port);
	void (*func_port_out)(struct s_cpu_state* cpu, u16 port, u16 val);
	int /* state */ (*func_interrupt)(struct s_cpu_state* cpu, u8 intr);

	u8 intq[MAX_INTQUEUE_SIZE];
	int intq_pos;
};

typedef struct s_cpu_state cpu_state;

#define STATE_END 0
#define STATE_CONTINUE 1
#define STATE_BLOCK 2
#define STATE_WAIT 3

void cpu_init_globals();
void cpu_init(cpu_state* cpu);
int cpu_execute(cpu_state* cpu, int cycles);

void cpu_push16(cpu_state* cpu, u16 v);
u16 cpu_pop16(cpu_state* cpu);

void cpu_emit_interrupt(cpu_state* cpu, u8 intr);
void cpu_set_ip(cpu_state* cpu, u16 cs, u16 ip);

// external

IMPLEMENT_FUNCTION void cpu_ext_log(const char* msg);

#endif /* __CPU_H__ */
