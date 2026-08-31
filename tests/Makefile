# Host-side tests for the portable text handling.
#
# src/wrap.c, src/sanitize.c, src/body.c and src/date.c depend on nothing but
# the C library, so they build natively. Everything else in src/ needs cc65 and
# a FujiNet, and is exercised through the emulator instead -- src/clock.c is
# nothing but a device call, so hosttest.c owns the two variables it would
# otherwise define.
#
# Built once per screen shape, because the core's fixed widths are overridable
# and the backends do override them. hosttest is the Atari's shape, hosttest80
# the Apple II's and hosttest32 the CoCo's, which is the only way the
# width-dependent wrap and split paths get covered at widths the Atari never
# reaches in either direction.
#
# hosttestadam is the fourth, and it is not a duplicate of the CoCo's. Both are
# 32 columns, but the CoCo pays for its narrow screen by dropping IDX_MAX to 11;
# the Adam has five more rows and keeps the portable 16. So this is the only
# shape where the widest index meets the narrowest wrap, and the only one whose
# row budget is 320.
#
# hosttestdos and hosttestdos40 are one backend, not two: MS-DOS ships a single
# binary that stores 78-column rows and picks its wrap width at boot from the
# video mode. hosttestdos is that binary on an 80-column screen; hosttestdos40
# is the same storage wrapping to 38, which is the only shape anywhere with
# WRAP_COLS narrower than BODY_COLS and therefore the only one that covers the
# GM_RT_COLS hook at all.
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

# Keep in step with CFLAGS_EXTRA_MSDOS. The storage shape is the Apple's
# 78-column one; only BODY_ROWS distinguishes hosttestdos from hosttest80.
CFLAGSDOS = -DBODY_COLS=78 -DBODY_ROWS=400 -DLINE_CAP=256 -DENT_SUBJ_LEN=128

# The MS-DOS build wraps at runtime: the same binary stores 78-column rows and
# wraps to 38 when the machine boots in 40 columns. This is the only shape
# where WRAP_COLS and BODY_COLS differ, so it is the only one that covers the
# GM_RT_COLS hook and a wrap far narrower than its stride.
CFLAGSDOS40 = $(CFLAGSDOS) -DGM_RT_COLS -DGM_TEST_COLS=38

SRC = hosttest.c ../src/wrap.c ../src/sanitize.c ../src/body.c ../src/date.c

all: hosttest hosttest80 hosttest32 hosttestadam hosttestdos hosttestdos40
	./hosttest
	./hosttest80
	./hosttest32
	./hosttestadam
	./hosttestdos
	./hosttestdos40

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

hosttestdos: $(DEPS)
	$(CC) $(CFLAGS) $(CFLAGSDOS) -o $@ $(SRC)

hosttestdos40: $(DEPS)
	$(CC) $(CFLAGS) $(CFLAGSDOS40) -o $@ $(SRC)

clean:
	rm -f hosttest hosttest80 hosttest32 hosttestadam hosttestdos hosttestdos40

.PHONY: all clean
