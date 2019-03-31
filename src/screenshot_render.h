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

#ifndef __SCREENSHOT_RENDER_H__
#define __SCREENSHOT_RENDER_H__

#include "types.h"

#define SCREENSHOT_TYPE_BMP 0

#define SCREENSHOT_BLINK_OFF 1
#define SCREENSHOT_BLINK_PHASE 2

USER_FUNCTION
int write_screenshot(FILE *output, int type, int scr_width, int flags, u8 *ram, u8 *charset, int char_width, int char_height, u32 *palette);

#endif /* __SCREENSHOT_RENDER_H__ */
