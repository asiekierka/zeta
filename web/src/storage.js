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

import { getIndexedDB, getLocalStorage, drawErrorMessage, xhrFetchAsArrayBuffer } from "./util";

function filterKeys(list, filter) {
	if (filter == null) return list;

	let newList = [];
	for (var i = 0; i < list.length; i++) {
		const key = list[i];
		if (filter(key)) newList.push(key);
	}
	return newList;
}

class BaseStorage {
	constructor(options) {
		this.ignoreCase = (options && options.ignoreCase) || false;
		this.readonly = (options && options.readonly) || false;
	}

	_transformKey(key) {
		if (this.ignoreCase == "upper") return key.toUpperCase();
		else if (this.ignoreCase) return key.toLowerCase();
		else return key;
	}

	canSet(key) {
		if (this.readonly) return false;
		if (this._canSet) return this._canSet(this._transformKey(key));
		return true;
	}

	get(key) {
		return this._get(this._transformKey(key));
	}

	list(filter) {
		return this._list(filter);
	}

	set(key, value) {
		if (this.readonly) return false;
		return this._set(this._transformKey(key), value);
	}
}

class CompositeStorage {
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
		return data.sort().filter(function(item, i, sortedData) {
			return (i <= 0) || (item != sortedData[i - 1]);
		});
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

class InMemoryStorage extends BaseStorage {
	constructor(inputMap, options) {
		super(options);

		this.map = {};
		for (var key in inputMap) {
			this.map[this._transformKey(key)] = inputMap[key];
		}
	}

	_get(key) {
		if (!this.map.hasOwnProperty(key)) {
			return null;
		}
		return this.map[key].slice(0);
	}

	_list(filter) {
		return filterKeys(Object.keys(this.map), filter);
	}

	_set(key, value) {
		this.map[key] = value;
		return true;
	}
}

class AsyncStorageWrapper extends InMemoryStorage {
	constructor(parent, options) {
		super({}, options);
		this.parent = parent;
	}

	_populate() {
		const self = this;

		return this.parent.list(null).then(result => {
			self.map = {};

			let getters = [];
			for (var i = 0; i < result.length; i++) {
				const key = result[i];
				getters.push(self.parent.get(key).then(result => {
					this._set(key, result);
				}));
			}

			return Promise.all(getters);
		});
	}

	_set(key, value) {
		if (super._set(key, value)) {
			this.parent.set(key, value);
			return true;
		} else {
			return false;
		}
	}
}

class BrowserBackedStorage extends BaseStorage {
	constructor(localStorage, prefix, options) {
		super(options);
		this.storage = localStorage;
		this.prefix = prefix + "_file_";
	}

	_get(key) {
		const result = this.storage.getItem(this.prefix + key.toUpperCase());
		if (result) {
			return result.split(",").map(s => parseInt(s));
		} else {
			return null;
		}
	}

	_list(filter) {
		var list = [];
		for (var i = 0; i < this.storage.length; i++) {
			const key = this.storage.key(i);
			if (key.startsWith(this.prefix)) {
				list.push(key.substring(this.prefix.length));
			}
		}
		return filterKeys(list, filter);
	}

	_set(key, value) {
		this.storage.setItem(this.prefix + key, value.join(","));
		return true;
	}
}

class IndexedDbBackedAsyncStorage {
	constructor(indexedDB, dbName) {
		this.indexedDB = indexedDB;
		this.dbName = dbName;
	}

	_open() {
		const self = this;
		return new Promise((resolve, reject) => {
			var dbRequest = self.indexedDB.open(this.dbName, 1);
			dbRequest.onupgradeneeded = event => {
				self.database = event.target.result;
				self.files = self.database.createObjectStore("files", {"keyPath": "filename"});
			}
			dbRequest.onsuccess = event => {
				self.database = dbRequest.result;
				resolve();
			}
			dbRequest.onerror = event => {
				reject();
			}
		});
	}

	canSet(key) {
		return Promise.resolve(true);
	}

	get(key) {
		const transaction = this.database.transaction(["files"], "readonly");
		return new Promise((resolve, reject) => {
			const request = transaction.objectStore("files").get(key.toUpperCase());
			request.onsuccess = event => {
				if (request.result && request.result.value) resolve(request.result.value);
				else resolve(null);
			}
			request.onerror = event => {
				resolve(null);
			}
		});
	}

	list(filter) {
		const transaction = this.database.transaction(["files"], "readonly");
		return new Promise((resolve, reject) => {
			const request = transaction.objectStore("files").getAllKeys();
			request.onsuccess = event => {
				resolve(filterKeys(request.result, filter));
			}
			request.onerror = event => {
				resolve([]);
			}
		});
	}

	set(key, value) {
		const transaction = this.database.transaction(["files"], "readwrite");
		return new Promise((resolve, reject) => {
			const request = transaction.objectStore("files").put({
				"filename": key,
				"value": value
			});
			request.onsuccess = event => {
				resolve(true);
			}
			request.onerror = event => {
				resolve(false);
			}
		});
	}
}

export function createBrowserBackedStorage(storage, dbName, options) {
	return new BrowserBackedStorage(storage, dbName, options);
}

export function createIndexedDbBackedAsyncStorage(dbName) {
	const indexedDB = getIndexedDB();
	if (!indexedDB) {
		return Promise.reject("IndexedDB not supported!");
	}
	const dbObj = new IndexedDbBackedAsyncStorage(indexedDB, dbName);
	return dbObj._open().then(_ => dbObj);
}

export function wrapAsyncStorage(asyncVfs, options) {
	const obj = new AsyncStorageWrapper(asyncVfs, options);
	return obj._populate().then(_ => obj);
}

export function createInMemoryStorage(inputMap, options) {
	return new InMemoryStorage(inputMap, options);
}

export function createCompositeStorage(providers, options) {
	return new CompositeStorage(providers, options);
}

export function createZipStorage(url, options, progressCallback) {
	return xhrFetchAsArrayBuffer(url, progressCallback)
		.then(xhr => new Promise((resolve, reject) => {
			let files;
			let fileMap = {};

			try {
				files = UZIP.parse(xhr.response);
			} catch (e) {
				reject(e);
				return;
			}

			for (var key in files) {
				const keyOrig = key;

				if (options && options.filenameMap) {
					if (typeof(options.filenameMap) === "object") {
						key = options.filenameMap[key] || undefined;
					} else if (typeof(options.filenameMap) === "string" && options.filenameMap.length > 0) {
						let cmpStr = options.filenameMap.toLowerCase();
						if (!cmpStr.endsWith("/")) cmpStr += "/";

						if (key.toLowerCase().startsWith(cmpStr)) {
							key = key.substring(cmpStr.length);
						} else {
							key = undefined;
						}
					} else if (typeof(options.filenameMap) === "function") {
						key = options.filenameMap(key);
					}
				}

				if (key) {
					if (key.indexOf("/") >= 0) {
						console.warn("skipped file inside subdirectory: " + url + " -> " + key);
						key = undefined;
					}
				}

				if (key) {
					if (!(options && options.filenameFilter) || options.filenameFilter(key)) {
						fileMap[key] = files[keyOrig];
					}
				}
			}

			resolve(new InMemoryStorage(fileMap, options));
		}));
}