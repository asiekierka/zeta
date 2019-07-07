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
#include "render_software.h"

#define POS_MUL (scr_width <= 40 ? 2 : 1)

void render_software_rgb(u32 *buffer, int scr_width, int row_length, int flags, u8 *ram, u8 *charset, int char_width, int char_height, u32 *palette) {
	int pos_mul = POS_MUL;
	int pos = 0;

	if (row_length < 0) {
		row_length = scr_width * char_width * pos_mul;
	}

	for (int y = 0; y < 25; y++) {
		for (int x = 0; x < scr_width; x++, pos += 2) {
			u8 chr = ram[0xB8000 + pos];
			u8 col = ram[0xB8000 + pos + 1];

			if (col >= 0x80 && !(flags & RENDER_BLINK_OFF)) {
				col &= 0x7F;
				if (flags & RENDER_BLINK_PHASE) {
					col = (col >> 4) * 0x11;
				}
			}

			u8 bg = col >> 4;
			u8 fg = col & 0xF;
			u8 *co = charset + (chr * char_height);

			for (int cy = 0; cy < char_height; cy++, co++) {
				int line = *co;
				for (int cx = 0; cx < char_width; cx++, line <<= 1) {
					int bpos = ((y * char_height + cy) * row_length) + ((x * char_width + cx) * pos_mul);
					int col = palette[(line & 0x80) ? fg : bg];
					buffer[bpos] = col;
					if (pos_mul == 2) buffer[bpos+1] = col;
				}
			}
		}
	}
}

void render_software_paletted(u8 *buffer, int scr_width, int row_length, int flags, u8 *ram, u8 *charset, int char_width, int char_height) {
	int pos_mul = POS_MUL;
	int pos = 0;

	if (row_length < 0) {
		row_length = scr_width * char_width * pos_mul;
	}

	for (int y = 0; y < 25; y++) {
		for (int x = 0; x < scr_width; x++, pos += 2) {
			u8 chr = ram[0xB8000 + pos];
			u8 col = ram[0xB8000 + pos + 1];

			if (col >= 0x80 && !(flags & RENDER_BLINK_OFF)) {
				col &= 0x7F;
				if (flags & RENDER_BLINK_PHASE) {
					col = (col >> 4) * 0x11;
				}
			}

			u8 bg = col >> 4;
			u8 fg = col & 0xF;
			u8 *co = charset + (chr * char_height);

			for (int cy = 0; cy < char_height; cy++, co++) {
				int line = *co;
				for (int cx = 0; cx < char_width; cx++, line <<= 1) {
					int bpos = ((y * char_height + cy) * row_length) + ((x * char_width + cx) * pos_mul);
					buffer[bpos] = (line & 0x80) ? fg : bg;
					if (pos_mul == 2) {
						buffer[bpos+1] = (line & 0x80) ? fg : bg;
					}
				}
			}
		}
	}
}
