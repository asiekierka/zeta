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

#include "posix_vfs.h"
#ifdef HAVE_OPENDIR
#define _DEFAULT_SOURCE
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "config.h"
#include "zzt.h"

#ifdef HAVE_FTRUNCATE
#include <unistd.h>
#endif

#ifdef HAVE_OPENDIR
#include <dirent.h>
#include <strings.h>
#include <sys/stat.h>
#endif

#define MAX_FNLEN 259
#define MAX_SPECLEN 16
#define MAX_VFS_FNLEN 12
#define MAX_VFS_DIRLEN 63

#define MAX_FILES_START 16
#define MAX_FILES_LIMIT 256

// #define DEBUG_VFS

static FILE **file_pointers;
static char **file_pointer_names;
static uint16_t file_pointers_size = 0;
static bool vfs_debug_enabled;

static char vfs_curdir[MAX_FNLEN+1];
static char vfs_basedir[MAX_FNLEN+1];
static char vfs_subdir[MAX_VFS_DIRLEN+1];

int vfs_posix_get_file_pointer_count(void) {
	return file_pointers_size;
}

char *vfs_posix_get_file_pointer_name(int i) {
	if (!vfs_debug_enabled || i < 0 || i >= file_pointers_size) return NULL;
	return file_pointer_names[i];
}

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

	if (strlen(dir) >= 3 && dir[1] == ':' && dir[2] == '\\') {
		// reset path
		strncpy(vfs_subdir, dir + 3, MAX_VFS_DIRLEN);
		vfs_subdir[MAX_VFS_DIRLEN] = 0;
		vfs_update_dirs();
		return 0;
	}

	if (strchr(dir, '\\') != NULL) {
		char dir_copy[MAX_FNLEN];
		dir_copy[sizeof(dir_copy) - 1] = 0;
		strncpy(dir_copy, dir, sizeof(dir_copy) - 1);
		const char *dir_tok = strtok(dir_copy, "\\");
		while (dir_tok != NULL) {
			if (vfs_chdir(dir_tok) < 0) return -1;
			dir_tok = strtok(NULL, "\\");
		}
		return 0;
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

#if !defined(HAVE_OPENDIR)
static void vfs_fix_case(char *fn) { }
int vfs_findfirst(u8* ptr, u16 mask, char* spec) { return -1; }
int vfs_findnext(u8* ptr) { return -1; }
#else
// TODO: This could be optimized better.
static void vfs_fix_case(char *pathname, bool do_case_fix) {
	char path_dir[MAX_FNLEN];
	DIR *dir;
	struct dirent *entry;

	char *last_path_sep = NULL;
	char *filename = NULL;
	for (int i = 0; i < strlen(pathname); i++) {
		if (do_case_fix && pathname[i] == '\\') {
			pathname[i] = 0;
			vfs_fix_case(pathname, false);
			pathname[i] = PATH_SEP;
		}
		if (pathname[i] == PATH_SEP) {
			last_path_sep = pathname + i;
		}
	}
	if (last_path_sep != NULL) {
		*last_path_sep = 0;
		strncpy(path_dir, vfs_curdir, MAX_FNLEN);
		vfs_path_cat(path_dir, pathname, MAX_FNLEN);
		*last_path_sep = PATH_SEP;
		dir = opendir(path_dir);
		filename = last_path_sep + 1;
	} else {
		dir = opendir(vfs_curdir);
		filename = pathname;
	}
	if (dir != NULL) {
		while ((entry = readdir(dir)) != NULL) {
			if (strcasecmp(filename, entry->d_name) == 0) {
				strncpy(filename, entry->d_name, strlen(filename));
				break;
			}
		}
		closedir(dir);
	}
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
	char path[MAX_FNLEN+1];

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
	strcpy(path, vfs_curdir);
	vfs_path_cat(path, name, MAX_FNLEN);
	stat(path, &entry_stat);
	result.size = entry_stat.st_size;
	if (S_ISDIR(entry_stat.st_mode)) {
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
#ifdef DEBUG_VFS
	fprintf(stderr, "posix vfs: found %s %02x\n", entry->name, entry->attr);
#endif
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

#ifdef DEBUG_VFS
	fprintf(stderr, "posix vfs: findspec %s in %s\n", spec, vfs_curdir);
#endif

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
		if (dir != NULL) {
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
		}
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
#ifdef DEBUG_VFS
	fprintf(stderr, "posix vfs: findfirst %s in %s\n", spec, vfs_curdir);
#endif

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
#endif /* HAVE_OPENDIR */

static int vfs_alloc_file_pointer(void) {
	int i = 0;
	while (i < file_pointers_size && file_pointers[i] != NULL) i++;
	if (i == file_pointers_size) {
		if (file_pointers_size < MAX_FILES_LIMIT) {
			FILE **new_file_pointers = realloc(file_pointers, sizeof(FILE*) * file_pointers_size * 2);
			if (new_file_pointers == NULL) {
				return -1;
			}
			if (vfs_debug_enabled) {
				char **new_file_pointer_names = realloc(file_pointer_names, sizeof(char*) * file_pointers_size * 2);
				if (new_file_pointer_names == NULL) {
					return -1;
				}
				file_pointer_names = new_file_pointer_names;
			}
			file_pointers = new_file_pointers;
			file_pointers_size *= 2;
		}
		if (i == file_pointers_size) {
			return -1;
		}
	}
	return i;
}

static int vfs_free_file_pointer(int i, bool user) {
	if (file_pointers[i] != NULL) {
		int result = fclose(file_pointers[i]);
		file_pointers[i] = NULL;
		if (vfs_debug_enabled) {
			free(file_pointer_names[i]);
			file_pointer_names[i] = NULL;
		}
		return result;
	} else {
		if (user && developer_mode) {
			zeta_show_developer_warning("Tried to close already closed file handle %d!", i);
		}
		return 0;
	}
}

void exit_posix_vfs(void) {
	if (file_pointers_size > 0) {
		for (int i = 0; i < file_pointers_size; i++) {
			vfs_free_file_pointer(i, false);
		}
		if (vfs_debug_enabled) {
			free(file_pointer_names);
			vfs_debug_enabled = false;
		}
		free(file_pointers);
		file_pointers_size = 0;
	}
}

void init_posix_vfs(const char* path, bool debug_enabled) {
	if (file_pointers_size > 0) {
		for (int i = 0; i < file_pointers_size; i++) {
			vfs_free_file_pointer(i, false);
		}
		if (!vfs_debug_enabled && debug_enabled) {
			file_pointer_names = malloc(sizeof(char*) * file_pointers_size);
		} else if (vfs_debug_enabled && !debug_enabled) {
			free(file_pointer_names);
		}
	} else {
		file_pointers_size = MAX_FILES_START;
		file_pointers = malloc(sizeof(FILE*) * file_pointers_size);
		if (debug_enabled) {
			file_pointer_names = malloc(sizeof(char*) * file_pointers_size);
		}
	}

	vfs_debug_enabled = debug_enabled;
	for (int i = 0; i < file_pointers_size; i++) {
		file_pointers[i] = NULL;
		if (vfs_debug_enabled) {
			file_pointer_names[i] = NULL;
		}
	}

	if (strlen(path) == 0) {
		strcpy(vfs_basedir, ".");
	} else {
		strncpy(vfs_basedir, path, MAX_FNLEN);
	}
	vfs_subdir[0] = 0;
	vfs_update_dirs();
}

int vfs_open(const char* filename, int mode) {
	const char *mode_str;
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

	if (strcmp(filename, "LPT1") == 0) {
		// ignore LPT1 - OneDrive doesn't like it
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

	i = vfs_alloc_file_pointer();
	if (i < 0) return -1;

	while (*path_filename == PATH_SEP) {
		path_filename++;
	}
	vfs_fix_case(path_filename, true);

	mode_str = (mode & 0x10000) ? "w+b" : (((mode & 0x03) == 0) ? "rb" : "r+b");
	file = fopen(path, mode_str);
	if (file == NULL) {
#ifdef DEBUG_VFS
		fprintf(stderr, "posix vfs: failed to open %s (%s)\n", path, mode_str);
#endif
		return -1;
	} else {
#ifdef DEBUG_VFS
		fprintf(stderr, "posix vfs: opened %s (%s) at %d\n", path, mode_str, i+1);
#endif
		file_pointers[i] = file;
#ifdef HAS_DEVELOPER_MODE
		if (vfs_debug_enabled) {
			file_pointer_names[i] = malloc(strlen(path_filename) + 20);
			if (file_pointer_names[i] != NULL) {
				sprintf(file_pointer_names[i], "%s @ %04X:%04X", path_filename, zzt_get_ip() >> 16, zzt_get_ip() & 0xFFFF);
			}
		}
#endif
		return i+1;
	}
}

static void vfs_check_error(FILE* fptr) {
	int err = ferror(fptr);
	if (err != 0) {
		fprintf(stderr, "posix vfs: file error %d\n", err);
	}
}

int vfs_read(int handle, u8* ptr, int amount) {
	if (handle <= 0 || handle > file_pointers_size) return -1;
	FILE* fptr = file_pointers[handle-1];
	int count = fread(ptr, 1, amount, fptr);
#ifdef DEBUG_VFS
//	fprintf(stderr, "posix vfs: read %d/%d bytes from %d\n", count, amount, handle);
	vfs_check_error(fptr);
#endif
	return count;
}

int vfs_write(int handle, u8* ptr, int amount) {
	if (handle <= 0 || handle > file_pointers_size) return -1;
	FILE* fptr = file_pointers[handle-1];
	int count = fwrite(ptr, 1, amount, fptr);
#ifdef DEBUG_VFS
//	fprintf(stderr, "posix vfs: wrote %d/%d bytes to %d\n", count, amount, handle);
	vfs_check_error(fptr);
#endif
	return count;
}

int vfs_truncate(int handle) {
#ifdef HAVE_FTRUNCATE
	if (handle <= 0 || handle > file_pointers_size) return -1;
	FILE* fptr = file_pointers[handle-1];
	int res = ftruncate(fileno(fptr), ftell(fptr));
#ifdef DEBUG_VFS
	fprintf(stderr, "posix vfs: truncated file\n");
	vfs_check_error(fptr);
#endif
	return res;
#else
	return -1;
#endif
}

int vfs_seek(int handle, int amount, int type) {
	if (handle <= 0 || handle > file_pointers_size) return -1;
	FILE* fptr = file_pointers[handle-1];
	switch (type) {
		default:
		case VFS_SEEK_SET: if (fseek(fptr, amount, SEEK_SET) != 0) { return -1; } break;
		case VFS_SEEK_CUR: if (fseek(fptr, amount, SEEK_CUR) != 0) { return -1; } break;
		case VFS_SEEK_END: if (fseek(fptr, amount, SEEK_END) != 0) { return -1; } break;
	}
	return ftell(fptr);
}

int vfs_close(int handle) {
#ifdef DEBUG_VFS
	fprintf(stderr, "posix vfs: closing %d\n", handle);
#endif
	if (handle <= 0 || handle > file_pointers_size) return -1;
	return vfs_free_file_pointer(handle-1, true);
}
