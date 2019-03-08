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

long vfs_time_ms() {
	clock_t c = clock();
	return c / (CLOCKS_PER_SEC/1000);
}

#define MAX_FNLEN 259

static FILE* file_pointers[MAX_FILES];
static char vfs_fnbuf[MAX_FNLEN+1];
static int vfs_fnprefsize;

int vfs_findfirst(u8* ptr, u16 mask, char* spec) { return -1; }
int vfs_findnext(u8* ptr) { return -1; }

void init_posix_vfs(const char* path) {
	strncpy(vfs_fnbuf, path, MAX_FNLEN);
	vfs_fnprefsize = strlen(vfs_fnbuf);
	for (int i = 0; i < MAX_FILES; i++) {
		file_pointers[i] = NULL;
	}
}

int vfs_open(const char* filename, int mode) {
	int len = strlen(filename);
	if (len > (MAX_FNLEN - vfs_fnprefsize)) {
		return -1;
	}

	int pos = 0;
	while (pos < MAX_FILES && file_pointers[pos] != NULL) pos++;
	if (pos == MAX_FILES) return -1;

	for (int i = 0; i < strlen(filename); i++) {
		char c = filename[i];
		if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
		vfs_fnbuf[i+vfs_fnprefsize] = c;
	}
	vfs_fnbuf[len+vfs_fnprefsize] = 0;

	FILE* file = fopen(vfs_fnbuf, (mode & 0x10000) ? "w+b" : (((mode & 0x03) == 0) ? "rb" : "r+b"));
	if (file == NULL) return -1;
	file_pointers[pos] = file;
	return pos+1;
}

int vfs_read(int handle, u8* ptr, int amount) {
	if (handle <= 0 || handle > MAX_FILES) return -1;
	FILE* fptr = file_pointers[handle-1];
	return fread(ptr, 1, amount, fptr);
}

int vfs_write(int handle, u8* ptr, int amount) {
	if (handle <= 0 || handle > MAX_FILES) return -1;
	FILE* fptr = file_pointers[handle-1];
	return fwrite(ptr, 1, amount, fptr);
}

int vfs_seek(int handle, int amount, int type) {
	if (handle <= 0 || handle > MAX_FILES) return -1;
	FILE* fptr = file_pointers[handle-1];
	switch (type) {
		default:
		case VFS_SEEK_SET: return fseek(fptr, amount, SEEK_SET);
		case VFS_SEEK_CUR: return fseek(fptr, amount, SEEK_CUR);
		case VFS_SEEK_END: return fseek(fptr, amount, SEEK_END);
	}
}

int vfs_close(int handle) {
	if (handle <= 0 || handle > MAX_FILES) return -1;
	FILE* fptr = file_pointers[handle-1];
	return fclose(fptr);
}
