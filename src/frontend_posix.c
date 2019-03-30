/**
 * Copyright (c) 2018, 2019 Adrian Siekierka
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

static int posix_vfs_exists(const char *filename) {
	int h = vfs_open(filename, 0);
	if (h >= 0) { vfs_close(h); return 1; }
	else { return 0; }
}

static void posix_zzt_help(int argc, char **argv) {
	fprintf(stderr, "Usage: %s [-e command] [-bt] [world file]\n", argv > 0 ? argv[0] : "zeta");
	fprintf(stderr, "\n");
	fprintf(stderr, "  -b     disable blinking, enable bright backgrounds\n");
	fprintf(stderr, "  -e []  execute command - repeat to run multiple commands\n");
	fprintf(stderr, "         by default, runs either ZZT.EXE or SUPERZ.EXE\n");
	fprintf(stderr, "  -t     enable world testing mode (skip K, C, ENTER)\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "See <https://zeta.asie.pl/> for more information.\n");
}

static int posix_zzt_init(int argc, char **argv) {
	char arg_name[257];
	char *execs[16];
	int exec_count = 0;
	int c;
	int skip_kc = 0;

#ifndef NO_GETOPT
	while ((c = getopt(argc, argv, "be:ht")) >= 0) {
		switch(c) {
			case 'b':
				video_blink = 0;
				break;
			case 'e':
				if (exec_count > 16) {
					fprintf(stderr, "Too many -e commands!\n");
					return -1;
				}
				execs[exec_count++] = optarg;
				break;
			case 'h':
				posix_zzt_help(argc, argv);
				exit(0);
				return -1;
			case 't':
				skip_kc = 1;
				break;
			case '?':
				fprintf(stderr, "Could not parse options! Try %s -h for help.\n", argv > 0 ? argv[0] : "running with");
				exit(0);
				return -1;
		}
	}
#endif

	zzt_init();

#ifdef NO_GETOPT
	if (argc > 1 && posix_vfs_exists(argv[1])) {
		strncpy(arg_name, argv[1], 256);
#else
	if (argc > optind && posix_vfs_exists(argv[optind])) {
		strncpy(arg_name, argv[optind], 256);
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
		strncpy(dot_ptr, ".zzt", 256 - (dot_ptr - arg_name));

		if (!posix_vfs_exists(arg_name)) {
			arg_name[0] = 0;
		}
	}

	if (exec_count > 0) {
		int exeh = 0;
		for (int i = 0; i < exec_count; i++) {
			char *space_ptr = strchr(execs[i], ' ');
			if (space_ptr != NULL) {
				space_ptr[0] = '\0';
				exeh = vfs_open(execs[i], 0);
				if (exeh < 0) {
					fprintf(stderr, "Could not load %s!\n", execs[i]);
					space_ptr[0] = ' ';
					return -1;
				}
				space_ptr[0] = ' ';
				fprintf(stderr, "'%s'\n", space_ptr + 1);
				zzt_load_binary(exeh, space_ptr + 1);
				vfs_close(exeh);
			} else {
				exeh = vfs_open(execs[i], 0);
				if (exeh < 0) {
					fprintf(stderr, "Could not load %s!\n", execs[i]);
					return -1;
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
			return -1;
		zzt_load_binary(exeh, arg_name);
		vfs_close(exeh);
	}

	if (skip_kc) {
		zzt_key('k', 0x25);
		zzt_key('c', 0x2E);
		zzt_key('\r', 0x1C);
	}
	return 0;
}
