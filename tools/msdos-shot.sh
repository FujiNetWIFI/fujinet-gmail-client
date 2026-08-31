#!/bin/bash
#
# Headless screen capture for the MS-DOS build.
#
# Builds the client with canned data, boots it in dosbox-x with no display,
# and decodes the text page it dumps. No X server, no window, no FujiNet and
# no human required -- and unlike the Atari and CoCo captures there is no
# debugger to drive: a DOS program can hand over its own B800 page, so the
# GM_SHOT hook in src/msdos/input.c writes SCREEN.BIN where a person would
# start typing, and the emulator's only job is to exist.
#
#   tools/msdos-shot.sh                          fake data, first screen
#   tools/msdos-shot.sh "K_DOWN,K_DOWN,K_ENTER"  fake data, scripted keys
#   MODE=40 tools/msdos-shot.sh                  40 columns  (gmail /40)
#   MODE=mono tools/msdos-shot.sh                BW table    (gmail /mono)
#   MACHINE=hercules tools/msdos-shot.sh         the MDA path, mode 7
#   MACHINE=pcjr tools/msdos-shot.sh             the PCjr's BIOS
#
# The key names are the K_* codes from src/gmail.h. Scripted keys are
# consumed first and then the program reaches the blocking read -- which is
# exactly where GM_SHOT catches it with the screen of interest painted.
#
# MACHINE=hercules is the capture that earns its keep: dosbox-x's hercules
# machine boots claiming mode 3, which is how the equipment-word probe in
# src/msdos/screen.c got its regression test. Check the --attrs pane for the
# 0x70 bar, 0x0f unread rows and the 0x01 underline under the reader's
# subject; none of them are visible in the glyphs.
#
# The build runs through defoogi (wcc lives nowhere else) and clobbers the
# msdos objects first: MSDOS_SHOT_FLAGS changes every object and make cannot
# see a flag change. Rebuild without flags afterwards to get the product
# binary back.
set -e

HERE="$(cd "$(dirname "$0")" && pwd)"
PROJ="$(dirname "$HERE")"
KEYS="${1:-}"
MODE="${MODE:-}"
MACHINE="${MACHINE:-}"
DOSBOX="${DOSBOX:-dosbox-x}"
TMO="${TMO:-60}"

cd "$PROJ"

if ! command -v "$DOSBOX" >/dev/null; then
    echo "dosbox-x not found -- set DOSBOX=/path/to/dosbox-x" >&2
    exit 1
fi

# wcc cannot carry a comma through -D (everything after it parses as a second
# source file), so the K_* names become the digit string GM_FAKE_KEYS_STR --
# see the note in src/msdos/input.c. The escaped quotes ride the same path
# GIT_VERSION does.
FLAGS="-DGM_FAKE_DATA -DGM_SHOT"
if [ -n "$KEYS" ]; then
    DIGITS=$(printf '%s' "$KEYS" | sed \
        -e 's/K_UP/1/g'    -e 's/K_DOWN/2/g'    \
        -e 's/K_LEFT/3/g'  -e 's/K_RIGHT/4/g'   \
        -e 's/K_ENTER/5/g' -e 's/K_BACK/6/g'    \
        -e 's/K_REFRESH/7/g' -e 's/K_QUIT/8/g'  \
        -e 's/[ ,]//g')
    case "$DIGITS" in
        *[!1-8]*) echo "unrecognised key in \"$KEYS\"" >&2; exit 1 ;;
    esac
    FLAGS="$FLAGS -DGM_FAKE_KEYS_STR=\\\"$DIGITS\\\""
fi

rm -rf build/gmail/msdos r2r/msdos
defoogi make msdos/product FUJINET_LIB= MSDOS_SHOT_FLAGS="$FLAGS"

STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT
cp r2r/msdos/gmail.exe "$STAGE"

ARGS=""
case "$MODE" in
    40)   ARGS=" /40" ;;
    80)   ARGS=" /80" ;;
    mono) ARGS=" /mono" ;;
esac

SDL_VIDEODRIVER=dummy timeout "$TMO" "$DOSBOX" \
    -defaultconf -nogui -fastlaunch \
    ${MACHINE:+-machine "$MACHINE"} \
    -c "mount c $STAGE" -c "c:" -c "gmail$ARGS" -c "exit" \
    >/dev/null 2>&1 || true

if [ ! -f "$STAGE/SCREEN.BIN" ]; then
    echo "no capture produced -- the program never reached a key read" >&2
    exit 1
fi

python3 "$HERE/msdos-decode.py" --attrs "$STAGE/SCREEN.BIN"
