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

#ifdef __APPLE__
#include <unistd.h>
#include <pwd.h>
#endif
#include <limits.h>

double posix_zzt_arg_note_delay = -1.0;

static void posix_vfs_chdir_arg0(char *argv0) {
	if (argv0 == NULL || argv0[0] == 0) return;

	char *path = strdup(argv0);
	if (path == NULL) return;
	char *final_path = strrchr(path, PATH_SEP);
	if (final_path == NULL) final_path = strrchr(path, '/');
	if (final_path == NULL) {
		free(path);
		return;
	}
	*final_path = 0;
	chdir(path);
	free(path);
}

static int posix_vfs_exists(const char *filename) {
	int h = vfs_open(filename, 0);
	if (h >= 0) { vfs_close(h); return 1; }
	else { return 0; }
}

static void posix_zzt_help(int argc, char **argv) {
	char *owner = (argv > 0 && argv[0] != NULL && strlen(argv[0]) > 0) ? argv[0] : "zeta";

	fprintf(stderr, "Usage: %s [arguments] [world file]\n", owner);
	fprintf(stderr, "\n");
	fprintf(stderr, "Arguments ([] - parameter; * - may specify multiple times):\n");
	fprintf(stderr, "  -b     disable blinking, enable bright backgrounds\n");
	fprintf(stderr, "  -d     developer mode: more debug for engine/fork developers\n");
	fprintf(stderr, "  -D []  set per-note delay, in milliseconds (supports fractions)\n");
	fprintf(stderr, " *-e []  execute command - repeat to run multiple commands\n");
	fprintf(stderr, "         by default, ZZT.EXE or SUPERZ.EXE is executed\n");
	fprintf(stderr, "  -h     show help\n");
	fprintf(stderr, " *-l []  load asset - in \"type:format:filename\" form or\n");
	fprintf(stderr, "         \"filename\" form to attempt a guess\n");
	fprintf(stderr, "         available types/formats: \n");
	fprintf(stderr, "         - charset:\n");
	fprintf(stderr, "             - chr     (MegaZeux-like; 8x[height], 256 chars)\n");
	fprintf(stderr, "             - chr2y   (as above, but doubled in height)\n");
	fprintf(stderr, "             - default (builtin font; ega or cga)\n");
	fprintf(stderr, "         - palette:\n");
	fprintf(stderr, "             - pal (MegaZeux-like; 16 colors ranged 00-3F)\n");
	fprintf(stderr, "             - pld (Toshiba UPAL; 64 EGA colors ranged 00-3F)\n");
	fprintf(stderr, "         if type prefixed with !, lock value\n");
	fprintf(stderr, "  -m []  set memory limit, in KB (64-640)\n");
	fprintf(stderr, "  -M []  set extended memory limit, in KB\n");
	fprintf(stderr, "  -t     enable world testing mode (skip K, C, ENTER)\n");
#ifndef FRONTEND_POSIX_NO_AUDIO
	fprintf(stderr, "  -V []  set starting volume (0-100)\n");
#endif
	fprintf(stderr, "\n");
	fprintf(stderr, "See <https://zeta.asie.pl/> for more information.\n");
}

#define IS_EXTENSION(s, e) (strcasecmp((s + strlen((s)) - strlen((e))), (e)) == 0)

#define INIT_ERR_GENERIC -1
#define INIT_ERR_NO_EXECUTABLE -2
#define INIT_ERR_NO_EXECUTABLE_ZZT -3

