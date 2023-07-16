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

#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 2
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <locale.h>

#include "zzt.h"
#include "posix_vfs.h"

long zeta_time_ms(void) {
	struct timespec spec;

	clock_gettime(CLOCK_REALTIME, &spec);
	return ((long) spec.tv_sec * 1000) + (long) (spec.tv_nsec / 1000000);
}

void cpu_ext_log(const char* s) {
//	fprintf(stderr, "%s\n", s);
}

void speaker_on(int cycles, double freq) {}
void speaker_off(int cycles) {}

int zeta_has_feature(int feature) {
	return 1;
}

void zeta_update_charset(int width, int height, u8* data) {
}

void zeta_update_palette(u32* data) {
}

void zeta_update_blink(int blink) {
}

void zeta_show_developer_warning(const char *format, ...) {
	va_list val;
	
	if (!developer_mode) return;
	va_start(val, format);
	vfprintf(stderr, format, val);
	va_end(val);
}

#include "asset_loader.h"
#include "frontend_posix.c"

int main(int argc, char** argv) {
	if (posix_zzt_init(argc, argv) < 0) {
		fprintf(stderr, "Could not load ZZT!\n");
		return 1;
	}

	int rcode = 0;

	while ((rcode = zzt_execute(64000)) > 0) {
		zzt_mark_frame();

/*		curr = clock();
		float secs = (float) (curr - last) / CLOCKS_PER_SEC;
		fprintf(stderr, "%.2f opc/sec\n", 1600000.0f / secs);
		last = curr; */

		zzt_mark_timer();
	}
}
