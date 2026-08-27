# fujinet-gmail-client

Gmail clients for retro platforms, talking to the FujiNet `N:GMAIL:` protocol
adapter.

| Platform | Path | Status |
|---|---|---|
| Intellivision | [`intv/`](intv/) | Working — inbox list + message reader, IntyBASIC |

The adapter itself lives in `fujinet-firmware/lib/network-protocol/`
(`Mailbox.cpp`, `GMAIL.cpp`). It reuses the FujiNet's Google Drive OAuth
grant, widened to include the `gmail.readonly` scope, so a client never
handles a credential — the user authorizes once in the FujiNet Web UI.

See [`intv/README.md`](intv/README.md) for build instructions, controls, and
the protocol's current limitations (notably: no read/unread flag is exposed,
so the Intellivision client infers it from a locally persisted high-water
mark).
