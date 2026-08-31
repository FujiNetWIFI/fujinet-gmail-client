/*
 * Message body ingest.
 *
 * Raw body text is never stored. Each line coming off the wire is accumulated,
 * sanitized and folded into fixed-width display rows on arrival, and the
 * original is discarded -- which is what keeps the body buffer to a size we
 * can actually budget for.
 *
 * This file is deliberately free of any platform or network dependency so the
 * awkward parts (line-ending soup, the accumulator overflow, truncation) can
 * be exercised by tests/hosttest.c on a normal machine.
 */

#include <string.h>

#include "gmail.h"

char          gm_body[BODY_ROWS][BODY_STRIDE];
unsigned int  gm_body_rows;
unsigned char gm_body_trunc;

#ifdef GM_RT_COLS
/* The runtime wrap width -- see the WRAP_COLS comment in gmail.h. The
   initializer only matters if a backend forgets to set it: full-width wrap
   into full-width storage, which is merely the non-runtime behaviour. */
unsigned char gm_wrap_cols = BODY_COLS;
#endif

static char         linebuf[LINE_CAP + 1];
static unsigned int line_len;
static unsigned char pending_lf;        /* saw CR, swallow a following LF */
static unsigned char high_run;          /* inside a run of non-ASCII bytes */

void body_reset(void)
{
    gm_body_rows = 0;
    gm_body_trunc = 0;
    line_len = 0;
    pending_lf = 0;
    high_run = 0;
}

/* Wrap whatever has accumulated into display rows and start a fresh line. */
static void flush_line(void)
{
    unsigned int avail;

    linebuf[line_len] = '\0';
    line_len = 0;
    high_run = 0;

    if (gm_body_rows >= BODY_ROWS) {
        gm_body_trunc = 1;
        return;
    }

    avail = BODY_ROWS - gm_body_rows;
    gm_body_rows += wrap_text(linebuf, gm_body[gm_body_rows],
                              avail, WRAP_COLS, BODY_STRIDE);

    if (gm_body_rows >= BODY_ROWS)
        gm_body_trunc = 1;
}

/*
 * The accumulator filled up before the line ended. Flush what we have, but
 * break at the last space so a word is not sliced in half, and carry the tail
 * forward into the next chunk. Only a single word longer than the whole
 * accumulator falls back to a hard split, which the wrapper handles anyway.
 */
static void flush_overflow(void)
{
    unsigned int brk = line_len;
    unsigned int tail;
    char save;

    while (brk > 0 && linebuf[brk - 1] != ' ')
        brk--;

    if (brk == 0) {
        flush_line();
        return;
    }

    tail = line_len - brk;
    save = linebuf[brk];        /* flush_line is about to NUL this */
    line_len = brk;
    flush_line();
    linebuf[brk] = save;

    memmove(linebuf, linebuf + brk, tail);
    line_len = tail;
}

/*
 * The adapter is asked for CRLF translation, which on the Atari should arrive
 * as ATASCII $9B. Accepting CR and LF as well costs nothing and makes the
 * ingest correct whichever the firmware actually sends -- and a body that has
 * been quoted-printable decoded upstream can carry bare LFs regardless.
 *
 * The end-of-line test has to come before the charset rules below, or $9B
 * would be collapsed to '?' as a high byte and the whole message would arrive
 * as one enormous line.
 */
void body_ingest(const unsigned char *p, unsigned int n)
{
    unsigned char c;

    while (n--) {
        c = *p++;

        if (pending_lf) {
            pending_lf = 0;
            if (c == 0x0A)
                continue;               /* the LF half of a CRLF */
        }

        if (c == 0x9B || c == 0x0D || c == 0x0A) {
            if (c == 0x0D)
                pending_lf = 1;
            flush_line();
            if (gm_body_trunc)
                return;
            continue;
        }

        /* Same charset policy as copy_san(). */
        if (c > 126) {
            if (high_run)
                continue;
            high_run = 1;
            c = '?';
        } else {
            high_run = 0;
            if (c < 32)
                c = ' ';
        }

        if (line_len >= LINE_CAP) {
            flush_overflow();
            if (gm_body_trunc)
                return;
        }
        linebuf[line_len++] = (char) c;
    }
}

/* Flush a body that did not end with a line terminator. */
void body_finish(void)
{
    if (line_len)
        flush_line();
}
