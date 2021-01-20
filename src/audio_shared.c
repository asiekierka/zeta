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
#include "config.h"
#include "audio_shared.h"
#include "logging.h"

#if defined(RESAMPLE_BANDLIMITED)
#include "math.h"

#ifndef M_PI
#define M_PI 3.14159265358979
#endif

#define TRIG_SHIFT 14
#define COEFF_MAX 512

static s16 cos_table[1 << TRIG_SHIFT];
static s16 coeff_table[COEFF_MAX];

void audio_generate_init(void) {
	for (int i = 0; i < (1 << (TRIG_SHIFT)); i++) {
		cos_table[i] = cosf(2 * M_PI * (i / (float) (1 << TRIG_SHIFT))) * 16384;
	}
	coeff_table[0] = 0;
	for (int i = 1; i < COEFF_MAX; i++) {
		coeff_table[i] = 32768 * sinf(0.5 * i * M_PI) / (i * M_PI);
	}
}

u16 audio_generate_sample(u16 min, u16 max, int freq_fixed, double freq_real, int pos_fixed, int freq_aud) {
#if 1
	// fixed-point
	s32 sample = 0;
	int coeff_pos = 1;

	int freq_real_fixed = (int) (freq_real * (1 << 6));
	int pos = freq_real_fixed;

	u64 cos_indice = ((u64) freq_real_fixed * pos_fixed / freq_aud);

	while (pos < (freq_aud << (6 - 1)) && coeff_pos < COEFF_MAX) {
		// -(1 << 28) = 0
		// (1 << 28) = 1
		sample += ((s64) coeff_table[coeff_pos] * cos_table[(cos_indice * coeff_pos) & ((1 << TRIG_SHIFT) - 1)]);

		pos += freq_real_fixed;
		coeff_pos++;
	}

	// sample = 0..1 in the .28 range
	sample = ((((sample >> 14) + (1 << 14)) * (max - min)) >> 15) + min;

	if (sample < min) sample = min;
	else if (sample > max) sample = max;

	return (u16) sample;
#else
	// floating-point/reference
	float sample = 0.0f;
	int coeff_pos = 1;

	float freq_float = (float) freq_real;
	float pos_float = pos_fixed / 256.0f;
	float pos = freq_float;

	while (pos < (freq_aud >> 1) && coeff_pos < COEFF_MAX) {
		float coeff = 2 * sinf(coeff_pos * 0.5 * M_PI) / (coeff_pos * M_PI);
		float v = coeff * cosf(freq_float * 2 * M_PI * coeff_pos * (pos_float / freq_aud));
		sample += v;

		pos += freq_float;
		coeff_pos++;
	}

	// sample = 0..1 in the .28 range
	sample = (int) ((sample + 1.0f) * ((max - min) >> 1)) + min;

	if (sample < min) sample = min;
	else if (sample > max) sample = max;

	return (u16) sample;
#endif
}
#else
void audio_generate_init(void) {

}

u16 audio_generate_sample(u16 min, u16 max, int freq_fixed, double freq_real, int pos_fixed, int freq_aud) {
#if defined(RESAMPLE_LINEAR)
	int stepSize = (1 << 8);
#endif
	pos_fixed %= (freq_fixed << 1);

#if defined(RESAMPLE_NEAREST)
	if (pos_fixed < freq_fixed) {
		return max;
	} else {
		return min;
	}
#elif defined(RESAMPLE_LINEAR)
	if (pos_fixed < freq_fixed) {
		if (pos_fixed < stepSize) {
			u32 trans_val = ((min * (stepSize - pos_fixed)) + (max * pos_fixed)) >> 8;
			return (u16) trans_val;
		} else {
			return max;
		}
	} else {
		int midPos = pos_fixed - freq_fixed;
		if (midPos < stepSize) {
			u32 trans_val = ((max * (stepSize - midPos)) + (min * midPos)) >> 8;
			return (u16) trans_val;
		} else {
			return min;
		}
	}
#else
	#error "Please define a resampling method!"
#endif

	// huh
	return (max + min) >> 1;
}
#endif

static double audio_delay_time = 1.0;

double audio_get_note_delay() {
	return audio_delay_time;
}

void audio_set_note_delay(double delay) {
	audio_delay_time = delay;
}

u8 audio_should_insert_pause(speaker_entry* entries, int pos) {
	if (pos <= 0) return 0;

	// ZZT always immediately disables a note... except for drums!
	if (entries[pos - 1].enabled) {
		// Clear out short blips (player walking, etc)
		if (!entries[pos].enabled) {
			// 30 cycles + 1 safety measure
			if ((entries[pos].cycles - entries[pos - 1].cycles) <= 31) {
				return 0;
			}
		}
		return 1;
	}

	// Insert short pauses between same notes.
	if (pos >= 2) {
		if (entries[pos - 2].enabled && entries[pos - 2].freq == entries[pos].freq) {
			return 1;
		}
	}

	return 0;
}

double audio_local_delay_time(int cycles_prev, int cycles_curr, int global_frequency) {
	double min_delay = 1000.0 * MINIMUM_NOTE_DELAY / global_frequency;
	if (cycles_prev >= cycles_curr) {
		return 0;
	} else {
		if ((cycles_curr - cycles_prev) <= 31) {
			return min_delay;
		} else {
			return audio_delay_time;
		}
	}
}
