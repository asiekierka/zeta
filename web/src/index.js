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

import { OscillatorBasedAudio, BufferBasedAudio } from "./audio.js";
import { CanvasBasedRenderer } from "./render.js";
import { createEmulator } from "./emulator.js";
import { getIndexedDB, getLocalStorage, drawErrorMessage, xhrFetchAsArrayBuffer } from "./util.js";
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

window.ZetaInitialize = function(options, callback) {
    console.log("         _        \n _______| |_ __ _ \n|_  / _ \\ __/ _` |\n / /  __/ || (_| |\n/___\\___|\\__\\__,_|\n\n " + VERSION);

    if (!options.render) throw new Error("Missing option: render!");
    if (!options.render.canvas) throw new Error("Missing option: render.canvas!");

    const canvas = options.render.canvas;
    canvas.contentEditable = true;
    const ctx = canvas.getContext('2d', {alpha: false});
    ctx.imageSmoothingEnabled = false;
    
    try {
        if (!options.path) throw new Error("Missing option: path!");
        if (!options.files) throw new Error("Missing option: files!");
    } catch (e) {
        drawErrorMessage(canvas, ctx, e);
        return Promise.reject(e);
    }

    const loadingScreen = new LoadingScreen(canvas, ctx, VERSION, options);

    var vfsPromises = [];
    var vfsProgresses = [];
    var vfsObjects = [];

    try {
        for (var s in options.files) {
            vfsProgresses.push(0);

            (function(i, file) {
                const progressUpdater = function(p) {
                    vfsProgresses[i] = Math.min(p, 1);
                    loadingScreen.progress(vfsProgresses.reduce((p, c) => p + c) / options.files.length);
                }

                var opts = file;
                if (!opts.hasOwnProperty("type")) throw new Error("Missing option: files.type!");
                if (!opts.hasOwnProperty("readonly")) opts.readonly = true;
                if (!opts.hasOwnProperty("ignoreCase")) opts.ignoreCase = "upper";
                opts.use83Names = true;

                if (opts.type == "zip") {
                    if (!opts.hasOwnProperty("url")) throw new Error("Missing option: files.url!");
                    vfsPromises.push(
                        createZipStorage(opts.url, opts, progressUpdater)
                        .then(o => vfsObjects.push(o))
                    );
                } else if (opts.type == "array") {
                    if (!opts.hasOwnProperty("filename")) throw new Error("Missing option: files.filename!");
                    if (!opts.hasOwnProperty("data")) throw new Error("Missing option: files.data!");
                    var inMemoryData = {}
                    inMemoryData[opts.filename] = new Uint8Array(opts.data);
                    vfsObjects.push(createInMemoryStorage(inMemoryData, opts))
                } else if (opts.type == "file") {
                    if (!opts.hasOwnProperty("url")) throw new Error("Missing option: files.url!");
                    if (!opts.hasOwnProperty("filename")) throw new Error("Missing option: files.filename!");
                    vfsPromises.push(
                        xhrFetchAsArrayBuffer(opts.url, progressUpdater)
                        .then(xhr => {
                            var inMemoryData = {}
                            inMemoryData[opts.filename] = new Uint8Array(xhr.response);
                            vfsObjects.push(createInMemoryStorage(inMemoryData, opts))
                        })
                    )
                }
            })(vfsProgresses.length - 1, options.files[s]);
        }
    } catch (e) {
        drawErrorMessage(canvas, ctx, e);
        return Promise.reject(e);
    }

    loadingScreen._drawBackground().then(_ => Promise.all(vfsPromises)).then(_ => {
        // add storage vfs
        var useFallback = false;
        if (options.storage && options.storage.type == "auto") {
            useFallback = true;
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
            var storagePromise = null;
            if (options.storage.type == "indexeddb") {
                if (!options.storage.database) throw new Error("Missing option: storage.database!");
                storagePromise = createIndexedDbBackedAsyncStorage("zeta_" + options.storage.database)
                    .then(result => wrapAsyncStorage(result, {"ignoreCase": "upper"}))
                    .then(result => vfsObjects.push(result));
            } else if (options.storage.type == "localstorage") {
                storagePromise = new Promise(resolve => {
                    if (!options.storage.database) throw new Error("Missing option: storage.database!");
                    let storageObj = window.localStorage;
                    if (options.storage.storage) storageObj = options.storage.storage;
                    if (storageObj == undefined) throw new Error("Could not find storage object!");
                    vfsObjects.push(createBrowserBackedStorage(storageObj, "zeta_" + options.storage.database, {"ignoreCase": "upper"}));
                    return true;
                });
            } else {
                throw new Error("Unknown storage type: " + options.storage.type);
            }
            if (useFallback) {
                storagePromise = storagePromise.catch(reason => {
                     console.log("Browser failed to initialize local storage (type " + options.storage.type + ", reason: " + reason + "). Storing to memory...");
                     vfsObjects.push(createInMemoryStorage({}, {"readonly": false, "ignoreCase": "upper"}));
                     return true;
                });
            }
            return storagePromise;
        } else {
            vfsObjects.push(createInMemoryStorage({}, {"readonly": false, "ignoreCase": "upper"}));
            return true;
        }
    }).then(_ => {
        // clear canvas
        ctx.fillStyle = "#000000";
        ctx.fillRect(0, 0, canvas.width, canvas.height);

        // init emulator
        let render_type = (options && options.render && options.render.type) || "auto";
        let audio_type = (options && options.audio && options.audio.type) || "auto";

        let render, audio;
        
        render = (emu) => new CanvasBasedRenderer(emu, options.render.canvas, options.render);
        
        if (audio_type == "oscillator") {
            audio = (emu) => new OscillatorBasedAudio(options.audio);
        } else {
            audio = (emu) => new BufferBasedAudio(emu, options.audio);
        }

        const vfs = createCompositeStorage(vfsObjects);
        return createEmulator(render, audio, vfs, options).then(emu => {
            if (callback != null) callback(emu);
            return emu;
        });
    }).then(_ => true).catch(reason => {
        callback(undefined, reason);
        drawErrorMessage(canvas, ctx, reason);
    });
}
