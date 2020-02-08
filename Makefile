ifneq (${ARCH},)
BUILDDIR = build/${ARCH}-${PLATFORM}
CC = ${ARCH}gcc
else
BUILDDIR = build/${PLATFORM}
CC = gcc
endif

RESDIR = res
SRCDIR = src
OBJDIR = ${BUILDDIR}/obj

CFLAGS = -g -O3 -flto -std=c18 -Wall
LDFLAGS = -g -O3 -flto -std=c18 -Wall

ifeq (${PLATFORM},mingw32-sdl)
USE_SDL = 1
CC = ${ARCH}-w64-mingw32-gcc
CFLAGS += -mwindows
LIBS = -Wl,-Bstatic -lmingw32 -lwinpthread -lgcc -lSDL2main -lpng -lz -Wl,-Bdynamic -lSDL2 -lopengl32
TARGET = $(BUILDDIR)/zeta86.exe
else ifeq (${PLATFORM},unix-sdl)
USE_SDL = 1
LIBS = -lGL -lSDL2 -lSDL2main -lpng
TARGET = $(BUILDDIR)/zeta86
else ifeq (${PLATFORM},unix-curses)
USE_CURSES = 1
LIBS = -lncursesw
TARGET = $(BUILDDIR)/zeta86
else ifeq (${PLATFORM},wasm)
CC = emcc
CFLAGS = -O3 --js-library src/emscripten_glue.js \
  -s WASM_OBJECT_FILES=0 --llvm-lto 1 \
  -s ENVIRONMENT=web \
  -s 'EXPORTED_FUNCTIONS=["_malloc","_free"]' \
  -s 'EXTRA_EXPORTED_RUNTIME_METHODS=["AsciiToString"]' \
  -s MODULARIZE=1 -s 'EXPORT_NAME="ZetaNative"' \
  -s ALLOW_MEMORY_GROWTH=0 -s ASSERTIONS=0 \
  -s 'MALLOC="emmalloc"' -s NO_FILESYSTEM=1 \
  -s TOTAL_MEMORY=4194304 -s TOTAL_STACK=262144 \
  -s DEFAULT_LIBRARY_FUNCS_TO_INCLUDE="[]" \
  -DNO_MEMSET
LDFLAGS = ${CFLAGS}
TARGET = $(BUILDDIR)/zeta_native.js
else
$(error Please specify PLATFORM: mingw32-sdl, unix-curses, unix-sdl, wasm)
endif

OBJS =	$(OBJDIR)/8x14.o \
	\
	$(OBJDIR)/cpu.o \
	$(OBJDIR)/zzt.o \
	$(OBJDIR)/audio_stream.o \
	$(OBJDIR)/audio_shared.o

ifeq (${USE_SDL},1)
OBJS += $(OBJDIR)/asset_loader.o \
	$(OBJDIR)/audio_writer.o \
	$(OBJDIR)/posix_vfs.o \
	$(OBJDIR)/render_software.o \
	$(OBJDIR)/screenshot_writer.o \
	$(OBJDIR)/util.o \
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

$(OBJDIR)/8x14.c: $(RESDIR)/8x14.bin
	@mkdir -p $(@D)
	xxd -i $< > $@

.PHONY: clean

clean:
	rm -f $(OBJS) $(TARGET)
