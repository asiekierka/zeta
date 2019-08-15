#!/bin/sh
if [ ! -d build ]; then mkdir build; fi
if [ ! -d build/web ]; then mkdir build/web; fi

rm build/zeta_native.js
rm build/zeta_native.js.mem
rm build/zeta_native.wasm
rm build/zeta_native.wasm.map
rm build/zeta_native.wast

xxd -i res/8x14.bin > res/8x14.c

emcc -O3 --js-library src/emscripten_glue.js \
  --llvm-lto 2 \
  -s ENVIRONMENT=web \
  -s 'EXPORTED_FUNCTIONS=["_malloc"]' \
  -s 'EXTRA_EXPORTED_RUNTIME_METHODS=["AsciiToString"]' \
  -s WASM=1 -s MODULARIZE=1 -s 'EXPORT_NAME="ZetaNative"' \
  -s ALLOW_MEMORY_GROWTH=0 -s ASSERTIONS=0 \
  -s 'MALLOC="emmalloc"' -s NO_FILESYSTEM=1 \
  -s TOTAL_MEMORY=4194304 -s TOTAL_STACK=262144 \
  -s DEFAULT_LIBRARY_FUNCS_TO_INCLUDE="[]" \
  -DNO_MEMSET \
  src/cpu.c src/zzt.c res/8x14.c src/audio_stream.c

mv a.out.js build/zeta_native.js
mv a.out.wasm build/zeta_native.wasm
sed -i -e "s/a\.out/zeta_native/g" build/zeta_native.js

rm build/web/*
cp web/* build/web/
cp build/zeta_native.js build/web/
cp build/zeta_native.wasm build/web/
