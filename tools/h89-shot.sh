#!/bin/sh
#
# Headless screen capture for the H89 CP/M build, in fujinet-go-h89-desktop's
# h89_headless: it boots Heath CP/M 2.2.04 from the H17, runs B:GMAIL from a
# second H17 disk, presses keys on the H19, and prints the screen as text and
# writes it as a PNG at each "shot".
#
#   tools/h89-shot.sh                         canned data, the inbox
#   tools/h89-shot.sh "down return shot"      move, read, capture
#   tools/h89-shot.sh "blue type:Lunch shot"  type into the form
#   REAL=1 tools/h89-shot.sh "shot return shot"   against a live fujinet-pc
#
# A step is a key the harness knows (f1..f5 erase blue red gray up down left
# right home esc return delete backspace tab), "type:TEXT" (\r and \e
# escapes), "wait:MS", or "shot". Each key is followed by WAIT ms (default
# 800) for the 9600-baud screen to catch up; a script with no shot gets one
# at the end.
#
# The text dump is the H19's characters only -- reverse video does not show
# in it, so the selection bar, the app bar and the keycaps are checked in the
# PNG (tools/h89/revmap.py).
#
# REAL=1 builds without GM_FAKE_DATA and dials the FujiNet at FUJINET
# (default 127.0.0.1:9993) over BoIP -- an RS232 fujinet-pc with Google
# authorised in its web UI. It runs without --warp: warp resumes after two
# seconds of card silence, and a Gmail open is longer than that. Nothing
# here sends mail unless the script answers a form's send? question or
# presses BLUE in one, so a script that does is a deliberate act.
#
# Shots land in r2r/h89-cpm/shot/ as NN.txt and NN.png.
set -e

RIG="${RIG:-$HOME/Workspace/fujinet-go-h89-desktop}"
BUILD="${BUILD:-defoogi make}"
WAIT="${WAIT:-800}"
# The whole run's limit, in seconds; a live inbox open alone can take a minute.
TIMEOUT="${TIMEOUT:-120}"
BOOT="${BOOT:-$RIG/tools/disks/boot/cpm-2.2.04-64k.h8d}"

FLAGS="-DGM_FAKE_DATA"
[ -n "$REAL" ] && FLAGS=""
SIZES=""

# The shot flags reach CFLAGS_EXTRA_H89-CPM through H89_SHOT_FLAGS. Objects
# are keyed on timestamps, not flags, so a switch between canned and live
# data rebuilds from clean.
STAMP=build/gmail/h89-cpm/.shotflags
if [ "$(cat $STAMP 2>/dev/null)" != "$FLAGS $SIZES" ]; then
    rm -rf build/gmail/h89-cpm r2r/h89-cpm
fi
$BUILD h89-cpm H89_SHOT_FLAGS="$FLAGS" $SIZES >/dev/null
mkdir -p build/gmail/h89-cpm && printf '%s' "$FLAGS $SIZES" > $STAMP

OUT=r2r/h89-cpm/shot
rm -rf $OUT
mkdir -p $OUT

SCRIPT="${1:-shot}"
case " $SCRIPT " in
    *" shot "*) ;;
    *) SCRIPT="$SCRIPT shot" ;;
esac

# The machine, whose options come before any step: then CP/M from h17:1,
# the program on h17:2, both read-only so a run never changes either image.
set -- --roms "$RIG/tools/roms" --timeout "$TIMEOUT"
if [ -n "$REAL" ]; then
    set -- "$@" --boip "${FUJINET:-127.0.0.1:9993}"
else
    set -- "$@" --warp
fi
set -- "$@" --disk "h17:1=$BOOT:ro" --disk "h17:2=$(pwd)/r2r/h89-cpm/gmail.h8d:ro"
set -- "$@" --until 'H:' --type 'B\r' --until 'A>' --wait 1000 \
    --prompt 'A>' --type 'B:GMAIL\r' --until 'Gmail' --wait 3000

n=0
for step in $SCRIPT; do
    case "$step" in
        shot)
            n=$((n + 1))
            NN=$(printf '%02d' $n)
            set -- "$@" --screen --ppm "$OUT/$NN.ppm"
            ;;
        type:*) set -- "$@" --type "${step#type:}" --wait "$WAIT" ;;
        wait:*) set -- "$@" --wait "${step#wait:}" ;;
        *)      set -- "$@" --key "$step" --wait "$WAIT" ;;
    esac
done

"$RIG/build/core/h89_headless" "$@" > $OUT/run.log 2>&1 || {
    cat $OUT/run.log >&2
    echo "h89-shot.sh: the run failed" >&2
    exit 1
}

# One text file per shot, out of the run's log, in the order they were taken
awk -v dir="$OUT" '
    /^---- screen ----/ { n++; f = sprintf("%s/%02d.txt", dir, n); inshot = 1; next }
    inshot && /^----------------/ { inshot = 0; close(f); next }
    inshot { print > f }
' $OUT/run.log

for p in $OUT/*.ppm; do
    convert "$p" "${p%.ppm}.png" && rm -f "$p"
done
ls $OUT/*.png
