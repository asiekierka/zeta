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

export class CanvasBasedRenderer {
	constructor(canvas, options) {
		this.canvas = canvas;
		this.ctx = canvas.getContext('2d', {alpha: false});
		this.ctx.imageSmoothingEnabled = false;
		this.offscreenCanvas = document.createElement('canvas');
		this.offscreenCanvas.width = 640;
		this.offscreenCanvas.height = 400;
		this.offscreenCtx = this.offscreenCanvas.getContext('2d', {alpha: false});
		this.offscreenCtx.imageSmoothingEnabled = false;

		this.blink_duration = Math.round(((options && options.blink_cycle_duration) || 0.534) * 1000);
		this.video_blink = this.blink_duration > 0;

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
			let self = this;
			coImg.src = options.charset_override;
			coImg.onload = function() {
				self.charset_override = coImg;
				self.rdDirty = true;
			};
		}
	}

	_updVideoMode(val, time) {
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

		this.drawChrWidth = this.chrWidth * Math.round(80 / this.scrWidth);
		this.pw = this.scrWidth*this.drawChrWidth;
		this.ph = this.scrHeight*this.chrHeight;
		this.cw = this.canvas.width;
		this.ch = this.canvas.height;
		this.scale = Math.floor(Math.min(this.cw / this.pw, this.ch / this.ph));
		if (this.scale < 1) this.scale = 1;

		if (this.video_blink) {
			if ((time % this.blink_duration) >= (this.blink_duration / 2)) {
				this.blink_state = 2;
			} else {
				this.blink_state = 1;
			}
		} else {
			this.blink_state = 0;
		}

		this.x_offset = Math.round((this.cw - this.pw*this.scale) / 2);
		this.y_offset = Math.round((this.ch - this.ph*this.scale) / 2);
	}

	_drawChar(targetCtx, x, y, chr, col) {
		switch (this.blink_state) {
			case 0:
				break;
			case 1:
				col = col & 0x7F;
				break;
			case 2:
				if (col >= 0x80) {
					col = ((col & 0x70) >> 4) * 0x11;				}
				break;
		}

		const buffered = this.chrBuf[y * 80 + x];
		const bufcmp = (chr << 8) | col;

		if (buffered === bufcmp) {
			return;
		} else {
			this.chrBuf[y * 80 + x] = bufcmp;
		}

		x = x * this.chrWidth;
		y = y * this.chrHeight;

		const bg = (col >> 4) & 15;
		const fg = (col & 15);

		targetCtx.fillStyle = this.palette[bg];
		targetCtx.fillRect(x, y, this.chrWidth, this.chrHeight);

		if (bg != fg) {
			targetCtx.drawImage(this.asciiFg[fg], (chr & 15) * this.chrWidth, ((chr >> 4) & 15) * this.chrHeight, this.chrWidth, this.chrHeight, x, y, this.chrWidth, this.chrHeight);
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

		for (var i = 0; i < 16; i++) {
			if (this.palette[i].toLowerCase() == '#ffffff') {
				this.asciiFg[i] = srcImg;
				continue;
			}
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

		this.chrBuf = [];
		this.rdDirty = false;
	};

	render(heap, mode, time) {
		if (this.rdDirty) this._updRenderData();
		this._updVideoMode(mode, time);

		let pos = 0;
		let xScale = Math.floor(80 / this.scrWidth);
		var targetCanvas = this.canvas;
		var targetCtx = this.ctx;

		if (this.scale > 1 || xScale > 1 || this.x_offset != 0 || this.y_offset != 0) {
			targetCanvas = this.offscreenCanvas;
			targetCtx = this.offscreenCtx;
		}

		if (this.asciiFg != null && this.palette != null) {
			for (var y = 0; y < 25; y++) {
				for (var x = 0; x < this.scrWidth; x++, pos+=2) {
					this._drawChar(targetCtx, x, y, heap[pos], heap[pos+1]);
				}
			}
		}

		if (targetCtx != this.ctx) {
			this.ctx.fillStyle = "#000000";
			this.ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);
			this.ctx.drawImage(targetCanvas, 0, 0, 80*this.chrWidth / xScale, 25*this.chrHeight, this.x_offset, this.y_offset, 80*this.chrWidth * this.scale, 25*this.chrHeight * this.scale);
		}
	}

	setBlinkEnabled(blink) {
		if (this.blink_duration > 0) {
			this.video_blink = blink != 0;
		}
	}

	setBlinkCycleDuration(duration) {
		this.blink_duration = Math.round(duration * 1000);
		this.video_blink = this.blink_duration > 0;
	}

	setCharset(width, height, heap) {
		this.chrWidth = width;
		this.chrHeight = height;
		this.charset = heap.slice(0);
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
