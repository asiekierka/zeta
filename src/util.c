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
#include "util.h"
#include "logging.h"

void fput16le(FILE *output, unsigned short i) {
	fputc((i & 0xFF), output);
	fputc(((i >> 8) & 0xFF), output);
}

void fput32le(FILE *output, unsigned int i) {
	fputc((i & 0xFF), output);
	fputc(((i >> 8) & 0xFF), output);
	fputc(((i >> 16) & 0xFF), output);
	fputc(((i >> 24) & 0xFF), output);
}

FILE *create_inc_file(char *filename, int length, const char *tpl, const char *mode) {
	FILE *file;
	int i = -1;
	while ((++i) <= 9999) {
		snprintf(filename, length, tpl, i);
		file = fopen(filename, "rb");
		if (!file) {
			file = fopen(filename, mode);
			if (!file) {
				fprintf(stderr, "Could not open file '%s' for writing!\n", filename);
				return NULL;
			}
			return file;
		} else {
			// file exists
			fclose(file);
		}
	}
	fprintf(stderr, "Could not generate filename to write to!\n");
	return NULL;
}
