# FujiNet Gmail client

A read-only Gmail inbox browser for 8-bit machines, talking to Gmail through a
FujiNet's GMAIL network adapter.

Two implementations live here:

- `intv/` — the original, in IntyBASIC for the Intellivision.
- `src/` — the C port. `src/` is portable across MekkoGX platforms; each target
  supplies a backend under `src/<platform>/`. **Atari 8-bit** and **Apple //e
  (enhanced)** are done in cc65, **Tandy Color Computer** in CMOC, and
  **Coleco Adam** in z88dk.

The Apple II target is `apple2enh`. `apple2` is not a build of this: it is the
unenhanced machine, with no MouseText and a character generator that would
render the unread column as random inverse capitals.

There is **no authentication in this program**. The FujiNet's GMAIL adapter
uses the Google grant stored in FujiNet config — the user authorizes once in
the FujiNet Web UI and the firmware handles token refresh. The console never
sees a credential, which is why the whole client is two device-spec opens plus
a user interface.

---

## Screens

**Inbox** — 16 messages per page, the selected row in inverse video, an unread
marker in the gutter, and the selected entry spelled out in full underneath the
list.

Forty columns:

```
        Gmail  Inbox                         <- Gmail "M" in P/M graphics
 Aug 28 14:32                    1-16/137
 >* Alice Kim     Re: lunch tomorrow
  * Bob Chen      Invoice #22 is attached
    Carol Diaz    Standup notes

 Alice Kim: Re: lunch tomorrow
 RET:READ  <>:PAGE  R:REFRESH  ESC:QUIT
```

Thirty-two, where the mark is drawn in semigraphics and the unread column is a
real coloured chip rather than a character:

```
 ▞▚  GMAIL              INBOX      <- the Gmail M, in SG4
 ▚▞  AUG 29 09:32    1-11/137
 ■ ALICE KIM    RE: LUNCH TOMORRO  <- ■ = solid Gmail red, unread
 ■ BOB CHEN     INVOICE #22 IS AT
   CAROL DIAZ   STANDUP NOTES
 ALICE KIM: RE: LUNCH TOMORROW     <- the selection, spelled out
 ENT:READ <>:PAGE R:REFR Q:QUIT
```

Eighty, where the date becomes a column of its own:

```
 M  Gmail   Inbox                                                    <- inverse app bar
                                                            1-16/137
 ◆> Aug 28 14:32  Alice Kim            Re: lunch tomorrow and the thing after
 ◆  Aug 28 09:15  Bob Chen             Invoice #22 is attached
    Aug 27 22:04  Carol Diaz           Standup notes for Thursday
  ──────────────────────────────────────────────────────────────────────────
  Re: lunch tomorrow and the thing after it
  from Alice Kim
 RET⏎:READ   ↑↓:MOVE   ◀▶:PAGE   R:REFRESH   ESC:QUIT
```

Thirty-two again on the Adam, but with five more rows and no hint bar, because
the SmartKeys carry that:

```
 M  Gmail                  Inbox   <- white envelope on a red app bar
 Aug 30 09:32       1-16/137       <- a gray rule row
 # Alice Kim    Re: lunch tomorr   <- # = solid Gmail red, unread
 # Bob Chen     Invoice #22 is a
   Carol Diaz   Standup notes
   ... sixteen rows, the Atari's page size at the CoCo's width
 Alice Kim: Re: lunch tomorrow     <- the selection, spelled out
 and the thing after it
 [Read][Prev Pg][Next Pg][Refresh][ ][Quit]   <- the SmartKeys band
```

MS-DOS renders the forty- and eighty-column layouts above and is the one
machine that decides *at runtime* which: the same `GMAIL.EXE` inherits
whatever video mode it is started in — 40×25 in modes 0/1, 80×25 in modes 2/3,
and the MDA's mode 7 — and picks the Atari's layout or the Apple's to match,
with a 25th row over the Apple's 24 spent on the content band. The chrome is
CP437's own furniture (the ◆ unread diamond, real arrows in the hint bar, a
single-line rule), the colour modes get an EDIT.EXE-family look with the hint
bar pulled to Gmail red, and mode 7 is where the MDA earns its own column:
intensity for unread rows, reverse video for the bars, and a real underline
under the reader's subject — the one attribute no other adapter has.

**Splash / busy / error** — flat screens with the logo. The error screen names
the failure and shows the raw codes beneath it (`open code 212 dev 144`), which
is the difference between a reportable bug and "it just says error".

### Keys

| Key | Inbox | Reader |
|---|---|---|
| `↑` | previous message, refetching past the top | scroll up one line |
| `↓` | next message, refetching past the bottom | scroll down one line |
| `←` | previous page | page up |
| `→` | next page | page down |
| `RETURN` | open the message | — |
| `ESC` | quit | back to the inbox, without refetching |
| `R` | refresh from the top | — |

The Atari's cursor keys need `Ctrl` held, which is a lot to ask while browsing a
mailbox, so the bare keycaps those arrows live on — `-` `=` `+` `*` — work too.
An enhanced //e has real arrow keys and needs no such workaround.

The Adam has all of these on the keyboard, and the six SmartKeys above it in
parallel: `Read` `Prev Pg` `Next Pg` `Refresh` and `Quit` on the inbox,
`Pg Up` `Pg Dn` `Up` `Down` and `Back` in the reader. A SmartKey is
discoverable and a keystroke is fast, and there is no reason to make anyone
choose. `UNDO` joins `ESC` as back, and `CLEAR` joins `R` as refresh.

A CoCo has no `ESC` key, so **`BREAK`** backs out of the reader — what every
FujiNet CoCo client does — and **`Q`** quits, rather than one key doing both.
That is an improvement rather than a concession: the key that leaves the program
is never one keystroke away from the key that leaves a message. `CLEAR` joins
`R` as refresh, mirroring the Atari's use of its own `CLEAR`.

MS-DOS takes the CoCo's split — `ESC` backs out of the reader and is inert in
the inbox, `Q` quits — and adds `PgUp`/`PgDn` as aliases for the page keys,
because they are free on this keyboard and they are what a DOS user's fingers
already do in a reader. On the PCjr the arrows themselves need the `Fn` shift,
so the aliases are no worse there than anything else.

---

## Building

Needs cc65 on `PATH`. `FUJINET_LIB` in the `Makefile` points at a local
fujinet-lib checkout; it also accepts a version number, a zip, or a git URL,
and if left empty it downloads the latest release.

```sh
make atari/product      # -> r2r/atari/gmail.com   (an XEX despite the extension)
make atari              # also builds a bootable .atr

make apple2enh/product  # -> r2r/apple2enh/gmail.a2s   (AppleSingle)
make apple2enh          # also builds a bootable ProDOS gmail.po

make coco               # -> r2r/coco/gmail.bin and gmail.dsk

defoogi make adam FUJINET_LIB=   # -> r2r/adam/gmail.ddp

defoogi make msdos FUJINET_LIB=  # -> r2r/msdos/gmail.exe and gmail.img
```

