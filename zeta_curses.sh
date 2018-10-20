#!/bin/sh
gcc -o build/zeta86 -g -O3 -std=c11 -Wall -lncurses \
  src/curses_glue.c src/posix_glue.c src/zzt.c src/cpu.c
