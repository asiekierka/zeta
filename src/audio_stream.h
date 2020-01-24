/**
 * Copyright (c) 2018, 2019, 2020 Adrian Siekierka
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

#ifndef __AUDIO_STREAM_H__
#define __AUDIO_STREAM_H__

#include "types.h"

USER_FUNCTION
void audio_stream_init(long time, int freq);
USER_FUNCTION
u8 audio_stream_get_volume();
USER_FUNCTION
u8 audio_stream_get_max_volume();
USER_FUNCTION
void audio_stream_set_volume(u8 volume);
USER_FUNCTION
void audio_stream_generate_u8(long time, u8 *stream, int len);
USER_FUNCTION
void audio_stream_append_on(long time, int cycles, double freq);
USER_FUNCTION
void audio_stream_append_off(long time, int cycles);

#endif
