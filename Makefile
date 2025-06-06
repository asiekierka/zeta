ifneq (${USE_CLANG},)
ifneq (${ARCH},)
BUILDDIR = build/${ARCH}-${PLATFORM}-clang
CC = ${ARCH}clang
else
BUILDDIR = build/${PLATFORM}-clang
CC = clang
endif
else
ifneq (${ARCH},)
BUILDDIR = build/${ARCH}-${PLATFORM}
CC = ${ARCH}gcc
else
BUILDDIR = build/${PLATFORM}
CC = gcc
endif
endif

VERSION ?= 1.1.3

RESDIR = res
SRCDIR = src
TOOLSDIR = tools
FONTSDIR = fonts
OBJDIR = ${BUILDDIR}/obj

CFLAGS = -g -O3 -flto -std=gnu18 -Wall -DVERSION=\"${VERSION}\"
LDFLAGS = -g -O3 -flto -std=gnu18 -Wall

ifeq (${PLATFORM},mingw32-sdl3)
USE_SDL3 = 1
WINDRES = windres
ifneq (${ARCH},)
WINDRES = ${ARCH}-w64-mingw32-windres
ifneq (${USE_CLANG},)
CC = ${ARCH}-w64-mingw32-clang
else
CC = ${ARCH}-w64-mingw32-gcc
endif
endif
CFLAGS += -mwindows
LDFLAGS += -mwindows
LIBS = -Wl,-Bstatic -lmingw32 -lwinpthread -lm -lgcc -lpng -lz -lssp -Wl,-Bdynamic -lSDL3 -lopengl32
TARGET = $(BUILDDIR)/zeta86.exe
else ifeq (${PLATFORM},mingw32-sdl2)
USE_SDL2 = 1
WINDRES = windres
ifneq (${ARCH},)
WINDRES = ${ARCH}-w64-mingw32-windres
ifneq (${USE_CLANG},)
CC = ${ARCH}-w64-mingw32-clang
else
CC = ${ARCH}-w64-mingw32-gcc
endif
endif
CFLAGS += -mwindows
LDFLAGS += -mwindows
LIBS = -Wl,-Bstatic -lmingw32 -lwinpthread -lm -lgcc -lSDL2main -lpng -lz -lssp -Wl,-Bdynamic -lSDL2 -lopengl32
TARGET = $(BUILDDIR)/zeta86.exe
else ifeq (${PLATFORM},macos-sdl3)
USE_SDL3 = 1
CC = clang
LIBS = -framework OpenGL -lSDL3 -lpng -lm
else ifeq (${PLATFORM},macos-sdl2)
USE_SDL2 = 1
CC = clang
LIBS = -framework OpenGL -lSDL2main -lSDL2 -lpng -lm
TARGET = $(BUILDDIR)/zeta86
ifeq (${ARCH},x86_64)
CFLAGS += -target x86_64-apple-macos10.12
LDFLAGS += -target x86_64-apple-macos10.12
else ifeq (${ARCH},aarch64)
CFLAGS += -target arm64-apple-macos11
LDFLAGS += -target arm64-apple-macos11
endif
else ifeq (${PLATFORM},unix-sdl2)
USE_SDL2 = 1
LIBS = -lGL -lSDL2 -lSDL2main -lpng -lm
TARGET = $(BUILDDIR)/zeta86
else ifeq (${PLATFORM},unix-sdl3)
USE_SDL3 = 1
LIBS = -lGL -lSDL3 -lpng -lm
TARGET = $(BUILDDIR)/zeta86
else ifeq (${PLATFORM},unix-curses)
USE_CURSES = 1
LIBS = -lncursesw -lm
TARGET = $(BUILDDIR)/zeta86
else ifeq (${PLATFORM},unix-headless)
USE_HEADLESS = 1
LIBS = -lm
TARGET = $(BUILDDIR)/zeta86
else ifeq (${PLATFORM},wasm)
CC = EMCC_CLOSURE_ARGS="--js $(realpath ${SRCDIR})/emscripten_externs.js" emcc
CFLAGS = -O3 -s STRICT=1 \
  -DNO_MEMSET -DAVOID_MALLOC --no-entry -DVERSION=\"${VERSION}\"
LDFLAGS = ${CFLAGS} \
   --closure 1 \
  -s ENVIRONMENT=web \
  -s 'EXPORTED_FUNCTIONS=["_malloc","_free","_audio_generate_init"]' \
  -s 'EXPORTED_RUNTIME_METHODS=["AsciiToString", "HEAPU8", "HEAPU32"]' \
  -s MODULARIZE=1 -s 'EXPORT_NAME="ZetaNative"' \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s 'MALLOC="emmalloc"' -s FILESYSTEM=0 \
  -s INITIAL_MEMORY=4194304 -s STACK_SIZE=262144 \
  -L $(realpath ${SRCDIR}) -lemscripten_glue.js
