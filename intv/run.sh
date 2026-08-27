#!/bin/sh
# run.sh -- build (if needed) and launch the Gmail client in the
# FujiNet-patched jzIntv, connected to a real FujiNet (or fujinet-pc)
# instance over BoIP.
#
# The FujiNet at the other end needs Google authorized in its Web UI
# ("Authorize with Google"), and that grant has to be new enough to carry
# the gmail.readonly scope -- a grant made before the mail protocol landed
# only has the Drive scope, and every Gmail call will come back 403.
#
# Override any of these on the command line, e.g.:
#   JZINTV=/path/to/jzintv FUJINET_TARGET=localhost:9995 ./run.sh
#   ./run.sh --fujinet-debug        # extra flags are passed straight to jzintv

set -e

cd "$(dirname "$0")"
SDL_AUDIODRIVER=pulseaudio
JZINTV_DIR=${JZINTV_DIR:-$HOME/Workspace/jzintv-20200712-src}
JZINTV=${JZINTV:-$JZINTV_DIR/bin/jzintv}
EXEC_BIN=${EXEC_BIN:-$JZINTV_DIR/rom/exec.bin}
GROM_BIN=${GROM_BIN:-$JZINTV_DIR/rom/grom.bin}
FUJINET_TARGET=${FUJINET_TARGET:-localhost:9995}

if [ ! -x "$JZINTV" ]; then
    echo "jzIntv not found or not executable at: $JZINTV" >&2
    echo "Set JZINTV_DIR or JZINTV to point at your FujiNet-patched jzIntv build." >&2
    exit 1
fi
if [ ! -f "$EXEC_BIN" ] || [ ! -f "$GROM_BIN" ]; then
    echo "Missing EXEC/GROM BIOS images:" >&2
    echo "  EXEC_BIN=$EXEC_BIN" >&2
    echo "  GROM_BIN=$GROM_BIN" >&2
    exit 1
fi

# Rebuild only if the ROM is missing or a source file changed since it was
# last built.
if [ ! -f gmail.rom ] || [ -n "$(find . -maxdepth 1 -name '*.bas' -newer gmail.rom)" ]; then
    echo "Building gmail.rom..."
    make
fi

echo "Launching jzIntv against FujiNet at $FUJINET_TARGET ..."
exec "$JZINTV" \
    -z 4 \
    -e "$EXEC_BIN" \
    -g "$GROM_BIN" \
    --fujinet="$FUJINET_TARGET" \
    "$@" \
    gmail.rom
