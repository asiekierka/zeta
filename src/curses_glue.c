/**
 * Copyright (c) 2018 Adrian Siekierka
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
#include <ncurses.h>
#include "zzt.h"

void init_posix_glue();

void speaker_on(double freq) {}
void speaker_off() {}

static int map_char_to_key[512];
static WINDOW* window;

int vfs_has_feature(int feature) {
	return 1;
}

static void platform_kbd_tick() {
	int c = getch();
	if (c == ERR) return;
	if (c == 263) c = 8;
	if (c == 0 || map_char_to_key[c] != 0) {
		int k = map_char_to_key[c];
		zzt_key(c, k);
	}
}

static void init_map_char_to_key() {
	map_char_to_key[258] = 0x50; // down
	map_char_to_key[260] = 0x4B; // left
	map_char_to_key[259] = 0x48; // up
	map_char_to_key[261] = 0x4D; // right
	map_char_to_key[13] = 0x1C; // enter
	map_char_to_key[0] = 0;
	map_char_to_key[0x1C] = 1; // esc
	for (int i = 1; i <= 9; i++) map_char_to_key[i + 48] = i + 1;
	char* chrs = "!@#$%^&*()";
	for (int i = 0; i < strlen(chrs); i++) map_char_to_key[(int)chrs[i]] = i + 1;
	map_char_to_key[48] = 11; // numbers
	map_char_to_key[45] = 12; // -
	map_char_to_key[95] = 12; // _
	map_char_to_key[61] = 13; // =
	map_char_to_key[43] = 12; // +
	map_char_to_key[8] = 14; // backspace
	map_char_to_key[9] = 15; // tab
	chrs = "qwertyuiop[]";
	for (int i = 0; i < strlen(chrs); i++) map_char_to_key[(int)chrs[i]] = 15 + i;
	chrs = "QWERTYUIOP{}";
	for (int i = 0; i < strlen(chrs); i++) map_char_to_key[(int)chrs[i]] = 15 + i;
	map_char_to_key[13] = 28; // enter
	// 29?
	chrs = "asdfghjkl;'";
	for (int i = 0; i < strlen(chrs); i++) map_char_to_key[(int)chrs[i]] = 29 + i;
	chrs = "ASDFGHJKL:\"";
	for (int i = 0; i < strlen(chrs); i++) map_char_to_key[(int)chrs[i]] = 29 + i;
	map_char_to_key[96] = 41; // `
	map_char_to_key[126] = 41; // ~
	// 42?
	map_char_to_key[92] = 43;
	map_char_to_key[124] = 43; // |
	chrs = "zxcvbnm,./";
	for (int i = 0; i < strlen(chrs); i++) map_char_to_key[(int)chrs[i]] = 43 + i;
	chrs = "ZXCVBNM<>?";
	for (int i = 0; i < strlen(chrs); i++) map_char_to_key[(int)chrs[i]] = 43 + i;
	// 54?
	map_char_to_key[42] = 55; // *
	// 56?
	map_char_to_key[32] = 57; // space
}

int main() {
	init_posix_glue();
	init_map_char_to_key();
	zzt_init();

	window = initscr();
	cbreak();
	noecho();
	nonl();
	wclear(window);
	nodelay(window, 1);
	scrollok(window, 1);
	idlok(window, 1);
	keypad(window, 1);

	clock_t last = clock();
	clock_t curr = last;

	while (zzt_execute(1600000)) {
		// refresh screen
		u8* ram = zzt_get_ram();
		for (int y = 0; y < 25; y++) {
			for (int x = 0; x < 80; x++) {
				u8 chr = ram[TEXT_ADDR(x,y)];
				if (chr == 2) chr = '@';
				else if (chr == 0) chr = 32;
				else if (chr < 32 || chr >= 128)
					chr = '0' + (chr % 10);
				mvwaddch(window, y, x, chr);
			}
		}
		wrefresh(window);
		zzt_mark_frame();

		curr = clock();
		float secs = (float) (curr - last) / CLOCKS_PER_SEC;
		fprintf(stderr, "%.2f opc/sec\n", 1600000.0f / secs);
		platform_kbd_tick();
		last = curr;

		zzt_mark_timer();
	}
}
