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

let ZetaKbdCtrlMap = {};

let ZetaKbdChrMap = {
	8: 8,
	9: 9,
	13: 13,
	27: 27
}

let addCharsInOrder = function(c, off) {
	for(var i = 0; i < c.length; i++) {
		ZetaKbdmap[c.charAt(i)]=off + i;
	}
}

addCharsInOrder("1234567890", 2);
addCharsInOrder("QWERTYUIOP{}", 16);
addCharsInOrder("qwertyuiop[]", 16);
addCharsInOrder("asdfghjkl;'", 30);
addCharsInOrder("ASDFGHJKL:\"", 30);
addCharsInOrder("zxcvbnm,./", 44);
addCharsInOrder("ZXCVBNM<>?", 44);

for (var i = 1; i <= 26; i++) {
	ZetaKbdCtrlMap[String.fromCharCode(64+i)] = i;
	ZetaKbdCtrlMap[String.fromCharCode(96+i)] = i;
}

for (var i = 1; i <= 10; i++) {
	ZetaKbdmap["F" + i] = 0x3A + i;
}

export const keymap = ZetaKbdmap;
export const keyctrlmap = ZetaKbdCtrlMap;
export const keychrmap = ZetaKbdChrMap;
