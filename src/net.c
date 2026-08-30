/*
 * FujiNet transport.
 *
 * There is no authentication here, and there is none anywhere else in the
 * program either. The FujiNet GMAIL adapter piggybacks on the Google grant
 * stored in FujiNet config; the user authorizes once in the FujiNet Web UI and
 * the firmware handles token refresh. A 212 back from an open just means
 * "not authorized yet".
 *
 * Two device specs, both handled by the adapter's Mailbox protocol:
 *
 *   N:GMAIL:///INBOX?range=<start>-<end>   aux1 = 6 (DIRECTORY), aux2 = 255 (raw)
 *   N:GMAIL:///INBOX/<msgnum>              aux1 = 4 (READ),      aux2 = 3   (CRLF)
 *
 * Note the three slashes: GMAIL: + // (empty host) + /INBOX (path).
 */

#include <stdlib.h>
#include <string.h>

#include <fujinet-network.h>
#include <fujinet-fuji.h>

#include "gmail.h"

/* ------------------------------------------------------------------ */
/* State owned by the fetch layer                                      */
/* ------------------------------------------------------------------ */

struct entry  gm_index[IDX_MAX];
unsigned char gm_count;
unsigned long gm_range;
unsigned long gm_total;
unsigned char gm_next;
unsigned char gm_ecode;
unsigned char gm_dev_ecode;
const char   *gm_stage = "";

/* ------------------------------------------------------------------ */
/* Scratch                                                             */
/* ------------------------------------------------------------------ */

/* The body read chunk. Every byte of it is BSS that the widest screen's row
   buffer would rather have, so a tight machine lowers it -- gm_fetch_body()
   loops until the channel is drained either way. */
#ifndef GM_RXBUF
#define GM_RXBUF    512
#endif

static char             url[64];
static struct wire_rec  wire;
static unsigned char    rxbuf[GM_RXBUF];
/* Last network_status() result. */
static unsigned int     st_bw;
static unsigned char    st_conn;
static unsigned char    st_err;


#ifdef GM_FAKE_DATA
static unsigned char fake_index(unsigned long range);
static unsigned char fake_body(void);
#endif

/*
 * Ask the device where it stands, returning the NDEV status byte, or
 * GM_NOREPLY when the status call itself failed.
 */
static unsigned char probe(void)
{
    st_bw = 0;
    st_conn = 0;
    st_err = 0;

    if (network_status(url, &st_bw, &st_conn, &st_err) != FN_ERR_OK) {
        gm_dev_ecode = fn_device_error;
        return GM_NOREPLY;
    }

    return st_err;
}

/*
 * Is this status byte a success?
 *
 * The Intellivision original treated 1 as the only success value, and
 * fujinet-lib's own header says the same ("1 for normal OK status, don't ask
 * why"). Neither is true here: over SIO the Mailbox protocol reports a healthy
 * channel as 0 in DVSTAT+3, and a listing with 3520 bytes staged and ready
 * still comes back as 0. Accept both -- every value that actually means
 * something (136 EOF, 167, 170, 210, 212) is distinct from either.
 */
static unsigned char st_ok(unsigned char code)
{
    return (code == 0 || code == GM_OK);
}

/*
 * Recover the protocol's own status byte from a failed open. The two buses
 * answer this question in completely different places.
 *
 * SIO: the Atari bus layer has already asked. atari/src/bus/bus_status.s sees
 * the device error (144), re-queries with network_status_unit to pull the
 * extended information, leaves the protocol's status byte in fn_network_error,
 * and only then reports the failure. Asking again would mean querying a channel
 * that never opened. Anything other than 144 -- 138 timeout, 139 NAK, 143
 * checksum -- means the device never gave us a usable reply at all.
 *
 * SmartPort: nobody has asked, and 144 is not even in the value space. The
 * apple2 bus layer stores a SmartPort code in fn_device_error (sp.inc: $00-$02,
 * $06, $11, $21-$2F, $30-$3F, $50, $7F), and fn_network_error is only ever
 * written by network_read and by the Atari's bus_status. So the 144 test can
 * never fire and every failed open would report as a timeout -- including the
 * 212 that means "authorize Google in the Web UI", which is the one error a
 * first-time user is guaranteed to hit and the one that has to name itself.
 *
 * The fix is to ask, once. The channel is still addressable because
 * network_open set the unit before issuing the control command, and
 * network_status needs nothing else.
 */
