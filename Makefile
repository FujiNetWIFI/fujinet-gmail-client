PRODUCT = gmail

# Only the platforms that actually have a backend under src/<platform>/. A bare
# `make` builds every entry here, so listing one without a backend does not
# produce "no such platform" -- it compiles the portable half and then dies on
# undefined _plat_init. apple2 is the one worth naming: it is the unenhanced
# machine, one keystroke from apple2enh, with no MouseText and a character
# generator that would render the marker column as random inverse capitals.
#
# adam_cpm is the other one worth naming, and it is absent for the opposite
# reason: PLATFORM_COMBOS below expands it to src/adam/ as well as
# src/adam_cpm/, so listing it would compile this backend's EOS and SmartKeys
# calls into a CP/M binary that links neither library. The combo stays, because
# it is also what points fnlib.py at the adam archive.
PLATFORMS += adam
PLATFORMS += apple2enh
PLATFORMS += atari
PLATFORMS += coco

# You can run 'make <platform>' to build for a specific platform,
# or 'make <platform>/<target>' for a platform-specific target.
# Example shortcuts:
#   make coco        → build for coco
#   make apple2/disk → build the 'disk' target for apple2

# SRC_DIRS may use the literal %PLATFORM% token.
# It expands to the chosen PLATFORM plus any of its combos.
SRC_DIRS = src src/%PLATFORM%

# FUJINET_LIB can be
# - a version number such as 4.7.6
# - a directory which contains the libs for each platform
# - a zip file with an archived fujinet-lib
# - a URL to a git repo
# - empty which will use whatever is the latest
# - undefined, no fujinet-lib will be used
FUJINET_LIB = /home/thomc/Workspace/fujinet-lib/build

# The Adam runs at 32x24 on the TMS9918A's GRAPHICS II page, and the SmartKeys
# band owns the bottom three rows -- so the screen is 32 wide like the CoCo's
# and 21 rows tall, which is five more than the CoCo has and the most this
# program has ever had.
#
# Those five rows are why IDX_MAX is not here. Every other backend spends its
# bottom row on a hint bar; this machine has six labelled keys with their
# captions drawn on the screen, so that row was never spent, and rows 3-18 hold
# the full portable sixteen entries per page -- the Atari's number on a screen
# eight columns narrower, where the CoCo has to come down to eleven.
#
# MSG_ROWS is the one that does come down, by exactly one. The reader spends a
# row on the rule between the subject and the body, which is where the page
# indicator lives now that there is no footer to put it in: rows 0-2 are sender,
# date and a two-row subject, row 3 is the rule, and 4-20 is the body.
#
# ENT_SUBJ_LEN is sized off the panel rather than off the list column, as on the
# CoCo: the panel is two rows of 32, and a typical "Name: " prefix eats about 17
# of those 64 cells. It costs IDX_MAX times whatever it is set to.
#
# BODY_ROWS x BODY_STRIDE is the largest single object in the program -- 320 x
# 33 is 10,560 bytes -- and it is larger here than on the CoCo because there is
# room for it. This target links at $0000 in all-RAM mode and the ceiling is the
# boot block at $C800, so 51K of address space against the CoCo's 27K. Check
# __BSS_END_tail in r2r/adam/gmail.map against $C800 before raising it, and
# check the -DGM_FAKE_DATA build too: it links the canned wire data alongside
# the real transport, ends 1,675 bytes higher, and is the one that runs out
# first. At 320 rows the product build has 13,172 bytes spare and the capture
# harness 11,497.
#
# LINE_CAP and GM_RXBUF are left at the portable defaults, which no other
# 32-column build can afford.
CFLAGS_EXTRA_ADAM  = -DBUILD_ADAM -Os
CFLAGS_EXTRA_ADAM += -DMSG_ROWS=17
CFLAGS_EXTRA_ADAM += -DBODY_COLS=32 -DBODY_ROWS=320 -DENT_SUBJ_LEN=48

# tools/adam-shot.sh appends -DGM_FAKE_DATA / -DGM_FAKE_KEYS through here, for
# the same reason the CoCo has COCO_SHOT_FLAGS: it cannot set CFLAGS_EXTRA_ADAM
# on the command line, because that variable carries every screen-shape knob
# above and a command-line assignment would replace the lot.
CFLAGS_EXTRA_ADAM += $(ADAM_SHOT_FLAGS)

