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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "audio_shared.h"
#include "logging.h"

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
                if ((cycles_curr - cycles_prev) > 3600) {
                        return audio_delay_time;
                } else {
                        double delay = (cycles_curr - cycles_prev) * audio_delay_time / 3600.0;
			if (delay < min_delay) delay = min_delay;
			return delay;
                }
	}
}
