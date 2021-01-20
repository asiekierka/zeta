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
#include <time.h>
#include "zzt.h"
#include "posix_vfs.h"

long zeta_time_ms() {
	clock_t c = clock();
	return c / (CLOCKS_PER_SEC/1000);
}

void cpu_ext_log(const char* s) {
	fprintf(stderr, "%s\n", s);
}

void speaker_on(double freq) {}
void speaker_off() {}

int zeta_has_feature(int feature) {
	return 1;
}

int main(int argc, char** argv) {
	init_posix_vfs("vfs/");
	zzt_init(argc > 1 ? argv[1] : "");

	clock_t last = clock();
	clock_t curr = last;

	while (zzt_execute(1)) {
		usleep(50000);
		//zzt_mark_frame();

		curr = clock();
/*		float secs = (float) (curr - last) / CLOCKS_PER_SEC;
		fprintf(stderr, "%.2f opc/sec\n", 1600000.0f / secs); */
		last = curr;

		//zzt_mark_timer();
	}
}
