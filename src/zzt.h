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

#ifndef __ZZT_H__
#define __ZZT_H__

#include "cpu.h"

#define PALETTE_COLOR_COUNT 16
#define MAX_MEMORY_KBS 736
#define MAX_FILES 16
#define TEXT_ADDR(x,y) (0xB8000 + ((y)*160) + ((x)*2))

// the long story:
// 3.579545 MHz - NTSC dotclock
// dotclock * 4 = 14.31818
// 14.31818 / 12 ~= 1.19318166 - PIT frequency
// 65535 - maximum PIT cycle count before reload
// (65535 / 1193181.66) = SYS_TIMER_TIME (seconds)
#define SYS_TIMER_TIME 54.92457871

USER_FUNCTION
int zzt_video_mode(void);
USER_FUNCTION
void zzt_key(int key_ch, int key_sc);
USER_FUNCTION
void zzt_keyup(int key_sc);
USER_FUNCTION
void zzt_kmod_set(int mask);
USER_FUNCTION
void zzt_kmod_clear(int mask);
USER_FUNCTION
void zzt_joy_set(int button);
USER_FUNCTION
void zzt_joy_axis(int axis, int value /* -127..127 */);
USER_FUNCTION
void zzt_joy_clear(int button);
USER_FUNCTION
void zzt_mouse_set(int button);
USER_FUNCTION
void zzt_mouse_axis(int axis, int value /* delta, in pixels */);
USER_FUNCTION
void zzt_mouse_clear(int button);
USER_FUNCTION
void zzt_init(int memory_kbs);
USER_FUNCTION
void zzt_load_binary(int handle, const char *arg);
USER_FUNCTION
int zzt_execute(int opcodes);
USER_FUNCTION
u8* zzt_get_ram(void);
USER_FUNCTION
void zzt_mark_frame(void);
USER_FUNCTION
void zzt_mark_timer(void);
USER_FUNCTION
void zzt_mark_timer_turbo(void);
USER_FUNCTION
int zzt_key_get_delay(void);
USER_FUNCTION
int zzt_key_get_repeat_delay(void);
USER_FUNCTION
void zzt_key_set_delay(int ms, int repeat_ms);
USER_FUNCTION
void zzt_set_timer_offset(long ms);
USER_FUNCTION
void zzt_set_max_extended_memory(int kilobytes);

USER_FUNCTION
int zzt_load_charset(int width, int height, u8* data);
USER_FUNCTION
int zzt_load_palette(u32* colors);
USER_FUNCTION
int zzt_load_ega_palette(u8* colors);
USER_FUNCTION
int zzt_load_blink(int blink);

IMPLEMENT_FUNCTION
long zeta_time_ms(void);

#define VFS_SEEK_SET 0
#define VFS_SEEK_CUR 1
#define VFS_SEEK_END 2
#define VFS_OPEN_READ 0
#define VFS_OPEN_WRITE 1
#define VFS_OPEN_RW 2
#define VFS_OPEN_TRUNCATE 0x10000

IMPLEMENT_FUNCTION
int vfs_open(const char* filename, int mode);
IMPLEMENT_FUNCTION
int vfs_seek(int handle, int pos, int type);
IMPLEMENT_FUNCTION
int vfs_read(int handle, u8* ptr, int amount);
IMPLEMENT_FUNCTION
int vfs_write(int handle, u8* ptr, int amount);
IMPLEMENT_FUNCTION
int vfs_close(int handle);
IMPLEMENT_FUNCTION
int vfs_findfirst(u8* ptr, u16 mask, char* spec);
IMPLEMENT_FUNCTION
int vfs_findnext(u8* ptr);
IMPLEMENT_FUNCTION
int vfs_getcwd(char *ptr, int size);
IMPLEMENT_FUNCTION
int vfs_chdir(const char *path);

#define FEATURE_JOY_CONNECTED 1
#define FEATURE_MOUSE_CONNECTED 2
IMPLEMENT_FUNCTION
int zeta_has_feature(int feature);

IMPLEMENT_FUNCTION
void zeta_update_charset(int width, int height, u8* data);
IMPLEMENT_FUNCTION
void zeta_update_palette(u32* colors);
IMPLEMENT_FUNCTION
void zeta_update_blink(int blink);

IMPLEMENT_FUNCTION
void speaker_on(int cycle, double freq);
IMPLEMENT_FUNCTION
void speaker_off(int cycle);

#endif /* __ZZT_H__ */
