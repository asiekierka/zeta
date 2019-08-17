Zeta HTML5 port: (very) rough instructions

index.html provides an effective template of a Zeta instance which will span the whole browser frame.

Basic requirements:

* A canvas object with a width of or larger than 640x350, specified in options.render.canvas.
    * Integer scaling will be applied automatically based on the canvas width/height - CSS styling only resizes the final texture!
* A presence of all the files herein other than "index.html" in a certain directory.

The entrypoint is "ZetaLoad(options);".

Mandatory option keys:

* render.canvas: the element of the desired canvas.
* path: the (relative or absolute) path to the Zeta engine files.
* files: an array containing file entries to be loaded.

Optional option keys:

* storage: define the settings for persistent storage. If not present, save files etc. will be stored in memory, and lost with as little as a page refresh.
    * type: can be "auto" (preferred), "localstorage" or "indexeddb".
    * database: for all of the above, a required database name. If you're hosting multiple games on the same domain, you may want to make this unique.
* render:
    * type: the engine to use for video rendering; can be "auto" (preferred) or "canvas"
    * blink: true if video blinking should be enabled, false otherwise
    * blink_duration: the length of a full blink cycle, in milliseconds
    * charset_override: the location of a PNG image file (16x16 chars) overriding the character set, if present
* audio:
    * type: the engine to use for audio rendering; can be "auto" (preferred), "buffer" or "oscillator" (pre-beta15; deprecated)
    * bufferSize (buffer): the audio buffer size, in samples
    * sampleRate (buffer): the audio sampling rate, in Hz
    * volume: the volume of the outputted audio stream; 0.2 by default

File entries can be either a string (denoting the relative or absolute path to a .ZIP file), or an array of a string and an options object containing the following optional keys:

* filenameFilter: function which returns true if a given filename should be accepted. This runs after filenameMap.
* filenameMap: filename mapping - can be in one of three forms:
    * string - describes a subdirectory whose contents are loaded (paths are /-separated, like on Unix)
    * object - describes a mapping of ZIP filenames to target filesystem filenames; no other files are loaded
    * function - accepts a ZIP filename and returns a target filesystem filename; return "undefined" to not load a file
