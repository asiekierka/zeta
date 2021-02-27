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
#define GIF_MAX_BUFFER_POS 255
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
	size_t file_delay_loc;

	char *vbuf;
	bool optimize;
	bool optimize_lcts; // Enables using LCTs as optimization. Requires pad_palette.
	bool pad_palette; // Enables 5-bit output and transparency color.

	int screen_width;
	int screen_height;
	int char_width;
	int char_height;
	u32 pit_ticks;

	u8 prev_video[4000];
	u32 global_palette[16];

	size_t draw_buffer_size;
	u8 *draw_buffer;
	bool force_full_redraw;

	lzw_encode_state lzw;

	u16 gif_delay_buffer;
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
#define GIF_FLAG_GCM_DEPTH(bits) ((bits) - 1)
#define GIF_GCE_DISPOSE(flag) ((flag) << 2)
#define GIF_GCE_DISPOSE_IGNORE GIF_GCE_DISPOSE(1)
#define GIF_GCE_DISPOSE_RESTORE_BACKGROUND GIF_GCE_DISPOSE(2)
#define GIF_GCE_DISPOSE_RESTORE_PREVIOUS GIF_GCE_DISPOSE(3)
#define GIF_GCE_USER_INPUT 0x02
#define GIF_GCE_TRANSPARENCY_INDEX 0x01
#define GIF_IMAGE_LOCAL_COLOR_MAP 0x80
#define GIF_IMAGE_INTERLACED 0x40
#define GIF_IMAGE_LCM_DEPTH(bits) ((bits) - 1)

static void gif_write_palette(gif_writer_state *s, u32 *palette, int colors) {
	for (int i = 0; i < (colors < 16 ? colors : 16); i++) {
		fputc(palette[i] >> 16, s->file);
		fputc(palette[i] >> 8, s->file);
		fputc(palette[i], s->file);
	}
	if (colors >= 32) {
		fwrite("\x00\x00\x00Support broken thumbnailers - 48 byte filler.", 48, 1, s->file);
	}
}