TARGET = $(BUILDDIR)/zeta_native.js
ifeq (${DEBUG},)
LDFLAGS += -s ASSERTIONS=0
else
LDFLAGS += -s ASSERTIONS=1
endif
else
$(error Please specify PLATFORM: macos-sdl3, mingw32-sdl3, unix-curses, unix-headless, unix-sdl2, unix-sdl3, wasm)
endif

OBJS =	$(OBJDIR)/8x8.o \
	$(OBJDIR)/8x14.o \
	\
	$(OBJDIR)/cpu.o \
	$(OBJDIR)/zzt.o \
	$(OBJDIR)/zzt_ems.o \
	$(OBJDIR)/ui.o \
	$(OBJDIR)/audio_stream.o \
	$(OBJDIR)/audio_shared.o

ifeq (${USE_SDL2},1)
CFLAGS += -DUSE_SDL -DUSE_SDL2
OBJS += $(OBJDIR)/asset_loader.o \
	$(OBJDIR)/posix_vfs.o \
	$(OBJDIR)/util.o \
	$(OBJDIR)/render_software.o \
	$(OBJDIR)/audio_writer.o \
	$(OBJDIR)/gif_writer.o \
	$(OBJDIR)/screenshot_writer.o \
	$(OBJDIR)/sdl2/frontend.o \
	$(OBJDIR)/sdl2/render_software.o \
	$(OBJDIR)/sdl2/render_opengl.o
else ifeq (${USE_SDL3},1)
CFLAGS += -DUSE_SDL -DUSE_SDL3
OBJS += $(OBJDIR)/asset_loader.o \
	$(OBJDIR)/posix_vfs.o \
	$(OBJDIR)/util.o \
	$(OBJDIR)/render_software.o \
	$(OBJDIR)/audio_writer.o \
	$(OBJDIR)/gif_writer.o \
	$(OBJDIR)/screenshot_writer.o \
	$(OBJDIR)/sdl3/frontend.o \
	$(OBJDIR)/sdl3/render_software.o \
	$(OBJDIR)/sdl3/render_opengl.o
else ifeq (${USE_CURSES},1)
OBJS += $(OBJDIR)/frontend_curses.o \
	$(OBJDIR)/asset_loader.o \
	$(OBJDIR)/posix_vfs.o
else ifeq (${USE_HEADLESS},1)
OBJS += $(OBJDIR)/frontend_headless.o \
	$(OBJDIR)/asset_loader.o \
	$(OBJDIR)/posix_vfs.o
endif

ifeq (${PLATFORM},mingw32-sdl)
OBJS += $(OBJDIR)/win32-resources.o
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(@D)
	$(CC) -o $@ $(LDFLAGS) $(OBJS) $(LIBS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/8x14.o: $(OBJDIR)/8x14.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -g -c -o $@ $<

$(OBJDIR)/8x14.c: $(OBJDIR)/8x14.bin $(TOOLSDIR)/bin2c.py
	@mkdir -p $(@D)
	python3 $(TOOLSDIR)/bin2c.py --field_name res_8x14_bin $(OBJDIR)/8x14.c $(OBJDIR)/8x14.h $(OBJDIR)/8x14.bin

$(OBJDIR)/8x14.bin: $(FONTSDIR)/pc_ega.png $(TOOLSDIR)/font2raw.py
	@mkdir -p $(@D)
	python3 $(TOOLSDIR)/font2raw.py $< 8 14 a $@

$(OBJDIR)/8x8.o: $(OBJDIR)/8x8.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -g -c -o $@ $<

$(OBJDIR)/8x8.c: $(OBJDIR)/8x8.bin $(TOOLSDIR)/bin2c.py
	@mkdir -p $(@D)
	python3 $(TOOLSDIR)/bin2c.py --field_name res_8x8_bin $(OBJDIR)/8x8.c $(OBJDIR)/8x8.h $(OBJDIR)/8x8.bin

$(OBJDIR)/8x8.bin: $(FONTSDIR)/pc_cga.png $(TOOLSDIR)/font2raw.py
	@mkdir -p $(@D)
	python3 $(TOOLSDIR)/font2raw.py $< 8 8 a $@

$(OBJDIR)/win32-resources.o: mingw/resources.rc
	@mkdir -p $(@D)
	$(WINDRES) -i $< -o $@	

.PHONY: clean

clean:
	rm -f $(OBJS) $(TARGET)
