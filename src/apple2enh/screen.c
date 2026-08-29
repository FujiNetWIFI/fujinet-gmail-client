/*
 * Text output straight into the 80-column text page.
 *
 * cc65's Apple II conio would work -- cputc() knows about the aux/main column
 * split -- but it wraps at the window edge and scrolls the whole screen when it
 * runs off the bottom, which would wreck a full-width footer, and it pays for a
 * bank switch on every single character. This composes a whole field into
 * screen codes and hands it to blit.s, which writes it in two runs.
 *
 * The screen-code mapping is the enhanced //e's alternate character set:
 *
 *      normal   c            ->  c | $80
 *      inverse  c in $40-$5F ->  c - $40        (uppercase and @[\]^_)
 *      inverse  c otherwise  ->  c              ($20-$3F and $60-$7F)
 *      MouseText glyph n     ->  $40 + n
 *
 * Everything arriving here from the wire has been through copy_san() or the
 * identical clamp in body.c, so it is $20-$7E; the $01-$1F range is therefore
 * free, and platform.h spends it on MouseText escapes.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <apple2.h>
#include <peekpoke.h>

#include "../gmail.h"
#include "platform.h"

/* Soft switches. */
#define SET80COL    0xC001      /* 80-column store: HISCR pages the text page */
#define SETALTCHAR  0xC00F      /* inverse lowercase and MouseText           */
#define TXTSET      0xC051
#define LORES       0xC056      /* limits 80STORE/HISCR to the text page     */

/* Shared with blit.s, which reads the run out of scr_buf with absolute,X --
   the one indexed mode left once (zp),y is spoken for by the destination. */
unsigned char scr_buf[SCR_COLS];
unsigned char scr_row;
unsigned char scr_col;
unsigned char scr_len;

void scr_blit(void);            /* blit.s */

static unsigned char mt_ok;     /* the machine has MouseText */
static signed char   oldmode = -1;

/*
 * Stand-ins for the MouseText glyphs.
 *
 * Two machines need these. An unenhanced //e has 80 columns but the old
 * character generator, where $40-$5F is a second set of inverse uppercase
 * rather than MouseText -- so the hint bar would read as random capitals. And
 * MouseText has no inverse form at all, so any glyph landing inside a selection
 * bar falls back here too, which is what keeps the bar solid.
 *
 * Indexed by the escape byte, so this is in the order of platform.h's MT_*.
 */
static const char mt_ascii[32] = {
    '?', '?', '?', '*',         /* 03 hourglass */
    '+', '?', '?', '?',         /* 04 check     */
    '<', '.', 'v', '^',         /* 08 left, 09 dots, 0A down, 0B up      */
    '-', '<', '#', '?',         /* 0C top rule, 0D return, 0E block      */
    '?', '?', '?', '-',         /* 13 rule      */
    '+', '>', ':', ':',         /* 14 corner, 15 right, 16/17 dither     */
    '[', ']', '|', '*',         /* 18/19 folder, 1A vrule, 1B diamond    */
    '=', '?', '?', '|'          /* 1C two rules, 1F vrule                */
};

static unsigned char sc(unsigned char c, unsigned char inv)
{
    if (c < 0x20) {
        /* A MouseText escape. The glyphs live where the inverse forms would
           be, so there is no inverse one to ask for -- an inverted field takes
           the stand-in instead. */
        if (mt_ok && !inv)
            return (unsigned char) (0x40 + c);
        c = (unsigned char) mt_ascii[c];
    }

    if (!inv)
        return (unsigned char) (c | 0x80);

    return (c >= 0x40 && c < 0x60) ? (unsigned char) (c - 0x40) : c;
}

/* ------------------------------------------------------------------ */
/* Fields                                                              */
/* ------------------------------------------------------------------ */

void scr_field(unsigned char row, unsigned char col, const char *s,
               unsigned char width, unsigned char inv)
{
    unsigned char n = 0;
    unsigned char pad;

    if (row >= SCR_ROWS || col >= SCR_COLS)
        return;
    if (col + width > SCR_COLS)
        width = (unsigned char) (SCR_COLS - col);

    while (n < width && *s)
        scr_buf[n++] = sc((unsigned char) *s++, inv);

    pad = sc(' ', inv);
    while (n < width)
        scr_buf[n++] = pad;

    scr_row = row;
    scr_col = col;
    scr_len = width;
    scr_blit();
}

