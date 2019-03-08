/*!
 * Copyright (c) 2018 Adrian Siekierka
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

var canvas = document.getElementById('zzt_canvas');
var ctx = canvas.getContext('2d', {alpha: false});
ctx.imageSmoothingEnabled = false;

var asciiImg = new Image();
var asciiFg = [];
var emu = undefined;

var palette = [
	"#000000", "#0000AA", "#00AA00", "#00AAAA",
	"#AA0000", "#AA00AA", "#AA5500", "#AAAAAA",
	"#555555", "#5555FF", "#55FF55", "#55FFFF",
	"#FF5555", "#FF55FF", "#FFFF55", "#FFFFFF"
];

var asciiFg = [];
var chrBuf = [];

function time_ms() { return new Date().getTime(); }

var video_blink = false;
var video_mode = 3;
var last_video_mode = 3;

function drawChar(x, y, chr, col) {
	if (video_mode != last_video_mode) {
		chrBuf = [];
		last_video_mode = video_mode;
	}

	var width = 16;
	if ((video_mode & 0x02) == 2) {
		width = 8;
	}

	x = x * width;
	y = y * 14;

	var buffered = chrBuf[y * 80 + x];
	if (video_blink && col >= 0x80) {
		col = col & 0x7F;

		var t = vfsg_time_ms();
		var blinkcount = (t % 466);
		if (blinkcount >= 233) {
			col = (col >> 4) * 0x11;
		}
	}

	var bufcmp = (chr << 8) | col;
/*	if (chr == 0 || chr == 32) {
		bufcmp &= 0xF0;
	} */

	if (buffered == bufcmp) {
		return;
	} else {
		chrBuf[y * 80 + x] = bufcmp;
	}

	var bg = (col >> 4) & 0x0F;
	var fg = (col & 15);

	ctx.fillStyle = palette[bg];
	ctx.fillRect(x, y, width, 14);

	if (bg != fg && chr != 0 && chr != 32) {
		ctx.drawImage(asciiFg[fg], (chr & 15) * 8, ((chr & 240) >> 4) * 14, 8, 14, x, y, width, 14);
	}
}

var vfs_progress = {};
var vfs = {};

function vfs_append(fn, then) {
	var loader = new ZipLoader(fn);
	loader.on("progress", function(event) {
		vfs_progress[fn] = (event.loaded / event.total);
		draw_progress(get_vfs_prog_total() / vfs_files.length);
	});

	loader.load().then(function() {
		vfs_progress[fn] = 0;
		for (let key in loader.files) {
			vfs[key.toUpperCase()] = loader.files[key];
			vfs[key.toUpperCase()].readonly = true;
		}

		then();
	});
}

var handles = {};

function vfsg_has_feature(id) {
	if (id == 1 /* joy connected */) return true;
	else if (id == 2 /* mouse connected */) return true;
	else return false;
}

function vfsg_open(fn, mode) {
	fn = emu.Pointer_stringify(fn).toUpperCase();
	var is_write = (mode & 0x3) == 1;
	if (is_write) {
		if (fn in vfs && vfs[fn].readonly) return -1;
		if (!(fn in vfs) || ((mode & 0x10000) != 0)) {
			vfs[fn] = {readonly: false, buffer: new Uint8Array(0)};
		}
	} else {
		if (!(fn in vfs)) return -1;
	}

	console.log("opening " + fn);
	var i = 1;
	while (i in handles) i++;
	handles[i] = {name: fn, pos: 0, mode: mode, array: vfs[fn].buffer, obj: vfs[fn]};

	return i;
}