static unsigned char open_error(void)
{
    /* Capture the raw device code now: the network_close() on the way out of
       the failure path issues another device command and overwrites it. */
    gm_dev_ecode = fn_device_error;

#ifdef __APPLE2__
    {
        unsigned char dev  = gm_dev_ecode;
        unsigned char code = probe();

        /* probe() overwrites gm_dev_ecode when the status call itself fails.
           The open's code is the one worth reporting, so put it back. */
        gm_dev_ecode = dev;

        /* GM_NOREPLY means the status call failed too, which on this bus is
           the only thing that really is "no reply". */
        return (code == GM_NOREPLY) ? 0 : code;
    }
#else
    if (fn_device_error == 144)
        return fn_network_error;
    return 0;
#endif
}

/*
 * Settle the status after an open. The Mailbox protocol stages the entire
 * result during open(), so a single status normally reports the full byte
 * count with no polling. The bounded retry is insurance against a slower
 * transport reporting zero for a moment -- it must never become an unbounded
 * wait, because that is exactly the failure mode we are avoiding.
 */
static unsigned char settle(void)
{
    unsigned char tries;
    unsigned char code;

    code = GM_NOREPLY;

    for (tries = 0; tries < 8; tries++) {
        code = probe();
        if (!st_ok(code))           /* EOF, an error, or no reply -- decided */
            return code;
        if (st_bw != 0)
            return code;
        if (st_conn == 0)           /* connected == 0 and nothing waiting */
            return code;
    }

    return code;
}

static void fail(unsigned char code)
{
    gm_ecode = code;
    network_close(url);
    plat_net_end();
}

/* ------------------------------------------------------------------ */
/* Index                                                               */
/* ------------------------------------------------------------------ */

static void build_index_url(unsigned long range)
{
    char num[12];

    strcpy(url, "N:GMAIL:///INBOX?range=");
    ultoa(range, num, 10);
    strcat(url, num);
    strcat(url, "-");
    ultoa(range + IDX_MAX - 1, num, 10);
    strcat(url, num);
}

/*
 * The wire's little-endian u32, assembled a byte at a time.
 *
 * This is the only place in the program that knows the wire's byte order for a
 * number, and it exists because the 6809 does not share the 6502's. See the
 * comment on struct wire_rec.
 */
static unsigned long rd32le(const uint8_t *p)
{
    return (unsigned long) p[0]
         | ((unsigned long) p[1] << 8)
         | ((unsigned long) p[2] << 16)
         | ((unsigned long) p[3] << 24);
}

static void parse_rec(unsigned char slot)
{
    struct entry *e = &gm_index[slot];

    /* The message number goes straight back out in a URL, so render it once
       and never touch it as a number again. */
    ultoa(rd32le(wire.msgnum), e->num, 10);

    /* displayName is preferred; the adapter leaves it empty when the header
       had no friendly name, and then the address is all we have. */
    copy_san(e->name,
             wire.name[0] ? wire.name : wire.email,
             ENT_NAME_LEN);
    copy_san(e->subject, wire.subject, ENT_SUBJ_LEN);

    memcpy(e->ts, wire.ts, 8);
    e->unread = 0;
}

void gm_calc_next(void)
{
    /* A short page always means end of list. */
    if (gm_count < IDX_MAX) {
        gm_next = 0;
        return;
    }
    gm_next = (gm_range + IDX_MAX < gm_total) ? 1 : 0;
}

unsigned char gm_fetch_index(unsigned long range)
{
    unsigned char code;
    unsigned char i, n;
    int           got;

    gm_count = 0;

    build_index_url(range);

#ifdef GM_FAKE_DATA
    return fake_index(range);
#endif

    plat_net_begin();

    /* A 16-entry listing is ~19 sequential upstream HTTPS round trips inside
       this one open, so widen the SIO timeout around it and put it straight
       back afterwards. */
    gm_stage = "open";
    fn_default_timeout = TMO_LONG;
    code = network_open(url, MB_MODE_DIR, MB_FMT_RAW);
    fn_default_timeout = TMO_NORM;

    if (code != FN_ERR_OK) {
        fail(open_error());
        return 0;
    }

    gm_stage = "status";
    code = settle();
    if (!st_ok(code) && code != GM_EOF) {
        fail(code == GM_NOREPLY ? 0 : code);
        return 0;
    }

    n = (unsigned char) (st_bw / REC_STRIDE);
    if (n > IDX_MAX)
        n = IDX_MAX;

    for (i = 0; i < n; i++) {
        /* Read exactly one record per call so a record never straddles. */
        gm_stage = "read";
        got = network_read(url, (uint8_t *) &wire, REC_STRIDE);
        if (got != REC_STRIDE)
            break;              /* short read: EOF or error, keep what we have */

        /* The listing is newest-first and msgNum is a 1-based position counted
           from the oldest, so record 0 is the only place the folder size is
           available. */
        if (i == 0)
            gm_total = rd32le(wire.msgnum) + range;

        parse_rec(i);
    }

    gm_count = i;
    gm_range = range;

    network_close(url);
    plat_net_end();

    gm_calc_next();
    return 1;
}

