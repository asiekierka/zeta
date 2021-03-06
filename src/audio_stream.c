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
#include "audio_shared.h"
#include "audio_stream.h"
#include "logging.h"

#ifdef AVOID_MALLOC
#define AUDIO_STREAM_SPEAKER_ENTRIES_STATIC
#endif

#define AUDIO_VOLUME_MAX 127
static u8 speaker_overrun_flagged = 0;
static long speaker_freq_ctr = 0;
static u8 audio_volume = AUDIO_VOLUME_MAX;
static double audio_prev_time;
static int audio_freq;
static bool audio_signed;
static bool audio_16bit;

#define DEFAULT_SPEAKER_ENTRY_LEN 128
static int speaker_entry_pos = 0;
#ifdef AUDIO_STREAM_SPEAKER_ENTRIES_STATIC
#define SPEAKER_ENTRY_LEN DEFAULT_SPEAKER_ENTRY_LEN
static speaker_entry speaker_entries[SPEAKER_ENTRY_LEN];
#else
static speaker_entry *speaker_entries;
static int speaker_entry_len = 0;
#endif

// #define AUDIO_STREAM_DEBUG
// #define AUDIO_STREAM_DEBUG_BUFFERS

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

void audio_stream_init(long time, int freq, bool asigned, bool a16bit) {
	speaker_overrun_flagged = 0;
	audio_prev_time = -1;
	audio_freq = freq;
	audio_signed = asigned;
	audio_16bit = a16bit;
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

static inline void audio_stream_clear(u8 *stream, u16 audio_smp_center, long audio_from, long audio_to) {
	long j;
	u16* stream16 = (u16*) stream;
	if (audio_16bit) {
		for (j = audio_from; j < audio_to; j++) {
			stream16[j] = audio_smp_center;
		}
	} else {
		memset(stream + audio_from, audio_smp_center, audio_to - audio_from);
	}
}

void audio_stream_generate(long time, u8 *stream, int len) {
	int i; long j;
	int freq_samples_fixed;
	int pos_samples_fixed;
	int k, note_played;
	double audio_res;
	double audio_curr_time;
	double res_to_samples;
	double audio_dfrom, audio_dto;
	long audio_from, audio_to, audio_last_to = -(1 << 30);
	u16* stream16 = (u16*) stream;

	u16 audio_smp_min = (128 - audio_volume);
	u16 audio_smp_max = (128 + audio_volume);
	u16 audio_smp_center = audio_signed ? 0 : 128;

	if (audio_16bit) {
		audio_smp_min <<= 8;
		audio_smp_max <<= 8;
		audio_smp_center <<= 8;
		len >>= 1;
	}

	audio_res = (len / (double) audio_freq * 1000);
	audio_curr_time = audio_prev_time + audio_res;
//	audio_curr_time = time;
	res_to_samples = len / audio_res;

	// handle the first
	if (audio_prev_time < 0) {
		audio_prev_time = time;
		audio_stream_clear(stream, audio_smp_center, 0, len);
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
		audio_stream_clear(stream, audio_smp_center, 0, len);
	} else {
		for (i = 0; i < speaker_entry_pos; i++) {
			// audio_dfrom/to = duration relative to the beginning of stream (ms)
			audio_dfrom = speaker_entries[i].ms - audio_prev_time;
			if (i == speaker_entry_pos - 1) {
				#ifdef AUDIO_STREAM_DEBUG
				fprintf(stderr, "[callback] guessing next sample length\n");
				#endif
				audio_dto = audio_res;
			} else audio_dto = speaker_entries[i+1].ms - audio_prev_time;

			// audio_from/to = duration relative to the beginning of stream (samples)
			audio_from = (long) (audio_dfrom * res_to_samples);
			audio_to = (long) (audio_dto * res_to_samples);

			#ifdef AUDIO_STREAM_DEBUG
			if (speaker_entries[i].enabled) {
				fprintf(stderr, "[callback] testing note @ %.2f Hz (%ld, %ld)\n", speaker_entries[i].freq, audio_from, audio_to);
			} else {
				fprintf(stderr, "[callback] testing off (%ld, %ld)\n", audio_from, audio_to);
			}
			#endif

			// Force minimum delay between notes.
			if (audio_from < audio_last_to) audio_from = audio_last_to;
			if (audio_to < (audio_from + MINIMUM_NOTE_DELAY)) audio_to = audio_from + MINIMUM_NOTE_DELAY;

                       // If first note and above 0, memset.
			if (i == 0 && audio_from > 0) {
				audio_stream_clear(stream, audio_smp_center, 0, audio_from < len ? audio_from : len);
			}

			// Clamp; if out of bounds, skip note.
			if (audio_from < 0) audio_from = 0;
			else if (audio_from >= len) break;
			if (audio_to < 0) {
				note_played = i;
				continue;
			}
			else if (audio_to > len) audio_to = len;

			// Emit the actual note.
			note_played = i;
			if (audio_to > audio_from) {
				if (speaker_entries[i].enabled) {
					#ifdef AUDIO_STREAM_DEBUG
					fprintf(stderr, "[callback] emitting note @ %.2f Hz (%ld, %ld)\n", speaker_entries[i].freq, audio_from, audio_to);
					#endif
					freq_samples_fixed = (int) ((audio_freq << 8) / (speaker_entries[i].freq * 2));
					pos_samples_fixed = (speaker_freq_ctr << 8);
					if (audio_signed) {
						if (audio_16bit) {
							for (j = audio_from; j < audio_to; j++) {
								stream16[j] = audio_generate_sample(audio_smp_min, audio_smp_max, freq_samples_fixed, speaker_entries[i].freq, pos_samples_fixed, audio_freq) ^ 0x8000;
								pos_samples_fixed += 256;
							}
						} else {
							for (j = audio_from; j < audio_to; j++) {
								stream[j] = audio_generate_sample(audio_smp_min, audio_smp_max, freq_samples_fixed, speaker_entries[i].freq, pos_samples_fixed, audio_freq) ^ 0x80;
								pos_samples_fixed += 256;
							}
						}
					} else {
						if (audio_16bit) {
							for (j = audio_from; j < audio_to; j++) {
								stream16[j] = audio_generate_sample(audio_smp_min, audio_smp_max, freq_samples_fixed, speaker_entries[i].freq, pos_samples_fixed, audio_freq);
								pos_samples_fixed += 256;
							}
						} else {
							for (j = audio_from; j < audio_to; j++) {
								stream[j] = audio_generate_sample(audio_smp_min, audio_smp_max, freq_samples_fixed, speaker_entries[i].freq, pos_samples_fixed, audio_freq);
								pos_samples_fixed += 256;
							}
						}
					}
					speaker_freq_ctr = (pos_samples_fixed >> 8);
				} else {
					#ifdef AUDIO_STREAM_DEBUG
					fprintf(stderr, "[callback] emitting off (%ld, %ld)\n", audio_from, audio_to);
					#endif
					speaker_freq_ctr = 0;
					audio_stream_clear(stream, audio_smp_center, audio_from, audio_to);
				}
			}

			if (audio_to >= len) break;
			audio_last_to = audio_to;
		}

		// Remove played notes.
		if (speaker_entry_pos > 0) {
			k = note_played;
			for (i = k; i < speaker_entry_pos; i++) {
				speaker_entries[i - k] = speaker_entries[i];
			}
			speaker_entry_pos -= k;
			if (speaker_entry_pos <= 1) {
				// If we only have one note, we can synchronize the
				// internal timer with the external timer.
				audio_curr_time = time;
			}
			if (speaker_entry_pos >= 1) {
				speaker_entries[0].ms = audio_curr_time;
			}
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

#ifndef AUDIO_STREAM_SPEAKER_ENTRIES_STATIC
static bool audio_stream_ensure_size(int len) {
	int old_speaker_len = speaker_entry_len;
	speaker_entry *old_speaker_entries = speaker_entries;

	if (speaker_entry_len == 0) {
		speaker_entry_len = DEFAULT_SPEAKER_ENTRY_LEN;
		speaker_entries = malloc(sizeof(speaker_entry) * speaker_entry_len);
	} else if (len > speaker_entry_len) {
		while (len > speaker_entry_len) {
			speaker_entry_len *= 2;
		}
		fprintf(stderr, "speaker buffer overrun! scaling (%d -> %d)\n", old_speaker_len, speaker_entry_len);
		speaker_entries = realloc(speaker_entries, sizeof(speaker_entry) * speaker_entry_len);
		if (speaker_entries == NULL) {
			speaker_entry_len = old_speaker_len;
			speaker_entries = old_speaker_entries;
			return false;
		}
	}

	return true;
}
#endif

void audio_stream_append_on(long time, int cycles, double freq) {
	// we want to reserve one extra speaker entry for an "off" command
	// otherwise, large on-off-on-off-on... cycles could end on an "on"
	// causing a permanent speaker noise
#ifdef AUDIO_STREAM_SPEAKER_ENTRIES_STATIC
	if (speaker_entry_pos >= (SPEAKER_ENTRY_LEN - 1)) {
		audio_stream_flag_speaker_overrun();
		return;
	}
#else
	if (!audio_stream_ensure_size(speaker_entry_pos + 2)) {
		audio_stream_flag_speaker_overrun();
		return;
	}
#endif

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
#ifdef AUDIO_STREAM_SPEAKER_ENTRIES_STATIC
	if (speaker_entry_pos >= SPEAKER_ENTRY_LEN) {
		audio_stream_flag_speaker_overrun();
		return;
	}
#else
	if (!audio_stream_ensure_size(speaker_entry_pos + 1)) {
		audio_stream_flag_speaker_overrun();
		return;
	}
#endif

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
