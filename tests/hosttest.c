/*
 * Host-side tests for the portable text handling.
 *
 * src/wrap.c, src/sanitize.c and src/body.c have no platform or network
 * dependency, so they build and run on a normal machine. That matters: the
 * line-ending soup, the accumulator overflow and the truncation edges are the
 * fiddliest logic in the program, and iterating on them through a 6502
 * cross-compile and an emulator round-trip is far too slow.
 *
 *   make -C tests && tests/hosttest
 */

#include <stdio.h>
#include <string.h>

#include "../src/gmail.h"

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

static void test_wrap(void)
{
    char rows[8][BODY_STRIDE];
    unsigned int n;

    puts("wrap");

    n = wrap_text("", rows[0], 8, 40, BODY_STRIDE);
    eq_int("empty yields one blank row", n, 1);
    eq_str("blank row", rows[0], "");

    n = wrap_text("short line", rows[0], 8, 40, BODY_STRIDE);
    eq_int("one row", n, 1);
    eq_str("one row text", rows[0], "short line");

    /* Greedy whole-word wrap: the break lands between words, never inside. */
    n = wrap_text("aaaa bbbb cccc dddd eeee ffff gggg hhhh iiii",
                  rows[0], 8, 40, BODY_STRIDE);
    eq_int("two rows", n, 2);
    eq_str("row 0", rows[0], "aaaa bbbb cccc dddd eeee ffff gggg hhhh");
    eq_str("row 1", rows[1], "iiii");

    /* A word longer than the whole row has nowhere to break, so it is split. */
    n = wrap_text("x 012345678901234567890123456789012345678901234567890",
                  rows[0], 8, 40, BODY_STRIDE);
    eq_int("hard split rows", n, 2);
    eq_str("hard split 0", rows[0], "x 01234567890123456789012345678901234567");
    eq_str("hard split 1", rows[1], "8901234567890");

    /* Running out of row budget ellipsizes rather than dropping the tail
       silently. */
    n = wrap_text("aaaa bbbb cccc dddd", rows[0], 1, 10, BODY_STRIDE);
    eq_int("budget rows", n, 1);
    eq_str("ellipsis", rows[0], "aaaa bb...");

    /* Exactly-full row: no spurious empty row after it. */
    n = wrap_text("0123456789", rows[0], 8, 10, BODY_STRIDE);
    eq_int("exact fit", n, 1);
    eq_str("exact fit text", rows[0], "0123456789");
}

static void ingest_str(const char *s)
{
    body_ingest((const unsigned char *) s, (unsigned int) strlen(s));
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

int main(void)
{
    test_sanitize();
    test_wrap();
    test_body();

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures != 0;
}
