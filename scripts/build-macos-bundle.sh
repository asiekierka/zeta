#!/bin/bash
set -e

BUILDDIR=.macos_bundle

if [ -f build-macos-bundle.sh ]; then
	cd ..
fi
if [ -d "$BUILDDIR" ]; then
	rm -r "$BUILDDIR"
fi
mkdir -p "$BUILDDIR"/Zeta.app/Contents/Frameworks/SDL3.framework/Versions/A
mkdir -p "$BUILDDIR"/Zeta.app/Contents/MacOS
mkdir -p "$BUILDDIR"/Zeta.app/Contents/Resources

for ARCH in x86_64 arm64; do
	CFLAGS="-iframework $HOME/Library/Frameworks/SDL3.xcframework/macos-arm64_x86_64" \
	LDFLAGS="-iframework $HOME/Library/Frameworks/SDL3.xcframework/macos-arm64_x86_64" \
	meson "$BUILDDIR"/"$ARCH" --cross-file scripts/cross-darwin-"$ARCH".txt
	cd "$BUILDDIR"/"$ARCH"
	meson compile
	cp zeta ../Zeta.app/Contents/MacOS/zeta-"$ARCH"
	cd ../Zeta.app/Contents/MacOS
	install_name_tool -add_rpath @executable_path/../Frameworks zeta-"$ARCH"
	cd ../../../..
done

cp -aR $HOME/Library/Frameworks/SDL3.xcframework/macos-arm64_x86_64/SDL3.framework/Versions/A/SDL3 "$BUILDDIR"/Zeta.app/Contents/Frameworks/SDL3.framework/Versions/A/
cp res/macos/Info.plist "$BUILDDIR"/Zeta.app/Contents/
cp res/macos/zeta.icns "$BUILDDIR"/Zeta.app/Contents/Resources/
cp res/macos/zeta.sh "$BUILDDIR"/Zeta.app/Contents/MacOS/
chmod +x "$BUILDDIR"/Zeta.app/Contents/macOS/zeta.sh

cd "$BUILDDIR"/Zeta.app/Contents/MacOS
lipo -create -output zeta zeta-x86_64 zeta-arm64
rm zeta-*
