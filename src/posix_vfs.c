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

#ifndef NO_OPENDIR
#define _DEFAULT_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "zzt.h"

#ifndef NO_OPENDIR
#include <dirent.h>
#include <strings.h>
#endif

#define MAX_FNLEN 259
#define MAX_SPECLEN 16

static FILE* file_pointers[MAX_FILES];
static char vfs_fnbuf[MAX_FNLEN+1];
static char vfs_fndir[MAX_FNLEN+1];
static int vfs_fnprefsize;

#ifdef NO_OPENDIR
static void vfs_fix_case(char *fn) { }
int vfs_findfirst(u8* ptr, u16 mask, char* spec) { return -1; }
int vfs_findnext(u8* ptr) { return -1; }
#else
static DIR *vfs_finddir;
static char vfs_findspec[MAX_SPECLEN+1];

static int vfs_find_filter(const struct dirent *entry) {
	const char *name = entry->d_name;
	int spec_len = strlen(vfs_findspec);
	return strlen(name) >= spec_len && strcasecmp(name + strlen(name) - spec_len, vfs_findspec) == 0;
}

int vfs_findfirst(u8* ptr, u16 mask, char* spec) {
	vfs_findspec[0] = 0; // clear findspec

	if (strncmp(spec, "*.", 2) == 0) {
		if (strlen(spec+1) > MAX_SPECLEN) {
			return -1;
		}
		strncpy(vfs_findspec, spec + 1, MAX_SPECLEN); // skip the *
		vfs_finddir = opendir(vfs_fndir);
		return vfs_findnext(ptr);
	} else {
		return -1;
	}
}

int vfs_findnext(u8* ptr) {
	struct dirent *entry;
	if (vfs_finddir == NULL) {
		return -1;
	}

	while ((entry = readdir(vfs_finddir)) != NULL) {
		if (vfs_find_filter(entry) != 0) {
			memset(ptr + 0x15, 0, 0x1E - 0x15);
			strcpy((char*) (ptr + 0x1E), entry->d_name);
			return 0;
		}
	}

	closedir(vfs_finddir);
	vfs_finddir = NULL;
	return -1;
}

static void vfs_fix_case(char *fn) {
	DIR *dir;
	struct dirent *entry;

	dir = opendir(vfs_fndir);
	while ((entry = readdir(dir)) != NULL) {
		if (strcasecmp(fn, entry->d_name) == 0) {
			strncpy(fn, entry->d_name, strlen(fn));
			break;
		}
	}

	closedir(dir);
}
#endif

void init_posix_vfs(const char* path) {
	strncpy(vfs_fnbuf, path, MAX_FNLEN);
	vfs_fnprefsize = strlen(vfs_fnbuf);
	for (int i = 0; i < MAX_FILES; i++) {
		file_pointers[i] = NULL;
	}

	if (strlen(path) == 0) {
		strcpy(vfs_fndir, ".");
	} else {
		strncpy(vfs_fndir, path, MAX_FNLEN);
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

	strncpy(vfs_fnbuf + vfs_fnprefsize, filename, MAX_FNLEN);
	if (vfs_fnprefsize == 0) {
		vfs_fix_case(vfs_fnbuf + vfs_fnprefsize);
	}

	FILE* file = fopen(vfs_fnbuf, (mode & 0x10000) ? "w+b" : (((mode & 0x03) == 0) ? "rb" : "r+b"));
	if (file == NULL) {
//		fprintf(stderr, "failed to open %s\n", vfs_fnbuf);
		return -1;
	}
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
	file_pointers[handle-1] = NULL;
	return fclose(fptr);
}
