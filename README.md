# Zeta

Zeta consists of:

* an 8086/80186 emulation core, based on lunatic86,
* an emulation environment geared specifically towards running ZZT and Super ZZT.

Currently, it has the following front-ends:

* *curses* - use ZZT in a terminal (sort of),
* *sdl* - primary desktop frontend,
* *web* - utilizes the WASM-compiled version of Zeta to allow using Zeta inside a web browser.

## Directory structure

* build/ - contains build output files,
* src/ - contains the source code to the Zeta emulator, as well as the Curse 

## Implementing your own front-end

Refer to src/zzt.h. Functions marked USER_FUNCTION are accessible to you to interface with
the emulator core, while functions marked IMPLEMENT_FUNCTION should be implemented.

Certain methods can be dummied out:

* vfs_write - if you don't need file writing,
* speaker_on/speaker_off - if you don't emulate the PC Speaker,
* vfs_findfirst/vfs_findnext - if you don't want file lookup to work.

There is, unfortunately, little documentation at this time.

## License

The source code release of Zeta generally available under the terms of the GPLv3 license.
For different licensing terms, please contact me directly.

The binary copy available [here](https://github.com/asiekierka/zeta-z2) is for usage by
the [Museum of ZZT](http://museumofzzt.com/). It may not be used by any other entity, however it is used
by the Museum with explicit permission.
