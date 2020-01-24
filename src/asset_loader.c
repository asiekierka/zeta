/**
 * Copyright (c) 2018, 2019, 2020 Adrian Siekierka
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
#include "zzt.h"

static u8 pld_to_pal[] = {
	0, 1, 2, 3, 4, 5, 20, 7,
	56, 57, 58, 59, 60, 61, 62, 63
};

static int zzt_load_chr(void *data, int dlen) {
	u8 *data8 = (u8*) data;

	if ((dlen & 0xFF) != 0) return -1;
	return zzt_load_charset(8, dlen >> 8, data8);
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
	u32 palette[16];
	u8 *data8 = (u8*) data;

	if (dlen < 192) return -1;

	for (int i = 0; i < 16; i++) {
		int pos = (int)(pld_to_pal[i]) * 3;

		palette[i] =
			  (((u32) (data8[pos] & 0x3F) * 255 / 63) << 16)
			| (((u32) (data8[pos + 1] & 0x3F) * 255 / 63) << 8)
			| (((u32) (data8[pos + 2] & 0x3F) * 255 / 63));
	}

	return zzt_load_palette(palette);
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
