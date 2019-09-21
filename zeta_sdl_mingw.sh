#!/bin/sh
xxd -i res/8x14.bin > res/8x14.c
if [ -f build/mingw_resources.o ]; then rm build/mingw_resources.o; fi
i686-w64-mingw32-windres mingw/resources.rc build/mingw_resources.o
i686-w64-mingw32-gcc -o build/zeta86.exe -g -O2 -std=c18 -Wall -mwindows \
  res/8x14.c src/posix_vfs.c src/audio_stream.c src/zzt.c src/cpu.c \
  src/screenshot_writer.c src/render_software.c build/mingw_resources.o \
  src/asset_loader.c \
  src/sdl/frontend.c src/sdl/render_software.c src/sdl/render_opengl.c \
  -Wl,-Bstatic -lmingw32 -lwinpthread -lgcc -lSDL2main -lpng -lz \
  -Wl,-Bdynamic -lSDL2 -lopengl32
