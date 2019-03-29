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

var canvas, ctx, audio, emu, render;
var zeta_options = {};

var date_s = Date.now();
var time_ms = function() { return Date.now() - date_s; }

var vfs_progress = {};
var vfs = null;
var handles = {};

var vfsg_time_ms_val = 0;

function vfsg_time_ms() {
	return vfsg_time_ms_val;
}

function vfsg_has_feature(id) {
	if (id == 1 /* joy connected */) return true;
	else if (id == 2 /* mouse connected */) return true;
	else return false;
}

function vfsg_open(fn, mode) {
	if (typeof fn !== "string") {
		fn = emu.AsciiToString(fn);
	}

	fn = fn.toUpperCase();
	var data = vfs.get(fn);
	var is_write = (mode & 0x3) == 1;

	if (is_write) {
		if (vfs.readonly()) return -1;
		if (data == null || ((mode & 0x10000) != 0)) {
			data = new Uint8Array(0);
		}
	} else {
		if (data == null) return -1;
	}

	console.log("opening " + fn);
	var i = 1;
	while (i in handles) i++;
	handles[i] = {name: fn, pos: 0, mode: mode, write_on_close: is_write, array: data};

	return i;
}

function vfsg_close(h) {
	if (h in handles) {
		if (handles[h].write_on_close) {
			vfs.set(handles[h].fn, handles[h].array);
		}
		delete handles[h];
		return 0;
	} else return -1;
}

function vfsg_seek(h, pos, type) {
	if (!(h in handles)) return -1;
	var newpos = pos;
	if (type == 1) {
		newpos += handles[h].pos;
	} else if (type == 2) {
		newpos += handles[h].array.length;
	}
	console.log("seek to " + newpos);
	if (newpos > handles[h].array.length)
		newpos = handles[h].array.length;
	else if (newpos < 0)
		newpos = 0;
	handles[h].pos = newpos;
	return 0;
}

function vfsg_read(h, ptr, amount) {
	if (!(h in handles)) return -1;
	h = handles[h];
	var maxlen = Math.min(amount, h.array.length - h.pos);
	console.log("reading " + maxlen + " bytes from " + h.pos + " to " + ptr);
	var heap = new Uint8Array(emu.HEAPU8.buffer, ptr, maxlen);
	for (var pos = 0; pos < maxlen; pos++) {
		heap[pos] = h.array[h.pos + pos];
	}
	console.log("read " + maxlen + " bytes");
	h.pos += maxlen;
	return maxlen;
}

function vfsg_write(h, ptr, amount) {
	if (!(h in handles)) return -1;
	h = handles[h];
	var len = h.array.length;
	var newlen = h.pos + amount;
	if (newlen > len) {
		var newA = new Uint8Array(newlen);
		newA.set(h.array, 0);
		h.array = newA;
		len = newlen;
	}
	var heap = new Uint8Array(emu.HEAPU8.buffer, ptr, amount);
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
		var suffix = spec.substring(1);
		var list = vfs.list(function(key) { return key.endsWith(suffix); });
		list.sort(function(a, b) {
			var lenDiff = a.length - b.length;
			if (lenDiff != 0) return lenDiff;
			else return a.localeCompare(b);
		});
		return list;
	} else {
		console.log("unknown findfirst spec: " + spec);
		return null;
	}
}

function vfsg_findfirst(ptr, mask, spec) {
	spec = emu.AsciiToString(spec);
	ff_list = [];
	var l = vfs_list(spec);
	if (l == null) return -1;
	ff_list = l;
	ff_pos = 0;
	return vfsg_findnext(ptr);
}

function vfsg_findnext(ptr) {
	if (ff_pos >= ff_list.length) return -1;
	var finddata = new Uint8Array(emu.HEAPU8.buffer, ptr, 0x100);

	// write documented fields
	finddata[0x15] = 0;
	finddata[0x16] = 0;
	finddata[0x17] = 0;
	finddata[0x18] = 0;
	finddata[0x19] = 0;

	var fn = ff_list[ff_pos];
	var size = vfs.get(fn).byteLength;
	finddata[0x1A] = size & 0xFF;
	finddata[0x1B] = (size >> 8) & 0xFF;
	finddata[0x1C] = (size >> 16) & 0xFF;
	finddata[0x1D] = (size >> 24) & 0xFF;

	for (var i = 0; i < fn.length; i++)
		finddata[0x1E + i] = fn.charCodeAt(i);
	finddata[0x1E + fn.length] = 0;

	ff_pos = ff_pos + 1;
	return 0;
}

var queuedFrame = false;

var zzt_frame = function() {
	poll_gamepads();

	var ptr = emu._zzt_get_ram();
	var heap = new Uint8Array(emu.HEAPU8.buffer, ptr + 0xB8000, 80*25*2);

	render.render(heap, emu._zzt_video_mode(), vfsg_time_ms());

	emu._zzt_mark_frame();
	queuedFrame = false;
}

