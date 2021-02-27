/**
 * Copyright (c) 2018, 2019, 2020, 2021 Adrian Siekierka
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "util.h"
#include "zzt.h"
#include "gif_writer.h"
#include "render_software.h"

#define GIF_MAX_CODE_COUNT 4096
#define GIF_MAX_COLOR_COUNT 17
#define GIF_MAX_BUFFER_POS 254
#define LZW_CODE_UNDEFINED 0xFFFF

typedef struct {
	u16 jumptable[(GIF_MAX_CODE_COUNT + 1) * GIF_MAX_COLOR_COUNT];
	u16 initial_max_code;
	u16 current_max_code;
	u16 current_code;
	u16 buffer_pos;
	u8 buffer[255];
	u8 bit_width;
	u32 bitstream_partial;
	u8 bitstream_offset;
} lzw_encode_state;

typedef struct s_gif_writer_state {
	FILE *file;
	char *vbuf;

	int screen_width;
	int screen_height;
	int char_width;
	int char_height;
	u32 pit_ticks;

	u8 text_buffer[4000];
	size_t draw_buffer_size;
	u8 *draw_buffer;

	lzw_encode_state lzw;
} gif_writer_state;

static void lzw_write_buffer(lzw_encode_state *lzw, FILE *file) {
	if (lzw->buffer_pos > 0) {
		fputc(lzw->buffer_pos, file);
		fwrite(lzw->buffer, lzw->buffer_pos, 1, file);
		lzw->buffer_pos = 0;
	}
}

static void lzw_clear_bitstream(lzw_encode_state *lzw, FILE *file, int limit) {
	while (lzw->bitstream_offset > limit) {
		if (lzw->buffer_pos >= GIF_MAX_BUFFER_POS) {
			lzw_write_buffer(lzw, file);
		}
		lzw->buffer[lzw->buffer_pos++] = lzw->bitstream_partial & 0xFF;
		lzw->bitstream_offset = (lzw->bitstream_offset < 8) ? 0 : (lzw->bitstream_offset - 8);
		lzw->bitstream_partial >>= 8;
	}
}

static void lzw_emit_code(lzw_encode_state *lzw, u16 code, FILE *file) {
	lzw->bitstream_partial |= (code << lzw->bitstream_offset);
	lzw->bitstream_offset += lzw->bit_width;
	lzw_clear_bitstream(lzw, file, 7);
}

static void lzw_clear(lzw_encode_state *lzw, FILE *file, bool start) {
	if (!start) {
		lzw_emit_code(lzw, lzw->initial_max_code - 2, file); // clear code
	}
	memset(lzw->jumptable, 0xFF, sizeof(u16) * GIF_MAX_CODE_COUNT * GIF_MAX_COLOR_COUNT);
	lzw->current_max_code = lzw->initial_max_code;
	lzw->current_code = LZW_CODE_UNDEFINED;
	lzw->bit_width = highest_bit_index(lzw->current_max_code - 1) + 1;
	if (start) {
		u16 bpp = highest_bit_index(lzw->initial_max_code - 2);
		fputc(bpp > 2 ? bpp : 2, file); // min code size
		lzw_emit_code(lzw, lzw->initial_max_code - 2, file); // clear code
	}
}

static void lzw_emit(lzw_encode_state *lzw, u8 color, FILE *file) {
	if (lzw->current_code == LZW_CODE_UNDEFINED) {
		lzw->current_code = GIF_MAX_CODE_COUNT;
	}
	int next_code_idx = lzw->current_code * GIF_MAX_COLOR_COUNT + color;
	if (lzw->jumptable[next_code_idx] == LZW_CODE_UNDEFINED) {
		// emit
		lzw_emit_code(lzw, lzw->current_code, file);
		// add new code
		if (lzw->current_max_code >= GIF_MAX_CODE_COUNT) {
			lzw_clear(lzw, file, false);
		} else {
			lzw->jumptable[next_code_idx] = lzw->current_max_code++;
			lzw->bit_width = highest_bit_index(lzw->current_max_code - 1) + 1;
		}
		// set to start
		lzw->current_code = color;
	} else {
		// jump
		lzw->current_code = lzw->jumptable[next_code_idx];
	}
}

static void lzw_init(lzw_encode_state *lzw, u16 colors, FILE *file) {
	lzw->initial_max_code = colors + 2;
	lzw->buffer_pos = 0;
	lzw_clear(lzw, file, true);
	// This area is never reset, so we set it here.
	for (u16 i = 0; i < GIF_MAX_COLOR_COUNT; i++) {
		lzw->jumptable[GIF_MAX_CODE_COUNT * GIF_MAX_COLOR_COUNT + i] = i;
	}
}

static void lzw_finish(lzw_encode_state *lzw, FILE *file) {
	if (lzw->current_code != LZW_CODE_UNDEFINED) {
		lzw_emit_code(lzw, lzw->current_code, file);
	}
	lzw_emit_code(lzw, lzw->initial_max_code - 1, file); // stop code
	lzw_clear_bitstream(lzw, file, 0);
	lzw_write_buffer(lzw, file);
}

#define GIF_FLAG_GLOBAL_COLOR_MAP 0x80
#define GIF_FLAG_COLOR_RESOLUTION(bits) (((bits) - 1) << 4)
#define GIF_FLAG_COLOR_DEPTH(bits) ((bits) - 1)
#define GIF_GCE_DISPOSE(flag) ((flag) << 2)
#define GIF_GCE_DISPOSE_IGNORE GIF_GCE_DISPOSE(1)
#define GIF_GCE_DISPOSE_RESTORE_BACKGROUND GIF_GCE_DISPOSE(2)
#define GIF_GCE_DISPOSE_RESTORE_PREVIOUS GIF_GCE_DISPOSE(3)
#define GIF_GCE_USER_INPUT 0x02
#define GIF_GCE_TRANSPARENCY_INDEX 0x01
#define GIF_IMAGE_LOCAL_COLOR_MAP 0x80
#define GIF_IMAGE_INTERLACED 0x40
#define GIF_IMAGE_COLOR_DEPTH(bits) ((bits) - 1)

gif_writer_state *gif_writer_start(const char *filename) {
	FILE *file = fopen(filename, "wb");
	if (file == NULL) return NULL;

	gif_writer_state *s = malloc(sizeof(gif_writer_state));
	memset(s, 0, sizeof(gif_writer_state));

	zzt_get_charset(&s->char_width, &s->char_height);
	zzt_get_screen_size(&s->screen_width, &s->screen_height);
	
	s->file = file;
	setvbuf(s->file, s->vbuf, _IOFBF, 65536);

	// write header
	fwrite("GIF89a", 6, 1, s->file);
	fput16le(s->file, s->screen_width * s->char_width);
	fput16le(s->file, s->screen_height * s->char_height);
	fputc(GIF_FLAG_GLOBAL_COLOR_MAP | GIF_FLAG_COLOR_RESOLUTION(8) | GIF_FLAG_COLOR_DEPTH(4), s->file);
	fputc(0, s->file); // background color
	fputc(0, s->file); // aspect ratio

	// write global color table
	u32 *palette = zzt_get_palette();
	for (int i = 0; i < 16; i++) {
		fputc(palette[i] >> 16, s->file);
		fputc(palette[i] >> 8, s->file);
		fputc(palette[i], s->file);
	}

	// write netscape looping extension
	fputc('!', s->file); // extension
	fputc(0xFF, s->file); // application extension
	fputc(11, s->file); // length
	fwrite("NETSCAPE2.0", 11, 1, s->file);
	fputc(3, s->file); // length
	fputc(1, s->file); // sub block ID
	fput16le(s->file, 0xFFFF); // unlimited repetitions
	fputc(0x00, s->file); // block terminator

	return s;
}

void gif_writer_stop(gif_writer_state *s) {
	// terminate file
	fputc(';', s->file);

	// clean up
	fclose(s->file);
	free(s->vbuf);
	if (s->draw_buffer != NULL) free(s->draw_buffer);
	free(s);
}

static u8* gif_alloc_draw_buffer(gif_writer_state *s, int pixel_count) {
	if (s->draw_buffer_size < pixel_count) {
		if (s->draw_buffer != NULL) free(s->draw_buffer);
		s->draw_buffer = malloc(pixel_count);
		s->draw_buffer_size = pixel_count;
	}
	return s->draw_buffer;
}

void gif_writer_frame(gif_writer_state *s) {
	// only draw every other frame
	if (((s->pit_ticks++) & 1) == 1) return;

	zzt_get_screen_size(&s->screen_width, &s->screen_height);
	u8 *charset = zzt_get_charset(&s->char_width, &s->char_height);
	u32 *palette = zzt_get_palette();
	u8 *video = zzt_get_ram() + 0xB8000;
	bool blink = zzt_get_blink() != 0;
	bool blink_active = blink && ((s->pit_ticks >> 1) & 2);

	int pw = s->char_width * s->screen_width;
	int ph = s->char_height * s->screen_height;
	
	// write GCE
	fputc('!', s->file); // extension
	fputc(0xF9, s->file); // graphic control extension
	fputc(4, s->file); // size
	fputc(GIF_GCE_DISPOSE_IGNORE, s->file); // flags
	fput16le(s->file, 11); // delay time (0.11s)
	fputc(0xFF, s->file); // transparent color index
	fputc(0x00, s->file); // block terminator

	// write image descriptor
	fputc(',', s->file); // extension
	fput16le(s->file, 0); // X pos
	fput16le(s->file, 0); // X pos
	fput16le(s->file, pw); // width
	fput16le(s->file, ph); // height
	fputc(0, s->file); // flags

	u8 *pixels = gif_alloc_draw_buffer(s, pw * ph);
	render_software_paletted(pixels, s->screen_width, -1, 0, video, charset, s->char_width, s->char_height);

	lzw_init(&s->lzw, 16, s->file);
	for (int iy = 0; iy < ph; iy++) {
		for (int ix = 0; ix < pw; ix++, pixels++) {
			lzw_emit(&s->lzw, *pixels & 0x0F, s->file);
		}
	}
	lzw_finish(&s->lzw, s->file);

	fputc(0x00, s->file); // block terminator
}