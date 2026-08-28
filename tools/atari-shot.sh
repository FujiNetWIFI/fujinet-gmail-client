#!/bin/bash
#
# Headless screen capture for the Atari build.
#
# Builds the client, runs it in atari800 with no display, breaks where it
# blocks on the keyboard, dumps all 64K, and decodes the text screen, the
# display list and the player/missile buffer out of it. No X server, no window
# and no human required, so it can be run from anywhere.
#
#   tools/atari-shot.sh                          fake data, first screen
#   tools/atari-shot.sh "K_DOWN,K_DOWN,K_ENTER"  fake data, scripted keys
#   REAL=1 TMO=300 tools/atari-shot.sh           real Gmail (needs fujinet-pc)
#
# The key names are the K_* codes from src/gmail.h. Scripted keys are consumed
# first and then the program falls through to the real blocking read, which is
# exactly where the breakpoint catches it with the screen of interest painted.
#
# For a real run, start fujinet-pc first and make sure only one instance is
# running -- two of them fight over the NetSIO port and one will exit mid-test.
set -e

HERE="$(cd "$(dirname "$0")" && pwd)"
PROJ="$(dirname "$HERE")"
# A directory of our own, with a dot-free name inside it: the atari800
# monitor's WRITE mangles any filename containing a '.' and drops the last
# character of one that does not, so we hand it a sacrificial name and take
# whatever file actually turns up.
OUTDIR="$(mktemp -d)"
OUT="$OUTDIR/dump"
KEYS="${1:-}"
TMO="${TMO:-90}"

cd "$PROJ"

EXTRA=""
[ -z "$REAL" ] && EXTRA="-DGM_FAKE_DATA"
[ -n "$KEYS" ] && EXTRA="$EXTRA -DGM_FAKE_KEYS=$KEYS"

# make keys off timestamps, not flags, so changing -D would otherwise silently
# relink the previous variant's object files.
rm -rf build/gmail/atari
if ! make atari/product CFLAGS_EXTRA_ATARI="$EXTRA" >"$OUTDIR/build" 2>&1; then
    cat "$OUTDIR/build"; echo "BUILD FAILED"; rm -rf "$OUTDIR"; exit 1
fi
rm -f "$OUTDIR/build"

BP=$(grep -aoE "_plat_key +[0-9A-F]{6}" r2r/atari/gmail.map | head -1 | awk '{print $2}')
BP=${BP#00}
[ -n "$BP" ] || { echo "could not find _plat_key in the map file"; exit 1; }
echo "breakpoint _plat_key = \$$BP   flags: ${EXTRA:-<real build>}"

NET=""
[ -n "$REAL" ] && NET="-netsio"

# The trailing X is sacrificial: the atari800 monitor's WRITE command drops the
# last character of the filename it is given.
printf 'BPC %s\nCONT\nWRITE 0000 FFFF %sX\nQUIT\n' "$BP" "$OUT" \
    | timeout "$TMO" env -u DISPLAY SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
      atari800 -nobasic $NET -monitor -run r2r/atari/gmail.com >/dev/null 2>&1 || true

DUMP=$(find "$OUTDIR" -type f -size 64k | head -1)
[ -n "$DUMP" ] || { echo "no dump -- the breakpoint was never reached"; rm -rf "$OUTDIR"; exit 1; }
python3 "$HERE/atari-decode.py" "$DUMP"
rm -rf "$OUTDIR"
