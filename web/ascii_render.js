AsciiRender = {};
AsciiRender.toCanvas = function(canvas, options) {
	var ctx = canvas.getContext('2d', {alpha: false});
	ctx.imageSmoothingEnabled = false;

	var video_blink = (options && options.blink) || false;
	var video_mode = -1;
	var chrBuf = [];
	var chrWidth, scrWidth;
	var chrHeight, scrHeight;
	var pw, ph, cw, ch;
	var scale = 1;

	var asciiFg = null;
	var charset = null;
	var palette = null;
	var rdDirty = false;

	var updVideoMode = function(val) {
		if (val != video_mode) {
			chrBuf = [];
			chrWidth = 16;
			scrWidth = 40;
			if ((video_mode & 0x02) == 2) {
				chrWidth = 8;
				scrWidth = 80;
			}
			video_mode = val;
		}

		pw = scrWidth*chrWidth;
		ph = 25*14;
		cw = canvas.width;
		ch = canvas.height;
		scale = Math.min(Math.floor(cw / pw), Math.floor(ch / ph));
	}

	var drawChar = function(x, y, chr, col, time) {
		x = x * chrWidth;
		y = y * 14;

		var buffered = chrBuf[y * 80 + x];
		if (video_blink && col >= 0x80) {
			col = col & 0x7F;

			if ((time % 466) >= 233) {
				col = (col >> 4) * 0x11;
			}
		}

		var bufcmp = (chr << 8) | col;

		if (buffered == bufcmp) {
			return;
		} else {
			chrBuf[y * 80 + x] = bufcmp;
		}

		var bg = (col >> 4) & 0x0F;
		var fg = (col & 15);

		var rw = chrWidth;
		var rh = chrHeight;

		if (scale > 1) {
			rw *= scale;
			rh *= scale;
			x = (x*scale) + ((cw - pw*scale) / 2);
			y = (y*scale) + ((ch - ph*scale) / 2);
		} else {
			x += ((cw - pw) / 2);
			y += ((ch - ph) / 2);
		}

		ctx.fillStyle = palette[bg];
		ctx.fillRect(x, y, rw, rh);

		if (bg != fg) {
			ctx.drawImage(asciiFg[fg], (chr & 15) * 8, ((chr >> 4) & 15) * 14, 8, 14, x, y, rw, rh);
		}
	}

	var updRenderData = function() {
		if (charset == null || palette == null) {
			return;
		}

		asciiFg = [];
		var srcImg = null;

		if (srcImg == null) {
			var charCanvas = document.createElement('canvas');
			charCanvas.width = 128;
			charCanvas.height = 224;
			var charCtx = charCanvas.getContext('2d');
			var charData = charCtx.createImageData(128, 224);
			var data = charData.data;
			var dpos = 0;

			for (var ch = 0; ch < 256; ch++) {
				var rx = (ch & 0xF) * 8;
				var ry = (ch >> 4) * 14;
				console.log(rx + " " + ry + " " + ch + " " + dpos);
				for (var cy = 0; cy < 14; cy++, dpos++) {
					var co = ((ry + cy) * 128) + rx;
					var ctmp = charset[dpos];
					for (var cx = 0; cx < 8; cx++, co++, ctmp = ctmp << 1) {
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

		asciiFg[15] = srcImg;
		for (var i = 0; i < 15; i++) {
			var charCanvas = document.createElement('canvas');
			charCanvas.width = 128;
			charCanvas.height = 224;
			var charCtx = charCanvas.getContext('2d');
			charCtx.globalCompositeOperation = 'copy';
			charCtx.drawImage(srcImg, 0, 0);
			charCtx.globalCompositeOperation = 'source-in';
			charCtx.fillStyle = palette[i];
			charCtx.fillRect(0, 0, 128, 224);
			asciiFg[i] = charCanvas;
		}

		rdDirty = false;
	};

	return {
		render: function(heap, mode, time) {
			if (rdDirty) updRenderData();
			updVideoMode(mode);

			var width = 40;
			var pos = 0;
			if ((video_mode & 0x02) == 2) width = 80;

			if (asciiFg != null && palette != null) for (var y = 0; y < 25; y++) {
				for (var x = 0; x < width; x++, pos+=2) {
					drawChar(x, y, heap[pos], heap[pos+1], time);
				}
			}
		},

		setCharset: function(width, height, heap) {
			chrWidth = width;
			chrHeight = height;
			scrHeight = chrHeight * 25;

			charset = heap;
			rdDirty = true;
		},

		setPalette: function(heap) {
			palette = new Array();
			for (var i = 0; i < 16; i++) {
				var s = (heap[i] & 0xFFFFFF).toString(16);
				while (s.length < 6) s = "0" + s;
				palette[i] = "#" + s;
			}
			rdDirty = true;
		}
	};
}
