#!/bin/sh
xxd -i res/8x14.bin > res/8x14.c
gcc -o build/zeta86 -g -O2 -std=c18 -Wall \
  res/8x14.c src/posix_vfs.c src/audio_stream.c \
  src/frontend_sdl.c src/zzt.c src/cpu.c \
  src/screenshot_writer.c src/render_software.c \
  -lGL -lSDL2 -lSDL2main
