#!/bin/bash
#
# Headless screen capture for the CoCo build.
#
# Builds the client, boots it in xroar with no display, breaks where it blocks
# on the keyboard, and decodes the 32x16 page out of the running machine over
# xroar's GDB target. No X server, no window and no human required.
#
#   tools/coco-shot.sh                          fake data, first screen
#   tools/coco-shot.sh "K_DOWN,K_DOWN,K_ENTER"  fake data, scripted keys
#   REAL=1 TMO=300 tools/coco-shot.sh           real Gmail
#
# The key names are the K_* codes from src/gmail.h. Scripted keys are consumed
# first and then the program falls through to the real blocking read -- which is
# exactly where the breakpoint catches it with the screen of interest painted.
#
# What comes back is the whole visual state, not a picture of it: the decoder
# reports each cell's glyph, its video sense and -- for a semigraphics cell --
# its colour and quadrant mask. The Gmail mark's four brand colours, the buff
# envelope behind them and the red unread chips down column 0 are all checkable
# that way, and none of them would be from text alone.
#
# A REAL=1 run wants fujinet-pc-coco running on localhost first. xroar's Becker
# port defaults to 65504 and so does fujinet-pc's boip port, so the two find
# each other with no configuration -- but the DriveWire vector at [$D93F] only
# exists in an HDB-DOS DriveWire ROM, which is why REAL swaps the cartridge.
#
# The RETURN in -type has to be \r and not \n. xroar parses the escape either
# way, but the translation table maps 0x0A to the CoCo's DOWN ARROW key, so \n
# types the command and then moves the cursor: the line sits on screen looking
# perfectly correct and is never entered.
#
# LOADM"GMAIL":EXEC is typed as a direct command rather than run out of an
# AUTOEXEC, for the reasons the top-level Makefile sets out at length. It is
# also exactly what a person types, so the capture exercises the real path.
set -e

HERE="$(cd "$(dirname "$0")" && pwd)"
PROJ="$(dirname "$HERE")"
KEYS="${1:-}"
TMO="${TMO:-120}"

cd "$PROJ"

XROAR="${XROAR:-$HOME/Workspace/xroar-1.5.5/src/xroar}"
ROMPATH="${ROMPATH:-$HOME/Workspace/CoCoPi-roms:$HOME/Vintage/Tandy CoCo}"
GDBPORT="${GDBPORT:-65520}"

if [ ! -x "$XROAR" ]; then
    echo "xroar not found at $XROAR -- set XROAR=/path/to/xroar" >&2
    exit 1
fi

BUILD="${BUILD:-make}"

EXTRA=""
[ -z "$REAL" ] && EXTRA="-DGM_FAKE_DATA"
[ -n "$KEYS" ] && EXTRA="$EXTRA -DGM_FAKE_KEYS=$KEYS"

# make keys off timestamps, not flags, so changing -D would otherwise silently
# relink the previous variant's object files.
rm -rf build/gmail/coco
if ! $BUILD coco COCO_SHOT_FLAGS="$EXTRA" >/tmp/coco-shot-build.log 2>&1; then
    cat /tmp/coco-shot-build.log; echo "BUILD FAILED"; exit 1
fi

# cmoc's map carries full symbols, which is what --intermediate is for.
#
# The capture waits for the CPU to be inside plat_vsync(), which is the frame
# wait the keyboard poll spins in. That is the only reason this client -- which
# has no wall clock and no need of frame timing at all -- has one: it is a tight
# loop of our own code that the program provably comes to rest inside once the
# scripted keys are spent, so a sampled PC lands in it. Polling inkey() alone
# would leave the CPU in the BASIC ROM's keyboard scan, where no symbol names it.
LO="$(awk '/^Symbol: _plat_vsync /{print $NF}' r2r/coco/gmail.map | head -1)"
HI="$(awk '/^Symbol: funcend_plat_vsync /{print $NF}' r2r/coco/gmail.map | head -1)"
if [ -z "$LO" ] || [ -z "$HI" ]; then
    echo "no _plat_vsync symbols in r2r/coco/gmail.map" >&2
    exit 1
fi

# A canned-data run touches no device, so plain Disk BASIC is enough and the
# emulator can run flat out. A real one needs the DriveWire ROM and has to keep
# real time, because the far end is a real server.
#
# xroar's -timeout counts *emulated* seconds, not wall clock, so pairing it
# with -no-ratelimit makes it fire before the machine has finished booting --
# "-no-ratelimit -timeout 30" exits in under a second. The emulated bound is
# therefore set absurdly high on a canned run and the real guard is the socket
# timeout in coco-rsp.py, with the trap below as the backstop.
if [ -n "$REAL" ]; then
    CART=(-cart-type rsdos -cart-rom hdbdosdw3cc2 -becker)
    SPEED=()
    EMUTMO="$TMO"
else
    CART=(-cart-type rsdos)
    SPEED=(-no-ratelimit)
    EMUTMO=36000
fi

"$XROAR" -ui null -vo null -ao null \
    -machine coco2bus -tv-type ntsc \
    -rompath "$ROMPATH" \
    "${CART[@]}" "${SPEED[@]}" \
    -load-fd0 r2r/coco/gmail.dsk \
    -gdb -gdb-port "$GDBPORT" \
    -timeout "$EMUTMO" \
    -type $'LOADM"GMAIL":EXEC\r' >/dev/null 2>&1 &
XPID=$!
trap 'kill $XPID 2>/dev/null || true' EXIT

# xroar halts the machine when the connection is accepted, so there is no race
# with the boot -- but the listener needs a moment to come up.
sleep 1

python3 "$HERE/coco-rsp.py" "$GDBPORT" "$LO" "$HI" "$TMO" > /tmp/coco-shot.hex
python3 "$HERE/coco-decode.py" /tmp/coco-shot.hex