/* ------------------------------------------------------------------ */
/* Body                                                                */
/* ------------------------------------------------------------------ */

static void build_body_url(const char *msgnum)
{
    strcpy(url, "N:GMAIL:///INBOX/");
    strcat(url, msgnum);
}

unsigned char gm_fetch_body(const char *msgnum)
{
    unsigned char code;
    unsigned int  chunk;
    int           got;

    body_reset();

    build_body_url(msgnum);

#ifdef GM_FAKE_DATA
    return fake_body();
#endif

    plat_net_begin();

    gm_stage = "open";
    fn_default_timeout = TMO_LONG;
    code = network_open(url, MB_MODE_READ, MB_TRANS_CRLF);
    fn_default_timeout = TMO_NORM;

    if (code != FN_ERR_OK) {
        fail(open_error());
        return 0;
    }

    gm_stage = "status";
    code = settle();
    if (!st_ok(code) && code != GM_EOF) {
        fail(code == GM_NOREPLY ? 0 : code);
        return 0;
    }

    /*
     * Never ask network_read() for more than the status just said is waiting.
     * Its internal loop spins without a timeout when nothing is available but
     * the connection is still up, so staying inside st_bw keeps that spin
     * unreachable.
     */
    while (st_bw != 0) {
        chunk = st_bw;
        if (chunk > sizeof(rxbuf))
            chunk = sizeof(rxbuf);

        got = network_read(url, rxbuf, chunk);
        if (got <= 0)
            break;

        body_ingest(rxbuf, (unsigned int) got);
        if (gm_body_trunc)
            break;

        code = probe();
        if (!st_ok(code))
            break;              /* EOF or error -- drained */
    }

    body_finish();

    network_close(url);
    plat_net_end();
    return 1;
}

/* ------------------------------------------------------------------ */
/* Fake data                                                           */
/* ------------------------------------------------------------------ */

#ifdef GM_FAKE_DATA

/*
 * Built with -DGM_FAKE_DATA, the two fetch functions above short-circuit into
 * these and the program never touches a FujiNet. That makes the whole UI --
 * layout, truncation, wrapping, paging, scrolling, the read/unread markers --
 * testable in an emulator with no adapter, no network and no Google account.
 *
 * The canned data is chosen to hit the awkward cases rather than to look
 * pretty: an empty display name, fields straddling every column budget, runs
 * of non-ASCII bytes, a control byte, a token too long to wrap, and enough
 * text to overrun BODY_ROWS.
 */

#define FAKE_TOTAL  137

/* 2026-08-29 14:32:00 UTC, the newest canned message, and two and a half hours
   between each. The canned clock in clock.c is the same day, so the newest
   entry reads as this morning rather than as something from the archive. */
#define FAKE_EPOCH  1788013920UL
#define FAKE_STEP   9000UL

static const char *const fake_name[8] = {
    "Alice Kim",
    "",                                     /* falls back to the address */
    "Bartholomew Vandersteen-Wu",           /* wider than the From column */
    "Carol",
    "D",
    "Eve \xc3\xa9\xc3\xa9 Adams",           /* run of high bytes -> one "?" */
    "Frank|Ogilvy",                         /* the | becomes a real control
                                               byte below -- writing '\t' here
                                               would not test what it looks
                                               like it tests on the Atari,
                                               because cc65 charmaps a
                                               source-literal tab to ATASCII
                                               $7F, which is a *high* byte, not
                                               a control one. apple2enh does no
                                               literal translation, so the trap
                                               is Atari-only -- but the
                                               substitution is what makes the
                                               canned data identical on both */
    "Grace Hopper"
};

static const char *const fake_subj[8] = {
    "Re: lunch tomorrow",
    "Invoice #22 is attached and overdue by three weeks now",
    "Standup notes",
    "",
    "Your FujiNet order has shipped",
    "URGENT: \xe2\x80\x94 action required \xe2\x80\x94 please read",
    "1",
    "Deployment window moved to Thursday 0200 UTC, please ack"
};

