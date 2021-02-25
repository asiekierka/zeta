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

#ifndef __ZZT_EMS_H__
#define __ZZT_EMS_H__

#include "cpu.h"

#define EMS_PAGE_SIZE 16384
#define EMS_PHYSICAL_PAGES 4
#define EMS_MAX_HANDLE 0x1000
#define EMS_HANDLE_NONE -1

typedef enum {
    EMS_STATUS_SUCCESS = 0x00,
    EMS_STATUS_INTERNAL_ERROR = 0x80,
    EMS_STATUS_INVALID_HANDLE = 0x83,
    EMS_STATUS_UNDEFINED_FUNCTION = 0x84,
    EMS_STATUS_OUT_OF_HANDLES = 0x85,
    EMS_STATUS_OUT_OF_MEMORY = 0x88,
    EMS_STATUS_ZERO_PAGES = 0x89,
    EMS_STATUS_INVALID_LOGICAL_PAGE = 0x8A,
    EMS_STATUS_INVALID_PHYSICAL_PAGE = 0x8B
} ems_status;

typedef struct {
    u8 *data;
    u16 page_count;
    bool used;
} ems_handle;

typedef struct {
    ems_handle *handles;
    s16 map_handle[EMS_PHYSICAL_PAGES];
    u16 map_page[EMS_PHYSICAL_PAGES];
    u16 handle_size;    
    u16 frame_segment;
} ems_state;

void cpu_func_intr_ems(cpu_state *cpu, ems_state *ems);
void ems_state_init(ems_state *ems, u16 frame_segment);

#endif /* __ZZT_EMS_H__ */
