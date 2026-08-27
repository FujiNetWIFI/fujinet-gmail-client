# FujiNet Gmail client — Intellivision

Reads your Gmail inbox on an Intellivision, over the FujiNet `N:GMAIL:`
protocol adapter. Written in IntyBASIC.

Two screens: an **inbox list** with a selection bar that bounce-scrolls the
highlighted line so a long `Sender: Subject` can be read in full, and a
**message reader** with the body word-wrapped to 20 columns and scrollable a
line or a page at a time.

Text is mixed case throughout. The program runs in colour-stack mode, where
GROM reaches cards 0–94 (ASCII 32–126) and lowercase comes free; the
alternative, foreground/background mode, caps out at card 63 and would force
every subject and every line of every message through an uppercase fold.

## Requirements

- A FujiNet with **Google authorized in its Web UI** ("Authorize with
  Google"). The Gmail adapter piggybacks on the Google Drive OAuth grant, so
  there is no password to type on the Intellivision and this program never
  sees a credential.
- That grant has to be new enough to carry the `gmail.readonly` scope. A
  grant made before the mail protocol landed only has the Drive scope, and
  every request comes back **`Google access denied`** until you re-authorize.
  If you see **`Authorize Google in the FujiNet Web UI`** instead, there is
  no grant at all.
- IntyBASIC v1.4.2 and `as1600` from the jzIntv SDK to build.
- For the emulator: a FujiNet-patched jzIntv, and a FujiNet (or `fujinet-pc`)
  reachable over BoIP.

## Build and run

```sh
make                     # -> gmail.bin + gmail.cfg (SD/cart menu), gmail.rom
./run.sh                 # build if stale, then launch jzIntv against BoIP
FUJINET_TARGET=host:9995 ./run.sh
./run.sh --fujinet-debug # extra flags pass straight through to jzIntv
```

`make` expects `intybasic` and `as1600` on `PATH` and IntyBASIC's
prologue/epilogue at `~/Workspace/IntyBASIC/intybasic/`; override with
`make INTYBASIC=… AS1600=… LIBDIR=…`.

## Controls

**Inbox**

| Control | Action |
|---|---|
| Disc up/down | Move the selection bar; steps to the next page of 8, then fetches the next 16 at the edge |
| Disc left/right | Page by 8 |
| Enter or any action button | Open the selected message |
| 9 or Clear | Refresh, back to the top of the inbox |

Leave the bar on one entry for about three quarters of a second and the line
starts panning back and forth so you can read the rest of it.

**Message**

| Control | Action |
|---|---|
| Disc up/down | Scroll one line |
| Disc left/right | Scroll one page (8 lines) |
| Clear or any action button | Back to the inbox |

The header's right-hand corner shows the page position, e.g. `3/12`. A
trailing `+` means the message was longer than the reader holds and was
truncated.

## Read and unread

The envelope in column 0 is closed and red for unread, open and tan for read.

**This is inferred locally, not read from Gmail.** The protocol adapter does
not expose read/unread state at all: its only per-message flag is IMPORTANT,
that flag exists solely in the human-readable listing format, and the raw
index this client parses has no flags field whatsoever. Gmail's own `UNREAD`
label is present in the JSON the firmware parses and is simply never looked
at.

So the client keeps a **high-water mark**: one timestamp, stored in a FujiNet
appkey (creator `GM`, app 1, key 0), and anything newer than it is shown as
unread. Opening a message advances the mark to that message's timestamp,
which marks it *and everything older* as read. That is inherent to a
high-water mark rather than a bug — it matches reading down an inbox, but it
does mean skipping ahead to an older message marks the newer ones you skipped
as read too. The mark persists across power cycles; it does not sync with
Gmail or with your phone.

If the firmware ever grows a flags byte in the raw index record — the `UNREAD`
label is one line away in `GMAIL.cpp` — that becomes the better source, and it
would also bring back the IMPORTANT flag, which is currently unavailable in
any format this client can use.

## Known limitations

- **Real PiRTO II hardware.** The pico firmware caps every non-`MOUNT`
  transaction at 5 seconds (`pico/intellivision/firmware/src/fujinet.c`),
  and opening the mailbox routinely takes longer than that — the adapter runs
  one HTTPS round trip *per message* inside the open, so a 16-entry listing is
  roughly 19 sequential requests and takes tens of seconds. Until the pico
  gets a per-command exception like `MOUNT_IMAGE` already has, this is an
  emulator-and-`fujinet-pc` program. jzIntv imposes no such limit.
- **Reading an old message in a large mailbox is slow.** Sequence numbers
  count from the oldest message, and resolving one pages `messages.list` down
  500 ids at a time.
- **Sequence numbers are not stable.** They are positions, not ids, so new
  mail shifts every number. The client refetches rather than caching across
  pages, so this only matters if mail arrives mid-session.
- **HTML-only messages show raw markup.** The adapter prefers `text/plain`
  and falls back to `text/html` without stripping tags.
- **No character set conversion.** Subjects and bodies are raw UTF-8 with no
  RFC 2047 decoding, so anything above ASCII 126 becomes a single `?` (one per
  run, so an accented letter costs one placeholder, not three).
- **Read-only.** The OAuth scope is `gmail.readonly`; there is no compose,
  reply, delete or mark-as-read.
- Messages longer than 128 wrapped rows are truncated (flagged with `+`).
- Only the inbox. The adapter accepts any label as a folder, but the folder
  name is fixed here.

## Layout notes

Two things about the screen are load-bearing rather than cosmetic, and both
are documented at length in `st_inbox.bas`:

- The selection bar spans **columns 1–19 only**. Column 0 stays on the
  surrounding colour-stack run — which is also why the read/unread icon lives
  there.
- **Row 10 is a permanent blank spacer.** The bar's closing colour-stack
  advance has to land on a row below it, so the list stops at row 9.

## Files

| File | |
|---|---|
| `gmail.bas` | Entry point, boot, shared helpers (`fmt_u32`, sanitizer), status/error screens, high-water mark |
| `constants.bas` | Screen/colour/input constants, GRAM cards, wire layout, scratch-RAM map |
| `fujinet.bas` | Mailbox transport. From `netcat/intv`, with two changes: a settable transaction timeout and settable open mode/translation |
| `input.bas` | Edge-detected controller input, from `fujinet-config/intv` |
| `screen.bas` | BACKTAB text helpers. Printable clamp is 126, not 95 — we need lowercase |
| `scroll.bas` | Bounce-scroll of the highlighted row, from `fujinet-lobby/intv` |
| `wrap.bas` | Greedy word wrap, from `fujinet-news/clients/intv` |
| `gfx.bas` | GRAM cards and the four-MOB Gmail logo |
| `st_inbox.bas` | Inbox screen |
| `st_msg.bas` | Message screen |
