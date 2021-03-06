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

#include <stdio.h>
#include <stdlib.h>
#include "render_software.h"

#define POS_MUL (scr_width <= 40 ? 2 : 1)

void render_software_rgb(u32 *buffer, int scr_width, int row_length, int flags, u8 *video, u8 *charset, int char_width, int char_height, u32 *palette) {
	int pos_mul = POS_MUL;
	int pos = 0;

	if (row_length < 0) {
		row_length = scr_width * char_width * pos_mul;
	}

	for (int y = 0; y < 25; y++) {
		for (int x = 0; x < scr_width; x++, pos += 2) {
			u8 chr = video[pos];
			u8 col = video[pos + 1];

			if (col >= 0x80 && !(flags & RENDER_BLINK_OFF)) {
				col &= 0x7F;
				if (flags & RENDER_BLINK_PHASE) {
					col = (col >> 4) * 0x11;
				}
			}

			u8 bg = col >> 4;
			u8 fg = col & 0xF;
			u8 *char_data = charset + (chr * char_height);

			for (int cy = 0; cy < char_height; cy++, char_data++) {
				int line = *char_data;
				int bpos = ((y * char_height + cy) * row_length) + ((x * char_width) * pos_mul);
				for (int cx = 0; cx < char_width; cx++, line <<= 1, bpos += pos_mul) {
					u32 col = palette[(line & 0x80) ? fg : bg];
					buffer[bpos] = col;
					if (pos_mul == 2) buffer[bpos+1] = col;
				}
			}
		}
	}
}

void render_software_paletted_range(u8 *buffer, int scr_width, int row_length, int flags, u8 *video, u8 *charset, int char_width, int char_height,
	int x1, int y1, int x2, int y2, render_software_char_draw_check_func char_draw_check_func)
{
	int pos_mul = POS_MUL;
	int x_pitch = (scr_width - (x2 - x1 + 1)) << 1;

	if (row_length < 0) {
		row_length = (x2 - x1 + 1) * char_width * pos_mul;
	}

	int pos = (y1 * scr_width + x1) << 1;

	for (int y = y1; y <= y2; y++) {
		for (int x = x1; x <= x2; x++, pos += 2) {
			if (char_draw_check_func != NULL && !char_draw_check_func(x, y)) {
				continue;
			}

			u8 chr = video[pos];
			u8 col = video[pos + 1];

			if (col >= 0x80 && !(flags & RENDER_BLINK_OFF)) {
				col &= 0x7F;
				if (flags & RENDER_BLINK_PHASE) {
					col = (col >> 4) * 0x11;
				}
			}

			u8 bg = col >> 4;
			u8 fg = col & 0xF;
			u8 *char_data = charset + (chr * char_height);

			for (int cy = 0; cy < char_height; cy++, char_data++) {
				int line = *char_data;
				int bpos = (((y - y1) * char_height + cy) * row_length) + (((x - x1) * char_width) * pos_mul);
				for (int cx = 0; cx < char_width; cx++, line <<= 1, bpos += pos_mul) {
					buffer[bpos] = (line & 0x80) ? fg : bg;
					if (pos_mul == 2) {
						buffer[bpos+1] = (line & 0x80) ? fg : bg;
					}
				}
			}
		}
		pos += x_pitch;
	}
}

void render_software_paletted(u8 *buffer, int scr_width, int row_length, int flags, u8 *video, u8 *charset, int char_width, int char_height) {
	render_software_paletted_range(buffer, scr_width, row_length, flags, video, charset, char_width, char_height,
		0, 0, scr_width - 1, 24, NULL);
}
