/**
 * Copyright (c) 2026 Adrian Siekierka
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

#include <SDL3/SDL.h>
#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_mutex.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef enum {
    CWD_STATE_RUNNING,
    CWD_STATE_SELECTED,
    CWD_STATE_EXIT
} cwd_state_t;

static atomic_int cwd_state;

static void cwd_prompt_callback(void *userdata, const char * const *filelist, int filter) {
    (void) userdata;
    (void) filter;

    if (filelist && *filelist) {
        chdir(*filelist);
        cwd_state = CWD_STATE_SELECTED;
    } else {
        if (!filelist) {
            fprintf(stderr, "Error opening dialog: %s\n", SDL_GetError());
        }
        cwd_state = CWD_STATE_EXIT;
    }
}

void prompt_user_cwd(void) {
    cwd_state = CWD_STATE_RUNNING;

    SDL_ShowOpenFolderDialog(cwd_prompt_callback, NULL,
        NULL, NULL, false);

    while (cwd_state == CWD_STATE_RUNNING) {
        SDL_Event event;
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
                case SDL_EVENT_QUIT:
                    cwd_state = CWD_STATE_EXIT;
                    break;
            }
        }
        SDL_Delay(50);
    }

    if (cwd_state == CWD_STATE_EXIT) {
        SDL_Quit();
        exit(0);
    }
}