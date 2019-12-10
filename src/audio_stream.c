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
#include "audio_stream.h"
#include "logging.h"

// #define AUDIO_STREAM_DEBUG
// #define AUDIO_STREAM_DEBUG_BUFFERS

#define AUDIO_VOLUME_MAX 127
#define SPEAKER_ENTRY_LEN 64
static int speaker_entry_pos = 0;
static speaker_entry speaker_entries[SPEAKER_ENTRY_LEN];
static u8 speaker_overrun_flagged = 0;
static long speaker_freq_ctr = 0;
static u8 audio_volume = AUDIO_VOLUME_MAX;
static double audio_prev_time;
static int audio_freq;

#ifdef AUDIO_STREAM_DEBUG
static void audio_stream_print(int pos) {
	speaker_entry *e = &speaker_entries[pos];
	if (e->enabled) {
		fprintf(stderr, "[%.2f cpu: %d] speaker on @ %.2f Hz\n", e->ms, e->cycles, e->freq);
	} else {
		fprintf(stderr, "[%.2f cpu: %d] speaker off\n", e->ms, e->cycles);
	}
}
#endif

#ifdef AUDIO_STREAM_DEBUG_BUFFERS
static void audio_stream_print_all_entries(void) {
	for (int i = 0; i < speaker_entry_pos; i++) {
		speaker_entry *e = &speaker_entries[i];
		if (e->enabled) {
			fprintf(stderr, "buffer[%d]: %.2f ms / on @ %.2f Hz\n", i, e->ms, e->freq);
		} else {
			fprintf(stderr, "buffer[%d]: %.2f ms / off\n", i, e->ms);
		}
	}
}
#endif

static void audio_stream_flag_speaker_overrun(void) {
	if (!speaker_overrun_flagged) {
		fprintf(stderr, "speaker buffer overrun!\n");
		speaker_overrun_flagged = 1;
	}
}

void audio_stream_init(long time, int freq) {
	speaker_overrun_flagged = 0;
	audio_prev_time = -1;
	audio_freq = freq;
}

