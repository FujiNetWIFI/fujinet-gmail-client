#!/bin/sh
# build-harness.sh -- tools/nes/harness.c against fujinet-go-nes-desktop's
# build tree (DESKTOP, DESKTOP_BUILD), into build/nes-harness.
set -e
D=${DESKTOP:-$HOME/Workspace/fujinet-go-nes-desktop}
B=${DESKTOP_BUILD:-$D/build}
cd "$(dirname "$0")/../.."
mkdir -p build
cc -O1 -g -std=gnu11 -I"$D/core/include" -c tools/nes/harness.c -o build/nes-harness.o
c++ -o build/nes-harness build/nes-harness.o "$B/core/libnes_session.a" "$B/core/libmesen_core.a" \
    -ldl -lpthread $(ls /usr/lib/libSDL3.so* 2>/dev/null | head -1)
