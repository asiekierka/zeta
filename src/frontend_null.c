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