static unsigned char fake_index(unsigned long range)
{
    unsigned char i, n, s;
    unsigned long num;
    unsigned long secs;

    gm_total = FAKE_TOTAL;
    gm_range = range;

    if (range >= FAKE_TOTAL) {
        gm_count = 0;
        gm_calc_next();
        return 1;
    }

    n = IDX_MAX;
    if (range + n > FAKE_TOTAL)
        n = (unsigned char) (FAKE_TOTAL - range);

    for (i = 0; i < n; i++) {
        /* Newest first, numbered from the oldest -- same as the wire, so
           gm_total = rec[0].msgnum + range still comes out right. */
        num = FAKE_TOTAL - range - i;
        s = (unsigned char) ((i + (unsigned char) range) & 7);

        memset(&wire, 0, sizeof(wire));

        /* Laid down as wire bytes, not assigned as a number, because that is
           what a record off the wire is -- and on a big-endian target the two
           are not the same thing. */
        wire.msgnum[0] = (unsigned char) num;
        wire.msgnum[1] = (unsigned char) (num >> 8);
        wire.msgnum[2] = (unsigned char) (num >> 16);
        wire.msgnum[3] = (unsigned char) (num >> 24);

        strcpy(wire.name, fake_name[s]);
        if (s == 6)
            wire.name[5] = 0x09;        /* a genuine wire control byte */
        strcpy(wire.email, "someone.long.address@example.com");
        strcpy(wire.subject, fake_subj[s]);

        /*
         * Timestamps ascend with the message number, so the high-water mark
         * has something monotonic to bite on -- and they are real epoch
         * seconds, because the date column renders them.
         *
         * They used to be the message number itself, which was monotonic and
         * nothing else: as an epoch, 137 is two minutes past midnight on the
         * 1st of January 1970, and any timezone west of UTC drags it below the
         * epoch and date_fmt correctly refuses it. Every date on the canned
         * screen came out blank.
         *
         * The year form is not reachable from one page at this spacing, which
         * is deliberate: it is covered by tests/hosttest.c, where it is an
         * assertion rather than something to eyeball. What the capture is for
         * is proving the column renders at all.
         */
        secs = FAKE_EPOCH - (unsigned long) (range + i) * FAKE_STEP;
        wire.ts[0] = (unsigned char) secs;
        wire.ts[1] = (unsigned char) (secs >> 8);
        wire.ts[2] = (unsigned char) (secs >> 16);
        wire.ts[3] = (unsigned char) (secs >> 24);

        parse_rec(i);
    }

    gm_count = n;
    gm_calc_next();
    return 1;
}

static const char *const fake_para[8] = {
    "Hi there,",
    "",
    "This is a synthetic message body. It exists to exercise the word wrap, "
    "the line and page scrolling and the paragraph handling without a FujiNet "
    "anywhere in sight, and it runs on long enough to need several pages.",
    "",
    /* The URL has to outrun the widest BODY_COLS any backend asks for, or the
       hard-split path stops being exercised on the wider screen: at 40 columns
       the old 72-character one split three ways, and at 78 it fitted on one row
       and tested nothing. */
    "Here is a token far too long to fit on one row, so the "
    "wrapper has to hard split it: "
    "https://example.com/a/very/long/path/that/keeps/going/and/going/and/going/"
    "and/going/and/going/until/it/cannot/possibly/fit/on/any/row/at/all",
    "",
    "Regards,",
    "The build"
};

static const unsigned char eol_atascii[1] = { 0x9B };
static const unsigned char eol_crlf[2]    = { 0x0D, 0x0A };
static const unsigned char eol_lf[1]      = { 0x0A };
static const unsigned char eol_cr[1]      = { 0x0D };

static unsigned char fake_body(void)
{
    unsigned char rep, i;

    body_reset();

    for (rep = 0; rep < 8 && !gm_body_trunc; rep++) {
        for (i = 0; i < 8; i++) {
            body_ingest((const unsigned char *) fake_para[i],
                        (unsigned int) strlen(fake_para[i]));

            /* Deliberately mixed terminators: the ingest has to cope with
               ATASCII EOL, bare CR, bare LF and CRLF, whichever the firmware
               turns out to send.

               These are byte arrays rather than string literals on purpose.
               cc65 charmaps a literal "\n" to ATASCII $9B for the Atari, so
               "\r\n" in source would reach the ingest as CR followed by EOL --
               two terminators, and a spurious blank row. apple2enh does no such
               translation and would be fine either way, which is precisely why
               the arrays stay: they make one canned body, not two.

               Bare CR is not a hypothetical. Protocol.h's native_eol defaults
               to CR and only the SIO network device overrides it to $9B, so a
               body arriving over SmartPort, DriveWire or AdamNet really is
               CR-terminated. This is the case that cost the calendar client an
               entirely empty screen. */
            switch (i & 3) {
            case 0:  body_ingest(eol_atascii, 1); break;
            case 1:  body_ingest(eol_crlf,    2); break;
            case 2:  body_ingest(eol_lf,      1); break;
            default: body_ingest(eol_cr,      1); break;
            }

            if (gm_body_trunc)
                break;
        }
    }

    body_finish();

    return 1;
}

#endif /* GM_FAKE_DATA */
