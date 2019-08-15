(function () {
	'use strict';

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
	let date_s = Date.now();
	function time_ms() {
	  return Date.now() - date_s;
	}

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
	let audioCtx = undefined;
	document.addEventListener('mousedown', function (event) {
	  if (audioCtx == undefined) {
	    audioCtx = new (window.AudioContext || window.webkitAudioContext)();
	  }
	});
	document.addEventListener('keydown', function (event) {
	  if (audioCtx == undefined) {
	    audioCtx = new (window.AudioContext || window.webkitAudioContext)();
	  }
	});
	class OscillatorBasedAudio {
	  constructor() {
	    this.lastCurrTime = 0;
	    this.lastTimeMs = 0;
	    this.timeSpeakerOn = 0;
	    this.audioGain = undefined;
	    this.pc_speaker = undefined;
	  }

	  on(freq) {
	    if (audioCtx == undefined) return;
	    let cTime = audioCtx.currentTime;

	    if (cTime != this.lastCurrTime) {
	      this.lastCurrTime = cTime;
	      this.lastTimeMs = time_ms();
	    }

	    let lastADelay = (time_ms() - this.lastTimeMs) / 1000.0; // console.log("pc speaker " + freq + " " + (audioCtx.currentTime + lastADelay));

	    if (this.pc_speaker == undefined) {
	      this.audioGain = audioCtx.createGain();
	      this.pc_speaker = audioCtx.createOscillator();
	      this.pc_speaker.type = 'square';
	      this.pc_speaker.frequency.setValueAtTime(freq, audioCtx.currentTime + lastADelay);
	      this.pc_speaker.connect(this.audioGain);
	      this.audioGain.connect(audioCtx.destination);
	      this.audioGain.gain.setValueAtTime(0.2, audioCtx.currentTime);
	      this.pc_speaker.start(0);
	    } else {
	      this.pc_speaker.frequency.setValueAtTime(freq, audioCtx.currentTime + lastADelay);
	    }

	    this.timeSpeakerOn = time_ms();
	  }

	  off() {
	    if (this.pc_speaker == undefined) return;
	    let cTime = audioCtx.currentTime;

	    if (cTime != this.lastCurrTime) {
	      this.lastCurrTime = cTime;
	      this.lastTimeMs = time_ms();
	    }

	    let lastADelay = (time_ms() - this.lastTimeMs) / 1000.0; // console.log("pc speaker off " + (audioCtx.currentTime + lastADelay));

	    this.pc_speaker.frequency.setValueAtTime(0, audioCtx.currentTime + lastADelay);
	  }

	}
	/*	ZetaAudio.createScriptProcessorBased = function(emu) {
			var pc_speaker = undefined;
			var emu = emu;

			var init_speaker = function() {
				pc_speaker = audioCtx.createScriptProcessor();

				var bufferSize = pc_speaker.bufferSize;
				var buffer = emu._malloc(bufferSize);
				var heap = new Uint8Array(emu.HEAPU8.buffer, buffer, bufferSize);

				emu._audio_stream_init(time_ms(), Math.floor(audioCtx.sampleRate));
				emu._audio_stream_set_volume(Math.floor(0.2 * emu._audio_stream_get_max_volume()));

				pc_speaker.onaudioprocess = function(event) {
					var out = event.outputBuffer.getChannelData(0);
					emu._audio_stream_generate_u8(time_ms() - (bufferSize * 1000 / audioCtx.sampleRate), buffer, bufferSize);
					for (var i = 0; i < bufferSize; i++) {
						out[i] = (heap[i] - 127) / 127.0;
					}
					console.log("audio " + bufferSize);
				};

				pc_speaker.connect(audioCtx.destination);
			};

			return {
				on: function(freq) {
					if (audioCtx == undefined) return;
					if (pc_speaker == undefined) init_speaker();
					emu._audio_stream_append_on(time_ms(), freq);
				},
				off: function() {
					if (pc_speaker == undefined) return;
					emu._audio_stream_append_off(time_ms());
				}
			};
		}; */

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
	class CanvasBasedRenderer {
	  constructor(canvas, options) {
	    this.canvas = canvas;
	    this.blink_duration = options && options.blink_duration || 466;
	    this.ctx = canvas.getContext('2d', {
	      alpha: false
	    });
	    this.ctx.imageSmoothingEnabled = false;
	    this.video_blink = true;

	    if (options && options.hasOwnProperty("blink")) {
	      this.video_blink = options.blink;
	    }

	    this.video_mode = -1;
	    this.chrBuf = [];
	    this.drawChrWidth = undefined;
	    this.chrWidth = undefined;
	    this.scrWidth = undefined;
	    this.chrHeight = undefined;
	    this.scrHeight = undefined;
	    this.pw = undefined;
	    this.ph = undefined;
	    this.cw = undefined;
	    this.ch = undefined;
	    this.scale = 1;
	    this.asciiFg = null;
	    this.charset = null;
	    this.palette = null;
	    this.rdDirty = false;
	    this.charset_override_enabled = options && options.charset_override || false;
	    this.charset_override = null;

	    if (this.charset_override_enabled) {
	      let coImg = new Image();
	      coImg.src = options.charset_override;

	      coImg.onload = function () {
	        this.charset_override = coImg;
	        this.rdDirty = true;
	      };
	    }
	  }

	  _updVideoMode(val) {
	    if (val != this.video_mode) {
	      this.chrBuf = [];

	      if ((val & 0x02) == 2) {
	        this.scrWidth = 80;
	      } else {
	        this.scrWidth = 40;
	      }

	      this.scrHeight = 25;
	      this.video_mode = val;
	    }

	    this.drawChrWidth = this.chrWidth * (80 / this.scrWidth);
	    this.pw = this.scrWidth * this.drawChrWidth;
	    this.ph = this.scrHeight * this.chrHeight;
	    this.cw = this.canvas.width;
	    this.ch = this.canvas.height;
	    this.scale = Math.min(Math.floor(this.cw / this.pw), Math.floor(this.ch / this.ph));
	  }

	  _drawChar(x, y, chr, col, time) {
	    if (this.video_blink && col >= 0x80) {
	      col = col & 0x7F;

	      if (time % this.blink_duration >= this.blink_duration / 2) {
	        col = (col >> 4) * 0x11;
	      }
	    }

	    let buffered = this.chrBuf[y * 80 + x];
	    let bufcmp = chr << 8 | col;

	    if (buffered == bufcmp) {
	      return;
	    } else {
	      this.chrBuf[y * 80 + x] = bufcmp;
	    }

	    x = x * this.drawChrWidth;
	    y = y * this.chrHeight;
	    let bg = col >> 4 & 0x0F;
	    let fg = col & 15;
	    let rw = this.drawChrWidth;
	    let rh = this.chrHeight;

	    if (this.scale > 1) {
	      rw *= this.scale;
	      rh *= this.scale;
	      x = x * this.scale + (this.cw - this.pw * this.scale) / 2;
	      y = y * this.scale + (this.ch - this.ph * this.scale) / 2;
	    } else {
	      x += (this.cw - this.pw) / 2;
	      y += (this.ch - this.ph) / 2;
	    }

	    this.ctx.fillStyle = this.palette[bg];
	    this.ctx.fillRect(x, y, rw, rh);

	    if (bg != fg) {
	      this.ctx.drawImage(this.asciiFg[fg], (chr & 15) * this.chrWidth, (chr >> 4 & 15) * this.chrHeight, this.chrWidth, this.chrHeight, x, y, rw, rh);
	    }
	  }

	  _updRenderData() {
	    if (this.palette == null) {
	      return;
	    }

	    let srcImg = null;

	    if (this.charset_override_enabled) {
	      if (this.charset_override == null) return;else srcImg = this.charset_override;
	    } else {
	      if (this.charset == null) return;
	    }

	    this.asciiFg = [];

	    if (srcImg == null) {
	      let charCanvas = document.createElement('canvas');
	      charCanvas.width = 16 * this.chrWidth;
	      charCanvas.height = 16 * this.chrHeight;
	      let charCtx = charCanvas.getContext('2d');
	      let charData = charCtx.createImageData(charCanvas.width, charCanvas.height);
	      let data = charData.data;
	      let dpos = 0;

	      for (var ch = 0; ch < 256; ch++) {
	        let rx = (ch & 0xF) * this.chrWidth;
	        let ry = (ch >> 4) * this.chrHeight;

	        for (var cy = 0; cy < this.chrHeight; cy++, dpos++) {
	          var co = (ry + cy) * charCanvas.width + rx;
	          var ctmp = this.charset[dpos];

	          for (var cx = 0; cx < this.chrWidth; cx++, co++, ctmp = ctmp << 1) {
	            var cc = (ctmp >> 7 & 0x01) * 255;
	            data[co * 4 + 0] = cc;
	            data[co * 4 + 1] = cc;
	            data[co * 4 + 2] = cc;
	            data[co * 4 + 3] = cc;
	          }
	        }
	      }

	      charCtx.putImageData(charData, 0, 0);
	      srcImg = charCanvas;
	    }

	    this.asciiFg[15] = srcImg;

	    for (var i = 0; i < 15; i++) {
	      let charCanvas = document.createElement('canvas');
	      charCanvas.width = 16 * this.chrWidth;
	      charCanvas.height = 16 * this.chrHeight;
	      let charCtx = charCanvas.getContext('2d');
	      charCtx.globalCompositeOperation = 'copy';
	      charCtx.drawImage(srcImg, 0, 0);
	      charCtx.globalCompositeOperation = 'source-in';
	      charCtx.fillStyle = this.palette[i];
	      charCtx.fillRect(0, 0, charCanvas.width, charCanvas.height);
	      this.asciiFg[i] = charCanvas;
	    }

	    this.rdDirty = false;
	  }

	  render(heap, mode, time) {
	    if (this.rdDirty) this._updRenderData();

	    this._updVideoMode(mode);

	    let pos = 0;

	    if (this.asciiFg != null && this.palette != null) {
	      for (var y = 0; y < 25; y++) {
	        for (var x = 0; x < this.scrWidth; x++, pos += 2) {
	          this._drawChar(x, y, heap[pos], heap[pos + 1], time);
	        }
	      }
	    }
	  }

	  setCharset(width, height, heap) {
	    this.chrWidth = width;
	    this.chrHeight = height;
	    this.charset = heap;
	    this.rdDirty = true;
	  }

	  setPalette(heap) {
	    this.palette = new Array();

	    for (var i = 0; i < 16; i++) {
	      let s = (heap[i] & 0xFFFFFF).toString(16);

	      while (s.length < 6) s = "0" + s;

	      this.palette[i] = "#" + s;
	    }

	    this.rdDirty = true;
	  }

	}

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
	class MapBasedVfs {
	  constructor(inputMap, options) {
	    this.map = {};

	    for (var key in inputMap) {
	      this.map[key.toUpperCase()] = inputMap[key];
	    }

	    this.readonly = options && options.readonly || false;
	  }

	  canSet(key) {
	    return this.readonly;
	  }

	  get(key) {
	    if (!this.map.hasOwnProperty(key.toUpperCase())) {
	      return null;
	    }

	    return this.map[key.toUpperCase()].slice(0);
	  }

	  list(filter) {
	    let a = [];

	    for (var key in this.map) {
	      if (filter == null || filter(key)) {
	        a.push(key);
	      }
	    }

	    return a;
	  }

	  set(key, value) {
	    if (this.readonly) return false;
	    this.map[key.toUpperCase()] = value;
	    return true;
	  }

	}

	class VfsBasedVfs {
	  constructor(providers) {
	    this.providers = providers;
	  }

	  canSet(key) {
	    for (var p = 0; p < this.providers.length; p++) {
	      let provider = this.providers[p];

	      if (provider.canSet(key)) {
	        return true;
	      }
	    }

	    return false;
	  }

	  get(key) {
	    for (var p = this.providers.length - 1; p >= 0; p--) {
	      let provider = this.providers[p];
	      const result = provider.get(key);
	      if (result != null) return result;
	    }

	    return null;
	  }

	  list(filter) {
	    let data = [];

	    for (var p = 0; p < this.providers.length; p++) {
	      let provider = this.providers[p];
	      data = data.concat(provider.list(filter));
	    }

	    return data.sort();
	  }

	  set(key, value) {

	    for (var p = this.providers.length - 1; p >= 0; p--) {
	      let provider = this.providers[p];
	      if (provider.set(key, value)) return true;
	    }

	    return false;
	  }

	}

	function createVfsFromMap(inputMap, options) {
	  return new MapBasedVfs(inputMap, options);
	}
	function createVfsFromVfs(providers) {
	  return new VfsBasedVfs(providers);
	}
	function createVfsFromZip(url, options, progressCallback) {
	  return new Promise(resolve => {
	    var xhr = new XMLHttpRequest();
	    xhr.open("GET", url, true);
	    xhr.responseType = "arraybuffer";

	    xhr.onprogress = function (event) {
	      if (progressCallback != null) progressCallback(event.loaded / event.total);
	    };

	    xhr.onload = function () {
	      console.log(this.response);
	      if (progressCallback != null) progressCallback(1);
	      let files = UZIP.parse(this.response);
	      let fileMap = {};

	      for (var key in files) {
	        if (!(options && options.filenameFilter) || options.filenameFilter(key)) {
	          fileMap[key] = files[key];
	        }
	      }

	      resolve(new MapBasedVfs(fileMap, options));
	    };

	    xhr.send();
	  });
	}

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
	let ZetaKbdmap = {
	  ArrowUp: 0x48,
	  ArrowLeft: 0x4B,
	  ArrowRight: 0x4D,
	  ArrowDown: 0x50,
	  Home: 0x47,
	  End: 0x4F,
	  Insert: 0x52,
	  Delete: 0x53,
	  PageUp: 0x49,
	  PageDown: 0x51,
	  Enter: 0x1C,
	  Escape: 0x01,
	  Backspace: 0x0E,
	  Tab: 0x0F,
	  "-": 12,
	  "_": 12,
	  "=": 13,
	  "+": 13,
	  "`": 41,
	  "~": 41,
	  "\\": 43,
	  "|": 43,
	  "*": 55,
	  " ": 57
	};

	let addCharsInOrder = function (c, off) {
	  for (var i = 0; i < c.length; i++) {
	    ZetaKbdmap[c.charAt(i)] = off + i;
	  }
	};

	addCharsInOrder("1234567890", 2);
	addCharsInOrder("QWERTYUIOP{}", 16);
	addCharsInOrder("qwertyuiop[]", 16);
	addCharsInOrder("asdfghjkl;'", 30);
	addCharsInOrder("ASDFGHJKL:\"", 30);
	addCharsInOrder("zxcvbnm,./", 44);
	addCharsInOrder("ZXCVBNM<>?", 44);

	for (var i = 1; i <= 10; i++) {
	  ZetaKbdmap["F" + i] = 0x3A + i;
	}

	const keymap = ZetaKbdmap;

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
	let emu;
	let vfs = createVfsFromMap({});
	let handles = {};
	function setWrappedEmu(inputEmu) {
	  emu = inputEmu;
	}
	function setWrappedVfs(inputVfs) {
	  vfs = inputVfs;
	}
	function initVfsWrapper() {
	  window.vfsg_open = function (fn, mode) {
	    if (typeof fn !== "string") {
	      fn = emu.AsciiToString(fn);
	    }

	    fn = fn.toUpperCase();
	    const data = vfs.get(fn);
	    const is_write = (mode & 0x3) == 1;

	    if (is_write) {
	      if (!vfs.canSet(fn)) return -1;

	      if (data == null || (mode & 0x10000) != 0) {
	        data = new Uint8Array(0);
	      }
	    } else {
	      if (data == null) return -1;
	    }

	    console.log("opening " + fn);
	    let i = 1;

	    while (i in handles) i++;

	    handles[i] = {
	      fn: fn,
	      pos: 0,
	      mode: mode,
	      write_on_close: is_write,
	      array: data
	    };
	    return i;
	  };

	  window.vfsg_close = function (h) {
	    if (h in handles) {
	      if (handles[h].write_on_close) {
	        vfs.set(handles[h].fn, handles[h].array);
	      }

	      delete handles[h];
	      return 0;
	    } else return -1;
	  };

	  window.vfsg_seek = function (h, offset, type) {
	    if (!(h in handles)) return -1;
	    let newpos;

	    switch (type) {
	      case 0:
	        /* SEEK_SET */
	        newpos = offset;
	        break;

	      case 1:
	        /* SEEK_CUR */
	        newpos = handles[h].pos + offset;
	        break;

	      case 2:
	        /* SEEK_END */
	        newpos = handles[h].array.length + offset;
	        break;
	    }

	    console.log("seek to " + newpos);
	    if (newpos > handles[h].array.length) newpos = handles[h].array.length;else if (newpos < 0) newpos = 0;
	    handles[h].pos = newpos;
	    return 0;
	  };

	  window.vfsg_read = function (h, ptr, amount) {
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
	  };

	  window.vfsg_write = function (h, ptr, amount) {
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
	  };

	  var ff_list = [];
	  var ff_pos = 0;

	  var vfs_list = function (spec) {
	    spec = spec.toUpperCase();

	    if (spec.startsWith("*.")) {
	      let suffix = spec.substring(1);
	      let list = vfs.list(key => key.endsWith(suffix));
	      list.sort((a, b) => {
	        const lenDiff = a.length - b.length;
	        if (lenDiff != 0) return lenDiff;else return a.localeCompare(b);
	      });
	      return list;
	    } else {
	      console.log("unknown findfirst spec: " + spec);
	      return null;
	    }
	  };

	  window.vfsg_findfirst = function (ptr, mask, spec) {
	    spec = emu.AsciiToString(spec);
	    ff_list = [];
	    const l = vfs_list(spec);
	    if (l == null) return -1;
	    ff_list = l;
	    ff_pos = 0;
	    return vfsg_findnext(ptr);
	  };

	  window.vfsg_findnext = function (ptr) {
	    if (ff_pos >= ff_list.length) return -1;
	    const finddata = new Uint8Array(emu.HEAPU8.buffer, ptr, 0x100); // write documented fields

	    finddata[0x15] = 0;
	    finddata[0x16] = 0;
	    finddata[0x17] = 0;
	    finddata[0x18] = 0;
	    finddata[0x19] = 0;
	    let fn = ff_list[ff_pos];
	    let size = vfs.get(fn).byteLength;
	    finddata[0x1A] = size & 0xFF;
	    finddata[0x1B] = size >> 8 & 0xFF;
	    finddata[0x1C] = size >> 16 & 0xFF;
	    finddata[0x1D] = size >> 24 & 0xFF;

	    for (var i = 0; i < fn.length; i++) {
	      finddata[0x1E + i] = fn.charCodeAt(i);
	    }

	    finddata[0x1E + fn.length] = 0;
	    ff_pos = ff_pos + 1;
	    return 0;
	  };
	}

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
	    this.mouseSensitivity = options && options.mouseSensitivity || 4;
	    this.frameQueued = false;
	    this.time_ms_cached = time_ms();
	    this.last_timer_time = 0;
	    this.timer_dur = 1000 / 18.2;
	    this.opcodes = 1000;
	    const self = this;

	    window.vfsg_time_ms = function () {
	      return self.time_ms_cached;
	    };

	    window.vfsg_has_feature = function (id) {
	      if (id == 1
	      /* joy connected */
	      ) return true;else if (id == 2
	      /* mouse connected */
	      ) return true;else return false;
	    };

	    window.zetag_update_charset = function (width, height, char_ptr) {
	      const data = new Uint8Array(emu.HEAPU8.buffer, char_ptr, 256 * height);
	      render.setCharset(width, height, data);
	    };

	    window.zetag_update_palette = function (palette_ptr) {
	      const data = new Uint32Array(emu.HEAPU32.buffer, palette_ptr, 16);
	      render.setPalette(data);
	    };

	    window.speakerg_on = function (freq) {
	      if (!document.hasFocus()) {
	        speakerg_off();
	        return;
	      }

	      if (audio != undefined) audio.on(freq);
	    };

	    window.speakerg_off = function () {
	      if (audio != undefined) audio.off();
	    };

	    window.addEventListener("message", function (event) {
	      if (event.data == "zzt_tick") {
	        event.stopPropagation();

	        self._tick();
	      }
	    }, true);
	    document.addEventListener('keydown', function (event) {
	      if (event.target != element) return false;
	      let ret = false;
	      if (event.key == "Shift") emu._zzt_kmod_set(0x01);else if (event.key == "Control") emu._zzt_kmod_set(0x04);else if (event.key == "Alt" || event.key == "AltGraph") emu._zzt_kmod_set(0x08);else ret = false;
	      let chr = event.key.length == 1 ? event.key.charCodeAt(0) : event.keyCode < 32 ? event.keyCode : 0;
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
	    document.addEventListener('keyup', function (event) {
	      if (event.target != element) return false;
	      let ret = true;
	      if (event.key == "Shift") emu._zzt_kmod_clear(0x01);else if (event.key == "Control") emu._zzt_kmod_clear(0x04);else if (event.key == "Alt" || event.key == "AltGraph") emu._zzt_kmod_clear(0x08);else ret = false;
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
	    this.element.addEventListener("mousemove", function (e) {
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
	    this.element.addEventListener("mousedown", function (e) {
	      element.requestPointerLock();
	      if (emu == undefined) return;

	      emu._zzt_mouse_set(e.button);
	    });
	    this.element.addEventListener("mouseup", function (e) {
	      if (emu == undefined) return;

	      emu._zzt_mouse_clear(e.button);
	    });
	  }

	  _frame() {
	    this._pollGamepads();

	    const ptr = this.emu._zzt_get_ram();

	    const heap = new Uint8Array(this.emu.HEAPU8.buffer, ptr + 0xB8000, 80 * 25 * 2);
	    this.render.render(heap, this.emu._zzt_video_mode(), this.time_ms_cached);

	    this.emu._zzt_mark_frame();

	    this.frameQueued = false;
	  }

	  _resetLastTimerTime() {
	    this.last_timer_time = time_ms();
	  }

	  _tick() {
	    this.time_ms_cached = time_ms();

	    while (this.time_ms_cached - this.last_timer_time >= this.timer_dur) {
	      //    		console.log("timer, drift = " + (tms - last_timer_time - timer_dur) + " ms");
	      this.last_timer_time += this.timer_dur;

	      this.emu._zzt_mark_timer();
	    }

	    const rcode = this.emu._zzt_execute(this.opcodes);

	    const duration = time_ms() - this.time_ms_cached;

	    if (rcode) {
	      if (rcode == 1) {
	        if (duration < 3) {
	          this.opcodes = this.opcodes * 20 / 19;
	        } else if (duration > 6) {
	          this.opcodes = this.opcodes * 19 / 20;
	        }
	      }

	      if (!this.frameQueued) {
	        this.frameQueued = true;
	        window.requestAnimationFrame(() => this._frame());
	      }

	      const time_to_timer = this.timer_dur - (this.time_ms_cached + duration - this.last_timer_time);

	      if (rcode != 3 || time_to_timer <= 1) {
	        window.postMessage("zzt_tick", "*");
	      } else {
	        setTimeout(() => this._tick(), time_to_timer);
	      }
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

	        this.emu._zzt_joy_clear(0);

	        for (var j = 0; j < gamepad.buttons.length; j++) {
	          if (gamepad.buttons[j].pressed) {
	            this.emu._zzt_joy_set(0);

	            break;
	          }
	        }
	      }
	    }
	  }

	}

	function createEmulator(render, audio, vfs, options) {
	  return new Promise(resolve => {
	    ZetaNative().then(emu => {
	      const vfs_arg = options && options.arg || "";

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
	      if (handle < 0) handle = vfsg_open("superz.exe", 0);
	      if (handle < 0) throw "Could not find ZZT executable!";

	      emu._zzt_load_binary(handle, buffer);

	      vfsg_close(handle);

	      emu._zzt_set_timer_offset(Date.now() % 86400000);

	      emuObj._resetLastTimerTime();

	      emuObj._tick();

	      resolve(emuObj);
	    });
	  });
	}

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

	window.ZetaInitialize = function (options) {
	  if (!options.render) throw "Missing option: render!";
	  if (!options.render.canvas) throw "Missing option: render.canvas!";
	  if (!options.path) throw "Missing option: path!";
	  if (!options.files) throw "Missing option: files!";
	  const canvas = options.render.canvas;
	  canvas.contentEditable = true;
	  const ctx = canvas.getContext('2d', {
	    alpha: false
	  });
	  ctx.imageSmoothingEnabled = false; // TODO: bring back loading screen

	  var vfsPromises = [];
	  var vfsObjects = [];

	  for (var s in options.files) {
	    const file = options.files[s];

	    if (Array.isArray(file)) {
	      vfsPromises.push(createVfsFromZip(file[0], file[1], null).then(o => vfsObjects.push(o)));
	    } else {
	      vfsPromises.push(createVfsFromZip(file, null, null).then(o => vfsObjects.push(o)));
	    }
	  }

	  return Promise.all(vfsPromises).then(_ => {
	    const render = new CanvasBasedRenderer(options.render.canvas);
	    const audio = new OscillatorBasedAudio();
	    const vfs = createVfsFromVfs(vfsObjects);
	    return createEmulator(render, audio, vfs, options);
	  }).then(_ => true);
	};

}());
