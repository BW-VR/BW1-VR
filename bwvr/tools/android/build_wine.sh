#!/bin/bash
# Builds Wine for the headset app in the WoW64 mode (docs/android.md, "Wine
# in the app"): the Unix side for Android (bionic, API 29) with the NDK, and
# the PE side with llvm-mingw. Run it in a Linux of WSL2:
#
#   build_wine.sh <work folder> <abi>
#
# <abi> is one of:
#   aarch64  for the headsets: --enable-archs=i386,aarch64. The 32 bit code
#            runs in wowbox64.dll of Box64 (build_box64.ps1).
#   x86_64   for the Android emulator of the tests: --enable-archs=i386,x86_64.
#            The 32 bit code runs natively through wow64cpu.dll of Wine.
#
# The work folder has these, unpacked or as an archive (none of them is in
# the repo):
#   src/wine-11.0                                     the source of Wine
#   dl/freetype-2.13.3.tar.xz                         the source of FreeType
#   tools/android-ndk-r27d                            the NDK for Linux
#   tools/llvm-mingw-20261006-ucrt-ubuntu-22.04-x86_64
# The Linux needs build-essential, flex, bison, gettext and pkg-config.
#
# The steps, each in a build folder of its own:
#   src/wine-11.0           gets the patches of wine-patches/ one time.
#   build/freetype-host     FreeType for the tools of this Linux.
#   build/wine-tools        the tools of Wine for this Linux (winebuild, widl,
#                           wrc, winegcc, sfnt2fon). Configured one time.
#   build/freetype-<abi>    FreeType as a shared library, with the zlib of
#                           the NDK, no HarfBuzz, no PNG, no Brotli, no bzip2.
#                           Installed into build/freetype-<abi>/install.
#   build/wine-<abi>        Wine with that FreeType, no display driver of
#                           Wine (the app draws), no X, no debug data.
#   install/<abi>/wine      make install with a DESTDIR: the whole tree. Wine
#                           for Android puts bin and lib into a folder of the
#                           ABI of Android: wine/arm64-v8a or wine/x86_64,
#                           beside wine/share. The loader is
#                           lib/wine/<abi>-unix/wine of that folder, and
#                           libfreetype.so goes into its lib.
# pack_wine.sh makes the runtime tree of the APK from install/<abi>/wine.
# The logs are in logs/.
set -u
W=${1:?the work folder}
ABI=${2:?the ABI: aarch64 or x86_64}
# UNIX_CFLAGS: the code of the LDT in unix_private.h (only for x86) calls
# ffs(), and bionic declares it in strings.h, which Wine does not include.
# config.h defines _GNU_SOURCE, but strings.h comes before it, so the
# command line must define it too.
case "$ABI" in
  aarch64) ARCHS=i386,aarch64; APK_ABI=arm64-v8a; UNIX_CFLAGS=-O2 ;;
  x86_64)  ARCHS=i386,x86_64; APK_ABI=x86_64; UNIX_CFLAGS="-O2 -D_GNU_SOURCE -include strings.h" ;;
  *) echo "The ABI must be aarch64 or x86_64, not $ABI."; exit 1 ;;
esac
HOST=$ABI-linux-android
NDK=$W/tools/android-ndk-r27d/toolchains/llvm/prebuilt/linux-x86_64/bin
MINGW=$W/tools/llvm-mingw-20261006-ucrt-ubuntu-22.04-x86_64/bin
SRC=$W/src/wine-11.0
FT_VERSION=2.13.3
FT_ARCHIVE=$W/dl/freetype-$FT_VERSION.tar.xz
FT_SRC=$W/src/freetype-$FT_VERSION
JOBS=$(nproc)
for d in "$SRC" "$NDK" "$MINGW"; do
  [ -d "$d" ] || { echo "There is no $d."; exit 1; }
done
[ -d "$FT_SRC" ] || [ -f "$FT_ARCHIVE" ] || { echo "There is no $FT_ARCHIVE."; exit 1; }
mkdir -p "$W/build" "$W/logs" "$W/install"
LOG=$W/logs/$ABI

