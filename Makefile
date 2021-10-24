ifneq (${ARCH},)
BUILDDIR = build/${ARCH}-${PLATFORM}
CC = ${ARCH}gcc
else
BUILDDIR = build/${PLATFORM}
CC = gcc
endif

VERSION ?= unknown

RESDIR = res
SRCDIR = src
TOOLSDIR = tools
FONTSDIR = fonts
OBJDIR = ${BUILDDIR}/obj

CFLAGS = -g -O2 -flto -std=gnu18 -Wall -DVERSION=\"${VERSION}\"
LDFLAGS = -g -O2 -flto -std=gnu18 -Wall

ifeq (${PLATFORM},mingw32-sdl)
USE_SDL = 1
CC = ${ARCH}-w64-mingw32-gcc
CFLAGS += -mwindows
LIBS = -Wl,-Bstatic -lmingw32 -lwinpthread -lm -lgcc -lSDL2main -lpng -lz -lssp -Wl,-Bdynamic -lSDL2 -lopengl32
TARGET = $(BUILDDIR)/zeta86.exe
else ifeq (${PLATFORM},unix-sdl)
USE_SDL = 1
LIBS = -lGL -lSDL2 -lSDL2main -lpng -lm
TARGET = $(BUILDDIR)/zeta86
else ifeq (${PLATFORM},unix-curses)
USE_CURSES = 1
LIBS = -lncursesw -lm
TARGET = $(BUILDDIR)/zeta86
else ifeq (${PLATFORM},wasm)
CC = EMCC_CLOSURE_ARGS="--js $(realpath ${SRCDIR})/emscripten_externs.js" emcc
CFLAGS = -O3 --js-library ${SRCDIR}/emscripten_glue.js \
  -s STRICT=1 --closure 1 \
  -s ENVIRONMENT=web \
  -s 'EXPORTED_FUNCTIONS=["_malloc","_free"]' \
  -s 'EXPORTED_RUNTIME_METHODS=["AsciiToString"]' \
  -s MODULARIZE=1 -s 'EXPORT_NAME="ZetaNative"' \
  -s SUPPORT_ERRNO=0 \
  -s ALLOW_MEMORY_GROWTH=1 -s ASSERTIONS=0 \
  -s 'MALLOC="emmalloc"' -s FILESYSTEM=0 \
  -s INITIAL_MEMORY=4194304 -s TOTAL_STACK=262144 \
  -DNO_MEMSET -DAVOID_MALLOC --no-entry
LDFLAGS = ${CFLAGS}
TARGET = $(BUILDDIR)/zeta_native.js
else
$(error Please specify PLATFORM: mingw32-sdl, unix-curses, unix-sdl, wasm)
endif

OBJS =	$(OBJDIR)/8x8dbl.o \
	$(OBJDIR)/8x14.o \
	\
	$(OBJDIR)/cpu.o \
	$(OBJDIR)/zzt.o \
	$(OBJDIR)/zzt_ems.o \
	$(OBJDIR)/audio_stream.o \
	$(OBJDIR)/audio_shared.o

ifeq (${USE_SDL},1)
OBJS += $(OBJDIR)/asset_loader.o \
	$(OBJDIR)/posix_vfs.o \
	$(OBJDIR)/util.o \
	$(OBJDIR)/render_software.o \
	$(OBJDIR)/audio_writer.o \
	$(OBJDIR)/gif_writer.o \
	$(OBJDIR)/screenshot_writer.o \
	$(OBJDIR)/sdl/frontend.o \
	$(OBJDIR)/sdl/render_software.o \
	$(OBJDIR)/sdl/render_opengl.o
else ifeq (${USE_CURSES},1)
OBJS += $(OBJDIR)/frontend_curses.o \
	$(OBJDIR)/asset_loader.o \
	$(OBJDIR)/posix_vfs.o
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
	$(CC) -g -c -o $@ $<

$(OBJDIR)/8x14.c: $(OBJDIR)/8x14.bin $(TOOLSDIR)/bin2c.py
	@mkdir -p $(@D)
	python3 $(TOOLSDIR)/bin2c.py --field_name res_8x14_bin $(OBJDIR)/8x14.c $(OBJDIR)/8x14.h $(OBJDIR)/8x14.bin

$(OBJDIR)/8x14.bin: $(FONTSDIR)/pc_ega.png $(TOOLSDIR)/font2raw.py
	@mkdir -p $(@D)
	python3 $(TOOLSDIR)/font2raw.py $< 8 14 a $@

$(OBJDIR)/8x8dbl.o: $(OBJDIR)/8x8dbl.c
	@mkdir -p $(@D)
	$(CC) -g -c -o $@ $<

$(OBJDIR)/8x8dbl.c: $(OBJDIR)/8x8dbl.bin $(TOOLSDIR)/bin2c.py
	@mkdir -p $(@D)
	python3 $(TOOLSDIR)/bin2c.py --field_name res_8x8dbl_bin $(OBJDIR)/8x8dbl.c $(OBJDIR)/8x8dbl.h $(OBJDIR)/8x8dbl.bin

$(OBJDIR)/8x8dbl.bin: $(FONTSDIR)/pc_cga.png $(TOOLSDIR)/font2raw.py
	@mkdir -p $(@D)
	python3 $(TOOLSDIR)/font2raw.py $< 8 8 dbl $@

.PHONY: clean

clean:
	rm -f $(OBJS) $(TARGET)
