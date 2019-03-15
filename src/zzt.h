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

#ifndef __ZZT_H__
#define __ZZT_H__

#include "cpu.h"

#define MAX_FILES 16
#define TEXT_ADDR(x,y) (0xB8000 + ((y)*160) + ((x)*2))
#define SYS_TIMER_TIME 54.9451

USER_FUNCTION
int zzt_video_mode(void);
USER_FUNCTION
void zzt_key(int ch, int key);
USER_FUNCTION
void zzt_keyup(int key);
USER_FUNCTION
void zzt_kmod_set(int mod);
USER_FUNCTION
void zzt_kmod_clear(int mod);
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
void zzt_init();
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
int zzt_key_get_delay();
USER_FUNCTION
int zzt_key_get_repeat_delay();
USER_FUNCTION
void zzt_key_set_delay(int ms, int repeat_ms);

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

#define FEATURE_JOY_CONNECTED 1
#define FEATURE_MOUSE_CONNECTED 2
IMPLEMENT_FUNCTION
int zeta_has_feature(int feature);

IMPLEMENT_FUNCTION
void speaker_on(double freq);
IMPLEMENT_FUNCTION
void speaker_off(void);

#endif /* __ZZT_H__ */
