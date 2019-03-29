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

Zeta = function(options, callback) {
	var options = options;
	if (!options.render) throw "Missing option: render!";
	if (!options.render.canvas) throw "Missing option: render.canvas!";
	if (!options.path) throw "Missing option: path!";
	if (!options.files) throw "Missing option: files!";

	var scripts_array = [];
	var script_ldr = function() {
		if (scripts_array.length == 0) {
			callback(zeta_emu_create(options));
		} else {
			var scrSrc = scripts_array.shift();
			var scr = document.createElement("script");
			scr.onload = script_ldr;
			scr.src = scrSrc;
			document.body.appendChild(scr);
		}
	}

	var canvas = options.render.canvas;
	canvas.tabindex=1;
	var ctx = canvas.getContext('2d', {alpha: false});

	var imgload = new Image();
	imgload.onload = function() {
		ctx.imageSmoothingEnabled = false;
		ctx.drawImage(imgload,0,0,320,175,(canvas.width - 640)/2,(canvas.height - 350)/2,640,350);
	};
	imgload.src = options.path+"loading.png";

	if (options.dev) {
		scripts_array = [
			options.path+"uzip.min.js",
			options.path+"zeta_native.js",
			options.path+"zeta_vfs.js",
			options.path+"zeta_render.js",
			options.path+"zeta_audio.js",
			options.path+"zeta_kbdmap.js",
			options.path+"zeta.js"
		];
	} else {
		scripts_array = [
			options.path+"uzip.min.js",
			options.path+"zeta_native.js",
			options.path+"zeta.min.js"
		];
	}

	script_ldr();
}
