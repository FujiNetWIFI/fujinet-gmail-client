/*
 * Host-side tests for the portable text handling.
 *
 * src/wrap.c, src/sanitize.c, src/body.c and src/date.c have no platform or
 * network dependency, so they build and run on a normal machine. That matters:
 * the line-ending soup, the accumulator overflow, the truncation edges and the
 * calendar arithmetic are the fiddliest logic in the program, and iterating on
 * them through a 6502 cross-compile and an emulator round-trip is far too slow.
 *
 * Built twice, at both screen shapes -- see tests/Makefile. hosttest is the
 * Atari's 40 columns and hosttest80 the Apple II's 78, which is the only thing
 * that would catch a width the wrapper cannot actually reach.
 *
 *   make -C tests
 */

#include <stdio.h>
#include <string.h>

#include "../src/gmail.h"

/*
 * date.c reads these; clock.c defines them on a real target but cannot build
 * here, because it is nothing but a device call. Owning them makes the timezone
 * and the current year test inputs, which is what the offset cases below need.
 */
int          gm_tzoff;
unsigned int gm_year;

static int failures;
static int checks;

static void eq_str(const char *what, const char *got, const char *want)
{
    checks++;
    if (strcmp(got, want) != 0) {
        failures++;
        printf("  FAIL %s\n       got  \"%s\"\n       want \"%s\"\n",
               what, got, want);
    }
}

static void eq_int(const char *what, long got, long want)
{
    checks++;
    if (got != want) {
        failures++;
        printf("  FAIL %s: got %ld, want %ld\n", what, got, want);
    }
}

/* ------------------------------------------------------------------ */

static void test_sanitize(void)
{
    char b[16];

    puts("sanitize");

    copy_san(b, "hello", sizeof(b));
    eq_str("plain", b, "hello");

    /* Control bytes become spaces -- this is also what stops a stray EOL from
       reaching the screen driver and scrolling the display. */
    copy_san(b, "a\tb\x01\x1f" "c", sizeof(b));
    eq_str("controls to space", b, "a b  c");

    /* Each *run* of non-ASCII collapses to one '?', so a UTF-8 name does not
       explode into a row of question marks. */
    copy_san(b, "Jos\xc3\xa9", sizeof(b));
    eq_str("utf-8 run", b, "Jos?");
    copy_san(b, "a\xc3\xa9 b\xc3\xa9", sizeof(b));
    eq_str("two runs", b, "a? b?");

    copy_san(b, "0123456789abcdefghij", sizeof(b));
    eq_str("truncated", b, "0123456789abcde");
    eq_int("nul terminated", b[15], 0);

    copy_san(b, "", sizeof(b));
    eq_str("empty", b, "");
}

/*
 * These cases name their widths as literals -- 40 and 10 -- so the buffer they
 * wrap into has to be sized from the widest of those and not from BODY_STRIDE.
 *
 * It used to be BODY_STRIDE, which was 41 for the Atari and 79 for the Apple
 * and therefore always wide enough by accident. The CoCo's is 33: wrapping to
 * 40 columns then ran every row into the next, and two of these assertions
 * failed with the *previous* row's tail glued onto them. The wrap itself was
 * correct; the test had been writing out of bounds all along and had only ever
 * been handed buffers big enough to hide it.
 */
#define WRAP_STRIDE 41

