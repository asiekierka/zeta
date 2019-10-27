# Zeta HTML5 port

## (Very) rough instructions

index.html provides an effective template of a Zeta instance which will span the whole browser frame.

## Basic requirements

* A canvas object with a width of or larger than 640x350, specified in options.render.canvas.
    * Integer scaling will be applied automatically based on the canvas width/height - CSS styling only resizes the final texture!
* A presence of all the files herein other than "index.html" in a certain directory.

The entrypoint is "ZetaLoad(options, callback);". The callback is optional, and returns an instance of the emulator.

## Option keys

### Mandatory

* render.canvas: the element of the desired canvas.
* path: the (relative or absolute) path to the Zeta engine files.
* files: an array containing file entries to be loaded.

### Optional

* arg: the argument string provided to ZZT's executable. Ignored if "commands" is present.
* commands: a list containing commands to be executed in order. This will override "arg" *and* ZZT execution - the final entry on the list is assumed to be ZZT! Allowed types:
    * strings of the form "executable name" (no argument!),
    * arrays of the following form: ["executable name", "argument string"], f.e. [["BQUEST.COM", ""], ["BQZZT.EXE", "BQUEST.ZZT"]].
* storage: define the settings for persistent storage. If not present, save files etc. will be stored in memory, and lost with as little as a page refresh.
    * type: can be "auto" (preferred), "localstorage" or "indexeddb".
    * database: for all of the above, a required database name. If you're hosting multiple games on the same domain, you may want to make this unique.
* engine:
    * charset: the character set (8x14 only!) to initially load, as one of:
        * string - filename of a Zeta-supported format,
        * array - of bytes as would be contained in such a .chr file.
    * palette: the palette, to initially load, as one of:
        * string - filename of a Zeta-supported format,
        * object, with the following fields:
            * min - minimum palette value in the array (f.e. 0),
            * max - maximum palette value in the array (f.e. 255),
            * colors - an array of sixteen colors, either:
                * 3-component arrays of range min - max (inclusive),
                * "#012345" or "#012"-format strings.
* render:
    * type: the engine to use for video rendering; can be "auto" (preferred) or "canvas"
    * blink_cycle_duration: the length of a full blink cycle, in seconds; <= 0 to disable; the default is 0.466
    * charset_override: the location of a PNG image file (16x16 chars) overriding the engine's character set, if present
* audio:
    * type: the engine to use for audio rendering; can be "auto" (preferred), "buffer" or "oscillator" (pre-beta15; deprecated)
    * bufferSize (buffer): the audio buffer size, in samples
    * sampleRate (buffer): the audio sampling rate, in Hz
    * noteDelay: the minimum note delay, in milliseconds; 1 is default
    * volume: the volume of the outputted audio stream (range 0.0 - 1.0); 0.2 by default

File entries can be either a string (denoting the relative or absolute path to a .ZIP file), or an array of a string and an options object containing the following optional keys:

* filenameFilter: function which returns true if a given filename should be accepted. This runs after filenameMap.
* filenameMap: filename mapping - can be in one of three forms:
    * string - describes a subdirectory whose contents are loaded (paths are /-separated, like on Unix)
    * object - describes a mapping of ZIP filenames to target filesystem filenames; no other files are loaded
    * function - accepts a ZIP filename and returns a target filesystem filename; return "undefined" to not load a file

## Public emulator methods

* emu.loadCharset(charset) - argument format as in options.emulator.charset. Returns true upon success.
* emu.loadPalette(palette) - argument format as in options.emulator.palette. Returns true upon success.
* emu.setBlinkCycleDuration(duration) - sets blink cycle duration in seconds. Returns true upon success.
* emu.setVolume(volume) - sets volume in range 0.0 - 1.0. Returns true upon success.
