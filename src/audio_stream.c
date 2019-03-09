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

typedef struct {
	u8 enabled;
	double freq;
	long ms;
} speaker_entry;

#define AUDIO_VOLUME_MAX 127
#define SPEAKER_ENTRY_LEN 64
static int speaker_entry_pos = 0;
static speaker_entry speaker_entries[SPEAKER_ENTRY_LEN];
static long speaker_freq_ctr = 0;
static u8 audio_volume = AUDIO_VOLUME_MAX;
static long audio_prev_time;
static int audio_freq;

void audio_stream_init(long time, int freq) {
	audio_prev_time = time;
	audio_freq = freq;
}

u8 audio_stream_get_max_volume() {
	return AUDIO_VOLUME_MAX;
}

void audio_stream_set_volume(u8 volume) {
	if (volume < 0) volume = 0;
	else if (volume > AUDIO_VOLUME_MAX) volume = AUDIO_VOLUME_MAX;
	audio_volume = volume;
}

void audio_stream_generate_u8(long time, u8 *stream, int len) {
	int i; long j;
	float freq_samples;
	int k;
	double audio_curr_time = audio_prev_time + (len / (double) audio_freq * 1000);
//	double audio_curr_time = time;
	long audio_res = (audio_curr_time - audio_prev_time);
	float res_to_samples = len / (float) audio_res;
	long audio_from, audio_to;

	if (audio_curr_time < time) {
		audio_curr_time = time;
	}

//	fprintf(stderr, "audiogen time %.2f realtime %ld drift %.2f\n", audio_curr_time, time, time - audio_curr_time);

	if (speaker_entry_pos == 0) {
		memset(stream, 128, len);
	} else for (i = 0; i < speaker_entry_pos; i++) {
		audio_from = speaker_entries[i].ms - audio_prev_time;

		if (i == speaker_entry_pos - 1) audio_to = audio_res;
		else audio_to = speaker_entries[i+1].ms - audio_prev_time;

		// convert
		audio_from = (long) (audio_from * res_to_samples);
		audio_to = (long) (audio_to * res_to_samples);

		if (audio_from < 0) audio_from = 0;
		else if (audio_from >= len) break;
		if (audio_to < 0) audio_to = 0;
		else if (audio_to > len) audio_to = len;

		// emit
		if (audio_to > audio_from) {
			if (speaker_entries[i].enabled) {
				freq_samples = (float) (audio_freq / (speaker_entries[i].freq * 2));
				for (j = audio_from; j < audio_to; j++) {
					stream[j] = (((long) ((speaker_freq_ctr + j - audio_from) / freq_samples)) & 1) ? (128-audio_volume) : (128+audio_volume);
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
	speaker_entries[speaker_entry_pos++].enabled = 1;
}

void audio_stream_append_off(long time) {
	if (speaker_entry_pos >= SPEAKER_ENTRY_LEN) {
		fprintf(stderr, "speaker buffer overrun");
		return;
	}

	// prevent very short notes from being culled
	if (speaker_entry_pos > 0) {
		int last_en = speaker_entries[speaker_entry_pos - 1].enabled;
		long last_ms = speaker_entries[speaker_entry_pos - 1].ms;
		if (last_en && last_ms == time) {
			time++;
		}
	}

	speaker_entries[speaker_entry_pos].ms = time;
	speaker_entries[speaker_entry_pos++].enabled = 0;
}