static void test_wrap(void)
{
    char rows[8][WRAP_STRIDE];
    unsigned int n;

    puts("wrap");

    n = wrap_text("", rows[0], 8, 40, WRAP_STRIDE);
    eq_int("empty yields one blank row", n, 1);
    eq_str("blank row", rows[0], "");

    n = wrap_text("short line", rows[0], 8, 40, WRAP_STRIDE);
    eq_int("one row", n, 1);
    eq_str("one row text", rows[0], "short line");

    /* Greedy whole-word wrap: the break lands between words, never inside. */
    n = wrap_text("aaaa bbbb cccc dddd eeee ffff gggg hhhh iiii",
                  rows[0], 8, 40, WRAP_STRIDE);
    eq_int("two rows", n, 2);
    eq_str("row 0", rows[0], "aaaa bbbb cccc dddd eeee ffff gggg hhhh");
    eq_str("row 1", rows[1], "iiii");

    /* A word longer than the whole row has nowhere to break, so it is split. */
    n = wrap_text("x 012345678901234567890123456789012345678901234567890",
                  rows[0], 8, 40, WRAP_STRIDE);
    eq_int("hard split rows", n, 2);
    eq_str("hard split 0", rows[0], "x 01234567890123456789012345678901234567");
    eq_str("hard split 1", rows[1], "8901234567890");

    /* Running out of row budget ellipsizes rather than dropping the tail
       silently. */
    n = wrap_text("aaaa bbbb cccc dddd", rows[0], 1, 10, WRAP_STRIDE);
    eq_int("budget rows", n, 1);
    eq_str("ellipsis", rows[0], "aaaa bb...");

    /* Exactly-full row: no spurious empty row after it. */
    n = wrap_text("0123456789", rows[0], 8, 10, WRAP_STRIDE);
    eq_int("exact fit", n, 1);
    eq_str("exact fit text", rows[0], "0123456789");
}

static void ingest_str(const char *s)
{
    body_ingest((const unsigned char *) s, (unsigned int) strlen(s));
}

/*
 * No produced row may exceed BODY_COLS.
 *
 * This is the assertion the second binary exists for. Everything else in this
 * file passes at either width by construction -- the inputs are short and the
 * wrap tests name their widths as literals -- so nothing here would notice a
 * BODY_COLS that outran its BODY_STRIDE, a hard split off by one, or an
 * ellipsis written past the end of a row. Those all land as a row longer than
 * the screen, which on a real target is a blitter writing into the next line.
 */
static void rows_fit(const char *what)
{
    unsigned int i;

    checks++;
    for (i = 0; i < gm_body_rows; i++) {
        if (strlen(gm_body[i]) > BODY_COLS) {
            failures++;
            printf("  FAIL %s: row %u is %u wide, cap is %u\n       \"%s\"\n",
                   what, i, (unsigned int) strlen(gm_body[i]),
                   (unsigned int) BODY_COLS, gm_body[i]);
            return;
        }
    }
}

static void test_body(void)
{
    unsigned int i;
    char big[LINE_CAP * 3];

    puts("body ingest");

    /* Every line terminator the firmware might plausibly hand us, including a
       CRLF that must not produce a spurious blank row. */
    body_reset();
    ingest_str("alpha\x9b" "bravo\r\n" "charlie\n" "delta\r" "echo");
    body_finish();
    eq_int("mixed EOL rows", gm_body_rows, 5);
    eq_str("EOL 0", gm_body[0], "alpha");
    eq_str("EOL 1", gm_body[1], "bravo");
    eq_str("EOL 2", gm_body[2], "charlie");
    eq_str("EOL 3", gm_body[3], "delta");
    eq_str("EOL 4", gm_body[4], "echo");

    /* A blank source line survives as one blank row, so paragraph breaks are
       still visible. */
    body_reset();
    ingest_str("one\n\ntwo\n");
    body_finish();
    eq_int("paragraph rows", gm_body_rows, 3);
    eq_str("para 0", gm_body[0], "one");
    eq_str("para 1", gm_body[1], "");
    eq_str("para 2", gm_body[2], "two");

    /* A lone trailing CR at the very end must not leave a dangling blank. */
    body_reset();
    ingest_str("only\r\n");
    body_finish();
    eq_int("trailing crlf rows", gm_body_rows, 1);
    eq_str("trailing crlf", gm_body[0], "only");

    /* $9B has to be recognised as a terminator *before* the charset rules see
       it as a high byte, or the whole message arrives as one line. */
    body_reset();
    ingest_str("a\x9b" "b\x9b" "c");
    body_finish();
    eq_int("9b is EOL not '?'", gm_body_rows, 3);
    eq_str("9b row 0", gm_body[0], "a");

    /* Overflowing the line accumulator must break at a word boundary, not
       slice a word in half. */
    body_reset();
    big[0] = '\0';
    for (i = 0; i < (LINE_CAP / 5) + 8; i++)
        strcat(big, "word ");
    ingest_str(big);
    body_finish();
    checks++;
    for (i = 0; i < gm_body_rows; i++) {
        const char *r = gm_body[i];
        size_t len = strlen(r);
        if (len == 0)
            continue;
        /* every row must consist of whole "word" tokens */
        if (strstr(r, "wor ") || strstr(r, "wo ") || strstr(r, "w ") ||
            (len && r[len - 1] != 'd' && r[len - 1] != ' ')) {
            failures++;
            printf("  FAIL overflow sliced a word: row %u = \"%s\"\n", i, r);
            break;
        }
    }

    /* Truncation sets the flag the reader shows as a trailing '+'. */
    body_reset();
    for (i = 0; i < BODY_ROWS + 50; i++)
        ingest_str("line\n");
    body_finish();
    eq_int("truncated at cap", gm_body_rows, BODY_ROWS);
    eq_int("truncation flagged", gm_body_trunc, 1);

    body_reset();
    ingest_str("short\n");
    body_finish();
    eq_int("not truncated", gm_body_trunc, 0);
}

