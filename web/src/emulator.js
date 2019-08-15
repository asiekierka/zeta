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

import { time_ms, drawErrorMessage } from "./util.js";
import { keymap } from "./keymap.js";
import { initVfsWrapper, setWrappedEmu, setWrappedVfs } from "./vfs_wrapper.js"

/* var check_modifiers = function(event) {
	if (event.shiftKey) emu._zzt_kmod_set(0x01); else emu._zzt_kmod_clear(0x01);
	if (event.ctrlKey) emu._zzt_kmod_set(0x04); else emu._zzt_kmod_clear(0x04);
	if (event.altKey) emu._zzt_kmod_set(0x08); else emu._zzt_kmod_clear(0x08);
} */

class Emulator {
    constructor(element, emu, render, audio, vfs, options) {
        this.element = element;
        this.emu = emu;
        this.render = render;
        this.audio = audio;
        this.vfs = vfs;

        this.mouseSensitivity = (options && options.mouseSensitivity) || 4;

        this.frameQueued = false;
        this.time_ms_cached = time_ms();
        this.last_timer_time = 0;
        this.timer_dur = 1000 / 18.2;
        this.opcodes = 1000;

        const self = this;

        window.vfsg_time_ms = function() {
            return self.time_ms_cached;
        }

        window.vfsg_has_feature = function(id) {
            if (id == 1 /* joy connected */) return true;
            else if (id == 2 /* mouse connected */) return true;
            else return false;
        }        
        
        window.zetag_update_charset = function(width, height, char_ptr) {
	        const data = new Uint8Array(emu.HEAPU8.buffer, char_ptr, 256 * height);
	        render.setCharset(width, height, data);
        }

        window.zetag_update_palette = function(palette_ptr) {
	        const data = new Uint32Array(emu.HEAPU32.buffer, palette_ptr, 16);
        	render.setPalette(data);
        }

        window.speakerg_on = function(freq) {
        	if (!document.hasFocus()) {
		        speakerg_off();
          		return;
	        }

            if (audio != undefined) audio.on(freq);
        }

        window.speakerg_off = function() {
	        if (audio != undefined) audio.off();
        }

        window.addEventListener("message", function(event) {
            if (event.data == "zzt_tick") {
                event.stopPropagation();
                self._tick();
            }
        }, true);

        document.addEventListener('keydown', function(event) {
            if (event.target != element) return false;
            let ret = false;

            if (event.key == "Shift") emu._zzt_kmod_set(0x01);
            else if (event.key == "Control") emu._zzt_kmod_set(0x04);
            else if (event.key == "Alt" || event.key == "AltGraph") emu._zzt_kmod_set(0x08);
            else ret = false;

            let chr = (event.key.length == 1) ? event.key.charCodeAt(0) : (event.keyCode < 32 ? event.keyCode : 0);
            let key = keymap[event.key] || 0;
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
            if (event.target != element) return false;
            let ret = true;

            if (event.key == "Shift") emu._zzt_kmod_clear(0x01);
            else if (event.key == "Control") emu._zzt_kmod_clear(0x04);
            else if (event.key == "Alt" || event.key == "AltGraph") emu._zzt_kmod_clear(0x08);
            else ret = false;

            var key = keymap[event.key] || 0;
            if (key > 0) {
                emu._zzt_keyup(key);
                ret = true;
            }

            if (ret) {
                event.preventDefault();
            }
            return false;
        }, false);

        this.element.addEventListener("mousemove", function(e) {
            if (emu == undefined) return;
    
            const mx = e.movementX * self.mouseSensitivity;
            const my = e.movementY * self.mouseSensitivity;
            emu._zzt_mouse_axis(0, mx);
            emu._zzt_mouse_axis(1, my);
    /*		var mouseX = e.clientX - o.offsetLeft;
            var mouseY = e.clientY - o.offsetTop;
            if (mouseX < 0) mouseX = 0;
            else if (mouseX >= 640) mouseX = 639;
            if (mouseY < 0) mouseY = 0;
            else if (mouseY >= 350) mouseY = 349; */
        });
    
        this.element.addEventListener("mousedown", function(e) {
            element.requestPointerLock();
    
            if (emu == undefined) return;
            emu._zzt_mouse_set(e.button);
        });
    
        this.element.addEventListener("mouseup", function(e) {
            if (emu == undefined) return;
            emu._zzt_mouse_clear(e.button);
        });
    }

