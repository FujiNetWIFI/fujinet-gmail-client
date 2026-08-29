#!/bin/bash
#
# Headless screen capture for the Apple II build.
#
# Builds the client, boots it in applen with no terminal of its own, takes a
# save state, and decodes the 80x24 text screen out of the two text pages in it.
# No X server, no window and no human required.
#
#   tools/apple2-shot.sh                          fake data, first screen
#   tools/apple2-shot.sh "K_DOWN,K_DOWN,K_ENTER"  fake data, scripted keys
#   REAL=1 WAIT=45 tools/apple2-shot.sh           real Gmail (needs fujinet-pc)
#
# The key names are the K_* codes from src/gmail.h. Scripted keys are consumed
# first and then the program falls through to the real blocking read, which is
# exactly where the capture catches it with the screen of interest painted.
#
# applen comes from the FujiNet fork of AppleWin and is not built by default;
# see the Apple II notes in README.md. Point APPLEN at it, or let the search
# below find it.
#
# For a real run, start fujinet-pc first -- applen inserts the SmartPort relay
# in slot 5, which is how it reaches the adapter.
set -e

HERE="$(cd "$(dirname "$0")" && pwd)"
PROJ="$(dirname "$HERE")"
OUTDIR="$(mktemp -d)"
STATE="$OUTDIR/state.yaml"
KEYS="${1:-}"

# ProDOS boot plus the loader is about fourteen seconds, and applen has to run
# at real speed: --headless never creates the ncurses window, and the F11 that
# takes the save state is read through it.
WAIT="${WAIT:-20}"

cd "$PROJ"

if [ -z "$APPLEN" ]; then
    for c in "$HOME/Workspace/AppleWin/build/applen" \
             "$HOME/Workspace/AppleWin/build-linux/applen" \
             "$(command -v applen 2>/dev/null)"; do
        [ -x "$c" ] && APPLEN="$c" && break
    done
fi
[ -x "$APPLEN" ] || { echo "no applen -- set APPLEN=/path/to/applen"; exit 1; }

# CFLAGS_EXTRA rather than CFLAGS_EXTRA_APPLE2ENH: the latter carries the screen
# width overrides this backend needs, and assigning it on the command line would
# replace them rather than add to them.
EXTRA=""
[ -z "$REAL" ] && EXTRA="-DGM_FAKE_DATA"
[ -n "$KEYS" ] && EXTRA="$EXTRA -DGM_FAKE_KEYS=$KEYS"

# make keys off timestamps, not flags, so changing -D would otherwise silently
# relink the previous variant's object files.
rm -rf build/gmail/apple2enh
if ! make apple2enh CFLAGS_EXTRA="$EXTRA" >"$OUTDIR/build" 2>&1; then
    cat "$OUTDIR/build"; echo "BUILD FAILED"; rm -rf "$OUTDIR"; exit 1
fi
rm -f "$OUTDIR/build"

echo "applen $APPLEN   wait ${WAIT}s   flags: ${EXTRA:-<real build>}"

python3 "$HERE/apple2-run.py" "$APPLEN" "$PROJ/r2r/apple2enh/gmail.po" \
        "$STATE" "$WAIT"
python3 "$HERE/apple2-decode.py" "$STATE"
rm -rf "$OUTDIR"
