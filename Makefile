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
PLATFORMS += h89-cpm
PLATFORMS += h89-hdos
PLATFORMS += msdos

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
# the real transport, ends 1,681 bytes higher, and is the one that runs out
# first. At 320 rows the product build has 11,419 bytes spare and the capture
# harness 9,738.
#
# LINE_CAP is left at the portable default, which no other 32-column build can
# afford.
#
# GM_PKT and GM_RXBUF are the one pair that is not a screen shape. AdamNet has
# no length field in a channel read: the device streams a whole packet of up to
# 1024 bytes whatever the client asked for, and fujinet-lib copies all of it
# into the caller's buffer while advancing its cursor by the requested length
# instead. A read for anything other than min(1024, bytes waiting) therefore
# overruns the buffer and drops the surplus from the stream. GM_PKT tells
# src/net.c to stage the listing in packets, and GM_RXBUF has to be at least as
# large so a body read takes a whole one; net.c refuses to compile otherwise.
# FRM_VBODY x FRM_BODY_COLS is what the compose form shows: twelve full-width
# lines, which is what this screen has room for under two header fields.
# FRM_NBODY is what it holds -- four windows' worth, scrolled -- and it is the
# number that costs RAM. Unlike gm_body the form cannot ride on an overlay: a
# forward reads gm_body while the form is up. At 33 bytes a line the whole
# frmbuf is ~1.7K, which is what the spare noted above is for.
CFLAGS_EXTRA_ADAM  = -DBUILD_ADAM -Os
CFLAGS_EXTRA_ADAM += -DMSG_ROWS=17
CFLAGS_EXTRA_ADAM += -DBODY_COLS=32 -DBODY_ROWS=320 -DENT_SUBJ_LEN=48
CFLAGS_EXTRA_ADAM += -DFRM_NBODY=48 -DFRM_VBODY=12 -DFRM_BODY_COLS=32
CFLAGS_EXTRA_ADAM += -DGM_PKT=1024 -DGM_RXBUF=1024
CFLAGS_EXTRA_ADAM += -DGM_PROGRESS_UI

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
# FRM_BODY_COLS stays two under the 78-column window so a full body line
# still leaves the cursor a cell and the row a margin. The whole frmbuf is
# ~1.35K, and BODY_ROWS pays for it: at 240 the compose build overflowed BSS
# by 346 bytes, so the body gives back eight rows (632 bytes) -- 232 rows is
# still eleven and a half pages of a message.
CFLAGS_EXTRA_APPLE2ENH  = -DBODY_COLS=78 -DBODY_ROWS=176 -DLINE_CAP=256
CFLAGS_EXTRA_APPLE2ENH += -DENT_SUBJ_LEN=128 -DMSG_ROWS=20
CFLAGS_EXTRA_APPLE2ENH += -DFRM_NBODY=42 -DFRM_VBODY=14 -DFRM_BODY_COLS=76

# apple2enh.cfg presumes RAM ends at $9600, leaving room for the ProDOS file
# buffers this client never opens -- fujinet-lib talks SmartPort directly and
# nothing here touches the filesystem. $BF00 is the ProDOS global page, the real
# ceiling, and the 10.5K between the two is what pays for the wider body buffer.
#
# Same -Wl trap as the Atari's line above.
LDFLAGS_EXTRA_APPLE2ENH  = -Wl -D,__HIMEM__=0xBF00
LDFLAGS_EXTRA_APPLE2ENH += --mapfile r2r/apple2enh/gmail.map

