# FujiNet Gmail client

A read-only Gmail inbox browser for 8-bit machines, talking to Gmail through a
FujiNet's GMAIL network adapter.

Two implementations live here:

- `intv/` — the original, in IntyBASIC for the Intellivision.
- `src/` — the C port. `src/` is portable across MekkoGX platforms; each target
  supplies a backend under `src/<platform>/`. **Atari 8-bit (cc65) is done**;
  the other platforms listed in the `Makefile` still need a backend.

There is **no authentication in this program**. The FujiNet's GMAIL adapter
uses the Google grant stored in FujiNet config — the user authorizes once in
the FujiNet Web UI and the firmware handles token refresh. The console never
sees a credential, which is why the whole client is two device-spec opens plus
a user interface.

---

## Screens

**Inbox** — 16 messages per page, the selected row in inverse video, an unread
marker in column 1, and the selected entry spelled out in full underneath the
list (both list columns truncate hard at 40 columns).

```
        Gmail  Inbox                         <- Gmail "M" in P/M graphics
                                 1-16/137
 >* Alice Kim     Re: lunch tomorrow
  * Bob Chen      Invoice #22 is attached
    Carol Diaz    Standup notes

 Alice Kim: Re: lunch tomorrow
 RET:READ  <>:PAGE  R:REFRESH  ESC:QUIT
```

**Message reader** — sender, subject wrapped to two lines, then the body
word-wrapped to 40 columns with a page indicator.

**Splash / busy / error** — flat screens with the logo. The error screen names
the failure and shows the raw codes beneath it (`open code 212 dev 144`), which
is the difference between a reportable bug and "it just says error".

### Keys

| Key | Inbox | Reader |
|---|---|---|
| `Ctrl+↑` / `-` | previous message, refetching past the top | scroll up one line |
| `Ctrl+↓` / `=` | next message, refetching past the bottom | scroll down one line |
| `Ctrl+←` / `+` | previous page | page up |
| `Ctrl+→` / `*` | next page | page down |
| `RETURN` | open the message | — |
| `ESC` | quit | back to the inbox, without refetching |
| `R` | refresh from the top | — |

The Atari's cursor keys need `Ctrl` held, which is a lot to ask while browsing
a mailbox, so the bare keycaps those arrows live on work too.

---

## Building

Needs cc65 on `PATH`. `FUJINET_LIB` in the `Makefile` points at a local
fujinet-lib checkout; it also accepts a version number, a zip, or a git URL,
and if left empty it downloads the latest release.

```sh
make atari/product      # -> r2r/atari/gmail.com   (an XEX despite the extension)
make atari              # also builds a bootable .atr
```

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
line-accumulator overflow, wrapping and truncation. These are the fiddliest
parts of the program and they have no platform dependency, so they run
natively instead of through a cross-compile and an emulator:

```sh
make -C tests
```

**Headless screen capture** builds, runs in `atari800` with no display, breaks
where the program blocks on the keyboard, and decodes the text screen, the
display list and the P/M buffer out of a full RAM dump:

```sh
tools/atari-shot.sh                          # fake data, first screen
tools/atari-shot.sh "K_DOWN,K_DOWN,K_ENTER"  # scripted keys, fake data
REAL=1 TMO=300 tools/atari-shot.sh           # against real Gmail
```

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
  timestamp either, so an occasional entry can flip.
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

Built on [MekkoGX](https://github.com/FozzTexx/MekkoGX), a cross-platform build
template for retro machines: `SRC_DIRS = src src/%PLATFORM%` is what picks up
`src/atari/` automatically, and `FUJINET_LIB` is what fetches and links
fujinet-lib.