function zetag_update_charset(width, height, char_ptr) {
	var data = new Uint8Array(emu.HEAPU8.buffer, char_ptr, 256 * height);
	render.setCharset(width, height, data);
}

function zetag_update_palette(palette_ptr) {
	var data = new Uint32Array(emu.HEAPU32.buffer, palette_ptr, 16);
	render.setPalette(data);
}

var last_timer_time = 0;
var timer_dur = 1000 / 18.2;
var opcodes = 1000;

var zzt_tick = function() {
	vfsg_time_ms_val = time_ms();

	var tms = vfsg_time_ms();
	while ((tms - last_timer_time) >= timer_dur) {
//		console.log("timer, drift = " + (tms - last_timer_time - timer_dur) + " ms");
		last_timer_time += timer_dur;
		emu._zzt_mark_timer();
	}

	var rcode = emu._zzt_execute(opcodes);
	var duration = time_ms() - tms;
	if (rcode) {
		if (rcode == 1) {
			if (duration < 3) {
				opcodes = (opcodes * 20 / 19);
			} else if (duration > 6) {
				opcodes = (opcodes * 19 / 20);
			}
		}

		if (!queuedFrame) {
			queuedFrame = true;
			window.requestAnimationFrame(zzt_frame);
		}

		var time_to_timer = timer_dur - ((tms + duration) - last_timer_time);
		if (rcode != 3 || time_to_timer <= 1)
			window.postMessage("zzt_tick", "*");
		else
			setTimeout(zzt_tick, time_to_timer);
	}
}

window.addEventListener("message", function(event) {
	if (event.data == "zzt_tick") {
		event.stopPropagation();
		zzt_tick();
	}
}, true);

var vfs_arg = null;

var vfs_done = function() {
	vfs = ZetaVfs.fromProviders(vfs_providers);

	if (vfs_arg == null || vfs_arg == "") {
		if (vfs.contains("ZZT.EXE") && !vfs.contains("TOWN.ZZT")) {
			var zlist = vfs_list("*.ZZT");
			if (zlist.length > 0) {
				vfs_arg = zlist[0];
			}
		} else if (vfs.contains("SUPERZ.EXE") && !vfs.contains("MONSTER.SZT")) {
			var zlist = vfs_list("*.SZT");
			if (zlist.length > 0) {
				vfs_arg = zlist[0];
			}
		}
	}

	if (!zeta_options.render.hasOwnProperty("blink")) {
		zeta_options.render.blink = !vfs.contains("BLINKX.COM");
	}

	render = ZetaRender.toCanvas(zeta_options.render.canvas, zeta_options.render);
	vfs_arg = (vfs_arg || "").toUpperCase();

	draw_progress(1.0);
	ZetaNative().then(function(c) {
		emu = c;
		audio = ZetaAudio.createOscillatorBased();

		var buffer = emu._malloc(vfs_arg.length + 1);
		var heap = new Uint8Array(emu.HEAPU8.buffer, buffer, vfs_arg.length + 1);
		for (var i=0; i < vfs_arg.length; i++) {
			heap[i] = vfs_arg.charCodeAt(i);
		}
		heap[vfs_arg.length] = 0;

		emu._zzt_init();

		var handle = vfsg_open("zzt.exe", 0);
		if (handle < 0)
			handle = vfsg_open("superz.exe", 0);
		if (handle < 0)
			throw "Could not find ZZT executable!";
		emu._zzt_load_binary(handle, buffer);
		vfsg_close(handle);

		var ram = emu._zzt_get_ram();
		last_timer_time = time_ms();
		zzt_tick();
	});
}

function speakerg_on(freq) {
	if (!document.hasFocus()) {
		speakerg_off();
		return;
	}

	if (audio != undefined) audio.on(freq);
}

function speakerg_off() {
	if (audio != undefined) audio.off();
}

/* var check_modifiers = function(event) {
	if (event.shiftKey) emu._zzt_kmod_set(0x01); else emu._zzt_kmod_clear(0x01);
	if (event.ctrlKey) emu._zzt_kmod_set(0x04); else emu._zzt_kmod_clear(0x04);
	if (event.altKey) emu._zzt_kmod_set(0x08); else emu._zzt_kmod_clear(0x08);
} */

