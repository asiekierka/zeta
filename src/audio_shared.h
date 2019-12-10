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

USER_FUNCTION
double audio_get_note_delay();
USER_FUNCTION
void audio_set_note_delay(double delay);
USER_FUNCTION
double audio_local_delay_time(int cycles_prev, int cycles_curr, int global_frequency);
USER_FUNCTION
u8 audio_should_insert_pause(speaker_entry* entries, int pos);

#endif