/*
 * The width-dependent half, which is the whole reason this file is compiled
 * twice. Nothing here hardcodes a column count.
 */
static void test_body_width(void)
{
    char big[BODY_COLS * 8 + LINE_CAP + 1];
    unsigned int want, ntok, i;

    puts("body ingest at BODY_COLS");

    /*
     * A paragraph of five-character tokens, long enough to wrap several times.
     * "aaaa " packs exactly (BODY_COLS + 1) / 5 tokens per row: the trailing
     * space of the last token on a row is the break, so a row holds as many
     * whole tokens as fit in BODY_COLS + 1 columns.
     *
     * The token count comes off LINE_CAP because this is a test of the *wrap*,
     * and a paragraph longer than the line accumulator is no longer one line by
     * the time the wrapper sees it -- flush_overflow() has already broken it at
     * a space and carried the tail into the next row, which adds a row the
     * model here does not predict. It used to be a flat 40 tokens, which is 200
     * characters and happened to fit both of the LINE_CAPs that existed; the
     * CoCo's is 128 and the assertion started counting an accumulator flush as
     * a wrap. LINE_CAP characters exactly is safe -- the overflow check runs
     * before the store, not after.
     */
    body_reset();
    ntok = LINE_CAP / 5;
    big[0] = '\0';
    for (i = 0; i < ntok; i++)
        strcat(big, "aaaa ");
    ingest_str(big);
    body_finish();

    want = (ntok + (BODY_COLS + 1) / 5 - 1) / ((BODY_COLS + 1) / 5);
    eq_int("token rows scale with width", gm_body_rows, want);
    rows_fit("token paragraph");

    /* A single token three rows long has nowhere to break, so it is hard split
       at exactly the width -- every row but the last is full. */
    body_reset();
    for (i = 0; i < BODY_COLS * 3; i++)
        big[i] = 'x';
    big[BODY_COLS * 3] = '\0';
    ingest_str(big);
    body_finish();
    eq_int("hard split rows", gm_body_rows, 3);
    rows_fit("hard split");
    eq_int("split row 0 is full", strlen(gm_body[0]), BODY_COLS);
    eq_int("split row 1 is full", strlen(gm_body[1]), BODY_COLS);
    eq_int("split row 2 is full", strlen(gm_body[2]), BODY_COLS);

    /* The ellipsis path writes into the last row it is allowed, which is the
       one place a row can be built up rather than copied. */
    body_reset();
    for (i = 0; i < BODY_ROWS + 20; i++)
        ingest_str(big);
    body_finish();
    eq_int("truncated at cap", gm_body_rows, BODY_ROWS);
    rows_fit("at the row cap");
}

/* ------------------------------------------------------------------ */

