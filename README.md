# FujiNet Gmail client

A read-only Gmail inbox browser for 8-bit machines, talking to Gmail through a
FujiNet's GMAIL network adapter.

Two implementations live here:

- `intv/` — the original, in IntyBASIC for the Intellivision.
- `src/` — the C port. `src/` is portable across MekkoGX platforms; each target
  supplies a backend under `src/<platform>/`. **Atari 8-bit** and **Apple //e
  (enhanced)** are both done, in cc65.

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

**Message reader** — sender and date, subject wrapped to two lines, then the
body word-wrapped to the screen width with a page indicator.

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
```

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

Gmail needs a Google grant with the `gmail.readonly` scope, authorized through
the FujiNet Web UI. Only run one fujinet-pc instance — two of them fight over
the NetSIO port and one will exit mid-session.

## Testing

**Host tests** cover the portable text handling — the line-ending soup, the
line-accumulator overflow, wrapping, truncation and the epoch arithmetic. These
are the fiddliest parts of the program and they have no platform dependency, so
they run natively instead of through a cross-compile and an emulator:

```sh
make -C tests
```

Built twice, because the core's fixed widths are overridable and both backends
override them: `hosttest` is the Atari's shape and `hosttest80` the Apple II's.
That is the only way the width-dependent paths get covered at a width the Atari
never reaches, and the assertion that earns the second binary is the one that
checks no produced row is wider than `BODY_COLS` — a `BODY_STRIDE` mismatch or
an off-by-one in the hard split lands there and nowhere else.

The date tests are worth their space for one reason: 2100 is not a leap year,
and the full Gregorian rule is the only thing that makes `civil_from_days` get
it right. The two assertions a day either side of 2100-03-01 are what hold it.

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

`gm_body` is 18,960 of that BSS. `BODY_ROWS` comes *down* from the Atari's 300
even though the buffer grows, because at 78 columns a message needs about half
the rows: 240 × 78 holds 18,720 characters against the Atari's 12,000. Check the
map before raising it, and mind that `__STACKSIZE__` trades directly against
BSS.

---

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
  unset `[General] timezone`, labels everything in UTC. Nothing else breaks.
- **Slow first paint.** A 16-entry listing is roughly nineteen sequential
  upstream HTTPS round trips inside a single open, and can take 30–60 seconds.
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
tests/          host-native tests, built at both screen shapes
tools/          headless capture and decode, per platform
intv/           the IntyBASIC original, built on its own
mekkogx/        the cross-platform build template
```

`SRC_DIRS = src src/%PLATFORM%` globs both backend directories, so adding a file
is all that is needed to build it.

Built on [MekkoGX](https://github.com/FozzTexx/MekkoGX), a cross-platform build
template for retro machines: that glob is what picks up `src/atari/` and
`src/apple2enh/` automatically, and `FUJINET_LIB` is what fetches and links
fujinet-lib. `mekkogx/platforms/apple2enh.mk` is a copy of `apple2.mk` rather
than an include of it, because `common.mk` derives `PLATFORM` from the file that
included it.
