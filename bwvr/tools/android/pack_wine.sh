#!/bin/bash
# Makes the Wine tree of the APK for one ABI from the install of
# build_wine.sh (docs/android.md, "Wine in the app"). Run it in a Linux of
# WSL2 after build_wine.sh:
#
#   pack_wine.sh <work folder> <abi>       <abi>: aarch64 or x86_64
#
# The tree has only what a 32 bit Windows program needs: the loader,
# wineserver, the Unix libraries, the PE DLLs and programs of the two
# architectures (with the audio driver wineaaudio of patch 0003),
# libfreetype.so and share/wine (wine.inf, the NLS files, the fonts of
# Wine). The script:
#   - leaves out the DLLs of kLeftOutNames and kLeftOutPrefix of
#     android/src/wine_rules.h ("Only Vulkan"), the headers, the man pages,
#     the import libraries (.a), the shell scripts of bin (they need
#     /bin/sh, and the app starts the loader itself), and some programs with
#     a window that a game does not use (kProgramsOut below).
#   - puts wowbox64.dll of build_box64.ps1 and libwow64fex.dll of
#     build_fex.ps1 into aarch64-windows (aarch64): the two CPU DLLs of the
#     32 bit code that the key wow64_cpu of the app chooses from.
#   - adds the default value of HKLM\Software\Microsoft\Wow64\x86 (the CPU
#     DLL of the 32 bit code, CpuDll of wine_rules.h) to the section [Wow64]
#     of wine.inf, which wineboot reads before the 32 bit part of the prefix.
#   - strips each ELF file.
#   - checks the machine of each ELF file and of each PE file, and lists the
#     imports of each CPU DLL that the tree does not export.
#   - writes into bwvr/out/android/wine: <apk abi>.tar (ustar, the tree),
#     <apk abi>.id (the build id: a hash of the names and the bytes of the
#     tree), <apk abi>-files.txt, <apk abi>-left-out.txt and licenses/ (Wine,
#     FreeType, and for aarch64 Box64 and each fex*.txt of build_fex.ps1).
# It ends with exit code 1 when a check fails.
set -u
W=${1:?the work folder}
ABI=${2:?the ABI: aarch64 or x86_64}
HERE=$(cd "$(dirname "$0")" && pwd)
BWVR=$(cd "$HERE/../.." && pwd)
RULES=$BWVR/android/src/wine_rules.h
OUT=$BWVR/out/android/wine
NDK=$W/tools/android-ndk-r27d/toolchains/llvm/prebuilt/linux-x86_64/bin
MINGW=$W/tools/llvm-mingw-20261006-ucrt-ubuntu-22.04-x86_64/bin
case "$ABI" in
  aarch64) APK_ABI=arm64-v8a; CPU=wowbox64.dll; ELF=AArch64; PE64=coff-arm64 ;;
  x86_64)  APK_ABI=x86_64; CPU=wow64cpu.dll; ELF="Advanced Micro Devices X86-64"; PE64=coff-x86-64 ;;
  *) echo "The ABI must be aarch64 or x86_64, not $ABI."; exit 1 ;;
esac
FROM=$W/install/$ABI/wine
S=$W/pack/$APK_ABI
[ -x "$FROM/$APK_ABI/lib/wine/$ABI-unix/wine" ] || { echo "There is no Wine in $FROM. Run build_wine.sh first."; exit 1; }
WOWBOX=$BWVR/out/android/box64/wowbox64.dll
BOX64_LICENSE=$BWVR/out/android/box64/LICENSE
FEX_DLL=$BWVR/out/android/fex/libwow64fex.dll
FEX_LICENSES=$BWVR/out/android/fex/licenses
if [ "$ABI" = aarch64 ]; then
  for f in "$WOWBOX" "$BOX64_LICENSE"; do
    [ -f "$f" ] || { echo "There is no $f. Run build_box64.ps1 -Mingw first."; exit 1; }
  done
  for f in "$FEX_DLL" "$FEX_LICENSES/fex.txt"; do
    [ -f "$f" ] || { echo "There is no $f. Run build_fex.ps1 first."; exit 1; }
  done
fi
# Programs of Wine with a window, for a person at a desktop. A game does
# not start them.
kProgramsOut="notepad wordpad write winemine winhlp32 iexplore winefile"
FAULTS=0
fault() { echo "   FAULT $*"; FAULTS=$((FAULTS + 1)); }

NAMES=$(sed -n '/kLeftOutNames\[\]/,/};/p' "$RULES" | grep -o '"[^"]*"' | tr -d '"')
PREFIX=$(sed -n 's/.*kLeftOutPrefix\[\] = "\([^"]*\)".*/\1/p' "$RULES")
[ -n "$NAMES" ] && [ -n "$PREFIX" ] || { echo "Cannot read the left out names of $RULES."; exit 1; }

