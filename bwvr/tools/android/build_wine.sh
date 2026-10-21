#!/bin/bash
# Builds Wine for the headsets: the Unix side for Android on ARM64 (bionic,
# API 29) with the NDK, and the PE side for aarch64 and i386 with
# llvm-mingw, in the WoW64 mode (docs/android.md, "What the app cannot do
# yet"). Run it in a Linux of WSL2:
#
#   build_wine.sh <work folder>
#
# The work folder has these, unpacked (none of them is in the repo):
#   src/wine-11.0                                     the source of Wine
#   tools/android-ndk-r27d                            the NDK for Linux
#   tools/llvm-mingw-20261006-ucrt-ubuntu-22.04-x86_64
# The Linux needs build-essential, flex, bison, gettext and pkg-config.
#
# The script builds the tools of Wine for this Linux first (winebuild,
# widl, wrc, winegcc) in build/wine-tools, then the cross build in
# build/wine-android. Wine gets no display driver (the app of the mod
# draws), no X and no FreeType. The logs are in logs/.
set -u
W=${1:?the work folder}
NDK=$W/tools/android-ndk-r27d/toolchains/llvm/prebuilt/linux-x86_64/bin
MINGW=$W/tools/llvm-mingw-20261006-ucrt-ubuntu-22.04-x86_64/bin
SRC=$W/src/wine-11.0
for d in "$SRC" "$NDK" "$MINGW"; do
  [ -d "$d" ] || { echo "There is no $d."; exit 1; }
done
mkdir -p "$W/build/wine-tools" "$W/build/wine-android" "$W/logs"

echo "== The tools of Wine for this Linux"
cd "$W/build/wine-tools" || exit 1
"$SRC/configure" --enable-win64 --without-x --without-freetype --without-mingw \
  --disable-tests > "$W/logs/tools-configure.txt" 2>&1 || {
  echo "The configure of the tools failed. See logs/tools-configure.txt."; exit 1; }
make -j"$(nproc)" __tooldeps__ > "$W/logs/tools-make.txt" 2>&1 || {
  echo "The build of the tools failed. See logs/tools-make.txt."; exit 1; }
# wrc of a build outside the source looks for the NLS files beside its
# own build. __tooldeps__ does not put them there.
mkdir -p nls
cp "$SRC"/nls/*.nls nls/

echo "== Wine for Android ARM64, WoW64 (aarch64 and i386)"
export PATH="$NDK:$MINGW:$PATH"
cd "$W/build/wine-android" || exit 1
"$SRC/configure" --host=aarch64-linux-android \
  --with-wine-tools=../wine-tools --enable-archs=i386,aarch64 \
  --disable-wineandroid_drv --without-x --without-freetype --disable-tests \
  CC=aarch64-linux-android29-clang CXX=aarch64-linux-android29-clang++ \
  > "$W/logs/cross-configure.txt" 2>&1 || {
  echo "The configure of the cross build failed. See logs/cross-configure.txt."; exit 1; }
make -j"$(nproc)" > "$W/logs/cross-make.txt" 2>&1 || {
  echo "The cross build failed. See logs/cross-make.txt."; exit 1; }

R=$NDK/llvm-readelf
O=$MINGW/llvm-objdump
for f in loader/wine server/wineserver dlls/ntdll/ntdll.so; do
  echo "$f: $($R -h "$f" | sed -n 's/ *Machine: *//p')"
done
for f in dlls/wow64/aarch64-windows/wow64.dll dlls/ntdll/i386-windows/ntdll.dll; do
  echo "$f: $($O -f "$f" | grep -o 'file format [^ ]*')"
done
echo "== Wine is in $W/build/wine-android"
