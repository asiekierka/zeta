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
