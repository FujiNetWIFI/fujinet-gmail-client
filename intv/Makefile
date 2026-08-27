# intv/Makefile -- IntyBASIC build for the Intellivision Gmail client.
#
# Standalone, same as the other IntyBASIC FujiNet clients
# (fujinet-lobby/intv, fujinet-news/clients/intv et al.): IntyBASIC/as1600
# is a different toolchain from the cc65/cmoc-family builds the other
# platforms use, so there is nothing to wire it into.
#
# Only the main .bas is passed to the compiler; the rest arrive via
# INCLUDE. They are still listed in SOURCES so a change to any of them
# retriggers the build.
#
# Override INTYBASIC/AS1600/LIBDIR on the command line if your tools
# aren't on PATH, e.g.:
#   make INTYBASIC=/path/to/intybasic LIBDIR=/path/to/IntyBASIC/intybasic

INTYBASIC ?= intybasic
AS1600    ?= as1600
LIBDIR    ?= $(HOME)/Workspace/IntyBASIC/intybasic/

PRODUCT = gmail
MAIN    = gmail.bas
SOURCES = $(MAIN) constants.bas fujinet.bas input.bas screen.bas \
          scroll.bas wrap.bas gfx.bas st_inbox.bas st_msg.bas

.PHONY: all clean run

all: $(PRODUCT).bin $(PRODUCT).rom

$(PRODUCT).asm: $(SOURCES)
	$(INTYBASIC) $(MAIN) $(PRODUCT).asm $(LIBDIR)

# .bin + .cfg -- for loading from SD via the cartridge menu.
# The .lst is worth keeping an eye on: IntyBASIC will silently continue
# past the end of the $5000-$6FFF segment, and a cart that spills into
# $7000 fails EXEC's boot detection.
$(PRODUCT).bin: $(PRODUCT).asm
	$(AS1600) -o $(PRODUCT).bin -l $(PRODUCT).lst $(PRODUCT).asm

# .rom -- Intellicart format, for jzIntv and network boot.
$(PRODUCT).rom: $(PRODUCT).asm
	$(AS1600) -o $(PRODUCT).rom $(PRODUCT).asm

run: $(PRODUCT).rom
	./run.sh

clean:
	rm -f $(PRODUCT).asm $(PRODUCT).bin $(PRODUCT).cfg $(PRODUCT).rom $(PRODUCT).lst
