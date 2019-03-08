#!/bin/sh
xxd -i res/8x14.bin > res/8x14.c
gcc -o build/zeta86 -g -O2 -std=c11 -Wall -lGL -lSDL2 \
  res/8x14.c src/posix_vfs.c src/frontend_sdl.c src/zzt.c src/cpu.c