document.addEventListener('keydown', function(event) {
	if (event.target != canvas) return false;
	var ret = false;

//	check_modifiers(event);

	if (event.key == "Shift") emu._zzt_kmod_set(0x01);
	else if (event.key == "Control") emu._zzt_kmod_set(0x04);
	else if (event.key == "Alt" || event.key == "AltGraph") emu._zzt_kmod_set(0x08);
	else ret = false;

	var chr = (event.key.length == 1) ? event.key.charCodeAt(0) : (event.keyCode < 32 ? event.keyCode : 0);
	var key = ZetaKbdmap[event.key] || 0;
	if (key >= 0x46 && key <= 0x53) chr = 0;
	if (chr > 0 || key > 0) {
		emu._zzt_key(chr, key);
		ret = true;
	}

	if (ret) {
		event.preventDefault();
	}
	return false;
}, false);

document.addEventListener('keyup', function(event) {
	if (event.target != canvas) return false;
	var ret = true;

//	check_modifiers(event);

	if (event.key == "Shift") emu._zzt_kmod_clear(0x01);
	else if (event.key == "Control") emu._zzt_kmod_clear(0x04);
	else if (event.key == "Alt" || event.key == "AltGraph") emu._zzt_kmod_clear(0x08);
	else ret = false;

	var key = ZetaKbdmap[event.key] || 0;
	if (key > 0) {
		emu._zzt_keyup(key);
		ret = true;
	}

	if (ret) {
		event.preventDefault();
	}
	return false;
}, false);

var vfs_providers = [];
var vfs_files = [];
var vfs_files_pos = 0;

var draw_progress = function(p) {
	var cx = (canvas.width - 640) / 2;
	var cy = (canvas.height - 350) / 2;

	ctx.fillStyle = "#ff0000";
	ctx.fillRect(cx + 14*2, cy + 112*2, p * 292*2, 20);
}

var get_vfs_prog_total = function() {
	var i = 0;
	for (var k in vfs_progress) {
		i += vfs_progress[k];
	}
	return i;
}

var draw_vfs_progress = function() {
	draw_progress(get_vfs_prog_total() / vfs_files.length);
}

var vfs_on_loaded = function(p) {
	vfs_providers.push(p);
	draw_vfs_progress();

	vfs_files_pos = vfs_files_pos + 1;
	if (vfs_files_pos == vfs_files.length) {
		vfs_done();
	}
}

/* gamepad logic */
var poll_gamepads = function() {
	var gamepads = navigator.getGamepads();
	for (var i = 0; i < gamepads.length; i++) {
		var gamepad = gamepads[i];
		if (gamepad == null) continue;
		if (gamepad.axes.length >= 2 && gamepad.buttons.length >= 1) {
			var ax0 = gamepad.axes[0];
			var ax1 = gamepad.axes[1];
			ax0 = Math.round(ax0 * 127);
			ax1 = Math.round(ax1 * 127);
			emu._zzt_joy_axis(0, ax0);
			emu._zzt_joy_axis(1, ax1);
			emu._zzt_joy_clear(0);
			for (var j = 0; j < gamepad.buttons.length; j++) {
				if (gamepad.buttons[j].pressed) {
					emu._zzt_joy_set(0);
					break;
				}
			}
		}
	}
}

var mouseSensitivity = 4;

var attach_mouse_handler = function(o) {
	o.addEventListener("mousemove", function(e) {
		if (emu == undefined) return;

		var mx = e.movementX * mouseSensitivity;
		var my = e.movementY * mouseSensitivity;
		emu._zzt_mouse_axis(0, mx);
		emu._zzt_mouse_axis(1, my);
/*		var mouseX = e.clientX - o.offsetLeft;
		var mouseY = e.clientY - o.offsetTop;
		if (mouseX < 0) mouseX = 0;
		else if (mouseX >= 640) mouseX = 639;
		if (mouseY < 0) mouseY = 0;
		else if (mouseY >= 350) mouseY = 349; */
	});

	o.addEventListener("mousedown", function(e) {
		o.requestPointerLock();

		if (emu == undefined) return;
		emu._zzt_mouse_set(e.button);
	});

	o.addEventListener("mouseup", function(e) {
		if (emu == undefined) return;
		emu._zzt_mouse_clear(e.button);
	});
}

function zeta_emu_create(options) {
	zeta_options = options;
	canvas = options.render.canvas;
	canvas.contentEditable = true;
	ctx = canvas.getContext('2d', {alpha: false});
	ctx.imageSmoothingEnabled = false;

	attach_mouse_handler(canvas);

	vfs_files = options.files;
	vfs_arg = options.arg;

	for (var s in vfs_files) {
		vfs_progress[s] = 0;
		if (Array.isArray(vfs_files[s])) {
			ZetaVfs.fromZip(vfs_files[s][0], vfs_files[s][1], function(p) {
				vfs_progress[s] = p;
				draw_vfs_progress();
			}, vfs_on_loaded);
		} else {
			ZetaVfs.fromZip(vfs_files[s], null, function(p) {
				vfs_progress[s] = p;
				draw_vfs_progress();
			}, vfs_on_loaded);
		}
	}

	return {};
}
