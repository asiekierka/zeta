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

ZetaKbdmap = {
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

(function() {
	var addCharsInOrder = function(c, off) {
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

	for (var i = 1; i <= 10; i++) {
		ZetaKbdmap["F" + i] = 0x3A + i;
	}
})();
