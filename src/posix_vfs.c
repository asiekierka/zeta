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
#include <sys/stat.h>
#endif

#ifdef _WIN32
#define PATH_SEP '\\'
#else
#define PATH_SEP '/'
#endif

#define MAX_FNLEN 259
#define MAX_SPECLEN 16
#define MAX_VFS_FNLEN 12
#define MAX_VFS_DIRLEN 63

static FILE* file_pointers[MAX_FILES];
static int vfs_initialized = 0;

static char vfs_curdir[MAX_FNLEN+1];
static char vfs_basedir[MAX_FNLEN+1];
static char vfs_subdir[MAX_VFS_DIRLEN+1];

static void vfs_path_cat(char *dest, const char *src, size_t n) {
	size_t len = strlen(dest);
	if (len > 0 && len < n && dest[len - 1] != PATH_SEP) {
		dest[len++] = PATH_SEP;
		dest[len] = '\0';
	}
	strncpy(dest + len, src, n - len);
}

static void vfs_update_dirs(void) {
	vfs_curdir[MAX_FNLEN] = 0;
	strncpy(vfs_curdir, vfs_basedir, MAX_FNLEN);
	vfs_path_cat(vfs_curdir, vfs_subdir, MAX_FNLEN);
}

int vfs_getcwd(char *buf, int size) {
	int i;
	strncpy(buf, vfs_subdir, size - 1);
	buf[size - 1] = 0;
	for (i = 0; i < strlen(buf); i++) {
		if (buf[i] == PATH_SEP)
			buf[i] = '\\';
	}
	return 0;
}

int vfs_chdir(const char *dir) {
	char *dir_split;
	int i;

	if (strlen(dir) == 3 && dir[1] == ':' && dir[2] == '\\') {
		// reset path
		vfs_subdir[0] = 0;
		vfs_update_dirs();
		return 0;
	}

	if (strchr(dir, '\\') != NULL) {
		// do not support multiple path resolution for now
		return -1;
	}

	if (strcmp(dir, ".") == 0) {
		// remain in place
		return 0;
	} else if (strcmp(dir, "..") == 0) {
		// descend
		dir_split = strrchr(vfs_subdir, PATH_SEP);
		if (dir_split == NULL) dir_split = vfs_subdir;
		*dir_split = 0; // cut split location
	} else {
		// append
		vfs_subdir[MAX_VFS_DIRLEN] = 0;
		vfs_path_cat(vfs_subdir, dir, MAX_VFS_DIRLEN);
		for (i = 0; i < strlen(vfs_subdir); i++) {
			if (vfs_subdir[i] == '\\')
				vfs_subdir[i] = PATH_SEP;
		}
	}
	vfs_update_dirs();
	return 0;
}

#if defined(NO_OPENDIR)
static void vfs_fix_case(char *fn) { }
int vfs_findfirst(u8* ptr, u16 mask, char* spec) { return -1; }
int vfs_findnext(u8* ptr) { return -1; }
#else
static void vfs_fix_case(char *fn) {
	DIR *dir;
	struct dirent *entry;

	dir = opendir(vfs_curdir);
	while ((entry = readdir(dir)) != NULL) {
		if (strcasecmp(fn, entry->d_name) == 0) {
			strncpy(fn, entry->d_name, strlen(fn));
			break;
		}
	}
	closedir(dir);
}

#define VFS_ATTR_DIR 0x10

typedef struct {
	u16 attr;
	u16 time;
	u16 date;
	u32 size;
	char name[MAX_VFS_FNLEN + 1];
} vfs_dirent;

static vfs_dirent vfs_process_entry(const struct dirent *entry, u16 mask, const char *spec) {
	const char *name = entry->d_name;
	struct stat entry_stat;
	vfs_dirent result;
	result.attr = 0;
	result.time = 0;
	result.date = 0;
	result.size = 0;
	result.name[0] = 0;
	result.name[MAX_VFS_FNLEN] = 0;

	// skip too large names
	int name_len = strlen(name);
	if (name_len > MAX_VFS_FNLEN) {
		return result;
	}

	// skip names which don't match spec
	int spec_len = spec != NULL ? strlen(spec) : 0;
	if (spec_len > 0) {
		if (strlen(name) < spec_len) return result;
		if (strcasecmp(name + strlen(name) - spec_len, spec) != 0) return result;
	}

	// generate attribute mask & compare
	stat(name, &entry_stat);
	result.size = entry_stat.st_size;
	if (entry_stat.st_mode & S_IFDIR) {
		result.attr |= VFS_ATTR_DIR;
	}
	if (((~mask) & (~result.attr)) != (~mask)) {
		return result;
	}

	// all checks passed
	strcpy(result.name, name);
	return result;
}

static int vfs_apply_entry(u8* ptr, vfs_dirent *entry) {
	ptr[0x15] = (u8) (entry->attr);
	ptr[0x16] = (u8) (entry->time);
	ptr[0x17] = (u8) (entry->time >> 8);
	ptr[0x18] = (u8) (entry->date);
	ptr[0x19] = (u8) (entry->date >> 8);
	ptr[0x1A] = (u8) (entry->size);
	ptr[0x1B] = (u8) (entry->size >> 8);
	ptr[0x1C] = (u8) (entry->size >> 16);
	ptr[0x1D] = (u8) (entry->size >> 24);
	strcpy((char*) (ptr + 0x1E), entry->name);
	return 0;
}