echo "== The Wine tree of $APK_ABI from $FROM ($(du -sb "$FROM" | cut -f1) bytes)"
rm -rf "$S"
mkdir -p "$S" "$OUT/licenses"
cp -a "$FROM/." "$S/"
LEFT=$OUT/$APK_ABI-left-out.txt
: > "$LEFT"
gone() { echo "$1 ($2)" >> "$LEFT"; rm -rf "${S:?}/$1"; }

for d in include share/man share/applications; do
  [ -e "$S/$d" ] && gone "$d" "headers and man pages"
done
for f in "$S/$APK_ABI/bin/"*; do
  b=$(basename "$f")
  case "$b" in wineserver) ;; *) gone "$APK_ABI/bin/$b" "a shell script or a tool of the build" ;; esac
done
while IFS= read -r f; do
  gone "${f#"$S"/}" "an import library"
done < <(find "$S" -name '*.a' | LC_ALL=C sort)
while IFS= read -r f; do
  b=$(basename "$f")
  stem=$(echo "${b%%.*}" | tr 'A-Z' 'a-z')
  out=""
  for n in $NAMES; do [ "$stem" = "$n" ] && out=1; done
  case "$stem" in "$PREFIX"*) out=1 ;; esac
  for n in $kProgramsOut; do [ "$b" = "$n.exe" ] && { gone "${f#"$S"/}" "a program with a window"; continue 2; }; done
  [ -n "$out" ] && gone "${f#"$S"/}" "Only Vulkan"
done < <(find "$S/$APK_ABI/lib/wine" \( -type f -o -type l \) | LC_ALL=C sort)
echo "   left out: $(wc -l < "$LEFT") files and folders (see $(basename "$LEFT"))"

if [ "$ABI" = aarch64 ]; then
  cp "$WOWBOX" "$S/$APK_ABI/lib/wine/aarch64-windows/wowbox64.dll"
  cp "$FEX_DLL" "$S/$APK_ABI/lib/wine/aarch64-windows/libwow64fex.dll"
fi
INF=$S/share/wine/wine.inf
LINE="HKLM,Software\\Microsoft\\Wow64\\x86,,,\"$CPU\""
# The line goes through the environment: awk -v reads backslashes as escapes.
LINE="$LINE" awk '{ print } /^\[Wow64\]\r?$/ { print ENVIRON["LINE"] }' "$INF" > "$INF.new" && mv "$INF.new" "$INF"
grep -qF "$LINE" "$INF" || fault "wine.inf has no $LINE"
echo "   wine.inf, [Wow64]: $LINE"

