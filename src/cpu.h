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
	u8 segmod, halted;
	u32 keep_going;
	u32 cycles;

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
#define STATE_WAIT_FRAME 3
#define STATE_WAIT_PIT 4
#define STATE_WAIT_TIMER 5

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
