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

ZetaLoad = function(options, callback) {
	var scripts_array = [];
	var script_ldr = function() {
		if (scripts_array.length == 0) {
			if (callback) {
				callback(ZetaInitialize(options));
			} else {
				ZetaInitialize(options);
			}
		} else {
			var scrSrc = scripts_array.shift();
			var scr = document.createElement("script");
			scr.onload = script_ldr;
			scr.src = scrSrc;
			document.body.appendChild(scr);
		}
	}

	scripts_array = [
		options.path+"uzip.min.js",
		options.path+"zeta_native.js",
		options.path+"zeta.min.js"
	];
	if (options.dev) {
		scripts_array[2] = options.path+"zeta.js"
	}

	script_ldr();
}