if [ ! -d "$FT_SRC" ]; then
  tar -C "$W/src" -xJf "$FT_ARCHIVE" || { echo "Cannot unpack $FT_ARCHIVE."; exit 1; }
fi

# THE PATCHES of wine-patches/ (beside this script) go into the source of
# Wine one time. Each patch says why in its head.
echo "== The patches of Wine"
HERE=$(cd "$(dirname "$0")" && pwd)
for p in "$HERE"/wine-patches/*.patch; do
  [ -f "$p" ] || continue
  if patch -d "$SRC" -p1 -R --dry-run -s -f < "$p" > /dev/null 2>&1; then
    echo "   $(basename "$p"): in the source already"
  elif patch -d "$SRC" -p1 -N -s < "$p"; then
    echo "   $(basename "$p"): applied"
  else
    echo "The patch $p does not apply to $SRC."; exit 1
  fi
done

# sfnt2fon of the tools makes the bitmap fonts (.fon) of Wine from the
# TrueType fonts, and it needs FreeType. This Linux has no FreeType for
# development, so the tools get a FreeType of the same source, with its own
# zlib. The configure of Wine looks for a shared library, and the tools
# find it through their RPATH.
echo "== FreeType $FT_VERSION for the tools of this Linux"
FTH=$W/build/freetype-host
if [ ! -f "$FTH/install/lib/libfreetype.so" ]; then
  rm -rf "$FTH"
  mkdir -p "$FTH"
  cd "$FTH" || exit 1
  "$FT_SRC/configure" --prefix="$FTH/install" --enable-shared --disable-static \
    --with-zlib=no --without-harfbuzz --without-png --without-brotli --without-bzip2 \
    > "$W/logs/host-freetype-configure.txt" 2>&1 &&
  make -j"$JOBS" > "$W/logs/host-freetype-make.txt" 2>&1 &&
  make install >> "$W/logs/host-freetype-make.txt" 2>&1 || {
    echo "The FreeType of the tools failed. See logs/host-freetype-*.txt."; exit 1; }
fi

echo "== The tools of Wine for this Linux"
mkdir -p "$W/build/wine-tools"
cd "$W/build/wine-tools" || exit 1
if [ ! -f .bwvr-freetype ]; then
  "$SRC/configure" --enable-win64 --without-x --without-mingw --disable-tests \
    FREETYPE_CFLAGS="-I$FTH/install/include/freetype2" \
    FREETYPE_LIBS="-L$FTH/install/lib -Wl,-rpath,$FTH/install/lib -lfreetype" \
    > "$W/logs/tools-configure.txt" 2>&1 || {
    echo "The configure of the tools failed. See logs/tools-configure.txt."; exit 1; }
  touch .bwvr-freetype
fi
make -j"$JOBS" __tooldeps__ > "$W/logs/tools-make.txt" 2>&1 || {
  echo "The build of the tools failed. See logs/tools-make.txt."; exit 1; }
# wrc of a build outside the source looks for the NLS files beside its
# own build. __tooldeps__ does not put them there.
mkdir -p nls
cp "$SRC"/nls/*.nls nls/

export PATH="$NDK:$MINGW:$PATH"
# No pkg-config of this Linux in a cross build: its files name the libraries
# of this Linux, not of Android.
export PKG_CONFIG_LIBDIR=/nonexistent

echo "== FreeType $FT_VERSION for $HOST"
FT=$W/build/freetype-$ABI
rm -rf "$FT"
mkdir -p "$FT"
cd "$FT" || exit 1
"$FT_SRC/configure" --host="$HOST" --prefix="$FT/install" \
  --enable-shared --disable-static \
  --with-zlib=yes --without-harfbuzz --without-png --without-brotli --without-bzip2 \
  CC="$HOST"29-clang CFLAGS=-O2 \
  ZLIB_CFLAGS= ZLIB_LIBS=-lz \
  > "$LOG-freetype-configure.txt" 2>&1 || {
  echo "The configure of FreeType failed. See logs/$ABI-freetype-configure.txt."; exit 1; }
make -j"$JOBS" > "$LOG-freetype-make.txt" 2>&1 && make install >> "$LOG-freetype-make.txt" 2>&1 || {
  echo "The build of FreeType failed. See logs/$ABI-freetype-make.txt."; exit 1; }
echo "libfreetype.so: SONAME $(llvm-readelf -d "$FT/install/lib/libfreetype.so" | sed -n 's/.*(SONAME).*\[\(.*\)\]/\1/p')"

echo "== Wine for $HOST, WoW64 ($ARCHS)"
B=$W/build/wine-$ABI
mkdir -p "$B"
cd "$B" || exit 1
"$SRC/configure" --host="$HOST" --prefix=/wine \
  --with-wine-tools=../wine-tools --enable-archs="$ARCHS" \
  --disable-wineandroid_drv --without-x --disable-tests \
  CC="$HOST"29-clang CXX="$HOST"29-clang++ \
  CFLAGS="$UNIX_CFLAGS" CXXFLAGS=-O2 CROSSCFLAGS=-O2 \
  FREETYPE_CFLAGS="-I$FT/install/include/freetype2" \
  FREETYPE_LIBS="-L$FT/install/lib -lfreetype" \
  > "$LOG-configure.txt" 2>&1 || {
  echo "The configure of Wine failed. See logs/$ABI-configure.txt."; exit 1; }
grep -E "^checking for -lfreetype|FreeType" "$LOG-configure.txt"
grep -E "define SONAME_LIBFREETYPE" include/config.h
# The audio driver of patch 0003 needs aaudio/AAudio.h and libaaudio of the
# NDK. With no driver the build has no sound, so the script stops.
grep -E "checking for AAudioStreamBuilder_openStream in -laaudio" "$LOG-configure.txt"
[ -f dlls/wineaaudio.drv/Makefile ] || {
  echo "The configure of Wine has no dlls/wineaaudio.drv. See logs/$ABI-configure.txt."; exit 1; }
make -j"$JOBS" > "$LOG-make.txt" 2>&1 || {
  echo "The build of Wine failed. See logs/$ABI-make.txt."; exit 1; }

echo "== make install into install/$ABI"
rm -rf "$W/install/$ABI"
make install DESTDIR="$W/install/$ABI" > "$LOG-install.txt" 2>&1 || {
  echo "make install failed. See logs/$ABI-install.txt."; exit 1; }
cp "$FT/install/lib/libfreetype.so" "$W/install/$ABI/wine/$APK_ABI/lib/"
cp "$FT_SRC/docs/FTL.TXT" "$W/install/$ABI/FTL.TXT"
cp "$SRC/COPYING.LIB" "$W/install/$ABI/COPYING.LIB"

T=$W/install/$ABI/wine/$APK_ABI
R=$NDK/llvm-readelf
O=$MINGW/llvm-objdump
for f in lib/wine/$ABI-unix/wine bin/wineserver lib/wine/$ABI-unix/ntdll.so lib/libfreetype.so \
         lib/wine/$ABI-unix/wineaaudio.so; do
  echo "$f: $($R -h "$T/$f" | sed -n 's/ *Machine: *//p')"
done
echo "lib/wine/$ABI-unix/wineaaudio.so needs:" $($R -d "$T/lib/wine/$ABI-unix/wineaaudio.so" |
  sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p')
for f in lib/wine/$ABI-windows/wow64.dll lib/wine/i386-windows/ntdll.dll \
         lib/wine/i386-windows/wineaaudio.drv lib/wine/$ABI-windows/wineaaudio.drv; do
  echo "$f: $($O -f "$T/$f" | grep -o 'file format [^ ]*')"
done
echo "== Wine is in $W/install/$ABI/wine ($(du -sh "$W/install/$ABI/wine" | cut -f1))"
