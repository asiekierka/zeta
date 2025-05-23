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
#include "types.h"

#define PALETTE_COLOR_COUNT 16
#define MAX_MEMORY_KBS 736
#define TEXT_ADDR(x,y) (0xB8000 + ((y)*160) + ((x)*2))

extern bool developer_mode;

typedef enum {
	DEFAULT_CHARSET_STYLE_EGA,
	DEFAULT_CHARSET_STYLE_CGA
} zzt_default_charset_style_t;

typedef enum {
	BLINK_OVERRIDE_OFF,
	BLINK_OVERRIDE_FREEZE, // blink off
	BLINK_OVERRIDE_ENABLE, // blink on
	BLINK_OVERRIDE_DISABLE // high colors
} zzt_blink_override_t;

#define ZZT_KMOD_RSHIFT 0x01
#define ZZT_KMOD_LSHIFT 0x02
#define ZZT_KMOD_CTRL   0x04
#define ZZT_KMOD_ALT    0x08

USER_FUNCTION
int zzt_video_mode(void);
USER_FUNCTION
void zzt_key(int key_ch, int key_sc);
USER_FUNCTION
void zzt_keyup(int key_sc);
USER_FUNCTION
int zzt_kmod_get(void);
USER_FUNCTION
void zzt_kmod_set(int mask);
USER_FUNCTION
void zzt_kmod_clear(int mask);
USER_FUNCTION
void zzt_set_lock_charset(bool value);
USER_FUNCTION
void zzt_set_lock_palette(bool value);
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
int zzt_get_blink_duration_ms(void);
USER_FUNCTION
void zzt_set_blink_duration_ms(int value);
USER_FUNCTION
int zzt_get_active_blink_duration_ms(void);
USER_FUNCTION
int zzt_get_blink_user_override(void);
USER_FUNCTION
void zzt_set_blink_user_override(int value);
USER_FUNCTION
void zzt_key_set_delay(int ms, int repeat_ms);
USER_FUNCTION
void zzt_set_timer_offset(long ms);
USER_FUNCTION
void zzt_set_max_extended_memory(int kilobytes);
USER_FUNCTION
double zzt_get_pit_tick_ms(void);

typedef struct {
	u8 chr;
	u8 key;
	bool found;
} zzt_key_t;

USER_FUNCTION
zzt_key_t zzt_key_pop(void);
USER_FUNCTION
u32 zzt_get_ip(void);
USER_FUNCTION
int zzt_get_cycles(void);

USER_FUNCTION
void zzt_get_screen_size(int *width, int *height);
USER_FUNCTION
u8 *zzt_get_charset(int *width, int *height);
USER_FUNCTION
bool zzt_get_charset_default(void);
USER_FUNCTION
u32 *zzt_get_palette(void);
USER_FUNCTION
int zzt_get_blink(void);

USER_FUNCTION
int zzt_load_charset(int width, int height, u8* data, bool is_default);
USER_FUNCTION
void zzt_force_default_charset(zzt_default_charset_style_t style);
USER_FUNCTION
int zzt_load_palette(u32* colors);
USER_FUNCTION
int zzt_load_palette_default(void);
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
int vfs_truncate(int handle);
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
void zeta_show_developer_warning(const char *format, ...);
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
