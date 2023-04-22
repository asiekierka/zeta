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
#include "util.h"
#include "logging.h"

void fput16le(FILE *output, unsigned short i) {
#ifdef ZETA_BIG_ENDIAN
	fputc((i & 0xFF), output);
	fputc(((i >> 8) & 0xFF), output);
#else
	fwrite(&i, 2, 1, output);
#endif
}

void fput32le(FILE *output, unsigned int i) {
#ifdef ZETA_BIG_ENDIAN
	fputc((i & 0xFF), output);
	fputc(((i >> 8) & 0xFF), output);
	fputc(((i >> 16) & 0xFF), output);
	fputc(((i >> 24) & 0xFF), output);
#else
	fwrite(&i, 4, 1, output);
#endif
}

void fput16be(FILE *output, unsigned short i) {
#ifdef ZETA_BIG_ENDIAN
	fwrite(&i, 2, 1, output);
#else
	fputc(((i >> 8) & 0xFF), output);
	fputc((i & 0xFF), output);
#endif
}

void fput32be(FILE *output, unsigned int i) {
#ifdef ZETA_BIG_ENDIAN
	fwrite(&i, 4, 1, output);
#else
	fputc(((i >> 24) & 0xFF), output);
	fputc(((i >> 16) & 0xFF), output);
	fputc(((i >> 8) & 0xFF), output);
	fputc((i & 0xFF), output);
#endif
}

int highest_bit_index(int value) {
	int i = 0;
	while (value != 1) {
		value >>= 1;
		i++;
	}
	return i;
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