    _frame() {
        this._pollGamepads();

        const ptr = this.emu._zzt_get_ram();
        const heap = new Uint8Array(this.emu.HEAPU8.buffer, ptr + 0xB8000, 80*25*2);

        this.render.render(heap, this.emu._zzt_video_mode(), this.time_ms_cached);

        this.emu._zzt_mark_frame();
        this.frameQueued = false;
    }

    _resetLastTimerTime() {
        this.last_timer_time = time_ms();
    }

    _tick() {
        this.time_ms_cached = time_ms();

        while ((this.time_ms_cached - this.last_timer_time) >= this.timer_dur) {
//    		console.log("timer, drift = " + (tms - last_timer_time - timer_dur) + " ms");
            this.last_timer_time += this.timer_dur;
            this.emu._zzt_mark_timer();
        }

        const rcode = this.emu._zzt_execute(this.opcodes);
        const duration = time_ms() - this.time_ms_cached;
        if (rcode) {
            if (rcode == 1) {
                if (duration < 3) {
                    this.opcodes = (this.opcodes * 20 / 19);
                } else if (duration > 6) {
                    this.opcodes = (this.opcodes * 19 / 20);
                }
            }

            if (!this.frameQueued) {
                this.frameQueued = true;
                window.requestAnimationFrame(() => this._frame());
            }

            const time_to_timer = this.timer_dur - ((this.time_ms_cached + duration) - this.last_timer_time);
            if (rcode != 3 || time_to_timer <= 1) {
                window.postMessage("zzt_tick", "*");
            } else {
                setTimeout(() => this._tick(), time_to_timer);
            }
        } else {
            drawErrorMessage(this.render.canvas, this.render.ctx, "Emulation stopped.");
        }
    }

    _pollGamepads() {
        const gamepads = navigator.getGamepads();
        for (var i = 0; i < gamepads.length; i++) {
            const gamepad = gamepads[i];
            if (gamepad != null && gamepad.axes.length >= 2 && gamepad.buttons.length >= 1) {
                const ax0 = Math.round(gamepad.axes[0] * 127);
                const ax1 = Math.round(gamepad.axes[1] * 127);
                this.emu._zzt_joy_axis(0, ax0);
                this.emu._zzt_joy_axis(1, ax1);
                let pressed = false;
                for (var j = 0; j < gamepad.buttons.length; j++) {
                    if (gamepad.buttons[j].pressed) {
                        pressed = true;
                        break;
                    }
                }
                if (pressed) this.emu._zzt_joy_set(0);
                else this.emu._zzt_joy_clear(0);
            }
        }
    }
}

export function createEmulator(render, audio, vfs, options) {
	return new Promise(resolve => {
        ZetaNative().then(emu => {
            const vfs_arg = (options && options.arg) || "";
            const buffer = emu._malloc(vfs_arg.length + 1);
            const heap = new Uint8Array(emu.HEAPU8.buffer, buffer, vfs_arg.length + 1);
            for (var i = 0; i < vfs_arg.length; i++) {
                heap[i] = vfs_arg.charCodeAt(i);
            }
            heap[vfs_arg.length] = 0;
            
            setWrappedVfs(vfs);
            setWrappedEmu(emu);
            initVfsWrapper();

            const emuObj = new Emulator(options.render.canvas, emu, render, audio, vfs, options);

            emu._zzt_init();

            let handle = vfsg_open("zzt.exe", 0);
            if (handle < 0)
                handle = vfsg_open("superz.exe", 0);
            if (handle < 0)
                throw "Could not find ZZT executable!";
            emu._zzt_load_binary(handle, buffer);
            vfsg_close(handle);

            emu._zzt_set_timer_offset(Date.now() % 86400000)
            
            emuObj._resetLastTimerTime();
            emuObj._tick();
            resolve(emuObj);
        });
    });
}