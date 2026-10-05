#!/bin/sh
# nes-shot.sh -- build the NES image with canned data (and optionally scripted
# keys), run it headless in fujinet-go-nes-desktop's core, and capture the
# screen once the script has run out.
#
#   tools/nes-shot.sh NAME [KEYS] [--kbd none|fb|subor] [-- harness lines...]
#
#   NAME   output basename: build/shots/NAME.txt (the text screen, with each
#          band's palette) and build/shots/NAME.png (the frame, 3x)
#   KEYS   GM_FAKE_KEYS, e.g. "K_DOWN,K_ENTER,K_RIGHT"; the program spends
#          them one at a time, each once the screen has caught up
#
# Extra harness commands after "--" run once the program is idle (pad
# presses, typing on the keyboard, more checks). REAL=1 builds without
# GM_FAKE_DATA; FAIL=N adds GM_FAKE_SEND_FAIL=N. GRANT=FNCONFIG lends the run
# a FujiNet's Google grant (the harness's --grant), which a REAL=1 run needs.
#
# Like the other platforms' capture scripts this reads the program's own
# memory, not the picture: the text comes from scr_txt in WRAM. "check"
# then holds the picture to it, cell by cell (tools/nes/harness.c).
set -e
cd "$(dirname "$0")/.."
name=$1; shift
keys=
kbd=none
case "$1" in --*|"") ;; *) keys=$1; shift ;; esac
if [ "$1" = "--kbd" ]; then kbd=$2; shift 2; fi
[ "$1" = "--" ] && shift

flags=
[ -n "$REAL" ] || flags="-DGM_FAKE_DATA"
[ -n "$keys" ] && flags="$flags -DGM_FAKE_KEYS=$keys"
[ -n "$FAIL" ] && flags="$flags -DGM_FAKE_SEND_FAIL=$FAIL"

rm -rf build/gmail/nes
make nes NES_SHOT_FLAGS="$flags" >build/nes-shot.log 2>&1 || { tail -20 build/nes-shot.log; exit 1; }
[ -x build/nes-harness ] || sh tools/nes/build-harness.sh

mkdir -p build/shots
{
    echo "sleep 3000"
    if [ -n "$keys" ]; then echo "idle 300000"; else echo "settle 10000"; fi
    for l in "$@"; do echo "$l"; done
    echo "settle 10000"
    # a full repaint has the screen off for a few frames, and the frame the
    # emulator hands back is the last one finished: let one or two go by
    echo "sleep 300"
    echo "dump"
    echo "check"
    echo "shot $PWD/build/shots/$name.ppm"
} | timeout 400 build/nes-harness --kbd "$kbd" ${GRANT:+--grant "$GRANT"} r2r/nes/gmail.nes 2>/dev/null \
  | grep -v '^\[fujinet\]' | tee build/shots/$name.txt
python3 -c "
from PIL import Image
im = Image.open('build/shots/$name.ppm')
im.resize((im.width * 3, im.height * 3), Image.NEAREST).save('build/shots/$name.png')"
