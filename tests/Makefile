# Host-side tests for the portable text handling.
#
# src/wrap.c, src/sanitize.c and src/body.c depend on nothing but the C
# library, so they build natively. Everything else in src/ needs cc65 and a
# FujiNet, and is exercised through the emulator instead.

CC      ?= cc
CFLAGS  ?= -O1 -g -Wall -Wextra -Wno-unused-parameter

SRC = hosttest.c ../src/wrap.c ../src/sanitize.c ../src/body.c

all: hosttest
	./hosttest

hosttest: $(SRC) ../src/gmail.h
	$(CC) $(CFLAGS) -o $@ $(SRC)

clean:
	rm -f hosttest

.PHONY: all clean
