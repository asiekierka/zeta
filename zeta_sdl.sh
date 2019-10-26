#!/bin/sh
xxd -i res/8x14.bin > res/8x14.c
gcc -o build/zeta86 -g -O2 -std=c18 -Wall \
  res/8x14.c src/posix_vfs.c src/audio_stream.c \
  src/zzt.c src/cpu.c \
  src/screenshot_writer.c src/render_software.c \
  src/asset_loader.c src/util.c src/audio_writer.c \
  src/sdl/frontend.c src/sdl/render_software.c src/sdl/render_opengl.c \
  -lGL -lSDL2 -lSDL2main -lpng
