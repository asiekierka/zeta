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

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "audio_shared.h"
#include "audio_stream.h"
#include "ui.h"
#include "zzt.h"

typedef struct {
    uint8_t screen_backup[80 * 25 * 2];
    bool screen_redraw;
    int option_y;
} ui_state_t;

#ifdef AVOID_MALLOC
static ui_state_t ui_state_prealloc;
#endif

static ui_state_t *ui_state = NULL;

#ifdef __EMSCRIPTEN__
static const char* ui_lines_header[] = {
    "zeta " VERSION,
};
#define UI_LINES_HEADER_COUNT 1
#else
static const char* ui_lines_header[] = {
    "zeta " VERSION,
    "",
    //2345678901234567890123456789012345
    "CTRL-F5    .GIF recording toggle",
    "CTRL-F6    .WAV recording toggle",
    "F9         turbo (hold)         ",
    "F12        take screenshot      ",
    "CTRL-+/-   window resize up/down",
    "ALT-ENTER  fullscreen toggle    ",
};
#define UI_LINES_HEADER_COUNT 8
#endif

static const char* ui_lines_options[] = {
    "  Volume:      ",
    "  Player step sound: [ ]",
    "  Exit"
};
#define UI_LINES_OPTIONS_COUNT 3

void ui_activate(void) {
#ifdef AVOID_MALLOC
    ui_state = &ui_state_prealloc;
#else
    ui_state = malloc(sizeof(ui_state_t));
#endif
    ui_state->screen_redraw = true;
    ui_state->option_y = 0;

    memcpy(ui_state->screen_backup, zzt_get_ram() + 0xB8000, 80 * 25 * 2);
    speaker_off(zzt_get_cycles());
}

static void ui_deactivate(void) {
    if (!ui_is_active()) return;

    memcpy(zzt_get_ram() + 0xB8000, ui_state->screen_backup, 80 * 25 * 2);
#ifndef AVOID_MALLOC
    free(ui_state);
#endif
    ui_state = NULL;
}

bool ui_is_active(void) {
    return ui_state != NULL;
}

static void ui_draw_char(int x, int y, uint8_t chr, uint8_t col) {
    if (x < 0 || y < 0 || x >= 80 || y >= 25) return;

    uint8_t *vid_mem = zzt_get_ram() + 0xB8000;
    vid_mem[y * 160 + x * 2] = chr;
    vid_mem[y * 160 + x * 2 + 1] = col;
}

static void ui_draw_string(int x, int y, const char *s, uint8_t col) {
    while (*s != '\0') {
        ui_draw_char(x++, y, *(s++), col);
    }
}

static void ui_draw_check(int x, int y, const char *s, bool value, uint8_t col) {
    if (value) {
        ui_draw_char(x + strlen(s) - 2, y, 'x', col);
    }
}

void ui_tick(void) {
    char sbuf[37];

    int wwidth = 36, wheight = UI_LINES_HEADER_COUNT + UI_LINES_OPTIONS_COUNT + 3;
    int swidth, sheight;
    zzt_get_screen_size(&swidth, &sheight);
    int wx = (swidth - wwidth) >> 1;
    int wy = (sheight - wheight) >> 1;

    if (ui_state->screen_redraw) {
        // clear area
        for (int iy = 0; iy < wheight; iy++) {
            for (int ix = 0; ix < wwidth; ix++) {
                ui_draw_char(wx+ix, wy+iy, 0, 0x10);
            }
        }

        // draw header lines
        for (int iy = 0; iy < UI_LINES_HEADER_COUNT; iy++) {
            const char *is = ui_lines_header[iy];
            ui_draw_string((swidth - strlen(is)) >> 1, wy + 1 + iy, is, iy == 0 ? 0x1E : 0x1F);
        }

        ui_state->screen_redraw = false;
    }

    {
        int woptx = wx + 2, wopty = wy + 2 + UI_LINES_HEADER_COUNT;

        // draw options lines
        for (int iy = 0; iy < UI_LINES_OPTIONS_COUNT; iy++) {
            const char *is = ui_lines_options[iy];
            ui_draw_string(woptx, wopty + iy, is, 0x1E);
        }

        // draw options values
        snprintf(sbuf, sizeof(sbuf) - 1, "%d%%", audio_stream_get_volume() * 10 / 6);
        ui_draw_string(woptx + 10, wopty, sbuf, 0x1F);
        ui_draw_check(woptx, wopty + 1, ui_lines_options[1], !audio_get_remove_player_movement_sound(), 0x1F);

        // draw arrow
        ui_draw_char(woptx, wopty + ui_state->option_y, '>', 0x1F);
    }

    zzt_key_t key = zzt_key_pop();
    if (key.found) {
        if (key.chr == 27) {
            // ESC
            ui_deactivate();
            return;
        } else if (key.chr == 13 || key.chr == 77) {
            if (ui_state->option_y == 0) {
                if (audio_stream_get_volume() > 54) {
                    audio_stream_set_volume(0);
                } else {
                    audio_stream_set_volume(audio_stream_get_volume() + 6);
                }
            } else if (ui_state->option_y == 1) {
                audio_set_remove_player_movement_sound(!audio_get_remove_player_movement_sound());
            }
        } else if (key.key == 72) {
            if (ui_state->option_y > 0) {
                ui_state->option_y--;
            }
        } else if (key.key == 80) {
            if (ui_state->option_y < UI_LINES_OPTIONS_COUNT - 1) {
                ui_state->option_y++;
            }
        }
    }
}
