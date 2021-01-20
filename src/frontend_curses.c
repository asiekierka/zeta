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

#include <ncurses.h>

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

static WINDOW* window;

#include "frontend_curses_tables.c"

int zeta_has_feature(int feature) {
	return 1;
}

void zeta_update_charset(int width, int height, u8* data) {
}

void zeta_update_palette(u32* data) {
}

static void platform_kbd_tick(void) {
	int c = getch();
	if (c == ERR) return;
	if (c == 263) c = 8;
	if (c == 0 || map_char_to_key[c] != 0) {
		int k = map_char_to_key[c];
		zzt_key(c, k);
		zzt_keyup(k);
	}
}

#include "asset_loader.h"
#include "frontend_posix.c"

static int nc_color_map[] = {
	COLOR_BLACK, COLOR_BLUE, COLOR_GREEN, COLOR_CYAN,
	COLOR_RED, COLOR_MAGENTA, COLOR_YELLOW, COLOR_WHITE
};

static void init_ncurses_colors(void) {
	for (int i = 0; i < 64; i++) {
		init_pair(i+1, nc_color_map[i&7], nc_color_map[i>>3]);
	}
}

int main(int argc, char** argv) {
	init_posix_vfs("");
	init_map_char_to_key();

	if (posix_zzt_init(argc, argv) < 0) {
		fprintf(stderr, "Could not load ZZT!\n");
		return 1;
	}

	setlocale(LC_ALL, "");

	window = initscr();
	cbreak();
	noecho();
	nonl();
	wclear(window);
	nodelay(window, 1);
	scrollok(window, 1);
	idlok(window, 1);
	keypad(window, 1);

	start_color();
	init_ncurses_colors();
/*	clock_t last = clock();
	clock_t curr = last; */

	long timer_ms = zeta_time_ms();
	long render_ms = timer_ms;

	int rcode = 0;

	while ((rcode = zzt_execute(64000)) > 0) {
		long curr_ms = zeta_time_ms();

		if ((curr_ms - render_ms) >= 10) {
			// refresh screen
			u8* ram = zzt_get_ram();
			for (int y = 0; y < 25; y++) {
				for (int x = 0; x < 80; x++) {
					u8 chr = ram[TEXT_ADDR(x,y)];
					u8 col = ram[TEXT_ADDR(x,y)+1];

					int attr = COLOR_PAIR(1+((col & 7) | ((col & 0x70) >> 1) ));
					if ((col & 0x08) != 0) attr |= A_BOLD;
					wattrset(window, attr);
					mvwaddstr(window, y, x, map_char_to_unicode[chr]);
				}
			}
			render_ms = curr_ms;
		}

		wrefresh(window);
		zzt_mark_frame();

/*		curr = clock();
		float secs = (float) (curr - last) / CLOCKS_PER_SEC;
		fprintf(stderr, "%.2f opc/sec\n", 1600000.0f / secs);
		last = curr; */

		if (rcode == 3) {
			long sleep_time = 55 - (curr_ms - timer_ms);
			if (sleep_time > 1) {
				usleep(sleep_time * 1000);
			}
		}

		if ((curr_ms - timer_ms) >= 55) {
			zzt_mark_timer();
			timer_ms = curr_ms;
		}

		platform_kbd_tick();
	}

	endwin();
}