u8 audio_stream_get_volume() {
	return audio_volume;
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
	double audio_res = (len / (double) audio_freq * 1000);
	double audio_curr_time = audio_prev_time + audio_res;
//	double audio_curr_time = time;
	double res_to_samples = len / audio_res;
	double audio_dfrom, audio_dto;
	long audio_from, audio_to, audio_last_to = 0;

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
	fprintf(stderr, "[callback] expected time %.2f received time %ld drift %.2f buffer size %d\n", audio_curr_time, time, time - audio_curr_time, speaker_entry_pos);
#endif

#ifdef AUDIO_STREAM_DEBUG_BUFFERS
	fprintf(stderr, "[callback] buffer state BEFORE (%d)\n", speaker_entry_pos);
	audio_stream_print_all_entries();
#endif

	if (speaker_entry_pos == 0) {
		audio_curr_time = time;
		memset(stream, 128, len);
	} else {
		double last_ms = audio_prev_time;
		for (i = 0; i < speaker_entry_pos; i++) {
			// Skip notes played *after* the last note we've played.
			if (speaker_entries[i].ms >= audio_curr_time) break;
			// Skip notes played *before* the last note we've played.
			if (speaker_entries[i].ms < last_ms) continue;
			last_ms = speaker_entries[i].ms;

			// Adjust to (0..sample) range.
			audio_dfrom = speaker_entries[i].ms - audio_prev_time;
			if (i == speaker_entry_pos - 1) audio_dto = audio_res;
			else audio_dto = speaker_entries[i+1].ms - audio_prev_time;
			// Fill out gaps, if any, on the final note.
			// Adjust.
			audio_from = (long) (audio_dfrom * res_to_samples);
			audio_to = (long) (audio_dto * res_to_samples);

			#ifdef AUDIO_STREAM_DEBUG
			if (speaker_entries[i].enabled) {
				fprintf(stderr, "[callback] testing note @ %.2f Hz (%ld, %ld)\n", speaker_entries[i].freq, audio_from, audio_to);
			} else {
				fprintf(stderr, "[callback] testing off (%ld, %ld)\n", audio_from, audio_to);
			}
			#endif

			// Ensure the duration is at least 1 sample.
			if (audio_dfrom >= audio_dto) continue;
			if (audio_from < audio_last_to) audio_from = audio_last_to;
			if (audio_to <= (audio_from + MINIMUM_NOTE_DELAY - 1)) audio_to = audio_from + MINIMUM_NOTE_DELAY;

			// Filter rough edges, and end if we've gone past the buffer area (so we can remember the notes).
			if (audio_from < 0) audio_from = 0;
			else if (audio_from >= len) break;
			if (audio_to < 0) audio_to = 0;
			else if (audio_to > len) audio_to = len;

			// Emit the actual note.
			if (audio_to > audio_from) {
				if (speaker_entries[i].enabled) {
					#ifdef AUDIO_STREAM_DEBUG
					fprintf(stderr, "[callback] emitting note @ %.2f Hz (%ld, %ld)\n", speaker_entries[i].freq, audio_from, audio_to);
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
					#ifdef AUDIO_STREAM_DEBUG
					fprintf(stderr, "[callback] emitting off (%ld, %ld)\n", audio_from, audio_to);
					#endif
					speaker_freq_ctr = 0;
					memset(stream + audio_from, 128, audio_to - audio_from);
				}
			}

			audio_last_to = audio_to;
		}

		// Remove played notes.
		if (speaker_entry_pos > 0) {
			k = i - 1;
			for (i = k; i < speaker_entry_pos; i++) {
				speaker_entries[i - k] = speaker_entries[i];
			}
			speaker_entry_pos -= k;
			if (speaker_entry_pos == 1) {
				// If we only have one note, we can synchronize the
				// internal timer with the external timer.
				audio_curr_time = time;
			}
			speaker_entries[0].ms = audio_curr_time;
		}

		// Clear debug/error flags.
		speaker_overrun_flagged = 0;
	}

#ifdef AUDIO_STREAM_DEBUG_BUFFERS
	fprintf(stderr, "[callback] buffer state AFTER (%d)\n", speaker_entry_pos);
	audio_stream_print_all_entries();
#endif

	audio_prev_time = audio_curr_time;
}

static void audio_stream_adjust_entry_timing(long time, int cycles) {
	if (speaker_entry_pos > 0) {
		double last_ms = speaker_entries[speaker_entry_pos - 1].ms + audio_local_delay_time(speaker_entries[speaker_entry_pos - 1].cycles, cycles, audio_freq);
		if (audio_should_insert_pause(speaker_entries, speaker_entry_pos) && last_ms >= time) {
			speaker_entries[speaker_entry_pos].ms = last_ms;
		}
	}
}

void audio_stream_append_on(long time, int cycles, double freq) {
	// we want to reserve one extra speaker entry for an "off" command
	// otherwise, large on-off-on-off-on... cycles could end on an "on"
	// causing a permanent speaker noise
	if (speaker_entry_pos >= (SPEAKER_ENTRY_LEN - 1)) {
		audio_stream_flag_speaker_overrun();
		return;
	}

	speaker_entries[speaker_entry_pos].ms = time;
	speaker_entries[speaker_entry_pos].cycles = cycles;
	speaker_entries[speaker_entry_pos].freq = freq;
	speaker_entries[speaker_entry_pos].enabled = 1;
	audio_stream_adjust_entry_timing(time, cycles);

#ifdef AUDIO_STREAM_DEBUG
	audio_stream_print(speaker_entry_pos);
#endif
	speaker_entry_pos++;
}

void audio_stream_append_off(long time, int cycles) {
	if (speaker_entry_pos >= SPEAKER_ENTRY_LEN) {
		audio_stream_flag_speaker_overrun();
		return;
	}

	if (time < audio_prev_time)
		time = audio_prev_time;

	speaker_entries[speaker_entry_pos].ms = time;
	speaker_entries[speaker_entry_pos].cycles = cycles;
	speaker_entries[speaker_entry_pos].enabled = 0;
	audio_stream_adjust_entry_timing(time, cycles);

#ifdef AUDIO_STREAM_DEBUG
	audio_stream_print(speaker_entry_pos);
#endif
	speaker_entry_pos++;
}
