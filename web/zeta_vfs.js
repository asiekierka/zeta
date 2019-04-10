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

var ZetaVfs = {};

ZetaVfs.fromMap = function(inMap, options) {
	var map = {};
	for (var key in inMap) {
		map[key.toUpperCase()] = inMap[key];
	}
	var readOnly = (options && options.readonly) || false;

	return {
		readonly: function() {
			return readOnly;
		},
		contains: function(key) {
			return map.hasOwnProperty(key.toUpperCase());
		},
		get: function(key) {
			if (!map.hasOwnProperty(key.toUpperCase())) return null;
			return map[key.toUpperCase()].slice(0);
		},
		list: function(filter) {
			var a = [];

			for (var key in map) {
				if (filter == null || filter(key)) {
					a.push(key);
				}
			}

			return a;
		},
		set: function(key, value) {
			if (readOnly) return false;
			map[key.toUpperCase()] = value;
			return true;
		}
	};
}

ZetaVfs.fromZip = function(url, filenameFilter, progressCallback, finishCallback) {
	var xhr = new XMLHttpRequest();
	xhr.open("GET", url, true);
	xhr.responseType = "arraybuffer";

	xhr.onprogress = function(event) {
		progressCallback(event.loaded / event.total);
	};

	xhr.onload = function() {
		console.log(this.response);
		progressCallback(1);

		var files = UZIP.parse(this.response);
		var fileMap = {};
		for (var key in files) {
			if (filenameFilter == null || filenameFilter(key)) {
				fileMap[key] = files[key];
			}
		}
		finishCallback(ZetaVfs.fromMap(fileMap));
	};

	xhr.send();
}

ZetaVfs.fromProviders = function(providers) {
	return {
		readonly: function() {
			for (var p = 0; p < providers.length; p++) {
				if (providers[p].readonly()) return true;
			}
			return false;
		},
		contains: function(key) {
			for (var p = 0; p < providers.length; p++) {
				if (providers[p].contains(key)) return true;
			}
			return false;
		},
		get: function(key) {
			for (var p = providers.length - 1; p >= 0; p--) {
				if (providers[p].contains(key)) return providers[p].get(key);
			}
			return null;
		},
		list: function(filter) {
			var data = [];
			for (var p = 0; p < providers.length; p++) {
				data = data.concat(providers[p].list(filter));
			}
			return data.sort();
		},
		set: function(key, value) {
			for (var p = providers.length - 1; p >= 0; p--) {
				if (providers[p].set(key, value)) return true;
			}
			return false;
		}
	};
};
