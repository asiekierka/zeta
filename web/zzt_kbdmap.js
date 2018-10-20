/*!
 * Copyright (c) 2018 Adrian Siekierka
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

zzt_kbdmap = {};
zzt_kbdmap["ArrowUp"] = 0x48;
zzt_kbdmap["ArrowLeft"] = 0x4B;
zzt_kbdmap["ArrowRight"] = 0x4D;
zzt_kbdmap["ArrowDown"] = 0x50;

zzt_kbdmap["Home"] = 0x47;
zzt_kbdmap["End"] = 0x4F;
zzt_kbdmap["Insert"] = 0x52;
zzt_kbdmap["Delete"] = 0x53;
zzt_kbdmap["PageUp"] = 0x49;
zzt_kbdmap["PageDown"] = 0x51;

zzt_kbdmap["Enter"] = 0x1C;
zzt_kbdmap["Escape"] = 1;

zzt_kbdmap["-"] = 12;
zzt_kbdmap["_"] = 12;
zzt_kbdmap["="] = 13;
zzt_kbdmap["+"] = 13;
zzt_kbdmap["Backspace"] = 14;
zzt_kbdmap["Tab"] = 15;

var addCharsInOrder = function(c, off) {
	for(var i = 0; i < c.length; i++) {
		zzt_kbdmap[c.charAt(i)]=off+1+i;
	}
}

addCharsInOrder("1234567890", 0);
addCharsInOrder("QWERTYUIOP{}", 15);
addCharsInOrder("qwertyuiop[]", 15);
addCharsInOrder("asdfghjkl;'", 29);
addCharsInOrder("ASDFGHJKL:\"", 29);
addCharsInOrder("zxcvbnm,./", 43);
addCharsInOrder("ZXCVBNM<>?", 43);

zzt_kbdmap["`"] = 41;
zzt_kbdmap["~"] = 41;
zzt_kbdmap["\\"] = 43;
zzt_kbdmap["|"] = 43;
zzt_kbdmap["*"] = 55;
zzt_kbdmap[" "] = 57;
