/**
 * Copyright (c) 2018, 2019, 2020 Adrian Siekierka
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

#ifndef __AUDIO_STREAM_H__
#define __AUDIO_STREAM_H__

#include "types.h"

USER_FUNCTION
void audio_stream_init(long time, int freq, bool asigned, bool a16bit);
USER_FUNCTION
u8 audio_stream_get_volume();
USER_FUNCTION
u8 audio_stream_get_max_volume();
USER_FUNCTION
void audio_stream_set_volume(u8 volume);
USER_FUNCTION
void audio_stream_generate(long time, u8 *stream, int len);
USER_FUNCTION
void audio_stream_append_on(long time, int cycles, double freq);
USER_FUNCTION
void audio_stream_append_off(long time, int cycles);

#endif
