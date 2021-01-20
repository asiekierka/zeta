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

#ifndef __AUDIO_SHARED_H__
#define __AUDIO_SHARED_H__

#include "types.h"

#define MINIMUM_NOTE_DELAY 2

typedef struct {
	u8 enabled;
	int cycles;
	double freq;
	double ms;
} speaker_entry;

void audio_generate_init(void);
u16 audio_generate_sample(u16 min, u16 max, int freq_fixed, double freq_real, int pos_fixed, int freq_aud);

USER_FUNCTION
double audio_get_note_delay();
USER_FUNCTION
void audio_set_note_delay(double delay);
USER_FUNCTION
double audio_local_delay_time(int cycles_prev, int cycles_curr, int global_frequency);
USER_FUNCTION
u8 audio_should_insert_pause(speaker_entry* entries, int pos);

#endif
