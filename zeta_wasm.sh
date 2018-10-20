#!/bin/sh
if [ ! -d build ]; then mkdir build; fi
if [ ! -d build/web ]; then mkdir build/web; fi

rm build/zeta86.js
rm build/zeta86.js.mem
rm build/zeta86.wasm
rm build/zeta86.wasm.map
rm build/zeta86.wast

WASM_BACKEND=1 emcc -O3 --js-library src/emscripten_glue.js \
  -s 'EXTRA_EXPORTED_RUNTIME_METHODS=["Pointer_stringify","setValue","getValue"]' \
  -s WASM=1 -s MODULARIZE=1 -s 'EXPORT_NAME="Zeta"' \
  -s ALLOW_MEMORY_GROWTH=0 -s ASSERTIONS=0 \
  -s 'MALLOC="emmalloc"' \
  -s TOTAL_MEMORY=4194304 -s TOTAL_STACK=262144 \
  -s DEFAULT_LIBRARY_FUNCS_TO_INCLUDE="[]" \
  src/cpu.c src/zzt.c

mv a.out.js build/zeta86.js
mv a.out.js.mem build/zeta86.js.mem
mv a.out.wasm build/zeta86.wasm
mv a.out.wasm.map build/zeta86.wasm.map
mv a.out.wast build/zeta86.wast
sed -i -e "s/a\.out/zeta86/g" build/zeta86.js

rm build/web/*
cp web/* build/web/
cp build/zeta86.js build/web/
cp build/zeta86.wasm build/web/
