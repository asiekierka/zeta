#!/bin/sh
xxd -i res/8x14.bin > res/8x14.c
gcc -o build/zeta86 -g -O2 -std=c11 -Wall -lncursesw \
  src/frontend_curses.c src/posix_vfs.c src/zzt.c src/cpu.c res/8x14.c