# The CoCo 1/2 runs at 32x16 on the VDG's semigraphics page -- the narrowest and
# shortest screen this client has, and one of the two that draw the Gmail mark
# as the mark rather than as sprites or as holes in an inverse block. The CoCo 3
# build is the other, at 80x24 on the GIME with the mark in attribute color; its
# knobs are in the MAKE_COCO3 branch below.
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
# At 320 the harness was down to 641 bytes, which would have meant the next
# string added to a screen breaking tools/coco-shot.sh while the product still
# fitted.
#
# BODY_ROWS came down from 288 to 208 when the compose form arrived: the form
# engine, the send path and their screens are ~2.6K of 6809 code and this is
# the machine with none to spare, so the body gives back 80 rows (2,640 bytes)
# to pay for it. At 240 the capture harness overflowed by 521 bytes, and 224
# would have left it 7 -- one string from breaking again -- so 208 it is,
# which is still seventeen pages of a message and leaves the harness ~535 and
# the product ~2K. The next rungs down are 192, then FRM_NBODY 6 -> 5.
#
# FRM_VBODY x FRM_BODY_COLS is what the compose form shows: six full-width
# lines, which is what a 16-row screen can spare under two header fields.
# FRM_NBODY is what it holds -- four of those windows, scrolled -- and it is
# the one that costs RAM. The frmbuf is ~800 bytes and cannot overlay gm_body,
# because a forward reads gm_body while the form is up.
#
# LINE_CAP is deliberately NOT lowered to match the narrower screen. It is the
# raw wire line accumulator, not a display width, and a line longer than it gets
# broken at a space and carried into the next row -- which shows up as one short
# row in the middle of a paragraph. Six and a quarter rows of slack at 32
# columns is the same 200 bytes the Atari spends for five.
#
# The CoCo 3 is a second binary rather than a runtime branch inside this one.
# The two machines never share a screen: a 1/2 is always the VDG's 32x16 and a
# 3 is always the GIME's 80x24, so a combined image would carry two backends,
# two marks and two layouts through a $7C00 ceiling that has 2K left -- and the
# 1/2, which is the machine with nothing to spare, would pay for all of it.
# Split, each build carries only its own half and keeps compile-time geometry.
#
# The knobs below are the 80-column shape. Nineteen list rows and twenty reader
# rows are what 24 rows leave after the two-row header, the two-row panel and
# the footer. BODY_ROWS is lower than the 1/2's 142 because the stride is 81
# rather than 33; in characters the two are within 3% of each other, which is
# the trade the wider screen makes rather than a reduction.
#
ifeq ($(MAKE_COCO3),COCO3)

CFLAGS_EXTRA_COCO  = -DCOCO3
CFLAGS_EXTRA_COCO += -DIDX_MAX=19 -DMSG_ROWS=20
CFLAGS_EXTRA_COCO += -DBODY_COLS=78 -DBODY_ROWS=50
CFLAGS_EXTRA_COCO += -DFRM_NBODY=12 -DFRM_VBODY=10 -DFRM_BODY_COLS=80
CFLAGS_EXTRA_COCO += -DENT_SUBJ_LEN=40 -DGM_RXBUF=256

else

CFLAGS_EXTRA_COCO  = -DIDX_MAX=11 -DMSG_ROWS=12
CFLAGS_EXTRA_COCO += -DBODY_COLS=32 -DBODY_ROWS=142
CFLAGS_EXTRA_COCO += -DFRM_NBODY=24 -DFRM_VBODY=6 -DFRM_BODY_COLS=32
CFLAGS_EXTRA_COCO += -DENT_SUBJ_LEN=48 -DGM_RXBUF=256

endif

CFLAGS_EXTRA_COCO += -DGM_PROGRESS_UI
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
#
# The CoCo 3 build gets the ceiling fujinet-fujirkle uses on the same machine.
# Its text page is banked in through the $C000 window rather than living in the
# 64K map, so nothing has to be kept clear below it, and the stack sits at the
# top of RAM under the $8000 ROM.
#
ifeq ($(MAKE_COCO3),COCO3)
LDFLAGS_EXTRA_COCO  = --org=1000 --limit=7E00 --initial-s=8000
else
LDFLAGS_EXTRA_COCO  = --org=1000 --limit=7C00 --initial-s=7F00
endif
LDFLAGS_EXTRA_COCO += --no-relocate -i

