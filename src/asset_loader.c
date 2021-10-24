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
#include <string.h>
#include "zzt.h"

static int zzt_load_chr(void *data, int dlen) {
	u8 *data8 = (u8*) data;

	if ((dlen & 0xFF) != 0) return -1;
	return zzt_load_charset(8, dlen >> 8, data8, false);
}

static int zzt_load_pal(void *data, int dlen) {
	u32 palette[16];
	u8 *data8 = (u8*) data;

	if (dlen < 48) return -1;

	for (int i = 0; i < 16; i++) {
		int pos = i * 3;

		palette[i] =
			  (((u32) (data8[pos] & 0x3F) * 255 / 63) << 16)
			| (((u32) (data8[pos + 1] & 0x3F) * 255 / 63) << 8)
			| (((u32) (data8[pos + 2] & 0x3F) * 255 / 63));
	}

	return zzt_load_palette(palette);
}

static int zzt_load_pld(void *data, int dlen) {

	if (dlen < 192) return -1;

	return zzt_load_ega_palette((u8*) data);
}

int zzt_load_asset(char *type, void *data, int dlen) {
	char category[17];
	char *format = strchr(type, ':') + 1;
	if ((format - type) > 16) return -1; // overflow protection
	strncpy(category, type, format - type - 1);
	category[format - type - 1] = '\0';

	if (strcmp(category, "charset") == 0) {
		if (strcmp(format, "chr") == 0) {
			return zzt_load_chr(data, dlen);
		}
	} else if (strcmp(category, "palette") == 0) {
		if (strcmp(format, "pal") == 0) {
			return zzt_load_pal(data, dlen);
		} else if (strcmp(format, "pld") == 0) {
			return zzt_load_pld(data, dlen);
		}
	}

	return -2;
}
