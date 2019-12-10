/**
 * Copyright (c) 2018, 2019 Adrian Siekierka
 *
 * This file is part of Zeta.
 *
 * Zeta is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Zeta is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Zeta.  If not, see <http://www.gnu.org/licenses/>.
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
