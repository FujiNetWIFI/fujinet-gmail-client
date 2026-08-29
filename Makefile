PRODUCT = gmail

# Only the platforms that actually have a backend under src/<platform>/. A bare
# `make` builds every entry here, so listing one without a backend does not
# produce "no such platform" -- it compiles the portable half and then dies on
# undefined _plat_init. apple2 is the one worth naming: it is the unenhanced
# machine, one keystroke from apple2enh, with no MouseText and a character
# generator that would render the marker column as random inverse capitals.
PLATFORMS += apple2enh
PLATFORMS += atari

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