# `make coco` puts GMAIL.BIN on a disk by itself, started by hand with
#
#   LOADM"GMAIL":EXEC
#
# `make coco-dist` builds the combined CoCo 1/2 + CoCo 3 disk instead, which
# carries an AUTOEXEC.BAS, the loader as GMAIL.BIN, and the two clients as
# GMAIL1.BIN and GMAIL3.BIN. See the recipe at the foot of this file.
#
# That AUTOEXEC is RUNM"GMAIL" -- an HDB-DOS command -- and it has to be RUNM
# rather than LOADM. decb's -t tokenizer does not know LOADM: it matches LOAD
# greedily and leaves the M as text, so the line comes back as LOAD M"GMAIL"
# and RUN answers ?SN ERROR. Storing it as ASCII with -a -l does not help
# either -- BASIC tokenizes that correctly on the way in and then ?SN ERRORs
# anyway, because Disk BASIC runs an ASCII program out of the disk buffer that
# LOADM itself needs. Disk I/O from an ASCII-loaded program does not work.

# The PC is the one machine that does not know its own width until it boots:
# the same GMAIL.EXE runs at 40x25 (modes 0/1), 80x25 (modes 2/3) and on an
# MDA (mode 7), inheriting whatever mode it was started in. GM_RT_COLS is the
# core hook for that: BODY_COLS sizes gm_body for the 80-column case, and
# src/msdos/screen.c sets gm_wrap_cols to 38 or 78 at plat_init() -- wrapping
# narrower than the stride is safe, and half-empty rows at 40 columns are the
# price of one binary. Every mode is 25 rows, so MSG_ROWS and IDX_MAX need no
# runtime treatment: rows 3-23 hold 21 body rows in the reader and the
# portable sixteen entries in the inbox.
#
# The budget is DGROUP, not address space. The small model puts every static
# and the stack in one 64K group, and gm_body at 400 x 79 = 31,600 bytes is
# the largest thing in it; with gm_index (16 x ~181), the 1K rxbuf, linebuf,
# the screen buffers and a 4K stack the group sits near 40K with over 20K
# spare. Check DGROUP in r2r/msdos/gmail.map before raising BODY_ROWS, and
# check the -DGM_FAKE_DATA build too -- it links the canned wire data
# alongside the real transport. 400 rows is deliberately above the Apple's
# 240: at 40 columns the same message needs about twice the rows it does at
# 78, and this binary has to be ready for either width.
#
# GM_RXBUF goes to 1K because every network_read is a whole INT F5/RS-232
# round trip through FUJINET.SYS; bigger chunks are fewer serial
# transactions, and DGROUP has the room.
#
# -os is optimise-for-size; wcc's default is no optimisation at all.
# The compose form stores 76-column body lines -- the 80-column shape -- and
# windows them narrower at runtime when the machine booted in 40 columns,
# exactly the GM_RT_COLS trade the body buffer makes. FRM_VBODY is the fourteen
# rows on screen; FRM_NBODY is the fifty-six it holds and scrolls between.
# ~4.4K of DGROUP, which is the one budget here with room for it.
CFLAGS_EXTRA_MSDOS  = -os -DGM_RT_COLS -DMSG_ROWS=21
CFLAGS_EXTRA_MSDOS += -DBODY_COLS=78 -DBODY_ROWS=400 -DLINE_CAP=256
CFLAGS_EXTRA_MSDOS += -DFRM_NBODY=56 -DFRM_VBODY=14 -DFRM_BODY_COLS=76
CFLAGS_EXTRA_MSDOS += -DENT_SUBJ_LEN=128 -DGM_RXBUF=1024
CFLAGS_EXTRA_MSDOS += -DGM_PROGRESS_UI

# tools/msdos-shot.sh appends -DGM_FAKE_DATA / -DGM_FAKE_KEYS through here,
# for the same reason the CoCo and the Adam have their *_SHOT_FLAGS: this
# variable carries every screen-shape knob above and a command-line
# assignment would replace the lot.
CFLAGS_EXTRA_MSDOS += $(MSDOS_SHOT_FLAGS)

