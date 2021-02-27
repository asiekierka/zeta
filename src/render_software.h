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

#ifndef __RENDER_SOFTWARE_H__
#define __RENDER_SOFTWARE_H__

#include "types.h"

#define RENDER_BLINK_OFF 1
#define RENDER_BLINK_PHASE 2

USER_FUNCTION
void render_software_rgb(u32 *buffer, int scr_width, int row_length, int flags, u8 *video, u8 *charset, int char_width, int char_height, u32 *palette);
USER_FUNCTION
void render_software_paletted(u8 *buffer, int scr_width, int row_length, int flags, u8 *video, u8 *charset, int char_width, int char_height);
USER_FUNCTION
void render_software_paletted_range(u8 *buffer, int scr_width, int row_length, int flags, u8 *video, u8 *charset, int char_width, int char_height, int x1, int y1, int x2, int y2);

#endif /* __RENDER_SOFTWARE_H__ */
