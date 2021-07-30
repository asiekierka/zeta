AUTOMAKE_OPTIONS = foreign subdir-objects

bin_PROGRAMS = zeta
zeta_SOURCES = \
	src/8x14.c \
	src/cpu.c \
	src/zzt.c \
	src/zzt_ems.c \
	src/audio_stream.c \
	src/audio_shared.c \
	src/util.c \
	src/asset_loader.c

if FRONTEND_SDL2
zeta_SOURCES += \
	src/posix_vfs.c \
	src/render_software.c \
	src/audio_writer.c \
	src/gif_writer.c \
	src/screenshot_writer.c \
	src/sdl/frontend.c \
	src/sdl/render_software.c \
	src/sdl/render_opengl.c
endif

if FRONTEND_NCURSES
zeta_SOURCES += \
	src/posix_vfs.c \
	src/frontend_curses.c
endif

zeta_CFLAGS = $(FRONTEND_CFLAGS)
zeta_CXXFLAGS = $(FRONTEND_CFLAGS)
zeta_LDADD = $(FRONTEND_LDFLAGS)

src/8x14.c: src/8x14.bin $(srcdir)/tools/bin2c.py
	@mkdir -p $(@D)
	$(PYTHON) $(srcdir)/tools/bin2c.py --field_name res_8x14_bin src/8x14.c src/8x14.h src/8x14.bin

src/8x14.bin: $(srcdir)/fonts/pc_ega.png $(srcdir)/tools/font2raw.py
	@mkdir -p $(@D)
	$(PYTHON) $(srcdir)/tools/font2raw.py $< 8 14 a $@

