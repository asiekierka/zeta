# Zeta

Zeta is a small emulator, implementing a fraction of a DOS-compatible environment just large enough to run ZZT and Super ZZT, allowing user-friendly and accurate usage 
of them in modern environments, as well as easy packaging, distribution and embedding of ZZT games.

## Compiling (Autotools, recommended)

`./autogen.sh && mkdir build && cd build && ../configure --with-frontend=sdl2 && make`

You're going to need the development headers/package for SDL2 (`sdl2-devel` on Fedora, `libsdl2-dev` on Debian/Ubuntu, `sdl2` on Arch, etc).

## Compiling (Makefile)

Use `make PLATFORM=platform`, where platform is usually of the form "platform-frontend".

### Recommended platforms

* macos-sdl2 - SDL2 frontend, macOS
* macos-sdl3 - SDL3 frontend, macOS
* mingw32-sdl3 - SDL3 frontend, Windows
* unix-sdl2 - SDL2 frontend, Linux
* unix-sdl3 - SDL3 frontend, Linux
* wasm - WebAssembly library

### Unsupported

* unix-curses - Curses frontend, Linux

### HTML5 version

On top of building the WebAssembly library, some extra files are required:

 * zeta.js (renamed to zeta.min.js) - compiled by running `npm run build` in web/
 * index.html, loader, libraries - available in web/res/

## License

Zeta is available under the terms of the MIT license, as described in `LICENSE`.