/* Little-endian into the wire's eight bytes, so a case reads as a number. */
static void ts_set(uint8_t ts[8], unsigned long secs)
{
    unsigned char i;

    for (i = 0; i < 8; i++)
        ts[i] = 0;

    ts[0] = (uint8_t) (secs & 0xFF);
    ts[1] = (uint8_t) ((secs >> 8) & 0xFF);
    ts[2] = (uint8_t) ((secs >> 16) & 0xFF);
    ts[3] = (uint8_t) ((secs >> 24) & 0xFF);
}

static void eq_date(const char *what, unsigned long secs, const char *want)
{
    uint8_t ts[8];
    char    got[ENT_DATE_LEN];

    ts_set(ts, secs);
    date_fmt(got, ts);
    eq_str(what, got, want);
}

static void test_date(void)
{
    uint8_t ts[8];
    char    got[ENT_DATE_LEN];

    puts("date");

    gm_tzoff = 0;
    gm_year = 2026;

    /* The everyday case: a message in the current year gets a time. */
    eq_date("in-year gets a time", 1787927520UL, "Aug 28 14:32");

    /* Outside it, the year replaces the time -- the trade every mail client
       makes, because the time of a message from two years ago says nothing. */
    eq_date("out-of-year gets a year", 1709208000UL, "Feb 29  2024");

    /* With no clock there is no current year to compare against, so everything
       takes the time form rather than everything taking a wrong year. */
    gm_year = 0;
    eq_date("no clock, still a time", 1709208000UL, "Feb 29 12:00");
    gm_year = 2026;

    /* 2100 is not a leap year. The full Gregorian rule is the only reason
       civil_from_days gets this right, and it is the classic place to get it
       wrong -- these two are a day apart. */
    eq_date("2100-02-28", 4107540600UL, "Feb 28  2100");
    eq_date("2100-03-01", 4107543300UL, "Mar 01  2100");

    /* The epoch itself, and the last second a signed 32-bit time_t can hold --
       which this code does not use, and must therefore survive. */
    gm_year = 1970;
    eq_date("first second", 1UL, "Jan 01 00:00");
    gm_year = 2038;
    eq_date("2038 rollover", 2147483647UL, "Jan 19 03:14");

    /* Past 2038, where a signed intermediate would have wrapped. */
    gm_year = 2106;
    eq_date("2106", 4294857600UL, "Feb 06 00:00");

    puts("date, timezone offsets");

    /* West of UTC, carrying the date backwards over midnight -- and over a year
       boundary, so this is also what proves the year comparison is against the
       *local* year and not the raw UTC one. */
    gm_tzoff = -300;                    /* CDT */
    gm_year = 2025;
    eq_date("west crosses midnight back", 1767241800UL, "Dec 31 23:30");

    /* The same instant with the clock a year on: now it is out of year, which
       it would not be if the offset had been applied after the comparison. */
    gm_year = 2026;
    eq_date("west, and the year follows it", 1767241800UL, "Dec 31  2025");

    /* East of UTC, carrying it forwards, on a half-hour offset. */
    gm_tzoff = 330;                     /* IST */
    eq_date("east crosses midnight on", 1798749900UL, "Jan 01  2027");

    gm_tzoff = 0;

    /* A timestamp above the low four bytes is a date past 2106, which this wire
       cannot legitimately carry -- so it is a corrupt record, not the future,
       and a blank column is the honest rendering. */
    ts_set(ts, 1787927520UL);
    ts[4] = 1;
    date_fmt(got, ts);
    eq_str("high half rejected", got, "");

    /* Likewise a zero, which is what an unset field looks like. */
    eq_date("zero rejected", 0UL, "");
}

int main(void)
{
    printf("BODY_COLS=%u BODY_ROWS=%u LINE_CAP=%u ENT_SUBJ_LEN=%u\n\n",
           (unsigned int) BODY_COLS, (unsigned int) BODY_ROWS,
           (unsigned int) LINE_CAP, (unsigned int) ENT_SUBJ_LEN);

    test_sanitize();
    test_wrap();
    test_body();
    test_body_width();
    test_date();

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures != 0;
}
