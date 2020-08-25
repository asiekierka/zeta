/**
 * Copyright (c) 2018, 2019, 2020 Adrian Siekierka
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
static int vfs_initialized = 0;

#if defined(NO_OPENDIR)
static void vfs_fix_case(char *fn) { }
int vfs_findfirst(u8* ptr, u16 mask, char* spec) { return -1; }
int vfs_findnext(u8* ptr) { return -1; }
#else
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

static int vfs_find_filter(const struct dirent *entry, const char *spec) {
	const char *name = entry->d_name;
	int name_len = strlen(name);
	if (name_len > 12) {
		// skip too large names
		return 0;
	}
	int spec_len = strlen(spec);
	return strlen(name) >= spec_len && strcasecmp(name + strlen(name) - spec_len, spec) == 0;
}

#if defined(POSIX_VFS_SORTED_DIRS)

static int vfs_dirent_size;
static int vfs_dirent_count;
static int vfs_dirent_pos;
static char** vfs_dirents = NULL;

static int vfs_find_strcmp(const void *a, const void *b) {
	return strcasecmp(*((const char **)a), *((const char **)b));
}

int vfs_findfirst(u8* ptr, u16 mask, char* spec) {
	DIR *dir;
	struct dirent *entry;
	int i;
	char** vfs_dirents_new;

	if (strncmp(spec, "*.", 2) == 0) {
		if (strlen(spec + 1) > MAX_SPECLEN) {
			return -1;
		}
		if (vfs_dirent_size == 0) {
			// initial allocation
			vfs_dirent_size = 64;
			vfs_dirents = malloc(vfs_dirent_size * sizeof(char*));
		} else {
			// freeing
			for (i = 0; i < vfs_dirent_size; i++) {
				if (vfs_dirents[i] != NULL) {
					free(vfs_dirents[i]);
					vfs_dirents[i] = NULL;
				}
			}
		}
		vfs_dirent_count = 0;
		dir = opendir(vfs_fndir);
		while ((entry = readdir(dir)) != NULL) {
			if (vfs_find_filter(entry, spec + 1) != 0) {
				vfs_dirents[vfs_dirent_count++] = strdup(entry->d_name);
				if (vfs_dirent_count >= vfs_dirent_size) {
					vfs_dirent_size *= 2;
					vfs_dirents_new = realloc(vfs_dirents, vfs_dirent_size * sizeof(char*));
					if (vfs_dirents_new != NULL) {
						vfs_dirents = vfs_dirents_new;
					} else {
						goto FindFirstDirentEarlyExit;
					}
				}
			}
		}
FindFirstDirentEarlyExit:
		closedir(dir);
		vfs_dirent_pos = 0;

		qsort(vfs_dirents, vfs_dirent_count, sizeof(char*), vfs_find_strcmp);

		return vfs_findnext(ptr);
	} else {
		return -1;
	}
}

int vfs_findnext(u8* ptr) {
	if (vfs_dirent_pos < vfs_dirent_count) {
		memset(ptr + 0x15, 0, 0x1E - 0x15);
		strcpy((char*) (ptr + 0x1E), vfs_dirents[vfs_dirent_pos++]);
		return 0;
	} else {
		return -1;
	}
}

#else

// on-demand implementation

static DIR *vfs_finddir;
static char vfs_findspec[MAX_SPECLEN+1];

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
		if (vfs_find_filter(entry, vfs_findspec) != 0) {
			memset(ptr + 0x15, 0, 0x1E - 0x15);
			strcpy((char*) (ptr + 0x1E), entry->d_name);
			return 0;
		}
	}

	closedir(vfs_finddir);
	vfs_finddir = NULL;
	return -1;
}
#endif
#endif /* !NO_OPENDIR */

void init_posix_vfs(const char* path) {
	if (vfs_initialized > 0) {
		for (int i = 0; i < MAX_FILES; i++) {
			if (file_pointers[i] != NULL) {
				fclose(file_pointers[i]);
				file_pointers[i] = NULL;
			}
		}
	}

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
	vfs_initialized = 1;
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
