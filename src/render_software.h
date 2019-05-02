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

#ifndef __RENDER_SOFTWARE_H__
#define __RENDER_SOFTWARE_H__

#include "types.h"

#define RENDER_BLINK_OFF 1
#define RENDER_BLINK_PHASE 2

USER_FUNCTION
void render_software_rgb(u32 *buffer, int scr_width, int flags, u8 *ram, u8 *charset, int char_width, int char_height, u32 *palette);
USER_FUNCTION
void render_software_paletted(u8 *buffer, int scr_width, int flags, u8 *ram, u8 *charset, int char_width, int char_height);

#endif /* __RENDER_SOFTWARE_H__ */
