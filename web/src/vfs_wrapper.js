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

import { createInMemoryStorage } from "./storage";

let emu;
let vfs = createInMemoryStorage({});
let handles = {};

export function setWrappedEmu(inputEmu) {
	emu = inputEmu;
}

export function setWrappedVfs(inputVfs) {
    vfs = inputVfs;
}

export function initVfsWrapper() {
	window.vfsg_getcwd = function(ptr, size) {
		if (size > 0) {
			let heap = new Uint8Array(emu.HEAPU8.buffer, ptr, size);
			heap[0] = 0;
		}
		return 0;
	}

	window.vfsg_chdir = function(ptr, size) {
		return -1;
	}

	window.vfsg_open = function(fn, mode) {
		if (typeof fn !== "string") {
			fn = emu.AsciiToString(fn);
		}

		fn = fn.toUpperCase();
		if (fn.length >= 3 && fn.charCodeAt(1) == 58 && fn.charCodeAt(2) == 92) {
			fn = fn.substring(3);
		}

		let data = vfs.get(fn);
		const is_write = (mode & 0x3) == 1;

		if (is_write) {
			if (!vfs.canSet(fn)) return -1;
			if (data == null || ((mode & 0x10000) != 0)) {
				data = new Uint8Array(0);
			} else {
				data = Uint8Array.from(data);
			}
		} else {
			if (data == null) return -1;
			data = Uint8Array.from(data);
		}

		let i = 1;
		while (i in handles) i++;
		// console.log("opening " + fn + " at " + i);
		handles[i] = {fn: fn, pos: 0, mode: mode, write_on_close: is_write, array: data};

		return i;
	}

	window.vfsg_close = function(h) {
		if (h in handles) {
			if (handles[h].write_on_close) {
				vfs.set(handles[h].fn, handles[h].array);
			}
			delete handles[h];
			return 0;
		} else return -1;
	}

	window.vfsg_seek = function(h, offset, type) {
		if (!(h in handles)) return -1;
		let newpos;
		switch (type) {
			case 0: /* SEEK_SET */
				newpos = offset;
				break;
			case 1: /* SEEK_CUR */
				newpos = handles[h].pos + offset;
				break;
			case 2: /* SEEK_END */
				newpos = handles[h].array.length + offset;
				break;
		}
		// console.log("seek to " + newpos);
		if (newpos > handles[h].array.length)
			newpos = handles[h].array.length;
		else if (newpos < 0)
			newpos = 0;
		handles[h].pos = newpos;
		return 0;
	}

	window.vfsg_read = function(h, ptr, amount) {
		if (!(h in handles)) return -1;
		h = handles[h];
		let maxlen = Math.min(amount, h.array.length - h.pos);
		// console.log("reading " + maxlen + " bytes from " + h.pos + " to " + ptr);
		const heap = new Uint8Array(emu.HEAPU8.buffer, ptr, maxlen);
		heap.set(h.array.subarray(h.pos, h.pos + maxlen));
		// console.log("read " + maxlen + " bytes");
		h.pos += maxlen;
		return maxlen;
	}

	window.vfsg_write = function(h, ptr, amount) {
		if (!(h in handles)) return -1;
		h = handles[h];
		let len = h.array.length;
		let newlen = h.pos + amount;
		if (newlen > len) {
			var newA = new Uint8Array(newlen);
			newA.set(h.array, 0);
			h.array = newA;
			len = newlen;
		}
		let heap = new Uint8Array(emu.HEAPU8.buffer, ptr, amount);
		for (var pos = 0; pos < amount; pos++) {
			h.array[h.pos + pos] = heap[pos];
		}
		// console.log("wrote " + amount + " bytes");
		h.pos += amount;
		return amount;
	}

	window.vfsg_truncate = function(h, newlen) {
		if (!(h in handles)) return -1;
		h = handles[h];
		if (h.array.length > newlen) {
			var newA = new Uint8Array(newlen);
			newA.set(h.array, 0);
			h.array = newA;
		} else if (h.array.length < newlen) {
			h.array = h.array.slice(0, newlen);
		}
		// console.log("truncated");
		return 0;
	}

	var ff_list = [];
	var ff_pos = 0;

	var vfs_list = function(spec) {
		spec = spec.toUpperCase();
		if (spec.startsWith("*")) {
			let suffix = spec.substring(1);
			let list = vfs.list(key => (suffix.length == 0) || key.endsWith(suffix));
			list.sort((a, b) => {
				const lenDiff = a.length - b.length;
				if (lenDiff != 0) return lenDiff;
				else return a.localeCompare(b);
			}).map(a => a.toUpperCase());
			return list;
		} else {
			console.log("unknown findfirst spec: " + spec);
			return null;
		}
	}

	window.vfsg_findfirst = function(ptr, mask, spec) {
		spec = emu.AsciiToString(spec);
		ff_list = [];
		const l = vfs_list(spec);
		if (l == null) return -1;
		ff_list = l;
		ff_pos = 0;
		return vfsg_findnext(ptr);
	}

	window.vfsg_findnext = function(ptr) {
		if (ff_pos >= ff_list.length) return -1;
		const finddata = new Uint8Array(emu.HEAPU8.buffer, ptr, 0x100);

		// write documented fields
		finddata[0x15] = 0;
		finddata[0x16] = 0;
		finddata[0x17] = 0;
		finddata[0x18] = 0;
		finddata[0x19] = 0;

		let fn = ff_list[ff_pos];
		let size = vfs.get(fn).byteLength;
		finddata[0x1A] = size & 0xFF;
		finddata[0x1B] = (size >> 8) & 0xFF;
		finddata[0x1C] = (size >> 16) & 0xFF;
		finddata[0x1D] = (size >> 24) & 0xFF;

		for (var i = 0; i < fn.length; i++) {
			finddata[0x1E + i] = fn.charCodeAt(i);
		}
		finddata[0x1E + fn.length] = 0;

		ff_pos = ff_pos + 1;
		return 0;
	}
}
