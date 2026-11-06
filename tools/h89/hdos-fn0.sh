#!/bin/bash
#
# The HDOS build run from a FujiNet disk, as most will run it: HDOS 2.0 with
# FN.DVD, gmail.h8d mounted in the FujiNet's first slot, LOAD FN:, MOUNT
# FN0:, RUN FN0:GMAIL.ABS. FN: costs the memory the HDOS build is sized
# for, and every overlay then comes in over the same card the program talks
# to Google through.
#
#   tools/h89/hdos-fn0.sh                       to the first screen
#   tools/h89/hdos-fn0.sh --key return --wait 5000 --screen
#                                               then h89_session_run steps
#   LP=1 tools/h89/hdos-fn0.sh                  LP: loaded too: GMAIL has to
#                                               say it does not fit, and leave
#   GRANT=1 tools/h89/hdos-fn0.sh               the FujiNet given the Google
#                                               grant of the installed
#                                               fujinet-go-h89, for a live run
#
# It runs h89_session_run, whose FujiNet is in-process; without GRANT a
# fetch ends on the "Authorize Google" screen, which still crosses overlay
# loads and bus calls. The boot disk is fujinet-h89's tests/make-hdos-boot.sh
# (FN.DVD and LP.DVD from that repo's build). It reads, it never sends.
#
# Log and screenshot land in r2r/h89-hdos/fn0/.
set -euo pipefail

RIG="${RIG:-$HOME/Workspace/fujinet-go-h89-desktop}"
FNH89="${FNH89:-$HOME/Workspace/fujinet-h89}"
OUT=r2r/h89-hdos/fn0
[ -f r2r/h89-hdos/gmail.h8d ] || { echo "no r2r/h89-hdos/gmail.h8d: defoogi make h89-hdos" >&2; exit 1; }

rm -rf "$OUT"
mkdir -p "$OUT/home/data/fujinet/SD"
OUT="$(cd "$OUT" && pwd)"
if [ -n "${GRANT:-}" ]; then
    trap 'rm -rf "$OUT/home"' EXIT        # the grant goes no further than the run
    cp -r "$HOME/.local/share/fujinet-go-h89/fujinet/." "$OUT/home/data/fujinet/"
fi
cp r2r/h89-hdos/gmail.h8d "$OUT/home/data/fujinet/SD/GMAIL.H8D"
bash "$FNH89/tests/make-hdos-boot.sh" "$OUT/boot.h8d" --rig "$RIG" >/dev/null

LOADS=(--type 'LOAD FN:\r' --wait 3000 --prompt '>')
[ -n "${LP:-}" ] && LOADS+=(--type 'LOAD LP:\r' --wait 3000 --prompt '>')
UNTIL=(--until 'Gmail' --wait 3000)
[ -n "${LP:-}" ] && UNTIL=(--until 'FEWER DRIVERS' --prompt '>')

# The slot, through the web UI, once the in-process FujiNet is up
( for _ in $(seq 1 120); do
    curl -s -o /dev/null 'http://127.0.0.1:11513/mount?hostslot=0&deviceslot=0&mode=1&filename=/GMAIL.H8D' && exit 0
    sleep 0.5
  done; exit 1 ) &
MOUNTER=$!

rc=0
"$RIG/build/core/h89_session_run" --roms "$RIG/tools/roms" --home "$OUT/home" \
    --timeout "${TIMEOUT:-300}" --disk "h17:1=$OUT/boot.h8d" \
    --until 'H:' --type 'B\r' --wait 3000 --type ' ' --wait 500 --type ' ' \
    --prompt 'ACTION? <BOOT>' --type '\r' \
    --prompt 'DATE (DD-MMM-YY)?' --type '11-OCT-80\r' --prompt '>' \
    "${LOADS[@]}" --debug 'mem 0x20d0 6' \
    --type 'MOUNT FN0:\r' --until 'MOUNTED ON FN0:' --prompt '>' \
    --type 'RUN FN0:GMAIL.ABS\r' "${UNTIL[@]}" \
    "$@" --screen --ppm "$OUT/screen.ppm" > "$OUT/run.log" 2>&1 || rc=$?
wait $MOUNTER || echo "hdos-fn0.sh: the slot was never mounted" >&2

sed -n 's/^\[fujinet\] //; /^---- mem 0x20d0/,/^----------------/p' "$OUT/run.log" | sed -n 2p
sed 's/^\[fujinet\] //' "$OUT/run.log" | awk '/^---- screen ----/ { s = ""; on = 1; next }
    on && /^----------------/ { on = 0; last = s; next } on { s = s $0 "\n" }
    END { printf "%s", last }' | python3 -c '
import re, sys
t = re.sub(r"\\x([0-9A-F]{2})", lambda m: chr(int(m.group(1), 16)), sys.stdin.read())
sys.stdout.write(re.sub(r"\\x[0-9A-F]?$", "", t, flags=re.M).encode("latin-1").decode("utf-8", "replace"))'
magick "$OUT/screen.ppm" "$OUT/screen.png" 2>/dev/null && rm -f "$OUT/screen.ppm" || true
[ $rc -eq 0 ] || { tail -20 "$OUT/run.log" >&2; echo "hdos-fn0.sh: the run failed ($rc)" >&2; }
exit $rc