# The map is how the DGROUP budget above gets checked, and the stack is set
# explicitly rather than inherited from wlink's default so the 4K in the
# arithmetic stays true.
LDFLAGS_EXTRA_MSDOS  = OPTION map=r2r/msdos/gmail.map
LDFLAGS_EXTRA_MSDOS += OPTION stack=4096

# Like the Adam, the MS-DOS build cannot run on the host: wcc lives only in
# the defoogi container, which mounts the project directory and nothing else,
# so the absolute FUJINET_LIB above is invisible inside it. Blank it and
# fnlib.py downloads the msdos release archive into the project's _cache/:
#
#   defoogi make msdos FUJINET_LIB=

# The Heathkit H89 runs CP/M (h89-cpm) or HDOS (h89-hdos). Both builds are
# src/h89/ through PLATFORM_COMBOS, and draw on the H19 terminal: 80x24, plus
# a 25th line that holds the keys.
#
# The screen is the Apple //e's 80 columns, so the knobs start as the Apple's.
# BODY_COLS is 76 because the reader's body sits inside a frame two columns
# in from each side, and MSG_ROWS is 19 because the frame's edges take the
# two rows of 3-23 the Apple would spend on text. FRM_BODY_COLS is a column
# under that, so a full line still leaves the cursor a cell inside the same
# frame on the form.
#
# Memory is the constraint here, not speed. z88dk links from 0100H and sets
# the stack at the BDOS, overwriting the CCP; a 62K CP/M -- what FNDISK needs
# -- has the BDOS at D806H, so tools/h89/budget.py fails the link if the end
# of BSS is past D406H (a 1K stack). Against that the H19's two screen copies
# are 4K, the index 2.9K and the form 3.4K, and gm_body takes what is left:
# BODY_ROWS x 77. Check budget.py's line in the build output before raising
# it, and check the -DGM_FAKE_DATA build too -- tools/h89-shot.sh's, which
# links the canned wire data as well and is the one that runs out first.
# GM_RXBUF is halved to 256, as the calendar client does here: a few more bus
# calls a message, for headroom under the ceiling.
H89_KNOBS  = -DBUILD_H89 -O3 -DGM_PROGRESS_UI -DBODY_COLS=76
H89_KNOBS += -DLINE_CAP=256 -DENT_SUBJ_LEN=128 -DMSG_ROWS=19
H89_KNOBS += -DFRM_NBODY=42 -DFRM_VBODY=14 -DFRM_BODY_COLS=75
H89_KNOBS += -DGM_RXBUF=256

# tools/h89-shot.sh appends -DGM_FAKE_DATA through here. BODY_ROWS is a make
# variable rather than a -D on the command line because zcc will not let a
# later -D redefine an earlier one, -U or no -U.
H89_KNOBS += $(H89_SHOT_FLAGS)
CFLAGS_EXTRA_H89-CPM = $(H89_KNOBS) -DBODY_ROWS=$(H89_BODY_ROWS)
H89_BODY_ROWS ?= 160

# Nothing here opens a file, reads argv or touches stdio, so the CP/M crt
# need not set up any of them. -m keeps the map budget.py reads.
LDFLAGS_EXTRA_H89-CPM  = -m -pragma-define:CLIB_OPEN_MAX=0
LDFLAGS_EXTRA_H89-CPM += -pragma-define:CRT_ENABLE_STDIO=0
LDFLAGS_EXTRA_H89-CPM += -pragma-define:CRT_ENABLE_COMMANDLINE=0

