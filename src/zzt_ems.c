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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "zzt_ems.h"

#include "logging.h"

#ifdef USE_EMS_EMULATION

#define EMS_HANDLE_ALLOC_UNIT 8
// #define DEBUG_EMS

static ems_status ems_resize_handles(ems_state *ems, u16 new_size) {
    int i;
    ems_handle *old_handles;

    if (ems->handle_size >= EMS_MAX_HANDLE) {
        return EMS_STATUS_OUT_OF_HANDLES;
    }

    if (ems->handle_size == 0) {
        ems->handles = malloc(sizeof(ems_handle) * new_size);
        if (ems->handles == NULL) {
            return EMS_STATUS_OUT_OF_HANDLES;
        }
    } else {
        old_handles = ems->handles;
        ems->handles = realloc(old_handles, sizeof(ems_handle) * new_size);
        if (ems->handles == NULL) {
            ems->handles = old_handles;
            return EMS_STATUS_OUT_OF_HANDLES;
        }
    }
    for (i = ems->handle_size; i < new_size; i++) {
        ems->handles[i].used = false;
    }
    ems->handle_size = new_size;
    return EMS_STATUS_SUCCESS;
}

static int ems_find_free_handle(ems_state *ems) {
    int i;

    for (i = 0; i < ems->handle_size; i++) {
        if (!ems->handles[i].used) {
            return i;
        }
    }

    if (ems_resize_handles(ems, ems->handle_size + EMS_HANDLE_ALLOC_UNIT) == EMS_STATUS_SUCCESS) {
        return i;
    } else {
        return -1;
    }
}

static inline bool ems_valid_handle(ems_state *ems, int handle) {
    return handle >= 0 && handle < ems->handle_size && ems->handles[handle].used;
}

static ems_status ems_unmap_page(cpu_state *cpu, ems_state *ems, int physical_page) {
    u8 *phys_data;
    int handle, logical_page;

#ifdef DEBUG_EMS
    fprintf(stderr, "ems: unmapping physical page %d\n", physical_page);
#endif

    if (physical_page >= EMS_PHYSICAL_PAGES) return EMS_STATUS_INVALID_PHYSICAL_PAGE;

    if (ems->map_handle[physical_page] != EMS_HANDLE_NONE) {
        handle = ems->map_handle[physical_page];
        if (ems_valid_handle(ems, handle)) {
            logical_page = ems->map_page[physical_page];

            if (logical_page < ems->handles[handle].page_count) {
                phys_data = cpu->ram + (ems->frame_segment << 4) + (physical_page * EMS_PAGE_SIZE);
                memcpy(ems->handles[handle].data + (logical_page * EMS_PAGE_SIZE), phys_data, EMS_PAGE_SIZE);
            }
        }

        ems->map_handle[physical_page] = EMS_HANDLE_NONE;
    }

    return EMS_STATUS_SUCCESS;
}

static ems_status ems_map_page(cpu_state *cpu, ems_state *ems, u8 physical_page, s16 logical_page, int handle) {
    u8 *phys_data;
    ems_status status;

#ifdef DEBUG_EMS
    fprintf(stderr, "ems: mapping page %d:%d to physical page %d\n", handle, logical_page, physical_page);
#endif

    if (physical_page >= EMS_PHYSICAL_PAGES) return EMS_STATUS_INVALID_PHYSICAL_PAGE;
    if (!ems_valid_handle(ems, handle)) return EMS_STATUS_INVALID_HANDLE;
    if (logical_page >= ems->handles[handle].page_count) return EMS_STATUS_INVALID_LOGICAL_PAGE;
    if ((status = ems_unmap_page(cpu, ems, physical_page)) != EMS_STATUS_SUCCESS) return status;

    if (logical_page >= 0) {
#ifdef USE_EMS_REALLOC
        if (ems->handles[handle].alloc_page_count <= logical_page) {
            if (ems->handles[handle].alloc_page_count == 0) {
                ems->handles[handle].data = malloc(sizeof(u8) * (logical_page + 1) * EMS_PAGE_SIZE);
                if (ems->handles[handle].data == NULL) {
                    return EMS_STATUS_INTERNAL_ERROR;
                }
            } else {
                phys_data = ems->handles[handle].data;
                ems->handles[handle].data = realloc(phys_data, sizeof(u8) * (logical_page + 1) * EMS_PAGE_SIZE);
                if (ems->handles[handle].data == NULL) {
                    ems->handles[handle].data = phys_data;
                    return EMS_STATUS_INTERNAL_ERROR;
                }
            }
#ifdef DEBUG_EMS
            fprintf(stderr, "ems: realloc handle %d, %d -> %d pages\n", handle, ems->handles[handle].alloc_page_count, logical_page + 1);
#endif
            ems->handles[handle].alloc_page_count = logical_page + 1;
        }
#endif

        phys_data = cpu->ram + (ems->frame_segment << 4) + (physical_page * EMS_PAGE_SIZE);
        memcpy(phys_data, ems->handles[handle].data + (logical_page * EMS_PAGE_SIZE), EMS_PAGE_SIZE);
    
        ems->map_handle[physical_page] = handle;
        ems->map_page[physical_page] = logical_page;
    }

    return EMS_STATUS_SUCCESS;
}

