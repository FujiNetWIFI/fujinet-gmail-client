# Host-side tests for the portable text handling.
#
# src/wrap.c, src/sanitize.c, src/body.c and src/date.c depend on nothing but
# the C library, so they build natively. Everything else in src/ needs cc65 and
# a FujiNet, and is exercised through the emulator instead -- src/clock.c is
# nothing but a device call, so hosttest.c owns the two variables it would
# otherwise define.
#
# Built four times, because the core's fixed widths are overridable and the
# backends do override them. hosttest is the Atari's shape, hosttest80 the
# Apple II's and hosttest32 the CoCo's, which is the only way the
# width-dependent wrap and split paths get covered at widths the Atari never
# reaches in either direction.
#
# hosttestadam is the fourth, and it is not a duplicate of the CoCo's. Both are
# 32 columns, but the CoCo pays for its narrow screen by dropping IDX_MAX to 11;
# the Adam has five more rows and keeps the portable 16. So this is the only
# shape where the widest index meets the narrowest wrap, and the only one whose
# row budget is 320.
#
# What none of them can catch is a code generator getting the arithmetic wrong.
# The CoCo's off-by-two day of the month -- see the note on mp in src/date.c --
# passed every assertion here and was found by capturing the running screen,
# because the fault was in CMOC's 8-bit multiply and not in this source at all.

CC      ?= cc
CFLAGS  ?= -O1 -g -Wall -Wextra -Wno-unused-parameter

# Keep in step with CFLAGS_EXTRA_APPLE2ENH in the top-level Makefile.
CFLAGS80 = -DBODY_COLS=78 -DBODY_ROWS=240 -DLINE_CAP=256 -DENT_SUBJ_LEN=128

# Keep in step with CFLAGS_EXTRA_COCO. LINE_CAP is absent from both, on purpose:
# the CoCo keeps the 200-byte default.
CFLAGS32 = -DBODY_COLS=32 -DBODY_ROWS=288 -DENT_SUBJ_LEN=48 -DIDX_MAX=11

# Keep in step with CFLAGS_EXTRA_ADAM. IDX_MAX and LINE_CAP are absent because
# this backend keeps both defaults -- which is the whole point of the shape.
CFLAGSADAM = -DBODY_COLS=32 -DBODY_ROWS=320 -DENT_SUBJ_LEN=48

SRC = hosttest.c ../src/wrap.c ../src/sanitize.c ../src/body.c ../src/date.c

all: hosttest hosttest80 hosttest32 hosttestadam
	./hosttest
	./hosttest80
	./hosttest32
	./hosttestadam

# Makefile is a prerequisite because the shapes are defined in it: editing
# CFLAGS32 to match a backend and getting a stale binary that still asserts the
# old width is a confusing way to spend ten minutes.
DEPS = $(SRC) ../src/gmail.h Makefile

hosttest: $(DEPS)
	$(CC) $(CFLAGS) -o $@ $(SRC)

hosttest80: $(DEPS)
	$(CC) $(CFLAGS) $(CFLAGS80) -o $@ $(SRC)

hosttest32: $(DEPS)
	$(CC) $(CFLAGS) $(CFLAGS32) -o $@ $(SRC)

hosttestadam: $(DEPS)
	$(CC) $(CFLAGS) $(CFLAGSADAM) -o $@ $(SRC)

clean:
	rm -f hosttest hosttest80 hosttest32 hosttestadam

.PHONY: all clean