# HDOS loads a program at 2280H and keeps everything from S.SYSM - S.OMAX up
# for itself and the drivers it has loaded: D090H once FN: is in, which it
# is when GMAIL runs from a FujiNet disk. That is 44.5K where CP/M has 54K,
# so the HDOS build has overlays. Three screens' code -- the index fetch, the
# inbox and reader together, and the form -- ride in a window under the
# program, one at a time, read from the end of GMAIL.ABS as they are wanted
# (src/h89/ovl.asm); GM_OVL marks the calls into them.
#
#   2280H  the stack, 510 bytes (z88dk's own 4480H org is Heath's split
#          octal 042.200 misread)
#   2480H  the overlay window, H89_OVL_SIZE
#          the program, then BSS, which is not in the file
#
# tools/h89/hdosovl.py checks that no overlay calls into another but through
# the loader, that each fits the window, and that the end of BSS is under
# H89_HDOS_TOP; then it packs the overlays onto GMAIL.ABS. The window is
# sized for the largest overlay (the inbox and reader), and gm_body takes
# what is left under the ceiling. Check hdosovl.py's lines in the build
# output before raising either.
CFLAGS_EXTRA_H89-HDOS = $(H89_KNOBS) -DGM_OVERLAYS -DBODY_ROWS=$(H89_HDOS_BODY_ROWS)
H89_HDOS_BODY_ROWS ?= 132
H89_OVL_WIN = 0x2480
H89_OVL_SIZE ?= 0x1600
H89_HDOS_ORG = $(shell printf '0x%X' $$(($(H89_OVL_WIN) + $(H89_OVL_SIZE))))
H89_HDOS_TOP ?= 0xD090

H89_OVL_fetch = src/sanitize.c src/h89/ui_fetch.c
H89_OVL_browse = src/body.c src/wrap.c src/h89/ui_inbox.c src/h89/ui_msg.c
H89_OVL_compose = src/compose.c src/form.c src/h89/ui_form.c

# The startup's BSS clear is src/h89/tpa.asm's, after it has checked there is
# room; the crt's stack goes two bytes under the window, since it pops a
# word it never pushed.
LDFLAGS_EXTRA_H89-HDOS  = -m -pragma-define:CRT_ORG_CODE=$(H89_HDOS_ORG)
LDFLAGS_EXTRA_H89-HDOS += -pragma-define:REGISTER_SP=$(shell printf '0x%X' $$(($(H89_OVL_WIN) - 2)))
LDFLAGS_EXTRA_H89-HDOS += -pragma-define:CLIB_OPEN_MAX=0
LDFLAGS_EXTRA_H89-HDOS += -pragma-define:CRT_ENABLE_STDIO=0
LDFLAGS_EXTRA_H89-HDOS += -pragma-define:CRT_ENABLE_COMMANDLINE=0
LDFLAGS_EXTRA_H89-HDOS += -pragma-define:CLIB_EXIT_STACK_SIZE=0
LDFLAGS_EXTRA_H89-HDOS += -pragma-define:CRT_INITIALIZE_BSS=0
LDFLAGS_EXTRA_H89-HDOS += -pragma-define:CRT_TRIM_BSS=1
LDFLAGS_EXTRA_H89-HDOS += -pragma-define:MMAP=-1 -Ca-Isrc/h89
LDFLAGS_EXTRA_H89-HDOS += -pragma-define:H89_OVL_WIN=$(H89_OVL_WIN)
ASFLAGS_EXTRA_H89-HDOS  = -DH89_HDOS -DH89_OVL_WIN=$(H89_OVL_WIN)
ASFLAGS_EXTRA_H89-HDOS += -DH89_ORG=$(H89_HDOS_ORG)

# fujinet-lib for the H89 is the h89-setup-after-reset branch, which has no
# release archive (and carries fn_default_timeout / fn_device_error, which
# net.c needs). `make h89-fnlib` copies a checkout (FNLIB_H89, by default the
# one beside this repo) into _cache/, where the defoogi container can see it,
# and the library is built from that copy by the same z88dk that links the
# program:
#
#   make h89-fnlib
#   defoogi make h89-cpm h89-hdos
#
# The copy's path is spelled out rather than built on $(CACHE_DIR): a
# platform pass reads this file before mekkogx defines that, and the rule
# below names its target as it is read. h89-fnlib itself and the post-link
# step are further down, past the include, so that neither becomes what a
# bare `make` builds.
FNLIB_H89 ?= $(HOME)/Workspace/fujinet-lib
FNLIB_H89_SRC = _cache/fujinet-lib-h89