#if defined(POSIX_VFS_SORTED_DIRS)

static int vfs_dirent_size;
static int vfs_dirent_count;
static int vfs_dirent_pos;
static vfs_dirent* vfs_dirents = NULL;

static int vfs_find_strcmp(const void *a, const void *b) {
	return strcasecmp(((const vfs_dirent *)a)->name, ((const vfs_dirent *)b)->name);
}

int vfs_findfirst(u8* ptr, u16 mask, char* spec) {
	DIR *dir;
	struct dirent *entry;
	vfs_dirent* vfs_dirents_new;

	if (spec[0] == '*') {
		if (strlen(spec + 1) > MAX_SPECLEN) {
			return -1;
		}
		if (vfs_dirent_size == 0) {
			// initial allocation
			vfs_dirent_size = 64;
			vfs_dirents = malloc(vfs_dirent_size * sizeof(vfs_dirent));
		}
		vfs_dirent_count = 0;
		dir = opendir(vfs_curdir);
		while ((entry = readdir(dir)) != NULL) {
			vfs_dirents[vfs_dirent_count] = vfs_process_entry(entry, mask, spec + 1);
			if (vfs_dirents[vfs_dirent_count].name[0] != 0) {
				vfs_dirent_count++;
				if (vfs_dirent_count >= vfs_dirent_size) {
					vfs_dirent_size *= 2;
					vfs_dirents_new = realloc(vfs_dirents, vfs_dirent_size * sizeof(vfs_dirent));
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

		qsort(vfs_dirents, vfs_dirent_count, sizeof(vfs_dirent), vfs_find_strcmp);

		return vfs_findnext(ptr);
	} else {
		return -1;
	}
}

int vfs_findnext(u8* ptr) {
	if (vfs_dirent_pos < vfs_dirent_count) {
		return vfs_apply_entry(ptr, &vfs_dirents[vfs_dirent_pos++]);
	} else {
		return -1;
	}
}

#else

// on-demand implementation

static DIR *vfs_finddir;
static char vfs_findspec[MAX_SPECLEN+1];
static u16 vfs_findmask;

int vfs_findfirst(u8* ptr, u16 mask, char* spec) {
	vfs_findspec[0] = 0; // clear findspec

	if (spec[0] == '*') {
		if (strlen(spec+1) > MAX_SPECLEN) {
			return -1;
		}
		strncpy(vfs_findspec, spec + 1, MAX_SPECLEN); // skip the *
		vfs_finddir = opendir(vfs_curdir);
		vfs_findmask = mask;
		return vfs_findnext(ptr);
	} else {
		return -1;
	}
}

int vfs_findnext(u8* ptr) {
	vfs_dirent vfs_entry;
	struct dirent *entry;
	if (vfs_finddir == NULL) {
		return -1;
	}

	while ((entry = readdir(vfs_finddir)) != NULL) {
		vfs_entry = vfs_process_entry(entry, vfs_findmask, vfs_findspec);
		if (vfs_entry.name[0] != 0) {
			return vfs_apply_entry(ptr, &vfs_entry);
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

	for (int i = 0; i < MAX_FILES; i++) {
		file_pointers[i] = NULL;
	}

	if (strlen(path) == 0) {
		strcpy(vfs_basedir, ".");
	} else {
		strncpy(vfs_basedir, path, MAX_FNLEN);
	}
	vfs_subdir[0] = 0;
	vfs_update_dirs();
	vfs_initialized = 1;
}

int vfs_open(const char* filename, int mode) {
	char path[MAX_FNLEN];
	char *path_base;
	char *path_filename;
	int i;
	FILE *file;

	path[MAX_FNLEN - 1] = 0;

	if (strstr(filename, "..\\") != NULL) {
		// ignore relative paths for now
		return -1;
	}

	if (strlen(filename) >= 3 && filename[1] == ':' && filename[2] == '\\') {
		// absolute path
		strncpy(path, vfs_basedir, MAX_FNLEN);
		path_base = path + strlen(path);
		vfs_path_cat(path, filename + 3, MAX_FNLEN);
		if (path[MAX_FNLEN - 1] != 0) {
			return -1;
		}

		for (i = 0; i < strlen(path_base); i++) {
			if (path_base[i] == '\\')
				path_base[i] = PATH_SEP;
		}
		path_filename = strrchr(path_base, PATH_SEP);
		if (path_filename == NULL)
			path_filename = path_base;
	} else {
		// relative path
		strncpy(path, vfs_curdir, MAX_FNLEN);
		path_filename = path + strlen(path);
		vfs_path_cat(path, filename, MAX_FNLEN);
	}
	if (path[MAX_FNLEN - 1] != 0) {
		return -1;
	}

	i = 0;
	while (i < MAX_FILES && file_pointers[i] != NULL) i++;
	if (i == MAX_FILES) return -1;

	while (*path_filename == PATH_SEP) {
		path_filename++;
	}
	vfs_fix_case(path_filename);

	file = fopen(path, (mode & 0x10000) ? "w+b" : (((mode & 0x03) == 0) ? "rb" : "r+b"));
	if (file == NULL) {
//		fprintf(stderr, "failed to open %s\n", path);
		return -1;
	}
	file_pointers[i] = file;
	return i+1;
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
