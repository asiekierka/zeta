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

var canvas = null;
var ctx = null;

var scripts_array = [];
var zzt_opts = {};
var script_ldr = function() {
	if (scripts_array.length == 0) {
		zzt_emu_create(zzt_opts);
		return;
	}

	var s = scripts_array.shift();
	var scr = document.createElement("script");
	scr.onload = script_ldr;
	scr.src = s;
	document.body.appendChild(scr);
}

function zzt_emu_load(path, options) {
	var canvas = document.getElementById('zzt_canvas');
	canvas.tabindex=1;

	var ctx = canvas.getContext('2d', {alpha: false});

	zzt_opts = options;
	zzt_opts.path = path;

	var imgload = new Image();
	imgload.onload = function() {
		ctx.imageSmoothingEnabled = false;
		ctx.drawImage(imgload,0,0,320,175,0,0,640,350);
	};
	imgload.src = path+"loading.png";
	scripts_array = [
		path+"uzip.min.js",
		path+"zeta86.js",
		path+"zzt.min.js"
	];
	script_ldr();
}