ifneq ($(filter h89-cpm h89-hdos,$(PLATFORM)),)
  FUJINET_LIB := __UNDEFINED__
  FNLIB_H89_LIB = $(FNLIB_H89_SRC)/r2r/$(PLATFORM)/fujinet.$(PLATFORM).lib
  EXTRA_INCLUDE += $(FNLIB_H89_SRC)/include
  LIBS += -L$(FNLIB_H89_SRC)/r2r/$(PLATFORM) -lfujinet.$(PLATFORM).lib
  EXECUTABLE_EXTRA_DEPS += $(FNLIB_H89_LIB)

# Every object includes the library's headers, so the library comes first.
$(patsubst %.c,build/$(PRODUCT)/$(PLATFORM)/%.o,$(wildcard src/*.c src/h89/*.c)): | $(FNLIB_H89_LIB)

$(FNLIB_H89_LIB):
	@[ -d $(FNLIB_H89_SRC)/bus/h89 ] || { echo "No $(FNLIB_H89_SRC): run 'make h89-fnlib' on the host first" >&2; exit 1; }
	MAKEFLAGS= $(MAKE) -C $(FNLIB_H89_SRC) $(PLATFORM)/r2r

# z80asm writes no dependency file for an .asm, and the build's -include of
# one would otherwise fall through to the top level's catch-all.
build/$(PRODUCT)/$(PLATFORM)/src/h89/ovl.d build/$(PRODUCT)/$(PLATFORM)/src/h89/tpa.d: ;
endif

# Each overlay's whole files, code and literals; the functions that ride in
# one from a file that stays resident say so with a #pragma codeseg.
ifeq ($(PLATFORM),h89-hdos)
$(foreach o,fetch browse compose,$(eval \
  $(patsubst %.c,build/$(PRODUCT)/$(PLATFORM)/%.o,$(H89_OVL_$(o))): \
    private CFLAGS += --codeseg=code_ovl_$(o) --constseg=rodata_ovl_$(o)))
endif

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
  adam_cpm+=adam \
  h89-cpm+=h89 \
  h89-hdos+=h89

include mekkogx/toplevel-rules.mk

# The H89's fujinet-lib, copied where defoogi can see it (see above).
.PHONY: h89-fnlib
h89-fnlib:
	rm -rf $(FNLIB_H89_SRC)
	mkdir -p $(FNLIB_H89_SRC)
	cp -r $(FNLIB_H89)/Makefile $(FNLIB_H89)/include $(FNLIB_H89)/common $(FNLIB_H89)/bus $(FNLIB_H89)/makefiles $(FNLIB_H89_SRC)/
	(cd $(FNLIB_H89) && git rev-parse --short HEAD && git status --porcelain Makefile bus/h89 common include makefiles) > $(FNLIB_H89_SRC)/VERSION

# The H89's memory budget (see CFLAGS_EXTRA_H89-CPM), and the .COM cut where
# BSS starts: z88dk writes BSS into it as zeros, which makes it too long for
# the 62K CCP to load. The map is written beside the program.
h89-cpm/executable-post::
	python3 tools/h89/budget.py r2r/h89-cpm/gmail.map --trim $(wildcard r2r/h89-cpm/gmail.com r2r/h89-cpm/GMAIL.COM)

# The HDOS build's checks and budget, and the overlays packed onto the
# program (see CFLAGS_EXTRA_H89-HDOS).
h89-hdos/executable-post::
	python3 tools/h89/hdosovl.py r2r/h89-hdos/gmail --objs build/$(PRODUCT)/h89-hdos \
	  --win $(H89_OVL_WIN) --size $(H89_OVL_SIZE) --top $(H89_HDOS_TOP)

# If you need to add extra platform-specific steps, do it below:
#   coco/r2r:: coco/custom-step1
#   coco/r2r:: coco/custom-step2
# or
#   apple2/disk: apple2/custom-step1 apple2/custom-step2

#################################################################
## COCO 1/2 + COCO 3 COMBINED DISK                             ##
#################################################################

COCO_R2R  = r2r/coco/$(PRODUCT)
COCO_DISK = $(COCO_R2R).dsk

# The CoCo 3 variant, through the same rules as the 1/2.
coco3:
	$(MAKE) coco MAKE_COCO3=COCO3

#
# One disk that runs on either machine: GMAIL1.BIN for the VDG, GMAIL3.BIN for
# the GIME, and GMAIL.BIN a loader that reads the model and RUNMs the right one
# -- fujinet-fujirkle's pattern, and support/coco/loader.c is its loader.
#
# AUTOEXEC.BAS is RUNM"GMAIL", which starts the loader, which picks the binary.
# The note further up about decb mangling LOADM is about LOADM: RUNM is what
# fujinet-fujirkle puts on its combined disk and it is what works here.
#
# The object tree has to go between the two builds. Make keys off timestamps
# rather than flags, so without this the second variant would silently relink
# the first one's objects.
#
coco-dist:
	$(MAKE) clean
	rm -rf build
	$(MAKE) coco
	mv $(COCO_R2R).bin $(COCO_R2R)1.bin

	rm -rf build
	$(MAKE) coco3
	mv $(COCO_R2R).bin $(COCO_R2R)3.bin

	cmoc -o $(COCO_R2R).bin support/coco/loader.c

	$(RM) $(COCO_DISK)
	decb dskini $(COCO_DISK)
	mkdir -p build/coco
	echo RUNM\"GMAIL\" > build/coco/autoexec.bas
	decb copy -t -0 build/coco/autoexec.bas $(COCO_DISK),AUTOEXEC.BAS
	decb copy -b -2 $(COCO_R2R).bin  $(COCO_DISK),GMAIL.BIN
	decb copy -b -2 $(COCO_R2R)1.bin $(COCO_DISK),GMAIL1.BIN
	decb copy -b -2 $(COCO_R2R)3.bin $(COCO_DISK),GMAIL3.BIN
	decb dir $(COCO_DISK)

# The MS-DOS image is a driver disk: GMAIL.EXE plus the pieces of
# fujinet-msdos a running FujiNet setup needs -- FUJINET.SYS and FUJIPRN.SYS
# (the two drivers CONFIG.SYS loads), the CONFIG.SYS that loads them, and
# FCONFIG for mounting and WiFi -- so the only thing the disk is missing is
# DOS itself (mformat lays no system tracks; SYS A: it, or copy the files to
# a bootable disk). The clone and the sub-make both happen inside the same
# defoogi run that built gmail.exe: wcc is container-only and the container
# has the network.
#
# Named parts rather than fujinet-config's `clean disk` + fn-msdos.img
# extraction, because the full driver build no longer fits the container:
# fmall and freset grew nasm dependencies, and defoogi ships wasm, not nasm.
# Building only what the disk carries sidesteps both.
#
# mcopy -t converts the two text files to CRLF on the way in. AUTOEXEC.BAT
# goes on last so nothing can shadow starting GMAIL.
FUJINET_MSDOS_REPO = https://github.com/FujiNetWIFI/fujinet-msdos.git
FUJINET_MSDOS_CACHE = $(CACHE_DIR)/fujinet-msdos
FN_MSDOS_PARTS = sys/fujinet.sys printer/fujiprn.sys fconfig/fconfig.com

msdos/disk-post::
	@if [ ! -d $(FUJINET_MSDOS_CACHE) ]; then \
	  git clone $(FUJINET_MSDOS_REPO) $(FUJINET_MSDOS_CACHE); \
	fi
	$(MAKE) -C $(FUJINET_MSDOS_CACHE) $(FN_MSDOS_PARTS)
	mcopy -o -i $(DISK) $(addprefix $(FUJINET_MSDOS_CACHE)/,$(FN_MSDOS_PARTS)) '::/'
	mcopy -t -o -i $(DISK) $(FUJINET_MSDOS_CACHE)/config.sys '::/CONFIG.SYS'
	mcopy -t -o -i $(DISK) src/msdos/AUTOEXEC.BAT '::/AUTOEXEC.BAT'