# -m keeps the map file, which is the only way to see how close the link is to
# the $C800 boot block. z88dk writes it next to the executable.
LDFLAGS_EXTRA_ADAM = -m

# The Adam build is the one that cannot run on the host. zcc is only in the
# defoogi container, and defoogi mounts the project directory and nothing else,
# so the absolute FUJINET_LIB above is invisible inside it. Blank it for this
# platform and fnlib.py downloads the archive into the project's own _cache/:
#
#   defoogi make adam FUJINET_LIB=
#
# The other three platforms keep the local checkout and keep building on the
# host, which is why this is a command-line override rather than a change to
# FUJINET_LIB itself.

# The Atari build carves 2K off the top of memory so we can place a 1K-aligned
# player/missile graphics buffer above the C stack. See src/atari/pmg.c.
#
# It has to go through -Wl: cl65 forwards a bare -D to the *compiler*, which
# never runs on a link-only invocation, so the flag would be silently dropped
# and no memory would actually be reserved.
LDFLAGS_EXTRA_ATARI  = -Wl -D,__RESERVED_MEMORY__=2048
LDFLAGS_EXTRA_ATARI += --mapfile r2r/atari/gmail.map

# The Apple II target is apple2enh -- an enhanced //e with 80-column hardware.
#
# Eighty columns is double the Atari's, so every fixed width in src/gmail.h that
# a backend is allowed to override gets overridden. BODY_ROWS comes *down* even
# though the buffer grows: at 78 columns a message needs roughly half the rows
# it does at 40, so 240 rows of 78 holds 18,720 characters against the Atari's
# 300 rows of 40 = 12,000.
CFLAGS_EXTRA_APPLE2ENH  = -DBODY_COLS=78 -DBODY_ROWS=240 -DLINE_CAP=256
CFLAGS_EXTRA_APPLE2ENH += -DENT_SUBJ_LEN=128 -DMSG_ROWS=20

# apple2enh.cfg presumes RAM ends at $9600, leaving room for the ProDOS file
# buffers this client never opens -- fujinet-lib talks SmartPort directly and
# nothing here touches the filesystem. $BF00 is the ProDOS global page, the real
# ceiling, and the 10.5K between the two is what pays for the wider body buffer.
#
# Same -Wl trap as the Atari's line above.
LDFLAGS_EXTRA_APPLE2ENH  = -Wl -D,__HIMEM__=0xBF00
LDFLAGS_EXTRA_APPLE2ENH += --mapfile r2r/apple2enh/gmail.map

# The CoCo runs at 32x16 on the VDG's semigraphics page -- the narrowest and
# shortest screen this client has ever had, and the only one that can draw the
# Gmail mark as the mark rather than as sprites or as holes in an inverse block.
#
# IDX_MAX is the one knob that is not a preference. No backend scrolls a window
# inside a page: ui_inbox() paints gm_count rows and that is the list. Sixteen
# screen rows pay for a two-row header, eleven list rows, a two-row panel and a
# footer, so eleven entries per page is what the screen can show and therefore
# what ?range= asks the adapter for.
#
# ENT_SUBJ_LEN is sized off the panel rather than off the list column: two rows
# of 32 is 64 cells, and a typical "NAME: " prefix eats about 17 of them. It
# costs IDX_MAX times whatever it is set to.
#
# BODY_ROWS x BODY_STRIDE is the largest single object in the program -- 288 x
# 33 is 9,504 bytes, about twenty-four pages of a message, and the narrow screen
# is why it is a larger row count than the Atari's rather than a smaller one.
#
# The number that bounds it is not the shipped build's. Check `Section:
# program_end` in r2r/coco/gmail.map against the $7C00 ceiling for BOTH
# variants: a -DGM_FAKE_DATA build links the canned wire data alongside the real
# transport, so it is about 1.8K larger and it is the one that runs out first.
# At 288 the shipped build has ~3.4K spare and the capture harness ~1.7K; at 320
# the harness was down to 641 bytes, which would have meant the next string
# added to a screen breaking tools/coco-shot.sh while the product still fitted.
#
# LINE_CAP is deliberately NOT lowered to match the narrower screen. It is the
# raw wire line accumulator, not a display width, and a line longer than it gets
# broken at a space and carried into the next row -- which shows up as one short
# row in the middle of a paragraph. Six and a quarter rows of slack at 32
# columns is the same 200 bytes the Atari spends for five.
CFLAGS_EXTRA_COCO  = -DIDX_MAX=11 -DMSG_ROWS=12
CFLAGS_EXTRA_COCO += -DBODY_COLS=32 -DBODY_ROWS=288
CFLAGS_EXTRA_COCO += -DENT_SUBJ_LEN=48 -DGM_RXBUF=256