void scr_text(unsigned char row, unsigned char col, const char *s,
              unsigned char inv)
{
    unsigned char len = (unsigned char) strlen(s);

    if (col >= SCR_COLS)
        return;
    if (col + len > SCR_COLS)
        len = (unsigned char) (SCR_COLS - col);
    scr_field(row, col, s, len, inv);
}

/* Right-align s so that its last character lands on column rcol. */
void scr_right(unsigned char row, unsigned char rcol, const char *s,
               unsigned char inv)
{
    unsigned char len = (unsigned char) strlen(s);

    if (len > rcol + 1)
        len = (unsigned char) (rcol + 1);
    scr_field(row, (unsigned char) (rcol + 1 - len), s, len, inv);
}

void scr_center(unsigned char row, const char *s, unsigned char inv)
{
    unsigned char len = (unsigned char) strlen(s);

    if (len >= SCR_COLS)
        scr_field(row, 0, s, SCR_COLS, inv);
    else
        scr_text(row, (unsigned char) ((SCR_COLS - len) / 2), s, inv);
}

/* A run of one repeated glyph -- rules and chrome bands. */
void scr_fill(unsigned char row, unsigned char col, const char *glyph,
              unsigned char width, unsigned char inv)
{
    unsigned char i;
    unsigned char c;

    if (row >= SCR_ROWS || col >= SCR_COLS)
        return;
    if (col + width > SCR_COLS)
        width = (unsigned char) (SCR_COLS - col);

    c = sc((unsigned char) *glyph, inv);
    for (i = 0; i < width; i++)
        scr_buf[i] = c;

    scr_row = row;
    scr_col = col;
    scr_len = width;
    scr_blit();
}

void scr_row_clear(unsigned char row)
{
    scr_field(row, 0, "", SCR_COLS, 0);
}

void scr_rows_clear(unsigned char first, unsigned char last)
{
    unsigned char r;

    for (r = first; r <= last; r++)
        scr_row_clear(r);
}

void scr_clear(void)
{
    scr_rows_clear(0, SCR_ROWS - 1);
}

/* ------------------------------------------------------------------ */
/* Platform lifecycle                                                  */
/* ------------------------------------------------------------------ */

void plat_init(void)
{
    oldmode = videomode(VIDEOMODE_80COL);
    if (oldmode < 0) {
        /* Still in 40 columns and nothing has been painted, so conio is the
           right way to say so -- and the only one that works. */
        clrscr();
        cputs("This needs an 80-column card.\r\n");
        exit(1);
    }

    /*
     * Set the switches the blitter depends on rather than inheriting them.
     * SET80COL is what makes HISCR page $0400-$07FF to aux instead of selecting
     * text page 2, LORES confines that to the text page, and SETALTCHAR is what
     * puts MouseText at $40-$5F. cc65's conio constructor does the last two,
     * but only if something drags conio in, and the painters here deliberately
     * do not.
     */
    POKE(TXTSET, 0);
    POKE(SET80COL, 0);
    POKE(SETALTCHAR, 0);
    (void) PEEK(LORES);

    /* MouseText arrived with the enhanced //e. Everything numbered above it --
       //c, //e card, IIgs -- has it; the plain //e ($30) does not. */
    mt_ok = (unsigned char) (get_ostype() > APPLE_IIE);

    cursor(0);
    scr_clear();
}

void plat_shutdown(void)
{
    scr_clear();
    if (oldmode >= 0)
        videomode((unsigned) oldmode);
    clrscr();
    cursor(1);
}

/*
 * The Atari suppresses display list interrupts around every device call,
 * because SIO runs with interrupts disabled and is timing critical. There is
 * nothing of ours running here to suppress: no interrupt handler, no frame
 * counter, no sound. SmartPort is a subroutine call into card firmware.
 */
void plat_net_begin(void)
{
}

void plat_net_end(void)
{
}
