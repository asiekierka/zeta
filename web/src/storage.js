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
		this.use83Names = (options && options.use83Names) || false;
		this.ignoreCase = (options && options.ignoreCase) || false;
		this.readonly = (options && options.readonly) || false;
	}

	_transformKey(key) {
		var newKey = key;

		if (this.ignoreCase == "upper") newKey = newKey.toUpperCase();
		else if (this.ignoreCase) newKey = newKey.toLowerCase();
			
		if (this.use83Names) {
			var key83 = "";
			for (var keyPart of newKey.split("\\")) {
				// TODO: Handle periods better.
				// https://docs.microsoft.com/en-us/openspecs/windows_protocols/ms-fscc/18e63b13-ba43-4f5f-a5b7-11e871b71f14
				var nkSplit = keyPart.split(".", 2);
				var findTildedName = false;
				if (nkSplit[0].length > 8) {
					findTildedName = true;
				}
				if (nkSplit.length >= 2 && nkSplit[1].length > 3) {
					nkSplit[1] = nkSplit[1].substring(0, 3);
				}
				if (findTildedName) {
					var i = 1;
					var fnConverted = nkSplit[0].replaceAll(" ", "");
					while (1) {
						var is = i.toString();
						nkSplit[0] = fnConverted.substring(0, 7 - is.length) + "~" + is;
						keyPart = nkSplit.join(".");
						if (this.get(key83 + (key83.length > 0 ? "\\" : "") + keyPart) == null) {
							break;
						}
						i = i + 1;
						if (i >= 1000) {
							throw new Error("Too many filenames! (at " + key + ")");
						}
					}
				} else {
					keyPart = nkSplit.join(".");
				}

				if (key83.length > 0) key83 += "\\";
				key83 += keyPart;
			}
			newKey = key83;
		}

		return newKey;
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
				reject("Could not open IndexedDB! Upgrade your browser or disable private/incognito mode.");
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
			const store = transaction.objectStore("files");
			if (typeof store.getAllKeys === 'function') {
				const request = store.getAllKeys();
				request.onsuccess = event => {
					resolve(filterKeys(request.result, filter));
				}
				request.onerror = event => {
					resolve([]);
				}
			} else {
				var result = [];
				var request;
				if (typeof store.openKeyCursor === 'function') {
					request = store.openKeyCursor();
				} else {
					request = store.openCursor();
				}
				request.onsuccess = event => {
					var cursor = event.target.result;
					if (cursor) {
						result.push(cursor.key);
						cursor.continue();
					} else {
						resolve(filterKeys(result, filter));
					}
				}
				request.onerror = event => {
					resolve(result);
				}
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

				if (options && options.filenameMapper) {
					if (typeof(options.filenameMapper) === "object") {
						key = options.filenameMapper[key] || undefined;
					} else if (typeof(options.filenameMapper) === "string" && options.filenameMapper.length > 0) {
						let cmpStr = options.filenameMapper.toLowerCase();
						if (!cmpStr.endsWith("/")) cmpStr += "/";

						if (key.toLowerCase().startsWith(cmpStr)) {
							key = key.substring(cmpStr.length);
						} else {
							key = undefined;
						}
					} else if (typeof(options.filenameMapper) === "function") {
						key = options.filenameMapper(key);
					}
				}

				if (key) {
					if (key.endsWith("/")) {
						continue;
					}
					key = key.replaceAll("/", "\\");
				}

				if (key) {
					if (!(options && options.filenameFilter) || options.filenameFilter(key)) {
						fileMap[key] = files[keyOrig];
					}
				}
			}

			resolve(createInMemoryStorage(fileMap, options));
		}));
}
