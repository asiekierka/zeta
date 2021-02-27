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
#include "types.h"
#include "util.h"
#include "audio_shared.h"
#include "audio_writer.h"

#define AUDIO_MIN_SAMPLE (25 << 8)
#define AUDIO_MAX_SAMPLE (231 << 8)
#define AUDIO_WRITER_MIN_ENTRIES_SIZE 4096

typedef struct s_audio_writer_state {
	FILE *file;
	u8 bits_per_sample;
	char *vbuf;

	double time_offset;
	int freq;

	speaker_entry *entries;
	u32 entries_count;
	u32 entries_size;
	int note_counter;
	u8 note_enabled; // last
	int note_cycles; // last
	double note_freq; // last
} audio_writer_state;

audio_writer_state *audio_writer_start(const char *filename, long time, int freq) {
	FILE *file = fopen(filename, "wb");
	if (file == NULL) return NULL;

	audio_writer_state *s = malloc(sizeof(audio_writer_state));
	memset(s, 0, sizeof(audio_writer_state));

	s->file = file;
	setvbuf(s->file, s->vbuf, _IOFBF, 65536);

	s->time_offset = time;
	s->freq = freq;
	s->bits_per_sample = 16;

	// write header
	fwrite("RIFF", 4, 1, s->file);
	fput32le(s->file, 0); // file size - 8, see audio_writer_stop
	fwrite("WAVE", 4, 1, s->file);
	// - fmt
	fwrite("fmt ", 4, 1, s->file);
	fput32le(s->file, 16); // chunk size
	fput16le(s->file, 1); // format (PCM)
	fput16le(s->file, 1); // channels (1)
	fput32le(s->file, s->freq); // frequency
	fput32le(s->file, s->freq); // bytes per second
	fput16le(s->file, 1); // bytes per full sample
	fput16le(s->file, s->bits_per_sample); // bits per sample
	// - data
	fwrite("data", 4, 1, s->file);
	fput32le(s->file, 0); // file size - 44, see audio_writer_stop

	return s;
}

static void audio_writer_advance(audio_writer_state *s, int pos, long time, int cycles, u8 enabled, double freq) {
	long i, samples;
	int freq_samples_fixed, pos_samples_fixed;

	if (audio_should_insert_pause(s->entries, pos)) {
		long shortest_time = s->time_offset + audio_local_delay_time(s->note_cycles, cycles, s->freq);
		if (time < shortest_time) {
			time = shortest_time;
		}
	}
	// write [s->time_offset..time] with audio_note values
	// rounded, as we adjust s->time_offset based on this
	samples = (long) (((time - s->time_offset) * s->freq / 1000.0) + 0.5);
	if (s->note_enabled) {
		// see audio_stream.c
		freq_samples_fixed = (int) ((s->freq << 8) / (s->note_freq * 2));
		pos_samples_fixed = (s->note_counter << 8);
		for (i = 0; i < samples; i++) {
			u16 sample = audio_generate_sample(AUDIO_MIN_SAMPLE, AUDIO_MAX_SAMPLE, freq_samples_fixed, s->note_freq, pos_samples_fixed, s->freq);
			sample >>= (16 - s->bits_per_sample);
			if (s->bits_per_sample == 16) {
				fput16le(s->file, sample ^ 0x8000);
			} else {
				fputc(sample, s->file);
			}
			pos_samples_fixed += (1 << 8);
		}
	} else {
		// write silence
		for (i = 0; i < samples; i++) {
			if (s->bits_per_sample == 16) {
				fput16le(s->file, 0);
			} else {
				fputc(128, s->file);
			}
		}
	}
	// set new note as last
	// audio_time_offset = time;
	s->time_offset += (samples * 1000.0 / s->freq);
	s->note_enabled = enabled;
	s->note_freq = freq;
	s->note_cycles = cycles;
	if (!enabled) s->note_counter = 0;
	else s->note_counter += samples;
}

void audio_writer_stop(audio_writer_state *s, long time, int cycles) {
	speaker_entry *e;
	audio_writer_speaker_off(s, time, cycles);
	// write... uhh... all data
	e = s->entries;
	for (int i = 0; i < s->entries_count; i++, e++) {
		audio_writer_advance(s, i, e->ms, e->cycles, e->enabled, e->freq);
	}
	// get filesize
	fseek(s->file, 0, SEEK_END);
	int filesize = (int) ftell(s->file);
	// fix WAV sizes
	fseek(s->file, 4, SEEK_SET);
	fput32le(s->file, filesize - 8);
	fseek(s->file, 40, SEEK_SET);
	fput32le(s->file, filesize - 44);
	// clean up
	fclose(s->file);
	free(s->vbuf);
	free(s->entries);
	free(s);
}

static speaker_entry* audio_writer_allocate_entry(audio_writer_state *s) {
	if (s->entries_count >= s->entries_size) {
		if (s->entries_size == 0) {
			s->entries_size = AUDIO_WRITER_MIN_ENTRIES_SIZE;
			s->entries = malloc(sizeof(speaker_entry) * s->entries_size);
		} else {
			s->entries_size *= 2;
			s->entries = realloc(s->entries, sizeof(speaker_entry) * s->entries_size);
		}
	}
	return &(s->entries[s->entries_count++]);
}

void audio_writer_speaker_on(audio_writer_state *s, long time, int cycles, double freq) {
	speaker_entry *e = audio_writer_allocate_entry(s);
	e->enabled = 1;
	e->freq = freq;
	e->ms = time;
	e->cycles = cycles;
}

void audio_writer_speaker_off(audio_writer_state *s, long time, int cycles) {
	speaker_entry *e = audio_writer_allocate_entry(s);
	e->enabled = 0;
	e->ms = time;
	e->cycles = cycles;
}