echo "== Strip and check the files"
elf=0; pe=0; other=0
while IFS= read -r f; do
  if [ "$(head -c 4 "$f" | od -An -c | tr -d ' ')" = '177ELF' ]; then
    "$NDK/llvm-strip" --strip-unneeded "$f" || fault "llvm-strip $f"
    m=$("$NDK/llvm-readelf" -h "$f" | sed -n 's/ *Machine: *//p')
    [ "$m" = "$ELF" ] || fault "${f#"$S"/} is an ELF file for $m"
    elf=$((elf + 1))
  elif [ "$(head -c 2 "$f" | od -An -c | tr -d ' ')" = MZ ]; then
    fmt=$("$MINGW/llvm-objdump" -f "$f" 2>/dev/null | grep -o 'file format [^ ]*' | cut -d' ' -f3)
    case "$f" in
      */i386-windows/*) want=coff-i386 ;;
      */$ABI-windows/*) want=$PE64 ;;
      *) want="" ;;
    esac
    [ -z "$want" ] || [ "$fmt" = "$want" ] || fault "${f#"$S"/} is $fmt, not $want"
    pe=$((pe + 1))
  else
    other=$((other + 1))
  fi
done < <(find "$S" -type f | LC_ALL=C sort)
echo "   $elf ELF files ($ELF), $pe PE files (coff-i386 in i386-windows, $PE64 in $ABI-windows), $other other files"
echo "   the libraries that the ELF files need:" $(find "$S" -type f -exec sh -c \
  '[ "$(head -c 4 "$1" | od -An -c | tr -d " ")" = "177ELF" ] && "$0" -d "$1"' "$NDK/llvm-readelf" {} \; |
  sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p' | sort -u)
for f in "$APK_ABI/lib/wine/$ABI-unix/wine" "$APK_ABI/bin/wineserver" \
         "$APK_ABI/lib/wine/$ABI-unix/ntdll.so" "$APK_ABI/lib/libfreetype.so" \
         "$APK_ABI/lib/wine/i386-windows/ntdll.dll" "$APK_ABI/lib/wine/$ABI-windows/$CPU" \
         "$APK_ABI/lib/wine/i386-windows/wineaaudio.drv" "$APK_ABI/lib/wine/$ABI-windows/wineaaudio.drv" \
         "$APK_ABI/lib/wine/$ABI-unix/wineaaudio.so" \
         share/wine/wine.inf share/wine/fonts/tahoma.ttf; do
  [ -e "$S/$f" ] || fault "the tree has no $f"
done
if [ "$ABI" = aarch64 ]; then
  # Each CPU DLL: its format, its imports, its exports BTCpu, and each
  # function that it imports from a DLL of the tree and that the tree does
  # not export. Wine gives such an import a stub, which stops the program at
  # a call of it.
  W64=$S/$APK_ABI/lib/wine/aarch64-windows
  for cpu in wowbox64.dll libwow64fex.dll; do
    [ -e "$W64/$cpu" ] || { fault "the tree has no $APK_ABI/lib/wine/aarch64-windows/$cpu"; continue; }
    fmt=$("$MINGW/llvm-objdump" -f "$W64/$cpu" | grep -o 'file format [^ ]*' | cut -d' ' -f3)
    [ "$fmt" = coff-arm64 ] || fault "$cpu is $fmt, not coff-arm64"
    "$MINGW/llvm-objdump" -p "$W64/$cpu" > "$W/pack/$cpu.txt"
    missing=""
    for d in $(sed -n 's/.*DLL Name: //p' "$W/pack/$cpu.txt" | tr 'A-Z' 'a-z'); do
      "$MINGW/llvm-objdump" -p "$W64/$d" | sed -n '/Export Table:/,/^$/p' |
        awk 'NF >= 3 && $1 ~ /^[0-9]+$/ { print $NF }' | LC_ALL=C sort -u > "$W/pack/exports-$d.txt"
      for n in $(awk -v want="$d" '/DLL Name:/ { dll = tolower($3) } NF == 2 && $1 ~ /^[0-9]+$/ && dll == want { print $2 }' \
                   "$W/pack/$cpu.txt" | LC_ALL=C sort -u | LC_ALL=C comm -23 - "$W/pack/exports-$d.txt"); do
        missing="$missing $d.$n"
      done
    done
    echo "   $cpu: $fmt, imports:" $(sed -n 's/.*DLL Name: //p' "$W/pack/$cpu.txt") \
      "- exports BTCpu: $(grep -c 'BTCpu' "$W/pack/$cpu.txt") - imports that the tree does not export:${missing:- none}"
  done
fi
echo "   the fonts:" $(cd "$S/share/wine/fonts" && ls)
# The app takes only files and folders from the tar file (wine_runtime.cpp).
[ "$(find "$S" -type l | wc -l)" -eq 0 ] || fault "the tree has symbolic links, and the app takes none"

echo "== The tar file"
cd "$S" || exit 1
ID=$(find . \( -type f -o -type l \) -printf '%p %l\n' | LC_ALL=C sort |
  while read -r p l; do
    if [ -L "$p" ]; then echo "$p -> $l"; else echo "$p $(sha256sum < "$p" | cut -c1-64)"; fi
  done | sha256sum | cut -c1-16)
find . -mindepth 1 | sed 's|^\./||' | LC_ALL=C sort > "$W/pack/$APK_ABI.list"
find . -type f -printf '%s %P\n' | LC_ALL=C sort -k2 > "$OUT/$APK_ABI-files.txt"
tar --format=ustar --owner=0 --group=0 --numeric-owner --mtime=@0 --no-recursion \
  -cf "$OUT/$APK_ABI.tar" -T "$W/pack/$APK_ABI.list" || fault "tar"
echo "$ID" > "$OUT/$APK_ABI.id"
cp "$W/install/$ABI/COPYING.LIB" "$OUT/licenses/wine.txt"
cp "$W/install/$ABI/FTL.TXT" "$OUT/licenses/freetype.txt"
if [ "$ABI" = aarch64 ]; then
  cp "$BOX64_LICENSE" "$OUT/licenses/box64.txt"
  rm -f "$OUT"/licenses/fex*.txt
  cp "$FEX_LICENSES"/fex*.txt "$OUT/licenses/"
fi
echo "   build id $ID"
echo "   the tree: $(find . -type f | wc -l) files, $(find . -type l | wc -l) links, $(du -sb . | cut -f1) bytes"
echo "   $OUT/$APK_ABI.tar: $(stat -c %s "$OUT/$APK_ABI.tar") bytes"
echo "== $FAULTS faults"
[ "$FAULTS" -eq 0 ]