CFLAGS_EXTRA_COCO += -fomit-frame-pointer

# CMOC declares the string, memory and integer-conversion functions in <cmoc.h>
# and ships no <string.h>, <stdlib.h> or <stdint.h> at all. src/coco/include/
# holds shims so the portable half can go on including them the way ordinary
# C89 does; the directory has no .c files, so the source glob steps over it.
CFLAGS_EXTRA_COCO += -Isrc/coco/include

# tools/coco-shot.sh appends -DGM_FAKE_DATA / -DGM_FAKE_KEYS through here. It
# cannot set CFLAGS_EXTRA_COCO on the command line the way tools/atari-shot.sh
# does, because on this platform that variable carries every screen-shape knob
# above and a command-line assignment would replace the lot.
CFLAGS_EXTRA_COCO += $(COCO_SHOT_FLAGS)

# With Disk BASIC present a BASIC program lives at $0E00, so the one thing the
# org must not do is collide with the line that is running LOADM. $1000 leaves
# that program 512 bytes -- one line, about 25 tokenised bytes, with no
# variables -- and gives us $1000 to $7C00, which is 27,648 for code, data and
# bss.
#
# Other CoCo clients in this family (fujinet-news, fujinet-config) org at $0E00
# and pay for it with a second-stage loader that pokes BASIC's direct-mode
# buffer and jumps into RUNM. That trick is ROM-version sensitive -- it gives
# ?UL ERROR on stock Disk BASIC 1.1 -- and 512 bytes is a cheaper price than a
# whole extra binary with its own file-type trap.
#
# The stack is placed explicitly rather than inherited from BASIC. LOADM leaves
# S wherever CLEAR put it, which moves if anyone edits the loader; $7F00 grows
# down into the 3K between our end and BASIC's string space and does not.
#
# --limit is what turns "silently corrupts the stack" into a build failure, so
# it goes in from the first link rather than after the first mystery. -i keeps
# the .map, which tools/coco-shot.sh reads its breakpoint symbol out of.
LDFLAGS_EXTRA_COCO  = --org=1000 --limit=7C00 --initial-s=7F00
LDFLAGS_EXTRA_COCO += --no-relocate -i

# The disk carries GMAIL.BIN and nothing else. It is started with
#
#   LOADM"GMAIL":EXEC
#
# and there is deliberately no AUTOEXEC.BAS to do that for you, because neither
# way of putting one on the disk survives contact:
#
#   - decb's -t runs its own BASIC tokeniser, and it does not know LOADM. It
#     matches LOAD greedily and leaves the M as text, so the line comes back as
#     LOAD M"GMAIL" and RUN answers ?SN ERROR.
#   - Stored as ASCII with -a -l, BASIC tokenises it correctly on the way in --
#     and then ?SN ERRORs anyway, because Disk BASIC runs an ASCII program out
#     of the disk buffer that LOADM itself needs. Disk I/O from an ASCII-loaded
#     program does not work.

# HIRESTXT_LIB can be
# - a version number such as 0.5.0.2
# - a directory which contains the built library
# - a URL to a git repo
# - empty which will use whatever is the latest
# - undefined, no hirestxt-mod will be used
# Only used for coco/dragon builds.
#HIRESTXT_LIB =

# Define extra dirs ("combos") that expand with a platform.
# Format: platform+=combo1,combo2
PLATFORM_COMBOS = \
  c64+=commodore \
  atarixe+=atari \
  msxrom+=msx \
  msxdos+=msx \
  adam_cpm+=adam

include mekkogx/toplevel-rules.mk

# If you need to add extra platform-specific steps, do it below:
#   coco/r2r:: coco/custom-step1
#   coco/r2r:: coco/custom-step2
# or
#   apple2/disk: apple2/custom-step1 apple2/custom-step2