static int posix_try_run_zzt(int exec_count, char **execs, char *arg_name, bool is_final_attempt) {
	if (exec_count > 0) {
		int exeh = 0;
		for (int i = 0; i < exec_count; i++) {
			char *space_ptr = strchr(execs[i], ' ');
			if (space_ptr != NULL) {
				space_ptr[0] = '\0';
				exeh = vfs_open(execs[i], 0);
				if (exeh < 0) {
					if (is_final_attempt) {
						fprintf(stderr, "Could not load %s!\n", execs[i]);
					}
					space_ptr[0] = ' ';
					return INIT_ERR_NO_EXECUTABLE;
				}
				space_ptr[0] = ' ';
				fprintf(stderr, "'%s'\n", space_ptr + 1);
				zzt_load_binary(exeh, space_ptr + 1);
				vfs_close(exeh);
			} else {
				exeh = vfs_open(execs[i], 0);
				if (exeh < 0) {
					if (is_final_attempt) {
						fprintf(stderr, "Could not load %s!\n", execs[i]);
					}
					return INIT_ERR_NO_EXECUTABLE;
				}
				zzt_load_binary(exeh, (i == exec_count - 1) ? arg_name : NULL);
				vfs_close(exeh);
			}

			// last binary is engine
			if (i == exec_count - 1) break;
			while (zzt_execute(10000) != STATE_END) { }
		}
	} else {
		int exeh = vfs_open("zzt.exe", 0);
		if (exeh < 0)
			exeh = vfs_open("superz.exe", 0);
		if (exeh < 0)
			return INIT_ERR_NO_EXECUTABLE_ZZT;
		zzt_load_binary(exeh, arg_name);
		vfs_close(exeh);
	}

	return 0;
}

