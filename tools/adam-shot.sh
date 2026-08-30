#!/bin/sh
#
# Headless screen capture for the Adam build.
#
# ADAMEm is an SDL program with no monitor and no GDB stub, so this cannot work
# the way tools/coco-shot.sh does -- there is nothing to set a breakpoint in.
# What it has instead is a snapshot format that carries all 16K of VRAM, and an
# -autosnap mode that writes one at shutdown. So the recipe is: run the machine
# blind for a fixed wall-clock time with SDL's dummy drivers, stop it, and
# decode the screen out of the state it left behind.
#
# That means the capture is timing-based rather than event-based, which is the
# one respect in which this harness is weaker than the other three. WAIT is
# generous by default for that reason; raise it if a capture comes back on the
# splash screen.
#
#   tools/adam-shot.sh                     canned data, first screen
#   tools/adam-shot.sh "K_DOWN,K_ENTER"           scripted K_* keys, then capture
#   REAL=1 WAIT=45 tools/adam-shot.sh      against fujinet-pc-adam
#
# Needs adamem built with the FujiNet bridge (see the manual at
# fujinet-manuals/connecting-an-adam-emulator-to-fujinet-pc). REAL=1 expects
# fujinet-pc-adam to be listening; without it the client sits on "FujiNet not
# found", which is itself a useful thing to be able to capture.
set -e

ADAMEM="${ADAMEM:-$HOME/Workspace/adamem_sdl/adamem}"
BUILD="${BUILD:-defoogi make}"
OUT="${OUT:-r2r/adam/screen.png}"
WAIT="${WAIT:-20}"
SNAP="${SNAP:-r2r/adam/screen.snap}"

FLAGS="-DGM_FAKE_DATA"
[ -n "$REAL" ] && FLAGS=""
[ -n "$1" ] && FLAGS="$FLAGS -DGM_FAKE_KEYS=$1"

# The shot flags land in CFLAGS_EXTRA_ADAM through ADAM_SHOT_FLAGS rather than
# on the command line: that variable carries every screen-shape knob the
# top-level Makefile sets, and a command-line assignment would replace the lot.
#
# FUJINET_LIB is blanked for the opposite reason -- it has to be a command-line
# override. The top-level Makefile points it at a local fujinet-lib checkout so
# the cc65 and CMOC platforms build on the host, and defoogi mounts the project
# directory and nothing else, so that absolute path does not exist inside the
# container. Empty makes fnlib.py download the archive into _cache/ instead.
rm -rf build/gmail/adam
$BUILD adam FUJINET_LIB= ADAM_SHOT_FLAGS="$FLAGS"

rm -f "$SNAP"
FUJI=""
[ -n "$REAL" ] && FUJI="-fujinet"

# The dummy drivers are what make this headless; without them adamem opens a
# window and needs an X display.
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  timeout "$WAIT" "$ADAMEM" $FUJI -autosnap 1 -snap "$SNAP" \
    -tapea r2r/adam/gmail.ddp >/dev/null 2>&1 || true

[ -f "$SNAP" ] || { echo "no snapshot written -- adamem did not shut down cleanly" >&2; exit 1; }

exec python3 tools/adam-decode.py "$SNAP" -o "$OUT"
