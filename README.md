# Zeta

Zeta is a small emulator, implementing a fraction of a DOS-compatible environment just large enough to run ZZT and Super ZZT, allowing user-friendly and accurate usage 
of them in modern environments, as well as easy packaging, distribution and embedding of ZZT games.

## Compiling

Use `make PLATFORM=platform`, where platform is generally of the form platform-frontend.

### Recommended platforms

* mingw32-sdl - SDL frontend, Windows
* unix-sdl - SDL frontend, Linux
* wasm - WebAssembly library

### Unsupported

* unix-curses - Curses frontend, Linux

### HTML5 version

On top of building the WebAssembly library, some extra files are required:

 * zeta.js (renamed to zeta.min.js) - compiled by running `npm run build` in web/
 * index.html, loader, libraries - available in web/res/

## License

Zeta is available under the terms of the MIT license, as described in `LICENSE`.
