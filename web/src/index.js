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

import { OscillatorBasedAudio } from "./audio.js";
import { CanvasBasedRenderer } from "./render.js";
import { createVfsFromMap, createVfsFromVfs, createVfsFromZip, wrapVfsSynchronously, createAsyncVfsFromIndexedDB, createVfsFromBrowserStorage } from "./vfs.js";
import { createEmulator } from "./emulator.js";
import { getIndexedDB, getLocalStorage } from "./util.js";

window.ZetaInitialize = function(options) {
    if (!options.render) throw "Missing option: render!";
	if (!options.render.canvas) throw "Missing option: render.canvas!";
	if (!options.path) throw "Missing option: path!";
    if (!options.files) throw "Missing option: files!";

	const canvas = options.render.canvas;
	canvas.contentEditable = true;
	const ctx = canvas.getContext('2d', {alpha: false});
	ctx.imageSmoothingEnabled = false;

    // TODO: bring back loading screen

    var vfsPromises = [];
    var vfsProgresses = [];
    var vfsObjects = [];

    for (var s in options.files) {
        vfsProgresses.push(0);
        const file = options.files[s]
		if (Array.isArray(file)) {
            var opts = file[1];
            if (!opts.hasOwnProperty("readonly")) {
                opts.readonly = true;
            }
            vfsPromises.push(
                createVfsFromZip(file[0], opts, null)
                    .then(o => vfsObjects.push(o))
            );
        } else {
            vfsPromises.push(
                createVfsFromZip(file, {"readonly": true}, null)
                    .then(o => vfsObjects.push(o))
            );
        }
	}

	return Promise.all(vfsPromises).then(_ => {
        // add storage vfs
        if (options.storage.type == "auto") {
            if (getIndexedDB() != null) {
                options.storage.type = "indexeddb";
            } else if (getLocalStorage() != null) {
                options.storage.type = "localstorage";
            } else {
                console.log("Browser does not support any form of local storage! Storing to memory...");
                options.storage = undefined;
            }
        }
        
        if (options.storage && options.storage.type) {
            if (options.storage.type == "indexeddb") {
                if (!options.storage.database) throw "Missing option: storage.database!";
                return createAsyncVfsFromIndexedDB("zeta_" + options.storage.database)
                    .then(result => wrapVfsSynchronously(result))
                    .then(result => vfsObjects.push(result));
            } else if (options.storage.type == "localstorage") {
                if (!options.storage.database) throw "Missing option: storage.database!";
                let storageObj = window.localStorage;
                if (options.storage.storage) storageObj = options.storage.storage;
                if (storageObj == undefined) throw "Could not find storage object!";
                vfsObjects.push(createVfsFromBrowserStorage(storageObj, "zeta_" + options.storage.database));
                return true;
            } else {
                throw "Unknown storage type: " + options.storage.type;
            }
        } else {
            vfsObjects.push(createVfsFromMap({}, {"readonly": false}));
            return true;
        }
    }).then(_ => {
        // initialize emulator

        const render = new CanvasBasedRenderer(options.render.canvas);
        const audio = new OscillatorBasedAudio();
        const vfs = createVfsFromVfs(vfsObjects);

        return createEmulator(render, audio, vfs, options);
    }).then(_ => true);
}