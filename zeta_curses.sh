#!/bin/sh
gcc -o build/zeta86 -g -O3 -std=c11 -Wall -lncurses \
  src/frontend_curses.c src/posix_vfs.c src/zzt.c src/cpu.c