The CoCo build needs `cmoc` and `decb` (from
[toolshed](https://github.com/nitros9project/toolshed)) rather than cc65. Its
disk holds `GMAIL.BIN` and nothing else; start it with

```
LOADM"GMAIL":EXEC
```

There is deliberately no `AUTOEXEC.BAS` to do that for you — neither way of
putting one on the disk survives contact with Disk BASIC, and the Makefile says
why at length.

The Adam build is the one that cannot run on the host, and the only one whose
command line differs. It needs `zcc` from [z88dk](https://z88dk.org/) with
[eoslib](https://github.com/tschak909/eoslib) and
[smartkeyslib](https://github.com/tschak909/smartkeyslib) installed into it,
which is what the `defoogi` container already carries — and because defoogi
mounts the project directory and nothing else, the absolute `FUJINET_LIB` above
does not exist inside it. `FUJINET_LIB=` blanks it for that one build, so
`fnlib.py` downloads the archive into the project's own `_cache/` instead; the
other three platforms keep the local checkout and keep building on the host.

`-create-app` hands the link to z88dk's `appmake`, which emits a 256 KB digital
data pack: a 1 KB boot block followed by 255 KB of payload, of which the boot
block loads as many 1 KB blocks as the linked image needs into `$0000` and jumps
there. `gmail_BOOTSTRAP.bin` is written alongside it and is *already* the first
kilobyte of the `.ddp` — nothing needs to be done with it.

The MS-DOS build is the Adam's situation again — `wcc` lives only in the
defoogi container, so `FUJINET_LIB=` blanks the absolute path the container
cannot see and `fnlib.py` downloads the msdos release archive into `_cache/`.
It is Open Watcom targeting the 8086 in the small model, so the binary runs on
everything from a PCjr up. The 360 KB `gmail.img` is a driver disk:
`GMAIL.EXE` plus `FUJINET.SYS`, `FUJIPRN.SYS`, the `CONFIG.SYS` that loads
them and `FCONFIG.COM`, built from a clone of
[fujinet-msdos](https://github.com/FujiNetWIFI/fujinet-msdos) inside the same
container run — named parts rather than that repo's own `disk` target, because
two of its subprojects now want `nasm`, which defoogi does not carry. The one
thing the image is missing is DOS itself: `mformat` lays no system tracks, so
`SYS A:` it from your DOS of choice, or copy the files onto a disk that
already boots.

`GMAIL.EXE` takes four switches, none normally needed: `/40` and `/80` force a
width (and the mode change) instead of inheriting the current one, `/MONO`
keeps the black-and-white attribute table on a colour adapter — for the LCD
and composite screens that render colour as mud — and `/SNOW` gates every
video write on the retrace for the genuine IBM CGA, the one card that snows in
80-column text. It is a switch rather than a heuristic because there is no
reliable way to detect a true CGA and every other machine would pay the wait
for a fault it does not have.

`make apple2enh` needs `ac` and `acx` from
[AppleCommander](https://github.com/AppleCommander/AppleCommander/releases/); it
downloads and caches a ProDOS 2.4.3 image to build the disk from.

`make atari` additionally needs `dir2atr` **and** `atr` from
[jhallen/atari-tools](https://github.com/jhallen/atari-tools) — `mekkogx`'s
disk recipe requires `atr` unconditionally even though the Atari path never
uses it. If you don't have it, either use `make atari/product` or run the whole
build in the container: `defoogi make atari`.

The Atari link line carries `-Wl -D,__RESERVED_MEMORY__=2048`, which carves 2 KB
off the top of memory for a 1 KB-aligned player/missile buffer. It has to go
through `-Wl`: `cl65` forwards a bare `-D` to the *compiler*, which never runs
on a link-only invocation, so the flag would be silently dropped and no memory
would actually be reserved.

## Running

```sh
cd /path/to/fujinet-pc-atari/build/dist && ./run-fujinet &
atari800 -nobasic -netsio -run r2r/atari/gmail.com
```

On the CoCo the bus is DriveWire rather than SIO, so the far end is
fujinet-pc-**coco** and the machine needs an HDB-DOS DriveWire ROM for the
vector at `[$D93F]`:

```sh
cd /path/to/fujinet-pc-coco/build/dist && ./run-fujinet &
xroar -machine coco2bus -cart-type rsdos -cart-rom hdbdosdw3cc2 -becker \
      -load-fd0 r2r/coco/gmail.dsk
```

xroar's Becker port and fujinet-pc's BoIP port both default to 65504, so the two
find each other with no configuration.

On the Adam the bus is AdamNet, the far end is fujinet-pc-**adam**, and the
direction of the connection is the other way round from both of the above:
ADAMEm *listens* and fujinet-pc connects in, so ADAMEm goes up first.

```sh
cd /path/to/fujinet-pc-adam/build/dist && ./run-fujinet &
adamem -fujinet -tapea r2r/adam/gmail.ddp
```

Both default to port 65216 and find each other with no configuration. Note that
the ADAM boots disk before tape, so with fujinet-pc serving its own CONFIG on
disk slot 1 that is what comes up; to boot the client instead, mount
`gmail.ddp` in slot 1 from CONFIG, or drop it in as fujinet-pc's
`data/autorun.ddp`.

**Device `0x03` has to be forwarded.** The client reads the clock from the
AdamNet clock device, and ADAMEm's default `an_forward_mask` covers the printer,
the disks, the network units and the Fuji gateway but not the clock. Without
`(1UL << 0x03)` in it the read fails, `src/clock.c` gives up, and every message
is labelled in UTC — quietly, because that is exactly what a machine with no
clock is supposed to do.

On the PC the far end is fujinet-pc-**rs232** and the resident driver carries
the bus: `CONFIG.SYS` loads `FUJINET.SYS` (both already on `gmail.img`), which
owns a COM port and installs the `INT F5` vector everything in this client
talks through. `FUJI_PORT` picks the port — **the PCjr's internal UART sits at
the COM2 address, so a PCjr needs `FUJI_PORT=2`** — and `FUJI_BPS` the rate.
Under 86Box, a machine with `serialN_device = fujinet` wired to fujinet-pc's
BoIP port is the same thing with the cable emulated; note that 86Box v7 reads
`86box.cfg` all-lowercase, and silently starts a default machine if your
config is named `86Box.cfg`. The driver sets DOS's clock from the FujiNet at
load, but the client does not read the DOS clock — it asks the clock device
for the ISO form directly, because the `+HHMM` offset is what turns the wire's
UTC timestamps into a local date column.

Gmail needs a Google grant with the `gmail.readonly` scope, authorized through
the FujiNet Web UI — and it is per fujinet-pc instance, so a grant authorized
against the Atari build's config is not visible to the CoCo build's. Only run
one instance per bus: two of them fight over the port and one will exit
mid-session.

## Testing

**Host tests** cover the portable text handling — the line-ending soup, the
line-accumulator overflow, wrapping, truncation and the epoch arithmetic. These
are the fiddliest parts of the program and they have no platform dependency, so
they run natively instead of through a cross-compile and an emulator:

```sh
make -C tests
```

Built once per screen shape, because the core's fixed widths are overridable
and every backend overrides them: `hosttest` is the Atari's shape, `hosttest80`
the Apple II's, `hosttest32` the CoCo's, `hosttestadam` the Adam's, and
`hosttestdos`/`hosttestdos40` the two faces of the MS-DOS build. That is the
only way the width-dependent paths get covered at widths the Atari never
reaches in either direction, and the assertion that earns the extra binaries is
the one that checks no produced row is wider than the wrap width — a
`BODY_STRIDE` mismatch or an off-by-one in the hard split lands there and
nowhere else.

The fourth is not a duplicate of the third, though both are 32 columns wide. The
CoCo pays for its narrow screen by dropping `IDX_MAX` to 11; the Adam has five
more rows and keeps the portable 16. So `hosttestadam` is the only shape where
the widest index meets the narrowest wrap, and the only one whose row budget is
320.

The MS-DOS pair is one backend, not two: the same binary stores 78-column rows
and picks its wrap width at boot from the video mode, through the `GM_RT_COLS`
hook in `src/gmail.h`. `hosttestdos` is that binary on an 80-column screen and
`hosttestdos40` the same storage wrapping to 38 — the only shape anywhere with
`WRAP_COLS` narrower than `BODY_COLS`, and therefore the only one that covers
the hook at all.

Adding the third shape found two latent bugs in this file rather than in the
program, both of the same kind: a test that named a width as a literal but sized
something from a `BODY_*` macro, and so had only ever been correct by accident.
`test_wrap()` wrapped to 40 columns into rows of `BODY_STRIDE`, which was 41 and
79 and is now also 33 — it had been writing out of bounds all along and had
never been handed a buffer small enough to notice. And the token-paragraph
assertion ingested a flat 200 characters as one line, which is fine while every
`LINE_CAP` is at least 200 and counts an accumulator flush as a wrap when it is
not. Both now derive their sizes from what they actually depend on.

The date tests are worth their space for one reason: 2100 is not a leap year,
and the full Gregorian rule is the only thing that makes `civil_from_days` get
it right. The two assertions a day either side of 2100-03-01 are what hold it.

What none of this can catch is a code generator getting the arithmetic wrong.
The CoCo's day of the month came out two days late while all 61 assertions
passed, because the fault was in CMOC's 8-bit multiply and not in this source —
see the note on `mp` in `src/date.c`. It was found by photographing the running
screen, which is what the capture tools below are for.

**Headless screen capture** builds, runs the client with no display, and decodes
the text screen out of the machine's memory. The Atari one breaks where the
program blocks on the keyboard and dumps all 64K, decoding the text screen, the
display list and the P/M buffer out of it:

```sh
tools/atari-shot.sh                          # fake data, first screen
tools/atari-shot.sh "K_DOWN,K_DOWN,K_ENTER"  # scripted keys, fake data
REAL=1 TMO=300 tools/atari-shot.sh           # against real Gmail
```

`tools/apple2-shot.sh` is the counterpart and takes the same arguments:

```sh
tools/apple2-shot.sh                          # fake data, first screen
tools/apple2-shot.sh "K_DOWN,K_ENTER"         # scripted keys, fake data
REAL=1 WAIT=45 tools/apple2-shot.sh           # against real Gmail
```

It drives `applen`, the ncurses frontend of the
[FujiNet fork of AppleWin](https://github.com/FujiNetWIFI/AppleWin), which is
not built by default — configure that tree with `-DBUILD_APPLEN=ON` and point
`APPLEN` at the result. It shares the emulator core with `sa2`, so the SmartPort
device relay works from it and a `REAL=1` run reaches fujinet-pc.

What gets decoded is a save state, not the terminal: `applen`'s own
`MapCharacter()` folds screen codes `$00-$1F` and `$40-$5F` onto the same
reversed glyphs, so inverse uppercase and MouseText come out identical, and
those are the two things worth telling apart. The decoder prints the text, an
inverse-video map and a MouseText map for that reason. Three things about
driving `applen` are easy to get wrong and all three are documented in
`tools/apple2-run.py`: `--state-filename` is ignored unless the file already
exists, `--headless` never creates the window that F11 is read through, and
`set_escdelay(0)` means the F11 sequence has to arrive in one write.

`tools/coco-shot.sh` is the third, same arguments again:

```sh
tools/coco-shot.sh                          # canned data, first screen
tools/coco-shot.sh "K_ENTER"                # scripted keys, canned data
REAL=1 TMO=300 tools/coco-shot.sh           # against fujinet-pc-coco
```

It drives [xroar](https://www.6809.org.uk/xroar/) with no display and reads the
512 bytes of the 32×16 page at `$0400` out of the running machine over xroar's
GDB target. That is the *entire* visual state of this backend, so `coco-decode.py`
can print four panes from it — text, inverse video, semigraphics colour, and the
screen expanded to 64×32 with every semigraphics cell broken into its four
quadrants. That last pane is drawn in exactly the form `src/coco/logo.c` comments
its byte tables in, so a capture of the mark can be compared against the source
picture character for character.

Three things about driving it are easy to get wrong, and all three are in the
script's comments: `-type` must use `\r`, because `\n` maps to the CoCo's DOWN
ARROW and types the command without entering it; xroar's `-timeout` counts
*emulated* seconds, so pairing it with `-no-ratelimit` fires it before the
machine finishes booting, and the real guard is the socket timeout in
`coco-rsp.py`; and `Z0` breakpoints are accepted, answer `OK` and never fire, so
`coco-rsp.py` interrupts and samples the PC in a loop instead.

`tools/adam-shot.sh` is the fourth, and the one that cannot work the way the
other three do. ADAMEm has no monitor and no GDB stub, so there is nothing to
break in. What it has instead is a snapshot format carrying all 16K of VRAM and
an `-autosnap` mode that writes one at shutdown, so the recipe is to run the
machine blind under SDL's dummy drivers, stop it after a fixed wall-clock time,
and decode the screen out of the state it left behind:

```sh
tools/adam-shot.sh                          # canned data, first screen
tools/adam-shot.sh "K_DOWN,K_ENTER"         # scripted keys, canned data
REAL=1 WAIT=90 tools/adam-shot.sh           # against fujinet-pc-adam
```

The capture is therefore timing-based rather than event-based, which is the one
respect in which this harness is weaker than the other three; `WAIT` is generous
by default for that reason.

`tools/msdos-shot.sh` is the fifth, and the easiest of the lot, because a DOS
program needs no debugger to give its screen up: the `GM_SHOT` hook in
`src/msdos/input.c` writes the B800 text page to `SCREEN.BIN` at the moment
the program would block on the keyboard, and the mounted host directory is
where the file lands. dosbox-x's only job is to exist, under SDL's dummy
video driver:

```sh
tools/msdos-shot.sh                          # canned data, first screen
tools/msdos-shot.sh "K_DOWN,K_DOWN,K_ENTER"  # scripted keys, canned data
MODE=40 tools/msdos-shot.sh                  # the 40-column layout
MACHINE=hercules tools/msdos-shot.sh         # the MDA path, mode 7
MACHINE=pcjr tools/msdos-shot.sh             # the PCjr's BIOS
```

`MACHINE=hercules` is the capture that earns its keep: dosbox-x's hercules
machine boots *claiming mode 3*, which is exactly the trap the equipment-word
probe in `src/msdos/screen.c` exists for, and how it got a regression test.
`tools/msdos-decode.py` prints the glyphs and an attribute pane; the 0x70
bars, the 0x0f unread rows and the 0x01 underline beneath the reader's subject
are only visible in the second.

One wart is this compiler's own: `wcc` cannot carry a comma through `-D` —
everything after it parses as a second file to compile — so the script
translates the `K_*` names into the digit string `GM_FAKE_KEYS_STR` and
`src/msdos/input.c` accepts either spelling. Nobody types digits by hand.

`tools/adam-decode.py` renders the result as a PNG, because on this machine a
picture is the honest output — the screen is a bitmap and the parts of the
client that only exist here are not checkable from glyphs. Two text panes come
with it: the background ink of every cell, which is what shows the app bar, the
rule row, the selection bar and the unread chips without needing to recognise a
character; and the sprite attribute table with a **per-scanline count**.

That second pane is the one worth reading. Four sprites is the hardware's limit
and this mark sits exactly on it, so anything that adds a fifth to those
scanlines silently costs the mark a stroke — and it is invisible in a screenshot
precisely because the emulator, unlike the hardware, draws all of them.

The snapshot carries all of RAM as well as all of VRAM, which makes it a
debugger of last resort: an address out of `r2r/adam/gmail.map` reads straight
out of the file at offset `2434 + 16384`. That is how the clock was confirmed to
be parsing — `_iso` held `2026-08-30T14:55:56+0000` and `_gm_year` held 2026 —
on a screen that had no date on it to look at.

That sampling is the only reason this backend has a frame wait at all. It has no
wall clock and no alarms, but a sampled PC has to land somewhere with a name on
it, so `plat_key_block()` polls `inkey()` around a tight `plat_vsync()` spin
rather than blocking in the BASIC ROM's keyboard scan.

`-DGM_FAKE_DATA` replaces both fetches with generators that deliberately hit
the awkward cases — an empty display name, fields straddling every column
budget, runs of non-ASCII, a control byte, a token too long to wrap, mixed line
terminators, and enough text to overrun the row cap. It needs no FujiNet, no
network and no Google account.

---

## Atari implementation notes

**Colour.** GRAPHICS 0 gives one background and one text luminance per frame,
so two display list interrupts split the screen into three bands: a light
header, a white message area, and a Gmail-red footer. We do not build a display
list — the OS already has a correctly aligned one whose LMS points at the
screen memory conio writes to, so `src/atari/dli.c` just sets the interrupt bit
on two of its mode bytes and can put them back exactly.

Worth knowing before changing any colour: **in ANTIC mode 2 a character's
pixels take their hue from `COLPF2` and only their luminance from `COLPF1`**.
Text is never a different colour from its band, just a different brightness.
All the real colour on screen comes from the logo, which has its own four
registers.

**The logo** is four players, one per stroke of the "M", positioned by
independent `HPOS` so the strokes touch rather than tiling on an eight-bit
grid. Double width makes it 44 colour clocks — eleven character cells — wide.
Priority runs lowest player first, so the red and yellow diagonals cross over
the pillars where they meet, which is the fold in the real mark.

**Text output** goes straight into screen memory rather than through conio.
`cputc()` wraps at column 40 and scrolls the whole screen when it runs off row
23, which would wreck a full-width footer. For the same reason the keyboard is
read by calling `KEYBDV` directly (`src/atari/key.s`): cc65's `cgetc()` calls
`setcursor()`, which writes a stale character back over the previous cursor
cell and clears the inverse-video bit on the current one.

**SIO.** Every network and fuji call is bracketed by `plat_net_begin()` /
`plat_net_end()`, which suppress display list interrupts for the duration —
SIO runs with interrupts disabled and is timing critical. Nothing is lost
visually, because the screens that are up during a transfer are flat by design.

**Status bytes.** Success is reported as **0** in `DVSTAT+3` over SIO. The
Intellivision version treated 1 as the only success value and fujinet-lib's own
header says the same ("1 for normal OK status, don't ask why"); neither is true
here. `st_ok()` in `src/net.c` accepts both — every value that actually means
something (136 EOF, 167, 170, 210, 212) is distinct from either.

**Memory** (48K/64K machine, `cfg/atari.cfg`):

```
$2000-$8D12   program: code, rodata, data, bss
    ...       C stack, growing down from $B41F
$B420-$BC1F   reserved by __RESERVED_MEMORY__
$B800-$BBFF     player/missile buffer, 1K aligned, PMBASE = $B8
$BC20-$BC3F   OS display list      (borrowed, interrupt bits only)
$BC40-$BFFF   OS screen memory     (written directly)
```

Check `__BSS_RUN__ + __BSS_SIZE__` against `$AC20` in `r2r/atari/gmail.map`
before raising `BODY_ROWS`.

---

## Apple II implementation notes

**One bit per pixel, so the extra forty columns carry the date.** The Atari
spends its colour on a player/missile logo and has none left for anything else;
80-column text has no colour at all. What replaces it is not a graphical trick
but a column. The wire has always sent an 8-byte timestamp that a 40-column
listing has to throw away — `From` and `Subject` already truncate hard — and at
80 there is room to render it. It is the column every mail client leads with,
and it is also the only thing that makes the local read/unread mark legible,
because that mark is a comparison against exactly this value. `src/date.c` and
`src/clock.c` are new and portable; the Atari now shows the same date on the
free half of its page-indicator row.

**No POSIX timezone parser.** `clock_get_time(buf, TZ_ISO_STRING)` hands back
`YYYY-MM-DDTHH:MM:SS+HHMM` already resolved through the FujiNet's `[General]`
timezone, so the trailing `+HHMM` *is* the offset, DST included. Reading it once
at boot is the whole of the timezone story. `clock_get_tz()` would give the rule
(`CST6CDT`) instead of the answer, which is more work for less. The buffer is 26
bytes and validated before it is trusted: the library's copy loop compares only
the low byte of a two-byte count, so a count of 0 or 256 stores 256 bytes.

**The unread marker moved out of the selection bar.** On the Atari it sits in
column 1, inside the bar. Here it is a MouseText diamond in column 0 with the
bar starting at column 1, because MouseText occupies the very character codes
the inverse forms would have used — there is no inverse of a glyph. A diamond
inside the bar would fall back to ASCII and change shape on the one row the
cursor is on.

**The app bar is inverse and the hint bar deliberately is not.** Same reason.
Row 23 is the row with the arrows on it, and an inverse hint bar would have to
spell them `^v<>`. Drawing hints on ordinary background is also what every Apple
II program that uses MouseText does.

**MouseText travels in the control range.** `copy_san()` clamps every index
field to `$20-$7E` and `body.c` applies the same rule byte by byte to message
text, so bytes `$01-$1F` can only come from a string literal in
`src/apple2enh/`. The blitter reads one as MouseText glyph `$40 + byte`, which
lets hint strings carry arrows as ordinary C literals. Spell them as octal
escapes: `"\x1B"` followed by a hex digit is one escape, not two characters.

**Text goes straight into the two text pages.** Even columns live in auxiliary
memory and odd in main, so a run of text is two interleaved runs in two banks.
`screen.c` composes a whole field and `blit.s` writes the odd half and then the
even half, so a field costs one bank switch rather than one per character. The
technique is cc65's own (`libsrc/apple2/cputc.s`): with 80STORE on, a touch of
`HISCR` pages `$0400-$07FF` to aux. Interrupts are held off across the aux
window, because the low-memory vectors a handler expects are not there while it
is open. It is `blit.s` and not `screen.s` because the build globs `*.c` and
`*.s` from one directory onto `<name>.o` — the same reason `src/atari/` splits
`dli.c` from `dlihw.s`.

**Nothing brackets the device calls.** `plat_net_begin()` and `plat_net_end()`
are empty. The Atari uses them to suppress display list interrupts, because SIO
runs with interrupts disabled and is timing critical; there is no interrupt
handler, frame counter or sound here to suppress, and SmartPort is a subroutine
call into card firmware. For the same reason there is no `timer.c`: the blocking
key read is a bare spin, because nothing on screen changes until a key arrives.

**`fn_default_timeout` does nothing on this bus, and the error path had to learn
that.** The only reader in fujinet-lib is the Atari bus layer, where it becomes
`DTIMLO`. SmartPort has no host-side timeout at all. Worse, `open_error()` used
to test `fn_device_error == 144`, an SIO status that is not even in SmartPort's
value space — so every failed open reported as a timeout, including the 212 that
means "authorize Google in the Web UI", which is the one error a first-time user
is guaranteed to hit. `src/net.c` now asks the device once, under
`#ifdef __APPLE2__`.

**Line endings differ per bus, and the ingest was already immune.** `Protocol.h`
defaults `native_eol` to CR and only the SIO network device overrides it to
`$9B`, so a body arriving over SmartPort really is CR-terminated. `src/body.c`
has always accepted `$9B`, CR, LF and CRLF. Do not "simplify" it — this is the
bug that cost the sibling calendar client an entirely empty screen.

**`__HIMEM__` goes to `$BF00`.** `apple2enh.cfg` presumes RAM ends at `$9600`,
leaving room for ProDOS file buffers this client never opens — fujinet-lib talks
SmartPort directly and nothing here touches the filesystem. `$BF00` is the
ProDOS global page, the real ceiling.

**Memory** (`r2r/apple2enh/gmail.map`):

```
$0803-$43A8   program: code, rodata, data, init      (15,270 bytes)
$43A9-$A2DA   bss                                    (24,370 bytes)
    ...       5,220 bytes spare
$B700-$BEFF   C stack, __STACKSIZE__ = 2048
$BF00         __HIMEM__ -- the ProDOS global page
```

---

## CoCo implementation notes

**Semigraphics is per byte, so this is the one backend that draws the mark.**
The 6847 decides from bit 7 whether a cell is a character or a 2×2 block of
colour, with no mode switch and no second display list: `$00-$3F` is a glyph
with INV asserted, `$40-$7F` the same glyph normal, `$80-$FF` a colour and a
quadrant mask. Gmail's four brand colours land on four of the eight the VDG has
and the envelope lands on buff, so the mark is a white envelope with four
coloured strokes on it — as ordinary bytes in screen RAM. The Atari needs four
players steered by an interrupt to get the same colours up; the Apple, at one
bit per pixel, can only make the envelope an inverse block and leave the strokes
as holes in it.

**Every colour boundary falls on a cell edge.** All four quadrants of a cell
share one colour, so the strokes are a whole cell — 8 pixels — wide and step in
whole cells. A partial quadrant mask would not soften that, it would make it
worse: the unlit quadrants of a red cell are *black*, not envelope. So every
cell of the mark is `Q_ALL` and the diagonal is a clean staircase rather than a
ragged one.

**A solid green cell is invisible, and green is one of the four.** An unlit SG4
quadrant is black and the text background is green, so the mark needs black
wherever it would otherwise meet the background — a gutter cell to the right of
the header mark, and a full one-cell frame around the large one, which also
gives it an edge on the flat screens where it is the only thing on the top half
of the display. The one place this cannot be fixed is the header mark's *bottom*
edge, which meets the first list row; three sides bounded is enough to read.

**The unread column is a real coloured chip.** The Atari's marker is an asterisk
because a 40-column row has no spare column, and the Apple's is a MouseText
diamond; here it is a solid Gmail-red cell for unread and black for read. It
also has to stay outside the selection bar, because inverse video is XOR `$40`
and on a byte `≥ $80` bit 6 is part of the colour field — an inverted red chip
comes out cyan rather than highlighted. The Atari keeps its column 0 out because
an inverse space is COLPF1 and covers the player, the Apple because MouseText
has no inverse form. Three machines, three unrelated reasons, one rule.

**The blank byte is `$60`, and there is no lowercase.** `memset(scr, 0, ...)`
paints a screen of inverse `@`. And the ROM has sixty-four glyphs, uppercase
only, so `sc()` folds case and every literal in `src/coco/` is written in
capitals. Two of those sixty-four are worth knowing: `$5E` (`^`) is an up arrow
and `$5F` (`_`) a left arrow, which is why the reader's scroll hint is `^V:LINE`
— there is no down arrow to pair with the up one.

**The page size is the list height.** No backend scrolls a window inside a page:
`ui_inbox()` paints `gm_count` rows and that is the list. Sixteen rows pay for a
two-row header, eleven list rows, a two-row panel and a footer, so `IDX_MAX`
comes down to 11 and `?range=` asks the adapter for eleven. `LIST_ROWS` used to
sit in `gmail.h` as a second name for the same number and was referenced by
nothing at all; it is gone rather than left to drift.

**The panel earns more here than anywhere.** From truncates at twelve columns
and Subject at seventeen, so the two rows under the list — sixty-four cells of
`name: subject` through the shared `wrap_text()` — are what make a row readable.
The Atari has the same panel with thirty columns of list to fall back on.

**CMOC computes `a * b` in eight bits when both operands fit in a byte.** This
is not what C says: both promote to `int` first. `153 * mp` in `civil_from_days`
reached 765, came back as 253, and put the day of the month two days late — in a
way `num2()`'s own `% 10` then hid, because 131 renders as "31". Every host
assertion passed. Widening `mp` to `int` puts one operand out of byte range and
the multiply back into sixteen bits. The rule to check any new arithmetic
against: **if both sides fit in a byte and the product does not, it needs
widening.** Nothing else in the tree was exposed — `5 * doy`, `365UL * yoe` and
`wrap.c`'s `row * stride` all have a wider operand already, and this backend
casts `row` to `unsigned int` before scaling it by `SCR_COLS`.

**The 6809 is big-endian, which found a real bug in shared code.**
`struct wire_rec` was read straight off the wire with `uint32_t msgnum`, and the
comment in `gmail.h` blessed it because "the wire is little-endian and so is the
6502". Every message number and the whole folder size came back byte-swapped
here. The numbers go straight back out in a URL, so nothing would have *looked*
wrong — every open would simply have 404'd. The field is now `uint8_t[4]` and
`rd32le()` in `net.c` is the only thing that knows the wire's byte order, the
same way `date.c` already was for the eight-byte timestamp. Two little-endian
backends had been running on that assumption for the whole life of the program.

**CMOC ships no `<string.h>`, `<stdlib.h>` or `<stdint.h>`.** Everything is in
`<cmoc.h>`, which it includes for every translation unit anyway, so
`src/coco/include/` holds three shims and the portable half goes on including
them the way ordinary C89 does. The `<stdint.h>` one must *defer* to `<coco.h>`
rather than define the types itself: spelled as typedefs it collides with
`coco.h`'s, and spelled as `#define`s it turns `coco.h`'s own block into
`typedef unsigned char unsigned char;` for any file that reaches the shim first.

**`clock_get_time()` exists on this bus, and `clock_get_tz()` does not.**
fujinet-lib declares the latter for every platform and builds it for some; the
CoCo archive holds `fn_clock/clock_get_time.o` and nothing else. This client only
ever wanted `TZ_ISO_STRING`, whose trailing `+HHMM` *is* the resolved offset, so
it needs no flag for that — where the sibling calendar client, which wanted the
POSIX rule for its settings screen, needed `GC_NO_CLOCK_TZ`.

**The program is linked at `$1000`, not `$0E00`.** With Disk BASIC present a
BASIC program lives at `$0E00`, so `LOADM` into `$0E00` destroys the line that is
running it. fujinet-news and fujinet-config pay for that address with a
second-stage loader that pokes BASIC's direct-mode buffer and jumps into RUNM;
that trick is ROM-sensitive and gives `?UL ERROR` on stock Disk BASIC 1.1.
Giving BASIC 512 bytes is a cheaper price. `--limit` is what turns "silently
corrupts the stack" into a build failure, and `plat_shutdown()` cold-starts
rather than returning, because there is nothing left to return to.

**Memory** (`r2r/coco/gmail.map`, `Section: program_end`):

```
$1000-$3FA8   program: code, rodata, data           (12,201 bytes)
$3FA9-$6E6F   bss, of which gm_body is 9,504        (11,975 bytes)
$6E70         program_end                    (3,472 bytes spare)
$7C00         --limit -- exceeding it is a link error, not a mystery
$7F00         --initial-s, the C stack growing down
```

The build that bounds `BODY_ROWS` is not the shipped one. A `-DGM_FAKE_DATA`
build links the canned wire data *alongside* the real transport and ends about
1.8K higher, so it is the one that runs out first — at 320 rows it had 641 bytes
left, which would have meant the next string added to a screen breaking
`tools/coco-shot.sh` while the product still fitted. Check both.

`gm_body` is 18,960 of that BSS. `BODY_ROWS` comes *down* from the Atari's 300
even though the buffer grows, because at 78 columns a message needs about half
the rows: 240 × 78 holds 18,720 characters against the Atari's 12,000. Check the
map before raising it, and mind that `__STACKSIZE__` trades directly against
BSS.

---

## Adam implementation notes

**This is the first backend that does not have to approximate the brand.**
GRAPHICS II is a 256x192 bitmap whose foreground and background are settable per
8x1 strip out of fifteen inks, and z88dk lays the name table out linearly so all
768 cells own their own eight pattern bytes and their own eight colour bytes. So
the app bar is a real red band, the unread column is a real red chip, and the
selection bar is a real gray highlight. The Atari has one background and one
text luminance per band, the CoCo has eight semigraphics colours and no text
colour at all, and the Apple has none.

Medium red is `#D56F5D` against Gmail's `#EA4335`. Dark red is too brown and
light red is pink, so the middle of the three is both the closest hue and the
only one white text reads against.

**The SmartKeys pay for a row, and the row buys back the Atari's page size.**
Rows 21-23 belong to smartkeyslib, which leaves twenty-one — but every other
backend spends its bottom row on a hint bar, because it has nowhere else to say
what the keys do. This machine has six labelled keys with their captions drawn
on the screen, so that row was never spent. Sixteen list rows fit, which is the
portable `IDX_MAX` and the Atari's number, on a screen eight columns narrower;
the CoCo has to come down to eleven.

`MSG_ROWS` is the one width that does come down, by exactly one. The reader
spends a row on the rule between the subject and the body, which is where the
page indicator lives now that there is no footer to put it in.

**The mark is four hardware sprites, and four is the ceiling.** A TMS9918A shows
at most four sprites on any scanline and silently drops the fifth, and a sprite
occupies every line its 16-pixel box covers whether or not its colour is
transparent. The four strokes of the "M" — a blue pillar, a red diagonal
descending right, a yellow diagonal descending left to meet it, and a green
pillar, which is the assignment `intv/gfx.bas` established and every backend
since has kept — are stacked on one spot, so they cover the same sixteen lines
and sit exactly on the limit.

Because they do, **the mark can never be wider than one sprite.** Stacking is
what makes it multicolour and stacking is what caps its width, so the white
envelope around the strokes is painted into the attribute plane rather than
drawn in a fifth sprite. That costs no sprite and no scanline, and it is the
same picture the CoCo draws in semigraphics bytes.

The two sizes are one pattern set. `logo_small()` runs the sprites unmagnified
at 16x16 — two cells square, which is what lets the app bar carry the mark
without spending a row on it — and `logo_large()` sets the VDP's global MAG bit
and gets 32x32 out of the same thirty-two bytes per stroke. The bit is global,
but the two are never on screen together: the flat screens have no app bar and
the app bar has no flat screen.

Which makes ending the sprite list load-bearing. `vdp_set_mode(2)` clears VRAM,
so slots 4 to 31 read `y=0` and sit across scanlines 1 to 16; unterminated,
twenty-eight invisible sprites push the mark off its own budget and it loses
whichever stroke the hardware gets to last. `logo_init()` writes `y=208` into
slot 4 once, and `logo_hide()` writes it into slot 0. `scr_clear()` does not
touch sprites — they live outside the character planes — so `ui_message()` has
to hide the mark explicitly or the large one from `ui_busy()` sits over the
letter.

**fujinet-lib has no clock for this bus at all.** Not `clock_get_time`, which
the CoCo does have, and not `clock_get_tz`, which it does not — `adam/src/` has
`fn_network/` and `fn_fuji/` and no `fn_clock/`.

The firmware does answer it. `lib/device/adamnet/adamClock.cpp` registers
`platformClock` at `FUJI_DEVICEID::CLOCK`, which on AdamNet is device `0x03`,
and `fujiClock` dispatches `APETIME_GET_ISO_LOCAL` (`0x49`, `'I'`) to
`send_string(get_current_time_iso(...))` — which appends a NUL, so what comes
back is `YYYY-MM-DDTHH:MM:SS+HHMM\0`, resolved through the FujiNet's
`[General]` timezone. That is exactly the buffer `src/clock.c` already parses,
so `src/adam/clock_adam.c` is a twenty-five byte read and the portable half
needs no `#ifdef`.

The sibling calendar client asks the *Fuji* device for `FUJI_GET_TIME` (`0xD2`)
instead, because `SIMPLE_BINARY` is all its `src/clock.c` wants. That form is
local wall-clock with no offset in it, which is no use here: this client needs
the trailing `+HHMM` to turn the wire's UTC epoch seconds into a date column,
and there is nothing to difference it against.

**fujinet-lib's Adam `fuji_*` calls return their booleans inverted, and this one
is on the critical path.** Thirty-seven of the `bool`-returning entry points in
`adam/src/fn_fuji/` return `fujiError_t` codes — so `FN_ERR_OK`, which is zero,
comes back as `false` and `FN_ERR_IO_ERROR` as `true`. The backend was written
to the `uint8_t` convention the `network_*` half uses and then declared with the
`fuji_*` half's.

`main.c`'s `have_fujinet()` probes with `fuji_get_adapter_config_extended()`
precisely because it is something only a real adapter can answer, so the symptom
is a FujiNet that is present, answering, and logging `Fuji cmd: GET ADAPTER
CONFIG EXTENDED` while the client insists it is not there.
`src/adam/fuji_adam.c` defines a corrected version, which leaves the library
member unreferenced so the linker never pulls it — the same trick
`clock_adam.c` uses for a function the archive does not carry at all. Delete it
once upstream returns real booleans. `fuji_read_appkey()`,
`fuji_write_appkey()` and `fuji_set_appkey_details()`, which `src/hwm.c` needs,
are among the correct ones.

**With no FujiNet answering, the client sits on the splash screen.** It looks as
though the cause is fujinet-lib wrapping each call in `while (1) { if (err ==
ADAMNET_TIMEOUT) continue; }`, but that loop is unreachable. eoslib spins one
level further down: `eos_write_character_device()` restarts itself internally
until the device settles and only ever returns a settled status, so
`ADAMNET_TIMEOUT` never reaches any caller on this bus. `ui_notfound()` is
therefore unreachable without a presence check that does not go through
`eos_write_character_device()` at all — which belongs in eoslib or fujinet-lib,
not here. Every Adam FujiNet client shares this.

**Colour and glyphs are written by different means, and every field repaints
both.** Glyphs go through z88dk's console, which knows how to blit a font cell
into eight pattern bytes anywhere; colour is `vdp_vfill` straight into the
attribute plane, one call per run. The console keeps its own notion of the
current colour and changes it whenever anything else prints — and
`smartkeys_display()` sets it six times — so a field whose colour came from
whatever `vdp_color()` was last called with is a field whose colour is a
function of paint order. Repainting the run makes each field depend on its own
arguments and nothing else.

**Nothing paints below row 20.** `scr_clear()` clears twenty-one rows rather
than calling `clrscr()`, which would take the SmartKeys legend with it and mean
asking smartkeyslib to paint it again. `sk_bind()` likewise suppresses the
repaint when the legend has not actually changed, which is what keeps a
`smartkeys_display()` — it clears and redraws all three rows — off every
`ui_inbox_sel()`.

**SmartKey labels are not clipped.** The six slots are 48, 40, 40, 40, 40 and 48
pixels wide, the font is proportional, and an overlong label is drawn straight
over its neighbour's slot. Every label here has been measured against
`smartkeys_font[]`; `Refresh` is the widest at 32 of its 40 pixels. Slot V is
left NULL on every screen, which paints it as yellow status rather than a
keycap and marks the two halves of the band — move and page on the left, act and
leave on the right.

**Column 0 stays out of the selection bar, for a fourth unrelated reason.** Here
it is that the chip's attribute byte *is* Gmail red and the bar's is gray, so a
chip inside the bar would stop being the brand colour. The Atari keeps its
column 0 out because an inverse space is COLPF1 and covers the player, the Apple
because MouseText has no inverse form, the CoCo because XOR `$40` on a
semigraphics byte recolours it.

**`plat_vsync()` is a `HALT`.** The VDP raises an NMI once per frame and z88dk's
coleco crt installs a handler for it unconditionally, so a HALT wakes once per
frame with no interrupt of our own to install and no multi-byte counter to read
twice — which is what the calendar client needs only because its HAL exposes a
tick count. It also keeps the key poll off the AdamNet bus between frames, which
a bare spin on `eos_end_read_keyboard()` would not.

**A read on this bus is a whole packet, whatever you asked for.** Every other
backend can say "give me 220 bytes" and get 220 bytes; AdamNet has no length in
a channel read at all. The client sends RECEIVE and then CLR, and the device
streams one packet -- `min(1024, bytes waiting)` -- decided entirely at its end.
fujinet-lib's `network_read_adam()` then copies that whole packet into the
caller's buffer, while the `network_read()` around it advances its cursor by
what was *requested*. So a short read does two things at once: it writes past
the end of the buffer, and it drops the surplus from the stream.

The listing is 220-byte records, and reading one per call is what every other
bus wants. Here that read consumed 1,024 bytes of the listing to use 220 of
them, so record 0 was right, records 1 and 3 were slices taken from arbitrary
offsets -- one of them landed exactly on record 13's timestamp and rendered it
as a message number -- record 2 fell in NUL padding and came out blank, and the
listing ended after four rows of a sixteen-row page. The overrun ran 804 bytes
past a 220-byte struct, through `rxbuf`, `st_bw` and the panel's wrap buffers.

`GM_PKT` in `src/net.c` is the fix and the documentation: set to 1,024 here and
0 everywhere else, it makes the index stage whole packets in a buffer of
`GM_PKT + REC_STRIDE` and slice records out, carrying the tail that straddles.
`GM_RXBUF` rises to 1,024 for the same reason -- a body read has to take a whole
packet too -- and `net.c` refuses to compile if it is smaller than `GM_PKT`.
Where `GM_PKT` is 0 the staging buffer is one record and the tail is always
empty, so the other three backends read exactly as they did before.

**`plat_shutdown()` hands the machine back to SmartWriter.** This build is
linked at `$0000` in all-RAM mode, so the boot block that loaded it is long
gone and there is nothing to return to.

**Memory** (`r2r/adam/gmail.map`):

```
$00AD-$4803   code                                   (18,262 bytes)
$4803-$4F33   rodata                                 ( 1,840 bytes)
$4F33-$59D3   data                                   ( 2,720 bytes)
$59D3-$9B65   bss, of which gm_body is 10,560        (16,786 bytes)
    ...       11,419 bytes spare
$C800-$C82B   the 43-byte boot block -- the ceiling
$D390         the stack, growing down
```

Fifty-one kilobytes of address space against the CoCo's twenty-seven, which is
why `LINE_CAP` stays at the portable default, `GM_RXBUF` goes *up* to 1,024 (see
the packet note above) and `BODY_ROWS` goes up to 320 rather than down. Check
`__BSS_END_tail` against `$C800` before raising any of them, and check the
`-DGM_FAKE_DATA` build too: it links the canned wire data alongside the real
transport and ends 1,681 bytes higher, so it is the one that runs out first. At
320 rows the product build has 11,419 bytes spare and the capture harness
9,738.

---

## MS-DOS implementation notes

Open Watcom (`wcc`) targeting the 8086 in the small model, which is what "runs
on all PCs" means in practice: nothing newer than an 8088 instruction, nothing
fancier than the text page every adapter in the family exposes, one 64 KB
group for all data. The build runs in the defoogi container, links upstream
fujinet-lib's msdos archive, and talks to the resident `FUJINET.SYS` driver
through software interrupt `F5h` — there is no serial code anywhere in this
program.

**The runtime width is the one genuinely new problem.** Every other backend
knows its screen shape at compile time; a PC inherits whatever video mode it
was started in. The screen layer probes the mode once in `plat_init()` — 40
columns for modes 0/1, 80 for 2/3 and the MDA's 7 — and everything follows
from that: `ui.c` keeps one set of painters and selects the Atari's or the
Apple's column layout from a table, and the body wrap follows through
`GM_RT_COLS`, a two-line hook in `src/gmail.h` that turns the wrap width
`body.c` passes into a variable while `BODY_COLS` goes on sizing the storage
for the widest case. Wrapping 38 columns into 79-byte rows wastes half of
each row at 40 columns, and that is the entire price of one binary that runs
everywhere; the reverse mismatch is the buffer overrun the `BODY_STRIDE`
comment in `gmail.h` has always warned about, which is why the stride stays
derived and only the width gets a runtime form.

**The adapter, not the mode, decides where the text page is.** Bits 4-5 of
the BIOS equipment word are `11` for a monochrome adapter, and that is the
authoritative test for `B000` versus `B800` — an MDA or Hercules machine is
not necessarily *in* mode 7 when the program starts. dosbox-x's hercules
machine boots claiming mode 3, and the first draft of the probe trusted the
mode and wrote four thousand bytes into an address no hardware was decoding.
`plat_init()` now checks the equipment word first and normalises a monochrome
machine to mode 7, which is also how the probe got a regression test
(`MACHINE=hercules tools/msdos-shot.sh`).

**Attribute roles instead of an `inv` flag.** The other backends' screen
layers take one bit of emphasis; this hardware has attributes worth naming, so
painters pass a role — `A_TEXT`, `A_EMPH`, `A_SEL`, `A_BAR`, `A_FOOT`,
`A_UNDER` — and `screen.c` resolves it through one of three tables picked at
init: colour (the EDIT.EXE-family look, hint bar pulled to Gmail red),
black-and-white (modes 0/2, or `/MONO`), and the MDA's, where `A_UNDER` is a
real underline. One improvement falls out for free: unread emphasis and the
selection bar are independent attributes, so an unread row keeps its mark
while selected — something no existing backend could afford. The unread mark
itself is CP437's `◆`, and there is no `sc()` charset mapping at all: the
byte in the string is the glyph in the cell, and since `copy_san()` clamps
every wire field to `$20`-`$7E`, the control and high ranges belong to the
chrome.

**Cells are written straight into video memory,** not through the BIOS — INT
10h costs two interrupts per cell and this program repaints whole screens on
every page turn, which is visible at 4.77 MHz. The one machine direct writes
upset is the genuine IBM CGA, which snows in 80-column text; `/SNOW` gates
every write on the start of a horizontal retrace for that card, and it is a
switch rather than a heuristic because a true CGA cannot be reliably
detected, snow is cosmetic, and the PCjr, the MDA and virtually every clone
would otherwise pay the wait for a fault they do not have.

**Five fujinet-lib symbols are overridden app-side,** the `src/adam/`
pattern — defining the symbol leaves the library's member unreferenced, and
each file says when it can be deleted:

- `clock_get_time()` (`clock_msdos.c`) — the msdos archive has no `fn_clock`
  at all, so without this the link fails. Device `0x45`, command `'I'`
  (`APETIME_GET_ISO_LOCAL`), twenty-five bytes of
  `YYYY-MM-DDTHH:MM:SS+HHMM\0` — the same command the Adam shim sends, and
  the reason the DOS clock is *not* used instead: `FUJITIME` sets DOS time
  from the FujiNet, so INT 21h would hand back the same wall clock with the
  `+HHMM` stripped, and the offset is the entire point.
- `fuji_get_adapter_config_extended()` (`fuji_msdos.c`) — checks the INT F5
  vector before the first bus call, because without `FUJINET.SYS` the vector
  is null and `int86x` through it jumps to `0000:0000`. A missing driver
  becomes the "FujiNet not found" screen naming `CONFIG.SYS`, instead of a
  crash. Unlike the Adam's, the library's bool here is not inverted; this
  override exists purely for the guard.
- `network_open()` and `network_read()` (`net_msdos.c`) — the current
  `FUJINET.SYS` speaks the FujiBusPacket protocol, in which `DH` is a field
  descriptor telling the driver how the aux bytes become typed parameters —
  and fujinet-lib 4.11.2's bus entries always send `DH=0`, no parameters at
  all. Commands that need none (status, close, the fuji and clock devices)
  work by luck; the two that carry them do not — the firmware logs
  `Insufficient open paramaters: 0` and NAKs, which is how the first live run
  against fujinet-pc-rs232 found it. The overrides carry their own bus entry
  with `DH` set properly: `DH=2` (mode and trans as two u8 params) for the
  open, `DH=5` (the length as one u16) for the read. The open also sends the
  devicespec at its real length rather than the library's flat 256 — the
  firmware takes the payload into a `std::string` verbatim — and the read
  replaces a `__WATCOMC__` branch that passed the unit *number* where a
  devicespec *pointer* was expected and bailed between chunks whenever the
  status error byte was not exactly 1, zero-as-healthy being precisely the
  Mailbox quirk `st_ok()` in `net.c` exists for.
- `network_error()` (`net_msdos.c`) — the library's version returns
  `network_status()`'s own return instead of the error byte it carried, and
  `network_open()` returns `network_error()` on a failed open — so a refused
  open reported *success*, and the 212 authorize-in-the-Web-UI error, the one
  a first-time user is guaranteed to hit, could never name itself.

`open_error()` in `net.c` gained `__MSDOS__` alongside `__APPLE2__`: the INT
F5 bus layer never writes `fn_network_error`, so recovering the protocol's
own code after a failed open takes the same single status query SmartPort
needs.

**The DGROUP budget** is the number to watch, not address space: the small
model puts every static and the stack in one 64 KB group. `gm_body` at
400 × 79 = 31,600 bytes is the largest thing in it and the whole group sits
near 42 KB — `OPTION map` is on the link line, and the Makefile comment above
`CFLAGS_EXTRA_MSDOS` carries the arithmetic. 400 rows is deliberately more
than the Apple's 240: at 40 columns the same message needs about twice the
rows it does at 78, and one binary has to be ready for either width.
`GM_RXBUF` goes to 1 K because every `network_read` is a whole INT F5/RS-232
round trip.

## Limitations

Inherited from the adapter and the original, not accidents of the port:

- **Read only.** The scope is `gmail.readonly` — no compose, reply, delete, or
  server-side mark-as-read.
- **Inbox only.** The adapter accepts any label as a folder; the folder name is
  hardcoded here.
- **Read/unread is local**, a single 8-byte timestamp in a FujiNet appkey
  (creator `"GM"`, app 1, key 0). A message is unread when it is strictly newer
  than that mark, so opening an older message marks everything older read too,
  including newer ones you skipped. Gmail's list order is not strictly by
  timestamp either, so an occasional entry can flip. The date column is what
  makes any of that visible — it is the same value the mark is compared against.
- **Dates are UTC without a clock.** The offset comes from one
  `clock_get_time` at boot, so a FujiNet with no clock device registered, or an
  unset `[General] timezone`, labels everything in UTC. Nothing else breaks. On
  the Adam that call goes to AdamNet device `0x03` directly, because fujinet-lib
  has no clock for this bus — under ADAMEm it needs `0x03` in the forwarding
  mask, or the same thing happens for the same reason.
- **Slow first paint.** A 16-entry listing is roughly nineteen sequential
  upstream HTTPS round trips inside a single open, and can take 30–60 seconds.
  The CoCo asks for 11 at a time, because that is what its screen shows.
- **No character set conversion.** Anything above plain ASCII becomes a single
  `?` per run; there is no RFC 2047 or UTF-8 decoding anywhere.
- **HTML-only messages show raw markup** — the adapter falls back to
  `text/html` without stripping tags.
- Message numbers are positions, not stable ids: new mail shifts them.
- Bodies longer than `BODY_ROWS` wrapped rows are truncated, flagged with a
  trailing `+` on the page indicator.

---

## Layout

```
src/            portable core
  gmail.h       every shared type and the plat_* / ui_* contract
  main.c        boot, the inbox loop and the reader loop
  net.c         the two device specs, the streaming read, canned data
  body.c        message body ingest -- line endings, overflow, truncation
  wrap.c        greedy word wrap
  sanitize.c    charset clamping
  date.c        epoch seconds to a rendered date column
  clock.c       the UTC offset and the current year, read once at boot
  hwm.c         the read/unread high-water mark, in an appkey
src/atari/      Atari 8-bit backend
  platform.h    geometry, palette, internal API
  screen.c      the screen-RAM blitter
  ui.c          every painter
  dli.c         colour bands
  dlihw.s       the interrupts and the vertical blank hook
  pmg.c         the player/missile "M"
  input.c       key mapping
  key.s         the blocking read
src/apple2enh/  Apple //e (enhanced) backend
  platform.h    geometry, MouseText codes, internal API
  screen.c      the 80-column blitter and the screen-code mapping
  blit.s        the aux/main column split
  ui.c          every painter
  logo.c        the "M" in one bit
  input.c       key mapping and the blocking read
src/coco/       Tandy Color Computer backend
  platform.h    geometry, the SG4 macros, internal API
  screen.c      the 32x16 blitter, plus plat_init/shutdown/net_begin/net_end
  ui.c          every painter
  logo.c        the "M" as semigraphics byte tables
  input.c       key mapping, the frame wait and the blocking read
  include/      <string.h>, <stdlib.h> and <stdint.h> shims for CMOC
src/adam/       Coleco Adam backend
  platform.h    geometry, the GRAPHICS II address macros, the palette
  screen.c      the attribute-plane blitter, plus plat_init/shutdown/net_*
  ui.c          every painter, and the three SmartKey legend sets
  logo.c        the Gmail "M" as four hardware sprites
  input.c       SmartKeys, key mapping, the frame wait and the blocking read
  clock_adam.c  clock_get_time() -- fujinet-lib has none for this bus
  fuji_adam.c   the adapter probe, whose library version inverts its bool
src/msdos/      MS-DOS backend
  platform.h    runtime geometry, attribute roles, CP437 glyphs, internal API
  screen.c      the video probe, the attribute tables and the direct blitter
  ui.c          every painter, at both widths
  logo.c        the "M" as coloured CP437 blocks
  input.c       INT 16h key mapping and the blocking read
  clock_msdos.c clock_get_time() -- fujinet-lib has none for this bus
  fuji_msdos.c  the adapter probe, guarding a null INT F5 vector
  net_msdos.c   network_error() and network_read(), replacing library bugs
  AUTOEXEC.BAT  @ECHO OFF and GMAIL, copied onto the disk with mcopy -t
tests/          host-native tests, built once per screen shape
tools/          headless capture and decode, per platform
intv/           the IntyBASIC original, built on its own
mekkogx/        the cross-platform build template
```

`SRC_DIRS = src src/%PLATFORM%` globs each backend directory, so adding a file is
all that is needed to build it. `src/coco/include/` holds no `.c` files, so the
glob steps over it and it reaches the compiler only as an `-I`.

Built on [MekkoGX](https://github.com/FozzTexx/MekkoGX), a cross-platform build
template for retro machines: that glob is what picks up `src/atari/`,
`src/apple2enh/`, `src/coco/`, `src/adam/` and `src/msdos/` automatically, and
`FUJINET_LIB` is what fetches and links
fujinet-lib. `mekkogx/platforms/apple2enh.mk` is a copy of `apple2.mk` rather
than an include of it, because `common.mk` derives `PLATFORM` from the file that
included it.
