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

import { OscillatorBasedAudio, BufferBasedAudio } from "./audio.js";
import { CanvasBasedRenderer } from "./render.js";
import { createEmulator } from "./emulator.js";
import { getIndexedDB, getLocalStorage, drawErrorMessage } from "./util.js";
import { createInMemoryStorage, createBrowserBackedStorage, wrapAsyncStorage, createIndexedDbBackedAsyncStorage, createCompositeStorage, createZipStorage } from "./storage.js";

const VERSION = "@VERSION@";

class LoadingScreen {
    constructor(canvas, ctx, version, options) {
        this.canvas = canvas;
        this.ctx = ctx;
        this.loaded = false;
        this.path = options.path;

        const self = this;
    }

    _drawBackground() {
        const self = this;
        return new Promise((resolve, reject) => {
            const loadingImage = new Image();
            loadingImage.onload = function() {
                const versionStr = VERSION;
                const w = loadingImage.width;
                const h = loadingImage.height;
                self.ctx.drawImage(loadingImage,0,0,w,h,(self.canvas.width - w*2)/2,(self.canvas.height - h*2)/2,w*2,h*2);

                self.ctx.font = "16px sans-serif";
                self.ctx.fillStyle = "#aaaaaa";
                self.ctx.textBaseline = "bottom";
                self.ctx.fillText(versionStr, (self.canvas.width + w*2) / 2 - 6 - self.ctx.measureText(versionStr).width, (self.canvas.height + h*2) / 2 - 6);

                self.loaded = true;
                resolve(true);
            };
            loadingImage.src = self.path + "loading.png";
        });
    }

    progress(p) {
        if (!this.loaded) return;
        
        const canvas = this.canvas;
        const ctx = this.ctx;
        const cx = (canvas.width - 640) / 2;
        const cy = (canvas.height - 350) / 2;
    
        ctx.fillStyle = "#ff0000";
        ctx.fillRect(cx + 14*2, cy + 112*2, p * 292*2, 20);
    }
}

window.ZetaInitialize = function(options) {
    console.log("         _        \n _______| |_ __ _ \n|_  / _ \\ __/ _` |\n / /  __/ || (_| |\n/___\\___|\\__\\__,_|\n\n " + VERSION);

    if (!options.render) throw "Missing option: render!";
    if (!options.render.canvas) throw "Missing option: render.canvas!";

    const canvas = options.render.canvas;
    canvas.contentEditable = true;
    const ctx = canvas.getContext('2d', {alpha: false});
    ctx.imageSmoothingEnabled = false;
    
    try {
        if (!options.path) throw "Missing option: path!";
        if (!options.files) throw "Missing option: files!";
    } catch (e) {
        drawErrorMessage(canvas, ctx, e);
        return Promise.reject(e);
    }

    const loadingScreen = new LoadingScreen(canvas, ctx, VERSION, options);

    var vfsPromises = [];
    var vfsProgresses = [];
    var vfsObjects = [];

    for (var s in options.files) {
        vfsProgresses.push(0);
        const file = options.files[s];
        const i = vfsProgresses.length - 1;
        const progressUpdater = function(p) {
            vfsProgresses[i] = Math.min(p, 1);
            loadingScreen.progress(vfsProgresses.reduce((p, c) => p + c) / options.files.length);            
        }

		if (Array.isArray(file)) {
            var opts = file[1];
            if (!opts.hasOwnProperty("readonly")) {
                opts.readonly = true;
            }
            if (!opts.hasOwnProperty("ignoreCase")) {
                opts.ignoreCase = "upper";
            }
            vfsPromises.push(
                createZipStorage(file[0], opts, progressUpdater)
                    .then(o => vfsObjects.push(o))
            );
        } else {
            vfsPromises.push(
                createZipStorage(file, {"readonly": true, "ignoreCase": "upper"}, progressUpdater)
                    .then(o => vfsObjects.push(o))
            );
        }
	}

    return loadingScreen._drawBackground().then(_ => Promise.all(vfsPromises)).then(_ => {
        // add storage vfs
        if (options.storage && options.storage.type == "auto") {
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
                return createIndexedDbBackedAsyncStorage("zeta_" + options.storage.database)
                    .then(result => wrapAsyncStorage(result, {"ignoreCase": "upper"}))
                    .then(result => vfsObjects.push(result));
            } else if (options.storage.type == "localstorage") {
                if (!options.storage.database) throw "Missing option: storage.database!";
                let storageObj = window.localStorage;
                if (options.storage.storage) storageObj = options.storage.storage;
                if (storageObj == undefined) throw "Could not find storage object!";
                vfsObjects.push(createBrowserBackedStorage(storageObj, "zeta_" + options.storage.database, {"ignoreCase": "upper"}));
                return true;
            } else {
                throw "Unknown storage type: " + options.storage.type;
            }
        } else {
            vfsObjects.push(createInMemoryStorage({}, {"readonly": false, "ignoreCase": "upper"}));
            return true;
        }
    }).then(_ => {
        let render_type = (options && options.render && options.render.type) || "auto";
        let audio_type = (options && options.audio && options.audio.type) || "auto";

        let render, audio;
        
        render = (emu) => new CanvasBasedRenderer(options.render.canvas, options.render);
        
        if (audio_type == "oscillator") {
            audio = (emu) => new OscillatorBasedAudio(options.audio);
        } else {
            audio = (emu) => new BufferBasedAudio(emu, options.audio);
        }

        const vfs = createCompositeStorage(vfsObjects);

        return createEmulator(render, audio, vfs, options);
    }).then(_ => true).catch(reason => {
        drawErrorMessage(canvas, ctx, reason);
    });
}