gif_writer_state *gif_writer_start(const char *filename, bool optimize, bool pad_palette) {
	FILE *file = fopen(filename, "wb");
	if (file == NULL) return NULL;

	gif_writer_state *s = malloc(sizeof(gif_writer_state));
	memset(s, 0, sizeof(gif_writer_state));

	zzt_get_charset(&s->char_width, &s->char_height);
	zzt_get_screen_size(&s->screen_width, &s->screen_height);
	
	s->file = file;
	setvbuf(s->file, s->vbuf, _IOFBF, 65536);

	// prepare internal flags
	s->force_full_redraw = true;
	s->optimize = optimize;
	s->pad_palette = pad_palette;
	s->optimize_lcts = true;
	
	// write header
	fwrite("GIF89a", 6, 1, s->file);
	fput16le(s->file, s->screen_width * s->char_width * (s->screen_width <= 40 ? 2 : 1));
	fput16le(s->file, s->screen_height * s->char_height);
	/* if (s->optimize_lcts) {
		fputc(GIF_FLAG_COLOR_RESOLUTION(8), s->file);
	} */ // Uncomment if going back to always emitting LCTs
	fputc(GIF_FLAG_GLOBAL_COLOR_MAP | GIF_FLAG_COLOR_RESOLUTION(8) | GIF_FLAG_GCM_DEPTH(s->pad_palette ? 5 : 4), s->file);
	fputc(0, s->file); // background color
	fputc(0, s->file); // aspect ratio

	// write global color table
	// if (!s->optimize_lcts) { // Uncomment if going back to always emitting LCTs
	u32 *palette = zzt_get_palette();
	memcpy(s->global_palette, palette, 16 * sizeof(u32));
	gif_write_palette(s, palette, s->pad_palette ? 32 : 16);

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

static void gif_writer_write_delay(gif_writer_state *s) {
	if (s->file_delay_loc > 0) {
		size_t curr_loc = ftell(s->file);
		fseek(s->file, s->file_delay_loc, SEEK_SET);
		fput16le(s->file, s->gif_delay_buffer >> 1);
		fseek(s->file, curr_loc, SEEK_SET);
		s->gif_delay_buffer &= 0x1;
		s->file_delay_loc = 0;
	}
}

void gif_writer_stop(gif_writer_state *s) {
	// terminate file
	gif_writer_write_delay(s);
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

static u8 vram_difference[2000 >> 3];

static bool can_draw_char_vram_difference(int x, int y) {
	return (vram_difference[y * 10 + (x >> 3)] & (1 << (x & 7))) != 0;
}

void gif_writer_frame(gif_writer_state *s) {
	s->gif_delay_buffer += 11;
	u32 palette_optimized[16];

	int new_screen_w, new_screen_h, new_char_w, new_char_h;
	zzt_get_screen_size(&new_screen_w, &new_screen_h);
	u8 *charset = zzt_get_charset(&new_char_w, &new_char_h);
	u32 *palette = zzt_get_palette();
	u8 *video = zzt_get_ram() + 0xB8000;
	bool blink = zzt_get_blink() != 0;
	bool blink_active = blink && ((s->pit_ticks >> 1) & 2);
	bool requires_lct = memcmp(palette, s->global_palette, 16 * sizeof(u32)) != 0;

	if (!s->optimize
		|| new_screen_w != s->screen_width || new_screen_h != s->screen_height
		|| new_char_w != s->char_width || new_char_h != s->char_height)
	{
		s->screen_width = new_screen_w;
		s->screen_height = new_screen_h;
		s->char_width = new_char_w;
		s->char_height = new_char_h;
		s->force_full_redraw = true;
	}

	int cx1 = s->screen_width - 1;
	int cy1 = s->screen_height - 1;
	int cx2 = 0;
	int cy2 = 0;

	bool optimize_lcts = s->optimize_lcts && requires_lct && !s->force_full_redraw;
	bool use_transparency = s->optimize && s->pad_palette && !s->force_full_redraw;
	int bit_depth = s->pad_palette ? 5 : 4;

	// calculate cx/cy/cw/ch boundaries and new prev_video state
	u8 *old_vram = s->prev_video;
	u8 *new_vram = video;
	u8 *vram_diff_ptr = vram_difference;
	if (use_transparency) {
		memset(vram_difference, 0, sizeof(vram_difference));
	}
	u16 used_palette_colors = 0;
	u8 used_palette_map[17];

	for (int cy = 0; cy < s->screen_height; cy++) {
		for (int cx = 0; cx < s->screen_width; cx++, old_vram += 2, new_vram += 2) {
			u8 nv_chr = new_vram[0];
			u8 nv_col = new_vram[1];
			if (blink && ((nv_col & 0x80) != 0)) {
				if (blink_active) {
					nv_col = (nv_col >> 4) * 0x11;
				} else {
					nv_col &= 0x7F;
				}
			}
			if (old_vram[0] != nv_chr || old_vram[1] != nv_col) {
				old_vram[0] = nv_chr;
				old_vram[1] = nv_col;
				if (cx1 > cx) cx1 = cx;
				if (cx2 < cx) cx2 = cx;
				if (cy1 > cy) cy1 = cy;
				if (cy2 < cy) cy2 = cy;
				if (optimize_lcts) {
					used_palette_colors |= (1 << (nv_col & 0x0F));
					used_palette_colors |= (1 << (nv_col >> 4));
				}
				if (use_transparency) {
					vram_diff_ptr[cx >> 3] |= (1 << (cx & 7));
				}
			}
		}
		vram_diff_ptr += 80 >> 3;
	}

	if (s->force_full_redraw) {
		cx1 = 0;
		cy1 = 0;
		cx2 = s->screen_width - 1;
		cy2 = s->screen_height - 1;
	}

	int px = cx1 * s->char_width * (s->screen_width <= 40 ? 2 : 1);
	int py = cy1 * s->char_height;
	int pw = (cx2 - cx1 + 1) * s->char_width * (s->screen_width <= 40 ? 2 : 1);
	int ph = (cy2 - cy1 + 1) * s->char_height;

	if (pw <= 0 || ph <= 0) {
		if (s->gif_delay_buffer >= 64000) {
			// failsafe
			px = 0; py = 0;
			pw = s->char_width * (s->screen_width <= 40 ? 2 : 1); ph = s->char_height;
			cx1 = 0; cy1 = 0;
			cx2 = 0; cy2 = 0;
		} else {
			return;
		}
	}

	if (optimize_lcts) {
		int colors_used = 0;
		int i = 0;
		while (used_palette_colors > 0) {
			if (used_palette_colors & 1) {
				used_palette_map[i] = colors_used;
				palette_optimized[colors_used++] = palette[i];
			}
			used_palette_colors >>= 1;
			i++;
		}
		if (colors_used < 16) {
			memset(palette_optimized + colors_used, 0, (16 - colors_used) * 4);
		}
		used_palette_map[0x10] = colors_used++;
		bit_depth = highest_bit_index(colors_used) + 1;
		if (bit_depth < 3) bit_depth = 3;
	}

	u8 *pixels = gif_alloc_draw_buffer(s, pw * ph);
	if (use_transparency) {
		memset(pixels, 0x10, pw * ph);
	}
	render_software_paletted_range(pixels, s->screen_width, -1, RENDER_BLINK_OFF, s->prev_video, charset, s->char_width, s->char_height,
		cx1, cy1, cx2, cy2, use_transparency ? can_draw_char_vram_difference : NULL);

	gif_writer_write_delay(s);

	// write GCE
	fputc('!', s->file); // extension
	fputc(0xF9, s->file); // graphic control extension
	fputc(4, s->file); // size
	fputc(GIF_GCE_DISPOSE_IGNORE | (use_transparency ? GIF_GCE_TRANSPARENCY_INDEX : 0), s->file); // flags
	s->file_delay_loc = ftell(s->file);
	fput16le(s->file, 11); // delay time (temporary)
	fputc(optimize_lcts ? used_palette_map[0x10] : ((bit_depth >= 5) ? 0x10 : 0), s->file); // transparent color index
	fputc(0x00, s->file); // block terminatorcd 

	// write image descriptor
	fputc(',', s->file); // extension
	fput16le(s->file, px); // X pos
	fput16le(s->file, py); // Y pos
	fput16le(s->file, pw); // width
	fput16le(s->file, ph); // height
	fputc(requires_lct ? GIF_IMAGE_LOCAL_COLOR_MAP | GIF_IMAGE_LCM_DEPTH(bit_depth) : 0x00, s->file); // flags

	if (requires_lct) {
		gif_write_palette(s, optimize_lcts ? palette_optimized : palette, 1 << bit_depth);
	}

	lzw_init(&s->lzw, 1 << bit_depth, s->file);
	if (optimize_lcts) {
		for (int iy = 0; iy < ph; iy++) {
			for (int ix = 0; ix < pw; ix++, pixels++) {
				lzw_emit(&s->lzw, used_palette_map[*pixels], s->file);
			}
		}
	} else {
		for (int iy = 0; iy < ph; iy++) {
			for (int ix = 0; ix < pw; ix++, pixels++) {
				lzw_emit(&s->lzw, *pixels, s->file);
			}
		}
	}
	lzw_finish(&s->lzw, s->file);

	fputc(0x00, s->file); // block terminator
	s->force_full_redraw = false;
}

void gif_writer_on_charset_change(gif_writer_state *s) {
	s->force_full_redraw = true;
}

void gif_writer_on_palette_change(gif_writer_state *s) {
	s->force_full_redraw = true;
}
