#!/usr/bin/python3
#
# Copyright (c) 2018, 2019, 2020 Adrian Siekierka
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

import sys, struct
from PIL import Image

im = Image.open(sys.argv[1]).convert("RGB")
fp = open(sys.argv[2], "wb")
imgdata = [0] * (256*128)

for ic in range(256):
	for iy in range(10):
		for ix in range(6):
			icx = ix + ((ic & 31)*6)
			icy = iy + ((ic >> 5)*10)
			imx = ix + ((ic & 31)*8)
			imy = iy + ((ic >> 5)*16)
#			block_pos = ((imx >> 5) + ((imy >> 3) * 8)) * 128 + (imx & 31) + ((imy & 7) * 32)
			block_pos = imy*256 + imx
			ip = im.getpixel((icx, icy))
			if ip[0] >= 128 and ip[1] >= 128 and ip[2] >= 128:
				imgdata[block_pos] = 15

for iy in range(128):
	for ix in range(0, 256, 2):
		v = (imgdata[iy*256+ix+1] << 4) | (imgdata[iy*256+ix] << 0)
		fp.write(struct.pack("<B", v))

fp.close()
