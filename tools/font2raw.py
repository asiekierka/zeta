#!/usr/bin/python3
#
# Copyright (c) 2020 Adrian Siekierka
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
# RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
# CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
# CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

from PIL import Image
import struct, sys

in_filename = sys.argv[1]
glyph_width = int(sys.argv[2])
glyph_height = int(sys.argv[3])
format = sys.argv[4]
out_filename = sys.argv[5]

im = Image.open(in_filename).convert("RGBA")

with open(out_filename, "wb") as fp:
	v = 0
	vp = 0
	for ig in range(0, 256):
		igx = (ig & 31) * glyph_width
		igy = (ig >> 5) * glyph_height
		for iy in range(0, glyph_height):
			for ix in range(0, glyph_width):
				pxl = im.getpixel((igx+ix, igy+iy))
				if pxl[0] > 128:
					v = v | (128 >> vp)
				vp = vp + 1
				if vp >= 8:
					fp.write(struct.pack("<B", v))
					v = 0
					vp = 0