function vfsg_close(h) {
	if (h in handles) {
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
	for (var pos = 0; pos < maxlen; pos++) {
		emu.setValue(ptr+pos, h.array[h.pos+pos], "i8");
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
		h.obj.buffer = newA;
		h.array = newA;
		len = newlen;
	}
	for (var pos = 0; pos < amount; pos++) {
		h.array[h.pos + pos] = emu.getValue(ptr+pos, "i8");
	}
	console.log("wrote " + amount + " bytes");
	h.pos += amount;
	return amount;
}

var ff_list = [];
var ff_pos = 0;

function vfs_list(spec) {
	spec = spec.toUpperCase();
	var list = [];
	if (spec.startsWith("*.")) {
		var suffix = spec.substring(1);
		for (let key in vfs) {
			if (key.endsWith(suffix)) {
				list.push(key);
			}
		}
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
	spec = emu.Pointer_stringify(spec);
	ff_list = [];
	var l = vfs_list(spec);
	if (l == null) return -1;
	ff_list = l;
	ff_pos = 0;
	return vfsg_findnext(ptr);
}

function vfsg_findnext(ptr) {
	if (ff_pos >= ff_list.length) return -1;

	// write documented fields
	emu.setValue(ptr + 0x15, 0, "i8");
	emu.setValue(ptr + 0x16, 0, "i8");
	emu.setValue(ptr + 0x17, 0, "i8");
	emu.setValue(ptr + 0x18, 0, "i8");
	emu.setValue(ptr + 0x19, 0, "i8");

	var size = vfs[ff_list[ff_pos]].buffer.length;
	emu.setValue(ptr + 0x1A, size & 0xFF, "i8");
	emu.setValue(ptr + 0x1B, (size >> 8) & 0xFF, "i8");
	emu.setValue(ptr + 0x1C, (size >> 16) & 0xFF, "i8");
	emu.setValue(ptr + 0x1D, (size >> 24) & 0xFF, "i8");

	var fn = ff_list[ff_pos];
	for (var i = 0; i < fn.length; i++)
		emu.setValue(ptr + 0x1E + i, fn.charCodeAt(i), "i8");
	emu.setValue(ptr + 0x1E + fn.length, 0, "i8");

	ff_pos = ff_pos + 1;
	return 0;
}

var queuedFrame = false;

function zzt_frame() {
	poll_gamepads();

	video_mode = emu._zzt_video_mode();
	var ptr = emu._zzt_get_ram();
	var width = 40;
	if ((video_mode & 0x02) == 2) width = 80;

	for (var y = 0; y < 25; y++) {
		for (var x = 0; x < width; x+=2) {
			var chc = emu.getValue(ptr + 0xB8000 + (y*width+x)*2, "i32");
			var chr1 = chc & 0xFF;
			var col1 = (chc >> 8) & 0xFF;
			var chr2 = (chc >> 16) & 0xFF;
			var col2 = (chc >> 24) & 0xFF;
			drawChar(x, y, chr1, col1);
			drawChar(x+1, y, chr2, col2);
		}
	}

	emu._zzt_mark_frame();
	queuedFrame = false;
}

var last_timer_time = 0;
var timer_dur = 1000 / 18.2;

var vfsg_time_ms_val = 0;

function vfsg_time_ms() {
	return vfsg_time_ms_val;
}

function zzt_tick() {
	vfsg_time_ms_val = time_ms();
	if (emu._zzt_execute(40000)) {
		if (!queuedFrame) {
			queuedFrame = true;
			window.requestAnimationFrame(zzt_frame);
		}

		var tms = time_ms();
		while ((tms - last_timer_time) >= timer_dur) {
			last_timer_time += timer_dur;
			emu._zzt_mark_timer();
		}
		if (document.hasFocus())
			window.postMessage("zzt_tick", "*");
		else
			setTimeout(zzt_tick, 20);
	}
}

window.addEventListener("message", function(event) {
	if (event.data == "zzt_tick") {
		event.stopPropagation();
		zzt_tick();
	}
}, true);

var vfs_arg = null;

function vfs_done() {
	if (vfs_arg == null && ("ZZT.EXE" in vfs)) {
		if ("TOWN.ZZT" in vfs) {
			vfs_arg = "";
		} else {
			var zlist = vfs_list("*.ZZT");
			if (zlist.length > 0) {
				vfs_arg = zlist[0];
			}
		}
	}

	if (vfs_arg == null && ("SUPERZ.EXE" in vfs)) {
		if ("MONSTER.SZT" in vfs) {
			vfs_arg = "";
		} else {
			var zlist = vfs_list("*.SZT");
			if (zlist.length > 0) {
				vfs_arg = zlist[0];
			}
		}
	}

	video_blink = !("BLINKX.COM" in vfs);

	vfs_arg = (vfs_arg || "").toUpperCase();

	draw_progress(1.0);
	Zeta().then(function(c) {
		emu = c;
		var psp_loc = emu._zzt_init();
		var ram = emu._zzt_get_ram();
		var arg = vfs_arg + '\r';
		emu.setValue(ram + psp_loc + 0x80, vfs_arg.length, "i8");
		for (var i = 0; i < vfs_arg.length; i++) {
			emu.setValue(ram + psp_loc + 0x81 + i, vfs_arg.charCodeAt(i), "i8");
		}

		last_timer_time = time_ms();
		zzt_tick();
	});
}

var audioCtx = undefined;
var audioGain = undefined;
var pc_speaker = undefined;

document.addEventListener('mousedown', function(event) {
	if (audioCtx == undefined) {
		audioCtx = new (window.AudioContext || window.webkitAudioContext) ();
		audioGain = audioCtx.createGain();
	}
});

// var minDuration = 1;
var lastCurrTime = 0;
var lastTimeMs = 0;
var timeSpeakerOn = 0;

function speakerg_on(freq) {
	if (!document.hasFocus()) {
		speakerg_off();
		return;
	}

	if (audioCtx == undefined)
		return;

	var cTime = audioCtx.currentTime;
	if (cTime != lastCurrTime) {
		lastCurrTime = cTime;
		lastTimeMs = time_ms();
	}

	var lastADelay = (time_ms() - lastTimeMs) / 1000.0;

//	console.log("pc speaker " + freq + " " + (audioCtx.currentTime + lastADelay));
	if (pc_speaker == undefined) {
		pc_speaker = audioCtx.createOscillator();
		pc_speaker.type = 'square';
		pc_speaker.frequency.setValueAtTime(freq, audioCtx.currentTime + lastADelay);
		pc_speaker.connect(audioGain);
		audioGain.connect(audioCtx.destination);
		audioGain.gain.setValueAtTime(0.2, audioCtx.currentTime);
		pc_speaker.start(0);
	} else {
		pc_speaker.frequency.setValueAtTime(freq, audioCtx.currentTime + lastADelay);
	}
	timeSpeakerOn = time_ms();
}

function speakerg_off() {
	if (pc_speaker == undefined)
		return;

	var cTime = audioCtx.currentTime;
	if (cTime != lastCurrTime) {
		lastCurrTime = cTime;
		lastTimeMs = time_ms();
	}

	var lastADelay = (time_ms() - lastTimeMs) / 1000.0;
//	console.log("pc speaker off " + (audioCtx.currentTime + lastADelay));
	pc_speaker.frequency.setValueAtTime(0, audioCtx.currentTime + lastADelay);
}

canvas.contentEditable = true;

document.addEventListener('keydown', function(event) {
	if (event.target != canvas) return false;
	var ret = true;

	if (audioCtx == undefined)
		audioCtx = new (window.AudioContext || window.webkitAudioContext) ();

	if (event.key == "Shift") emu._zzt_kmod_set(0x01);
	else if (event.key == "Control") emu._zzt_kmod_set(0x04);
	else if (event.key == "Alt" || event.key == "AltGraph") emu._zzt_kmod_set(0x08);
	else ret = false;

	var chr = (event.key.length == 1) ? event.key.charCodeAt(0) : event.keyCode;
	var key = zzt_kbdmap[event.key] || 0;
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
	var ret = true;
	if (event.target != canvas) return false;

	if (event.key == "Shift") emu._zzt_kmod_clear(0x01);
	else if (event.key == "Control") emu._zzt_kmod_clear(0x04);
	else if (event.key == "Alt" || event.key == "AltGraph") emu._zzt_kmod_clear(0x08);
	else ret = false;

	var key = zzt_kbdmap[event.key] || 0;
	if (key > 0) {
		emu._zzt_keyup(key);
		ret = true;
	}

	if (ret) {
		event.preventDefault();
	}
	return false;
}, false);

var vfs_files = [];
var vfs_files_pos = 0;

function draw_progress(p) {
	ctx.fillStyle = "#ff0000";
	ctx.fillRect(14*2, 112*2, p * 292*2, 20);
}

function get_vfs_prog_total() {
	var i = vfs_files_pos;
	for (var k in vfs_progress) {
		i += vfs_progress[k];
	}
	return i;
}

function vfs_on_loaded() {
	vfs_files_pos = vfs_files_pos + 1;
	draw_progress(get_vfs_prog_total() / vfs_files.length);
	if (vfs_files_pos == vfs_files.length) {
		vfs_done();
	}
}

/* gamepad logic */
function poll_gamepads() {
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

function attach_mouse_handler(o) {
	o.addEventListener("mousemove", function(e) {
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
		emu._zzt_mouse_set(e.button);
	});

	o.addEventListener("mouseup", function(e) {
		emu._zzt_mouse_clear(e.button);
	});
}

function zzt_emu_create(options) {
	asciiImg.src = options.charset_png || (options.path + "ascii.png");
	asciiImg.onload = function() {
		asciiFg[15] = asciiImg;
		for (var i = 0; i < 15; i++) {
			var charCanvas = document.createElement('canvas');
			charCanvas.width = 128;
			charCanvas.height = 224;
			var charCtx = charCanvas.getContext('2d');
			charCtx.globalCompositeOperation = 'copy';
			charCtx.drawImage(asciiImg, 0, 0);
			charCtx.globalCompositeOperation = 'source-in';
			charCtx.fillStyle = palette[i];
			charCtx.fillRect(0, 0, 128, 224);
			asciiFg[i] = charCanvas;
		}

		attach_mouse_handler(canvas);

		vfs_files = options.files;
		vfs_arg = options.arg;
		for (var s in vfs_files) {
			vfs_append(vfs_files[s], vfs_on_loaded);
		}
	};
}
