#!/bin/sh
xxd -i res/8x14.bin > res/8x14.c
i686-w64-mingw32-gcc -o build/zeta86.exe -g -O2 -std=c11 -Wall \
  res/8x14.c src/posix_vfs.c src/frontend_sdl.c src/zzt.c src/cpu.c \
  -lmingw32 -lSDL2main -lSDL2 -lopengl32
