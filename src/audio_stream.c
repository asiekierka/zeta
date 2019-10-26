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
#include "audio_stream.h"

#define AUDIO_STREAM_DEBUG

typedef struct {
	u8 enabled;
	double freq;
	double ms;
} speaker_entry;

#define AUDIO_VOLUME_MAX 127
#define SPEAKER_ENTRY_LEN 64
static int speaker_entry_pos = 0;
static speaker_entry speaker_entries[SPEAKER_ENTRY_LEN];
static long speaker_freq_ctr = 0;
static u8 audio_volume = AUDIO_VOLUME_MAX;
static double audio_prev_time;
static double audio_delay_time;
static int audio_freq;

#ifdef AUDIO_STREAM_DEBUG
static void audio_stream_print(int pos) {
	speaker_entry *e = &speaker_entries[pos];
	if (e->enabled) {
		printf("[%.2f] speaker on @ %.2f Hz\n", e->ms, e->freq);
	} else {
		printf("[%.2f] speaker off\n", e->ms);
	}
}
#endif

void audio_stream_init(long time, int freq) {
	audio_prev_time = -1;
	audio_delay_time = 1.0;
	audio_freq = freq;
}

u8 audio_stream_get_volume() {
	return audio_volume;
}

double audio_stream_get_note_delay() {
	return audio_delay_time;
}

void audio_stream_set_note_delay(double delay) {
	audio_delay_time = delay;
}

u8 audio_stream_get_max_volume() {
	return AUDIO_VOLUME_MAX;
}

void audio_stream_set_volume(u8 volume) {
	if (volume > AUDIO_VOLUME_MAX) volume = AUDIO_VOLUME_MAX;
	audio_volume = volume;
}

void audio_stream_generate_u8(long time, u8 *stream, int len) {
	int i; long j;
	int freq_samples_fixed;
	int pos_samples_fixed;
	int k;
	double audio_curr_time = audio_prev_time + (len / (double) audio_freq * 1000);
//	double audio_curr_time = time;
	double audio_res = (audio_curr_time - audio_prev_time);
	double res_to_samples = len / audio_res;
	double audio_dfrom, audio_dto;
	long audio_from, audio_to;

	// handle the first
	if (audio_prev_time < 0) {
		audio_prev_time = time;
		memset(stream, 128, len);
		speaker_entry_pos = 0;
		return;
	}

	if (audio_curr_time < time) {
		audio_curr_time = time;
	}

#ifdef AUDIO_STREAM_DEBUG
	fprintf(stderr, "[callback] expected time %.2f received time %ld drift %.2f\n", audio_curr_time, time, time - audio_curr_time);
#endif

	if (speaker_entry_pos == 0) {
		memset(stream, 128, len);
	} else for (i = 0; i < speaker_entry_pos; i++) {
		audio_dfrom = speaker_entries[i].ms - audio_prev_time;

		if (i == speaker_entry_pos - 1) audio_dto = audio_res;
		else audio_dto = speaker_entries[i+1].ms - audio_prev_time;

		// convert
		audio_from = (long) (audio_dfrom * res_to_samples);
		audio_to = (long) (audio_dto * res_to_samples);

#ifdef AUDIO_STREAM_DEBUG
		if (speaker_entries[i].enabled) {
			printf("[callback] testing note @ %.2f Hz (%ld, %ld)\n", speaker_entries[i].freq, audio_from, audio_to);
		}
#endif

		if (audio_from < 0) audio_from = 0;
		else if (audio_from >= len) break;
		if (audio_to < 0) audio_to = 0;
		else if (audio_to > len) audio_to = len;

		// emit
		if (audio_to > audio_from) {
			if (speaker_entries[i].enabled) {
#ifdef AUDIO_STREAM_DEBUG
				printf("[callback] emitting note @ %.2f Hz\n", speaker_entries[i].freq);
#endif
				freq_samples_fixed = (int) ((audio_freq << 8) / (speaker_entries[i].freq * 2));
				pos_samples_fixed = (speaker_freq_ctr << 8) % (freq_samples_fixed << 1);
				for (j = audio_from; j < audio_to; j++) {
					if ((pos_samples_fixed & 0xFFFFFF00) < (freq_samples_fixed & 0xFFFFFF00))
						stream[j] = 128+audio_volume;
					else
						stream[j] = 128-audio_volume;
					pos_samples_fixed = (pos_samples_fixed + 256) % (freq_samples_fixed << 1);
				}
				speaker_freq_ctr += audio_to - audio_from;
			} else {
				speaker_freq_ctr = 0;
				memset(stream + audio_from, 128, audio_to - audio_from);
			}
		}
	}

	if (speaker_entry_pos > 0) {
		k = i;
		if (k >= speaker_entry_pos) k = speaker_entry_pos - 1;
		for (i = k; i < speaker_entry_pos; i++) {
			speaker_entries[i - k] = speaker_entries[i];
		}
		speaker_entry_pos -= k;
		speaker_entries[0].ms = audio_curr_time;
	}

	audio_prev_time = audio_curr_time;
}

void audio_stream_append_on(long time, double freq) {
	if (speaker_entry_pos >= SPEAKER_ENTRY_LEN) {
		fprintf(stderr, "speaker buffer overrun");
		return;
	}

	speaker_entries[speaker_entry_pos].ms = time;
	speaker_entries[speaker_entry_pos].freq = freq;

	if (speaker_entry_pos > 0) {
		// ZZT always immediately disables a note... except for drums!
		u8 last_en = speaker_entries[speaker_entry_pos - 1].enabled;
		double last_ms = speaker_entries[speaker_entry_pos - 1].ms + audio_delay_time;
		if (last_en && last_ms >= time) {
			speaker_entries[speaker_entry_pos].ms = last_ms;
		}
	}

	speaker_entries[speaker_entry_pos++].enabled = 1;
#ifdef AUDIO_STREAM_DEBUG
	audio_stream_print(speaker_entry_pos - 1);
#endif
}

void audio_stream_append_off(long time) {
	if (speaker_entry_pos >= SPEAKER_ENTRY_LEN) {
		fprintf(stderr, "speaker buffer overrun");
		return;
	}

	if (time < audio_prev_time)
		time = audio_prev_time;

	speaker_entries[speaker_entry_pos].ms = time;

	if (speaker_entry_pos > 0) {
		// we let notes play for at least the delay time
		u8 last_en = speaker_entries[speaker_entry_pos - 1].enabled;
		double last_ms = speaker_entries[speaker_entry_pos - 1].ms + audio_delay_time;
		if (last_en && last_ms >= time) {
			speaker_entries[speaker_entry_pos].ms = last_ms;
		}
	}

	speaker_entries[speaker_entry_pos++].enabled = 0;
#ifdef AUDIO_STREAM_DEBUG
	audio_stream_print(speaker_entry_pos - 1);
#endif
}
