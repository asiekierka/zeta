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

#ifndef __AUDIO_WRITER_H__
#define __AUDIO_WRITER_H__

#include "types.h"
#include "audio_stream.h"

int audio_writer_start(const char *filename, long time, int freq);
void audio_writer_stop(long time);
void audio_writer_speaker_on(long time, double freq);
void audio_writer_speaker_off(long time);

#endif
