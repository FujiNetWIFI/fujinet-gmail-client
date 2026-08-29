# Host-side tests for the portable text handling.
#
# src/wrap.c, src/sanitize.c, src/body.c and src/date.c depend on nothing but
# the C library, so they build natively. Everything else in src/ needs cc65 and
# a FujiNet, and is exercised through the emulator instead -- src/clock.c is
# nothing but a device call, so hosttest.c owns the two variables it would
# otherwise define.
#
# Built twice, because the core's fixed widths are overridable and the backends
# do override them. hosttest is the Atari's shape and hosttest80 the Apple II's,
# which is the only way the width-dependent wrap and split paths get covered at
# a width the Atari never reaches.

CC      ?= cc
CFLAGS  ?= -O1 -g -Wall -Wextra -Wno-unused-parameter

# Keep in step with CFLAGS_EXTRA_APPLE2ENH in the top-level Makefile.
CFLAGS80 = -DBODY_COLS=78 -DBODY_ROWS=240 -DLINE_CAP=256 -DENT_SUBJ_LEN=128

SRC = hosttest.c ../src/wrap.c ../src/sanitize.c ../src/body.c ../src/date.c

all: hosttest hosttest80
	./hosttest
	./hosttest80

hosttest: $(SRC) ../src/gmail.h
	$(CC) $(CFLAGS) -o $@ $(SRC)

hosttest80: $(SRC) ../src/gmail.h
	$(CC) $(CFLAGS) $(CFLAGS80) -o $@ $(SRC)

clean:
	rm -f hosttest hosttest80

.PHONY: all clean
