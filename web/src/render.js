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

export class CanvasBasedRenderer {
	constructor(canvas, options) {
		this.canvas = canvas;
		this.blink_duration = (options && options.blink_duration) || 466;

		this.ctx = canvas.getContext('2d', {alpha: false});
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

		this.charset_override_enabled = (options && options.charset_override) || false;
		this.charset_override = null;
		if (this.charset_override_enabled) {
			let coImg = new Image();
			coImg.src = options.charset_override;
			coImg.onload = function() {
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

		this.pw = this.scrWidth*this.drawChrWidth;
		this.ph = this.scrHeight*this.chrHeight;
		this.cw = this.canvas.width;
		this.ch = this.canvas.height;
		this.scale = Math.min(Math.floor(this.cw / this.pw), Math.floor(this.ch / this.ph));
	}

	_drawChar(x, y, chr, col, time) {
		if (this.video_blink && col >= 0x80) {
			col = col & 0x7F;

			if ((time % this.blink_duration) >= (this.blink_duration / 2)) {
				col = (col >> 4) * 0x11;
			}
		}

		let buffered = this.chrBuf[y * 80 + x];
		let bufcmp = (chr << 8) | col;

		if (buffered == bufcmp) {
			return;
		} else {
			this.chrBuf[y * 80 + x] = bufcmp;
		}

		x = x * this.drawChrWidth;
		y = y * this.chrHeight;

		let bg = (col >> 4) & 0x0F;
		let fg = (col & 15);

		let rw = this.drawChrWidth;
		let rh = this.chrHeight;

		if (this.scale > 1) {
			rw *= this.scale;
			rh *= this.scale;
			x = (x*this.scale) + ((this.cw - this.pw*this.scale) / 2);
			y = (y*this.scale) + ((this.ch - this.ph*this.scale) / 2);
		} else {
			x += ((this.cw - this.pw) / 2);
			y += ((this.ch - this.ph) / 2);
		}

		this.ctx.fillStyle = this.palette[bg];
		this.ctx.fillRect(x, y, rw, rh);

		if (bg != fg) {
			this.ctx.drawImage(this.asciiFg[fg], (chr & 15) * this.chrWidth, ((chr >> 4) & 15) * this.chrHeight, this.chrWidth, this.chrHeight, x, y, rw, rh);
		}
	}

	_updRenderData() {
		if (this.palette == null) {
			return;
		}

		let srcImg = null;
		if (this.charset_override_enabled) {
			if (this.charset_override == null) return;
			else srcImg = this.charset_override;
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
					var co = ((ry + cy) * charCanvas.width) + rx;
					var ctmp = this.charset[dpos];
					for (var cx = 0; cx < this.chrWidth; cx++, co++, ctmp = ctmp << 1) {
						var cc = ((ctmp >> 7) & 0x01) * 255;
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
	};

	render(heap, mode, time) {
		if (this.rdDirty) this._updRenderData();
		this._updVideoMode(mode);

		let pos = 0;

		if (this.asciiFg != null && this.palette != null) {
			for (var y = 0; y < 25; y++) {
				for (var x = 0; x < this.scrWidth; x++, pos+=2) {
					this._drawChar(x, y, heap[pos], heap[pos+1], time);
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
