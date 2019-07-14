OBJDIR = obj
RESDIR = res
SRCDIR = src
BUILDDIR = build

CC = gcc
CFLAGS = -g -O2 -flto -std=c11 -Wall
LDFLAGS = -g -O2
LIBS = -lGL -lSDL2 -lSDL2main
TARGET = $(BUILDDIR)/zeta86

OBJS =	$(OBJDIR)/8x14.o \
	\
	$(OBJDIR)/cpu.o \
	$(OBJDIR)/zzt.o \
	$(OBJDIR)/audio_stream.o \
	\
	$(OBJDIR)/posix_vfs.o \
	$(OBJDIR)/frontend_sdl.o \
	$(OBJDIR)/render_software.o \
	$(OBJDIR)/screenshot_writer.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(LDFLAGS) $(LIBS) $(OBJS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/8x14.o: $(OBJDIR)/8x14.c
	$(CC) -g -c -o $@ $<

$(OBJDIR)/8x14.c: $(RESDIR)/8x14.bin
	xxd -i $< > $@
