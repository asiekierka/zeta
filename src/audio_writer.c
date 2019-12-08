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
#include "types.h"
#include "util.h"
#include "audio_shared.h"
#include "audio_writer.h"

#define AUDIO_MIN_SAMPLE 25
#define AUDIO_MAX_SAMPLE 231

static FILE *audio_file;
static char *audio_vbuf;
static double audio_time_offset;
static int audio_freq;

// states
typedef struct {
	u8 enabled;
	int cycles;
	double freq;
	long ms;
} audio_writer_entry;

static audio_writer_entry* audio_entries;
static u32 audio_entries_size, audio_entries_count;
static int audio_note_counter;
static u8 audio_note_enabled; // last
static int audio_note_cycles; // last
static double audio_note_freq; // last

static audio_writer_entry* audio_writer_allocate_entry(void) {
	if (audio_entries_count >= audio_entries_size) {
		if (audio_entries_size == 0) {
			audio_entries_size = 4096;
			audio_entries = malloc(sizeof(audio_writer_entry) * audio_entries_size);
		} else {
			audio_entries_size *= 2;
			audio_entries = realloc(audio_entries, sizeof(audio_writer_entry) * audio_entries_size);
		}
	}

	return &(audio_entries[audio_entries_count++]);
}

int audio_writer_start(const char *filename, long time, int freq) {
	if (audio_file != NULL) return -1;
	audio_file = fopen(filename, "wb");
	if (audio_file == NULL) return -1;
	setvbuf(audio_file, audio_vbuf, _IOFBF, 65536);
	// clean up fields
	audio_entries = NULL;
	audio_entries_size = 0;
	audio_entries_count = 0;
	audio_note_counter = 0;
	audio_note_enabled = 0;
	audio_note_freq = 0.0;
	audio_note_cycles = 0;
	audio_time_offset = time;
	audio_freq = freq;
	// write header
	fwrite("RIFF", 4, 1, audio_file);
	fput32le(audio_file, 0); // file size - 8, see audio_writer_stop
	fwrite("WAVE", 4, 1, audio_file);
	// - fmt
	fwrite("fmt ", 4, 1, audio_file);
	fput32le(audio_file, 16); // chunk size
	fput16le(audio_file, 1); // format (PCM)
	fput16le(audio_file, 1); // channels (1)
	fput32le(audio_file, freq); // frequency
	fput32le(audio_file, freq); // bytes per second
	fput16le(audio_file, 1); // bytes per full sample
	fput16le(audio_file, 8); // bits per sample
	// - data
	fwrite("data", 4, 1, audio_file);
	fput32le(audio_file, 0); // file size - 44, see audio_writer_stop
	return 0;
}

static void audio_writer_advance(long time, int cycles, u8 enabled, double freq) {
	long i, samples;
	int freq_samples_fixed, pos_samples_fixed;

	if (audio_note_enabled) {
		long shortest_time = audio_time_offset + audio_local_delay_time(audio_note_cycles, cycles, audio_freq);
		if (time < shortest_time) {
			time = shortest_time;
		}
	}
	// write [audio_time_offset..time] with audio_note values
	// rounded, as we adjust audio_time_offset based on this
	samples = (long) (((time - audio_time_offset) * audio_freq / 1000.0) + 0.5);
	if (audio_note_enabled) {
		// see audio_stream.c
		freq_samples_fixed = (int) ((audio_freq << 8) / (audio_note_freq * 2));
		pos_samples_fixed = (audio_note_counter << 8) % (freq_samples_fixed << 1);
		for (i = 0; i < samples; i++) {
			if ((pos_samples_fixed & 0xFFFFFF00) < (freq_samples_fixed & 0xFFFFFF00))
				fputc(AUDIO_MAX_SAMPLE, audio_file);
			else
				fputc(AUDIO_MIN_SAMPLE, audio_file);
			pos_samples_fixed = (pos_samples_fixed + 256) % (freq_samples_fixed << 1);
		}
	} else {
		// write silence
		for (i = 0; i < samples; i++)
			fputc(128, audio_file);
	}
	// set new note as last
	// audio_time_offset = time;
	audio_time_offset += (samples * 1000.0 / audio_freq);
	audio_note_enabled = enabled;
	audio_note_freq = freq;
	audio_note_cycles = cycles;
	if (!enabled) audio_note_counter = 0;
	else audio_note_counter += samples;
}

void audio_writer_stop(long time, int cycles) {
	audio_writer_entry *e;
	audio_writer_speaker_off(time, cycles);
	// write... uhh... all data
	e = audio_entries;
	for (int i = 0; i < audio_entries_count; i++, e++) {
		audio_writer_advance(e->ms, e->cycles, e->enabled, e->freq);
	}
	// get filesize
	fseek(audio_file, 0, SEEK_END);
	int filesize = (int) ftell(audio_file);
	// fix WAV sizes
	fseek(audio_file, 4, SEEK_SET);
	fput32le(audio_file, filesize - 8);
	fseek(audio_file, 40, SEEK_SET);
	fput32le(audio_file, filesize - 44);
	// clean up
	fclose(audio_file);
	audio_file = NULL;
	free(audio_vbuf);
	audio_vbuf = NULL;
	free(audio_entries);
	audio_entries = NULL;
	audio_entries_size = 0;
	audio_entries_count = 0;
}

void audio_writer_speaker_on(long time, int cycles, double freq) {
	audio_writer_entry *e = audio_writer_allocate_entry();
	e->enabled = 1;
	e->freq = freq;
	e->ms = time;
	e->cycles = cycles;
}

void audio_writer_speaker_off(long time, int cycles) {
	audio_writer_entry *e = audio_writer_allocate_entry();
	e->enabled = 0;
	e->ms = time;
	e->cycles = cycles;
}
