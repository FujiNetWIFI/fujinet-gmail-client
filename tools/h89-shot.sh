#!/bin/sh
#
# Headless screen capture for the H89 builds, in fujinet-go-h89-desktop's
# h89_headless: it boots Heath CP/M 2.2.04 from the H17 and runs B:GMAIL from
# a second H17 disk -- or with OS=hdos boots HDOS 2.0 and runs SY1:GMAIL.ABS
# -- presses keys on the H19, and prints the screen as text and writes it as
# a PNG at each "shot".
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
# STACK=1 runs it in h89_session_run instead and, at the end, reports how
# deep the HDOS build's stack went (src/h89/tpa.asm fills it with E5H).
#
# Shots land in r2r/h89-<os>/shot/ as NN.txt and NN.png.
set -e

RIG="${RIG:-$HOME/Workspace/fujinet-go-h89-desktop}"
BUILD="${BUILD:-defoogi make}"
WAIT="${WAIT:-800}"
# The whole run's limit, in seconds; a live inbox open alone can take a minute.
TIMEOUT="${TIMEOUT:-120}"
OS="${OS:-cpm}"
P=h89-$OS
if [ "$OS" = hdos ]; then
    BOOT="${BOOT:-$RIG/tools/disks/boot/hdos-2.0.h17disk}"
else
    BOOT="${BOOT:-$RIG/tools/disks/boot/cpm-2.2.04-64k.h8d}"
fi

FLAGS="-DGM_FAKE_DATA"
[ -n "$REAL" ] && FLAGS=""
SIZES=""

# The canned data takes room in the HDOS build: its records stay resident
# and its body builder rides with the reader, past the product's window.
[ "$OS" = hdos ] && [ -z "$REAL" ] && SIZES="H89_HDOS_BODY_ROWS=120 H89_OVL_SIZE=0x1700"

# The shot flags reach CFLAGS_EXTRA_H89-* through H89_SHOT_FLAGS. Objects
# are keyed on timestamps, not flags, so a switch between canned and live
# data rebuilds from clean.
STAMP=build/gmail/$P/.shotflags
if [ "$(cat $STAMP 2>/dev/null)" != "$FLAGS $SIZES" ]; then
    rm -rf build/gmail/$P r2r/$P
fi
$BUILD $P H89_SHOT_FLAGS="$FLAGS" $SIZES >/dev/null
mkdir -p build/gmail/$P && printf '%s' "$FLAGS $SIZES" > $STAMP

OUT=r2r/$P/shot
rm -rf $OUT
mkdir -p $OUT

SCRIPT="${1:-shot}"
case " $SCRIPT " in
    *" shot "*) ;;
    *) SCRIPT="$SCRIPT shot" ;;
esac

# The machine, whose options come before any step: then the OS from h17:1,
# the program on h17:2, read-only so a run never changes it. HDOS writes its
# boot disk, and asks the date only on a disk's first boot, so it boots a
# fresh copy every time.
RUNNER=h89_headless
[ -n "$STACK" ] && RUNNER=h89_session_run
set -- --roms "$RIG/tools/roms" --timeout "$TIMEOUT"
[ -n "$STACK" ] && set -- "$@" --home "$(pwd)/$OUT/home"
if [ -n "$REAL" ]; then
    set -- "$@" --boip "${FUJINET:-127.0.0.1:9993}"
else
    set -- "$@" --warp
fi
if [ "$OS" = hdos ]; then
    cp "$BOOT" "$OUT/boot.h8d"
    set -- "$@" --disk "h17:1=$(pwd)/$OUT/boot.h8d" --disk "h17:2=$(pwd)/r2r/$P/gmail.h8d:ro"
    set -- "$@" --until 'H:' --type 'B\r' --wait 3000 --type ' ' --wait 500 --type ' ' \
        --prompt 'ACTION? <BOOT>' --type '\r' \
        --prompt 'DATE (DD-MMM-YY)?' --type '11-OCT-80\r' \
        --prompt '>' --type 'MOUNT SY1:\r' --wait 2000 \
        --prompt '>' --type 'RUN SY1:GMAIL.ABS\r' --until 'Gmail' --wait 3000
else
    set -- "$@" --disk "h17:1=$BOOT:ro" --disk "h17:2=$(pwd)/r2r/$P/gmail.h8d:ro"
    set -- "$@" --until 'H:' --type 'B\r' --until 'A>' --wait 1000 \
        --prompt 'A>' --type 'B:GMAIL\r' --until 'Gmail' --wait 3000
fi

n=0
for step in $SCRIPT; do
    case "$step" in
        shot)
            n=$((n + 1))
            NN=$(printf '%02d' $n)
            set -- "$@" --screen --ppm "$(pwd)/$OUT/$NN.ppm"
            ;;
        type:*) set -- "$@" --type "${step#type:}" --wait "$WAIT" ;;
        wait:*) set -- "$@" --wait "${step#wait:}" ;;
        *)      set -- "$@" --key "$step" --wait "$WAIT" ;;
    esac
done

[ -n "$STACK" ] && set -- "$@" --debug 'mem 0x2280 0x100' --debug 'mem 0x2380 0x100'

"$RIG/build/core/$RUNNER" "$@" > $OUT/run.log 2>&1 || {
    cat $OUT/run.log >&2
    echo "h89-shot.sh: the run failed" >&2
    exit 1
}

# One text file per shot, out of the run's log, in the order they were taken
sed 's/^\[fujinet\] //' $OUT/run.log | awk -v dir="$OUT" '
    /^---- screen ----/ { n++; f = sprintf("%s/%02d.txt", dir, n); inshot = 1; next }
    inshot && /^----------------/ { inshot = 0; close(f); next }
    inshot { print > f }
'

for p in $OUT/*.ppm; do
    convert "$p" "${p%.ppm}.png" && rm -f "$p"
done
ls $OUT/*.png

# The lowest byte under the window that is no longer the fill
if [ -n "$STACK" ]; then
    sed 's/^\[fujinet\] //' $OUT/run.log | awk '
        /^---- mem 0x2[23]80/ { on = 1; next }
        on && /^----/ { on = 0 }
        on && /^[0-9A-F]{4} / {
            a = strtonum("0x" $1)
            for (i = 2; i <= 17 && i <= NF; i++) {
                if ($i != "E5" && !low) low = a
                a++
            }
        }
        END { if (low) printf "stack: deepest at %04XH, %d bytes under 247EH\n", low, 0x247E - low }'
fi
