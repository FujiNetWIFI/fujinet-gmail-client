#!/bin/sh
#
# The H89 build, natively on the host with canned data, rendered through a
# model of the H19: for seeing exactly what goes down the console line and
# what it draws, without an emulator.
#
#   tools/h89/hostrun.sh "shot return shot"      the screens, as text
#   ATTRS=1 tools/h89/hostrun.sh "down shot"     with reverse video marked
#   RAW=1 tools/h89/hostrun.sh "return down" | od -c
#                                                the bytes themselves
#   H89_BODY_ROWS=40 tools/h89/hostrun.sh "return right shot"
#                                                another of the Makefile's make
#                                                variables, as for the build
#
# Keys are hostcon.c's (the same names as tools/h89-shot.sh). It is not the
# H89 -- the H19 here is tools/h89/h19model.py, the console is stdout -- so
# tools/h89-shot.sh, on the emulator, is the word on how it looks.
set -e

OUT=build/h89-host
mkdir -p $OUT

# The knobs are the Makefile's CFLAGS_EXTRA_H89-CPM, read from make.
KNOBS=$(make -s -f Makefile -f - h89-knobs PLATFORM=h89-cpm \
    ${H89_BODY_ROWS:+H89_BODY_ROWS=$H89_BODY_ROWS} 2>/dev/null <<'MK' | tail -1
h89-knobs:
	@echo $(filter -D%,$(CFLAGS_EXTRA_H89-CPM))
MK
)

cc -O0 -g -w -o $OUT/gmail -include tools/h89/hostrun.h \
    -D__H89__ -DGM_FAKE_DATA $KNOBS $EXTRA -I_cache/fujinet-lib-h89/include \
    $(ls src/*.c) $(ls src/h89/*.c | grep -v '/con\.c$') tools/h89/hostcon.c

if [ -n "$RAW" ]; then
    H89_KEYS="$1" $OUT/gmail
else
    H89_KEYS="$1" $OUT/gmail | python3 tools/h89/h19model.py ${ATTRS:+--attrs}
fi