static int posix_zzt_init(int argc, char **argv) {
	char cwd[PATH_MAX+1];
	char arg_name[257];
	char *execs[16];
	char *loads[16];
	int exec_count = 0;
	int load_count = 0;
	int c;
	int skip_kc = 0;
	int memory_kbs = -1;
	int extended_memory_kbs = -1;
	int video_blink = 1;
	int starting_volume = 20;

	getcwd(cwd, PATH_MAX);

#ifdef __APPLE__
	if (access("../Info.plist", F_OK) == 0) {
		// We're in a relative path inside the .app.
		chdir("../../..");
	} else {
		if (!strcmp(cwd, "/") || !strncmp(cwd, "/Applications", 13)) {
			// We're in no reasonable path. Assume the user's root directory.
			struct passwd *pw = getpwuid(geteuid());
			if (pw != NULL)
				chdir(pw->pw_dir);
		}
	}
#endif

#ifdef USE_GETOPT
	while ((c = getopt(argc, argv, "dD:be:hl:m:M:tV:")) >= 0) {
		switch(c) {
			case 'd':
				developer_mode = true;
				break;
			case 'D':
				posix_zzt_arg_note_delay = atof(optarg);
				break;
			case 'b':
				video_blink = 0;
				break;
			case 'e':
				if (exec_count > 16) {
					fprintf(stderr, "Too many -e commands!\n");
					return INIT_ERR_GENERIC;
				}
				execs[exec_count++] = optarg;
				break;
			case 'l':
				if (load_count > 16) {
					fprintf(stderr, "Too many -l commands!\n");
					return INIT_ERR_GENERIC;
				}
				loads[load_count++] = optarg;
				break;
			case 'h':
				posix_zzt_help(argc, argv);
				exit(0);
				return INIT_ERR_GENERIC;
			case 'M':
				extended_memory_kbs = atoi(optarg);
				break;
			case 'm':
				memory_kbs = atoi(optarg);
				// intentional - going above 640K ought to be undocumented
				if (memory_kbs < 64 || memory_kbs > 1024) {
					fprintf(stderr, "Invalid memory amount specified!\n");
					return INIT_ERR_GENERIC;
				}
				break;
			case 't':
				skip_kc = 1;
				break;
#ifndef FRONTEND_POSIX_NO_AUDIO
			case 'V':
				starting_volume = atoi(optarg);
				break;
#endif
			case '?':
				fprintf(stderr, "Could not parse options! Try %s -h for help.\n", argv > 0 ? argv[0] : "running with");
				exit(0);
				return INIT_ERR_GENERIC;
		}
	}
#endif

	init_posix_vfs("", developer_mode);
	zzt_init(memory_kbs);
	zzt_set_max_extended_memory(extended_memory_kbs);
	zzt_load_blink(video_blink);
#ifndef FRONTEND_POSIX_NO_AUDIO
	if (starting_volume >= 0) {
		audio_stream_set_volume(starting_volume * 6 / 10);
	}
#endif

#ifdef USE_GETOPT
	if (argc > optind && posix_vfs_exists(argv[optind])) {
		strncpy(arg_name, argv[optind], 256);
#else
	if (argc > 1 && posix_vfs_exists(argv[1])) {
		strncpy(arg_name, argv[1], 256);
#endif
	} else if (argc > 0) {
		char *sl_ptr = strrchr(argv[0], '/');
		if (sl_ptr == NULL)
			sl_ptr = strrchr(argv[0], '\\');
		if (sl_ptr == NULL)
			sl_ptr = argv[0] - 1;

		strncpy(arg_name, sl_ptr + 1, 256);
		char *dot_ptr = strrchr(arg_name, '.');
		if (dot_ptr == NULL) dot_ptr = arg_name + strlen(arg_name);
		*dot_ptr = 0;
		if (strcasecmp(arg_name, "zeta86") != 0) {
			strncpy(dot_ptr, ".zzt", 256 - (dot_ptr - arg_name));

			if (!posix_vfs_exists(arg_name)) {
				strncpy(dot_ptr, ".szt", 256 - (dot_ptr - arg_name));
				if (!posix_vfs_exists(arg_name)) {
					arg_name[0] = 0;
				}
			}
		} else {
			arg_name[0] = 0;
		}
	}

	for (int i = 0; i < load_count; i++) {
		char *type, *filename;
		bool lock_on_load = false;

		if (loads[i][0] == '!') {
			lock_on_load = true;
			loads[i]++;
		}

		if (strrchr(loads[i], ':') == NULL) {
			filename = loads[i];
			if (IS_EXTENSION(filename, ".chr")) type = "charset:chr";
			else if (IS_EXTENSION(filename, ".pal")) type = "palette:pal";
			else if (IS_EXTENSION(filename, ".pld")) type = "palette:pld";
			else { 
				fprintf(stderr, "Could not guess type of %s!\n", filename);
				continue;
			}
		} else {
			type = loads[i];
			filename = strrchr(type, ':') + 1;
			type[filename - type - 1] = '\0';
		}

		if (strcmp(type, "charset:default") == 0) {
			if (strcmp(filename, "ega") == 0) {
				zzt_force_default_charset(DEFAULT_CHARSET_STYLE_EGA);
			} else if (strcmp(filename, "cga") == 0) {
				zzt_force_default_charset(DEFAULT_CHARSET_STYLE_CGA);
			}
			continue;
		}

		FILE *file = fopen(filename, "rb");
		if (!file) {
			fprintf(stderr, "Could not open %s!\n", filename);
			continue;
		}

		fseek(file, 0, SEEK_END);
		long fsize = ftell(file);
		fseek(file, 0, SEEK_SET);

		if (fsize <= 0 || fsize >= 1048576) {
			fprintf(stderr, "Could not read %s!\n", filename);
			fclose(file);
			continue;
		}

		u8 *buffer = (u8*) malloc((int) fsize);
		if (!fread(buffer, (int) fsize, 1, file)) {
			fprintf(stderr, "Could not read %s!\n", filename);
			fclose(file);
			continue;
		}

		fclose(file);

		if (zzt_load_asset(type, buffer, (int) fsize) < 0) {
			fprintf(stderr, "Could not load %s!\n", filename);
		}
		if (lock_on_load) {
			if (strstr(type, "charset") == type) {
				zzt_set_lock_charset(true);
			} else if (strstr(type, "palette") == type) {
				zzt_set_lock_palette(true);
			}
		}
		free(buffer);
	}

	int result = posix_try_run_zzt(exec_count, execs, arg_name, false);
	if (result != 0) {
		if (argv != NULL) {
			posix_vfs_chdir_arg0(argv[0]);
		}
		result = posix_try_run_zzt(exec_count, execs, arg_name, true);
		if (result != 0) {
			chdir(cwd);
			return result;
		}
	}

	zzt_set_timer_offset((time(NULL) % 86400) * 1000L);

	if (skip_kc) {
		zzt_key('k', 0x25);
		zzt_keyup(0x25);
		zzt_key('c', 0x2E);
		zzt_keyup(0x2E);
		zzt_key('\r', 0x1C);
		zzt_keyup(0x1C);
	}
	return 0;
}