static ems_status ems_alloc_pages(ems_state *ems, int num_pages, u16 *handle) {
    int idx = ems_find_free_handle(ems);
    if (idx < 0) return EMS_STATUS_OUT_OF_HANDLES;

#ifdef DEBUG_EMS
    fprintf(stderr, "ems: allocating %d pages\n", num_pages);
#endif

#ifdef USE_EMS_REALLOC
    ems->handles[idx].alloc_page_count = 0;
#else
    ems->handles[idx].data = malloc(sizeof(u8) * num_pages * EMS_PAGE_SIZE);
    if (ems->handles[idx].data == NULL) {
        return EMS_STATUS_OUT_OF_MEMORY;
    }
#endif
    ems->handles[idx].page_count = num_pages;
    ems->handles[idx].used = true;
    *handle = idx;
    return EMS_STATUS_SUCCESS;
}

static ems_status ems_dealloc_pages(ems_state *ems, int handle) {
    int i;

#ifdef DEBUG_EMS
    fprintf(stderr, "ems: deallocating handle %d\n", handle);
#endif

    if (!ems_valid_handle(ems, handle)) return EMS_STATUS_INVALID_HANDLE;

    for (i = 0; i < EMS_PHYSICAL_PAGES; i++) {
        if (ems->map_handle[i] == handle) {
            ems->map_handle[i] = EMS_HANDLE_NONE;
        }        
    }

    if (ems->handles[handle].data != NULL) {
        free(ems->handles[handle].data);
        ems->handles[handle].data = NULL;
    }
    ems->handles[handle].used = false;
    return EMS_STATUS_SUCCESS;
}

void cpu_func_intr_ems(cpu_state *cpu, ems_state *ems) {
	u8 cmd_id = cpu->ah;

	switch (cpu->ah) {
        case 0x40: // Status
            cpu->ah = EMS_STATUS_SUCCESS;
            break;
        case 0x41: // FrameSeg
            cpu->ah = EMS_STATUS_SUCCESS;
            cpu->bx = ems->frame_segment;
            break;
        case 0x42: // MaxPagesAvail - stub
            cpu->ah = EMS_STATUS_SUCCESS;
            cpu->bx = ems->max_pages;
            cpu->dx = ems->max_pages;
            break;
        case 0x43: // AllocPages
            cpu->ah = ems_alloc_pages(ems, cpu->bx, &(cpu->dx));
            cpu->dx += 1;
            break;
        case 0x44: // MapPage
            cpu->ah = ems_map_page(cpu, ems, cpu->al, cpu->bx, cpu->dx - 1);
            break;
        case 0x45: // DeallocPages
            cpu->ah = ems_dealloc_pages(ems, cpu->dx - 1);
            break;
        case 0x46:
            cpu->ax = 0x32; // Emulate EMS 3.2.
            break;
        case 0x4B: { // HandleCount
            cpu->ah = EMS_STATUS_SUCCESS;
            cpu->bx = ems->handle_size; // TODO: Should this report the count of all handles, or just used handles?
        } break;
        case 0x4C: { // HandlePages
            if (ems_valid_handle(ems, cpu->dx - 1)) {
                cpu->ah = EMS_STATUS_SUCCESS;
                cpu->bx = ems->handles[cpu->dx - 1].page_count;
            } else {
                cpu->ah = EMS_STATUS_INVALID_HANDLE;
            }
        } break;
        default: {
	    fprintf(stderr, "ems: unimplemented call %02X\n", cmd_id);
            cpu->ah = EMS_STATUS_UNDEFINED_FUNCTION;
        } break;
    }

#ifdef DEBUG_EMS
    fprintf(stderr, "ems: call %02X, status %02X\n", cmd_id, cpu->ah);
#endif
}

void ems_state_init(ems_state *ems, u16 frame_segment) {
    int i;

    memset(ems, 0, sizeof(ems_state));
    ems->max_pages = EMS_MAX_PAGES;
    ems->frame_segment = frame_segment;
    for (i = 0; i < EMS_PHYSICAL_PAGES; i++) {
        ems->map_handle[i] = EMS_HANDLE_NONE;
    }
}

void ems_set_max_pages(ems_state *ems, int max_pages) {
    ems->max_pages = (max_pages < 0 || max_pages > EMS_MAX_PAGES) ? EMS_MAX_PAGES : max_pages;
}

#endif /* USE_EMS_EMULATION */
