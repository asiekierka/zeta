/**
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

static u8 map_char_to_key[512];

static void init_map_char_to_key(void) {
	map_char_to_key[258] = 0x50; // down
	map_char_to_key[260] = 0x4B; // left
	map_char_to_key[259] = 0x48; // up
	map_char_to_key[261] = 0x4D; // right
	map_char_to_key[13] = 0x1C; // enter
	map_char_to_key[0] = 0;
	map_char_to_key[0x1C] = 1; // esc
	for (int i = 1; i <= 9; i++) map_char_to_key[i + 48] = i + 1;
	char* chrs = "!@#$%^&*()";
	for (int i = 0; i < strlen(chrs); i++) map_char_to_key[(int)chrs[i]] = i + 1;
	map_char_to_key[48] = 11; // numbers
	map_char_to_key[45] = 12; // -
	map_char_to_key[95] = 12; // _
	map_char_to_key[61] = 13; // =
	map_char_to_key[43] = 12; // +
	map_char_to_key[8] = 14; // backspace
	map_char_to_key[9] = 15; // tab
	chrs = "qwertyuiop[]";
	for (int i = 0; i < strlen(chrs); i++) map_char_to_key[(int)chrs[i]] = 15 + i;
	chrs = "QWERTYUIOP{}";
	for (int i = 0; i < strlen(chrs); i++) map_char_to_key[(int)chrs[i]] = 15 + i;
	map_char_to_key[13] = 28; // enter
	// 29?
	chrs = "asdfghjkl;'";
	for (int i = 0; i < strlen(chrs); i++) map_char_to_key[(int)chrs[i]] = 29 + i;
	chrs = "ASDFGHJKL:\"";
	for (int i = 0; i < strlen(chrs); i++) map_char_to_key[(int)chrs[i]] = 29 + i;
	map_char_to_key[96] = 41; // `
	map_char_to_key[126] = 41; // ~
	// 42?
	map_char_to_key[92] = 43;
	map_char_to_key[124] = 43; // |
	chrs = "zxcvbnm,./";
	for (int i = 0; i < strlen(chrs); i++) map_char_to_key[(int)chrs[i]] = 43 + i;
	chrs = "ZXCVBNM<>?";
	for (int i = 0; i < strlen(chrs); i++) map_char_to_key[(int)chrs[i]] = 43 + i;
	// 54?
	map_char_to_key[42] = 55; // *
	// 56?
	map_char_to_key[32] = 57; // space
}

static const char* map_char_to_unicode[] = {
 "\x20",
 "\u263A",
 "\u263B",
 "\u2665",
 "\u2666",
 "\u2663",
 "\u2660",
 "\u2022",
 "\u25D8",
 "\u25EF",
 "\u25D9",
 "\u2642",
 "\u2640",
 "\u266A",
 "\u266C",
 "\u263C",
 "\u25BA",
 "\u25C4",
 "\u2195",
 "\u203C",
 "\u00B6",
 "\u00A7",
 "\u25AC",
 "\u21A8",
 "\u2191",
 "\u2193",
 "\u2192",
 "\u2190",
 "\u2319",
 "\u2194",
 "\u25B2",
 "\u25BC",
 "\x20",
 "\x21",
 "\x22",
 "\x23",
 "\x24",
 "\x25",
 "\x26",
 "\x27",
 "\x28",
 "\x29",
 "\x2A",
 "\x2B",
 "\x2C",
 "\x2D",
 "\x2E",
 "\x2F",
 "\x30",
 "\x31",
 "\x32",
 "\x33",
 "\x34",
 "\x35",
 "\x36",
 "\x37",
 "\x38",
 "\x39",
 "\x3A",
 "\x3B",
 "\x3C",
 "\x3D",
 "\x3E",
 "\x3F",
 "\x40",
 "\x41",
 "\x42",
 "\x43",
 "\x44",
 "\x45",
 "\x46",
 "\x47",
 "\x48",
 "\x49",
 "\x4A",
 "\x4B",
 "\x4C",
 "\x4D",
 "\x4E",
 "\x4F",
 "\x50",
 "\x51",
 "\x52",
 "\x53",
 "\x54",
 "\x55",
 "\x56",
 "\x57",
 "\x58",
 "\x59",
 "\x5A",
 "\x5B",
 "\x5C",
 "\x5D",
 "\x5E",
 "\x5F",
 "\x60",
 "\x61",
 "\x62",
 "\x63",
 "\x64",
 "\x65",
 "\x66",
 "\x67",
 "\x68",
 "\x69",
 "\x6A",
 "\x6B",
 "\x6C",
 "\x6D",
 "\x6E",
 "\x6F",
 "\x70",
 "\x71",
 "\x72",
 "\x73",
 "\x74",
 "\x75",
 "\x76",
 "\x77",
 "\x78",
 "\x79",
 "\x7A",
 "\x7B",
 "\x7C",
 "\x7D",
 "\x7E",
 "\x7F",
 "\u00C7",
 "\u00FC",
 "\u00E9",
 "\u00E2",
 "\u00E4",
 "\u00E0",
 "\u00E5",
 "\u00E7",
 "\u00EA",
 "\u00EB",
 "\u00E8",
 "\u00EF",
 "\u00EE",
 "\u00EC",
 "\u00C4",
 "\u00C5",
 "\u00C9",
 "\u00E6",
 "\u00C6",
 "\u00F4",
 "\u00F6",
 "\u00F2",
 "\u00FB",
 "\u00F9",
 "\u00FF",
 "\u00D6",
 "\u00DC",
 "\u00A2",
 "\u00A3",
 "\u00A5",
 "\u20A7",
 "\u0192",
 "\u00E1",
 "\u00ED",
 "\u00F3",
 "\u00FA",
 "\u00F1",
 "\u00D1",
 "\u00AA",
 "\u00BA",
 "\u00BF",
 "\u2310",
 "\u00AC",
 "\u00BD",
 "\u00BC",
 "\u00A1",
 "\u00AB",
 "\u00BB",
 "\u2591",
 "\u2592",
 "\u2593",
 "\u2502",
 "\u2524",
 "\u2561",
 "\u2562",
 "\u2556",
 "\u2555",
 "\u2563",
 "\u2551",
 "\u2557",
 "\u255D",
 "\u255C",
 "\u255B",
 "\u2510",
 "\u2514",
 "\u2534",
 "\u252C",
 "\u251C",
 "\u2500",
 "\u253C",
 "\u255E",
 "\u255F",
 "\u255A",
 "\u2554",
 "\u2569",
 "\u2566",
 "\u2560",
 "\u2550",
 "\u256C",
 "\u2567",
 "\u2568",
 "\u2564",
 "\u2565",
 "\u2559",
 "\u2558",
 "\u2552",
 "\u2553",
 "\u256B",
 "\u256A",
 "\u2518",
 "\u250C",
 "\u2588",
 "\u2584",
 "\u258C",
 "\u2590",
 "\u2580",
 "\u03B1",
 "\u00DF",
 "\u0393",
 "\u03C0",
 "\u03A3",
 "\u03C3",
 "\u00B5",
 "\u03C4",
 "\u03A6",
 "\u0398",
 "\u03A9",
 "\u03B4",
 "\u221E",
 "\u03C6",
 "\u03B5",
 "\u2229",
 "\u2261",
 "\u00B1",
 "\u2265",
 "\u2264",
 "\u2320",
 "\u2321",
 "\u00F7",
 "\u2248",
 "\u00B0",
 "\u2219",
 "\u00B7",
 "\u221A",
 "\u207F",
 "\u00B2",
 "\u25A0",
 "\x20"
};
