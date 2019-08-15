/*!
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

 import { createVfsFromMap } from "./vfs"

let emu;
let vfs = createVfsFromMap({});
let handles = {};

export function setWrappedEmu(inputEmu) {
	emu = inputEmu;
}

export function setWrappedVfs(inputVfs) {
    vfs = inputVfs;
}

export function initVfsWrapper() {
	window.vfsg_open = function(fn, mode) {
		if (typeof fn !== "string") {
			fn = emu.AsciiToString(fn);
		}
	
		fn = fn.toUpperCase();
		const data = vfs.get(fn);
		const is_write = (mode & 0x3) == 1;
	
		if (is_write) {
			if (!vfs.canSet(fn)) return -1;
			if (data == null || ((mode & 0x10000) != 0)) {
				data = new Uint8Array(0);
			}
		} else {
			if (data == null) return -1;
		}
	
		console.log("opening " + fn);
		let i = 1;
		while (i in handles) i++;
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
		console.log("seek to " + newpos);
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
		console.log("reading " + maxlen + " bytes from " + h.pos + " to " + ptr);
		const heap = new Uint8Array(emu.HEAPU8.buffer, ptr, maxlen);
		for (var pos = 0; pos < maxlen; pos++) {
			heap[pos] = h.array[h.pos + pos];
		}
		console.log("read " + maxlen + " bytes");
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
		console.log("wrote " + amount + " bytes");
		h.pos += amount;
		return amount;
	}
	
	var ff_list = [];
	var ff_pos = 0;
	
	var vfs_list = function(spec) {
		spec = spec.toUpperCase();
		if (spec.startsWith("*.")) {
			let suffix = spec.substring(1);
			let list = vfs.list(key => key.endsWith(suffix));
			list.sort((a, b) => {
				const lenDiff = a.length - b.length;
				if (lenDiff != 0) return lenDiff;
				else return a.localeCompare(b);
			});
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