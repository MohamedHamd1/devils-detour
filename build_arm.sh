#!/bin/bash
# Cross-compile Devil's Detour for aarch64 (glibc >= 2.17) linking against the device's libSDL2.
set -e
cd "$(dirname "$0")"; ROOT="$(pwd)"
ZIG="python3 -m ziglang cc -target aarch64-linux-gnu.2.17"
CF="-O2 -ffp-contract=off -fno-stack-protector -D_FORTIFY_SOURCE=0 -I$ROOT/inc -I/usr/include/SDL2 -I$ROOT/src"
mkdir -p out/arm && cd out/arm
$ZIG $CF -c ../../src/trust.c -o trust.o
$ZIG $CF -c ../../src/font_data.c -o font_data.o
# stub libSDL2 so the linker records DT_NEEDED libSDL2-2.0.so.0 without us shipping SDL
readelf -sW trust.o font_data.o | awk '$7=="UND" && $8 ~ /^SDL_/ {print $8}' | sort -u > sdl_syms.txt
awk '{print "void " $1 "(void){}"}' sdl_syms.txt > stub.c
echo "$(wc -l < sdl_syms.txt) SDL symbols stubbed"
$ZIG -shared -Wl,-soname,libSDL2-2.0.so.0 stub.c -o libSDL2-2.0.so.0
ln -sf libSDL2-2.0.so.0 libSDL2.so
$ZIG trust.o font_data.o -L. -lSDL2 -lm -Wl,--gc-sections -s -o devilsdetour
file devilsdetour
