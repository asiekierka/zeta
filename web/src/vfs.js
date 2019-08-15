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

class MapBasedVfs {
	constructor(inputMap, options) {
		this.map = {};
		for (var key in inputMap) {
			this.map[key.toUpperCase()] = inputMap[key];
		}
		this.readonly = (options && options.readonly) || false;
	}

	canSet(key) {
		return this.readonly;
	}

	get(key) {
		if (!this.map.hasOwnProperty(key.toUpperCase())) {
			return null;
		}
		return this.map[key.toUpperCase()].slice(0);
	}

	list(filter) {
		let a = [];

		for (var key in this.map) {
			if (filter == null || filter(key)) {
				a.push(key);
			}
		}

		return a;
	}

	set(key, value) {
		if (this.readonly) return false;
		this.map[key.toUpperCase()] = value;
		return true;
	}
}

class VfsBasedVfs {
	constructor(providers) {
		this.providers = providers;
	}

	canSet(key) {
		for (var p = 0; p < this.providers.length; p++) {
			let provider = this.providers[p];
			if (provider.canSet(key)) {
				return true;
			}
		}
		return false;
	}

	get(key) {
		for (var p = this.providers.length - 1; p >= 0; p--) {
			let provider = this.providers[p];
			const result = provider.get(key);
			if (result != null) return result;
		}
		return null;
	}

	list(filter) {
		let data = [];
		for (var p = 0; p < this.providers.length; p++) {
			let provider = this.providers[p];
			data = data.concat(provider.list(filter));
		}
		return data.sort();
	}

	set(key, value) {
		let promise = Promise.resolve(false);
		for (var p = this.providers.length - 1; p >= 0; p--) {
			let provider = this.providers[p];
			if (provider.set(key, value)) return true;
		}
		return false;
	}
}

export function createVfsFromMap(inputMap, options) {
	return new MapBasedVfs(inputMap, options);
}

export function createVfsFromVfs(providers) {
	return new VfsBasedVfs(providers);
}

export function createVfsFromZip(url, options, progressCallback) {
	return new Promise(resolve => {
		var xhr = new XMLHttpRequest();
		xhr.open("GET", url, true);
		xhr.responseType = "arraybuffer";

		xhr.onprogress = function(event) {
			if (progressCallback != null) progressCallback(event.loaded / event.total);
		};

		xhr.onload = function() {
			console.log(this.response);
			if (progressCallback != null) progressCallback(1);

			let files = UZIP.parse(this.response);
			let fileMap = {};
			for (var key in files) {
				if (!(options && options.filenameFilter) || options.filenameFilter(key)) {
					fileMap[key] = files[key];
				}
			}
			resolve(new MapBasedVfs(fileMap, options));
		};

		xhr.send();
	});
}
