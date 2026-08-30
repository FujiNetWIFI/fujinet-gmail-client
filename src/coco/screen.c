/*
 * Text and semigraphics straight into the 32x16 page at $0400.
 *
 * There is no conio here to fight with: printf() and putchar() go out through
 * Color BASIC's console hook at $A002, which scrolls the screen when it runs
 * off row 15 and would wreck a full-width footer -- and which costs 1.1K to
 * link. Nothing in this backend uses either.
 *
 * scr_field() is the primitive everything else is built on. The raw pair,
 * scr_cell() and scr_fill(), is what SG4 goes through: the mark, its frame,
 * the header gutter and the unread column are all raw bytes, and none of them
 * may be routed through sc().
 */

#include <string.h>
#include <coco.h>

#include "../gmail.h"
#include "platform.h"

/*
 * ASCII to 6847 screen code.
 *
 * The glyph set is 64 entries -- '@' A-Z [ \ ] ^ _ then space through '?' --
 * indexed by (byte & $3F), with bit 6 as the INV pin. Bit 6 *set* is normal
 * video on this machine, which is the opposite of what the name suggests.
 *
 * So after folding case, ORing $40 is the whole mapping: $20-$3F becomes
 * $60-$7F and $40-$5F is already there. Everything reaching here has been
 * through copy_san() or body_ingest(), so it is $20-$7E, and the two codes
 * above '_' have no glyph at all.
 *
 * Two of the sixty-four are worth knowing about because the footers use them:
 * $5E ('^') is the ROM's up arrow and $5F ('_') its left arrow. There is no
 * down arrow and no tilde, which is why the reader's scroll hint reads "^V".
 */
static unsigned char sc(unsigned char c)
{
    if (c >= 'a' && c <= 'z')
        c = (unsigned char) (c - 0x20);         /* no lowercase in this ROM */

    if (c < 0x20 || c > 0x5F)
        c = '?';

    return (unsigned char) (c | 0x40);
}

/* ------------------------------------------------------------------ */
/* Clearing                                                            */
/* ------------------------------------------------------------------ */

void scr_clear(void)
{
    memset(SCR_RAM, SCR_BLANK, (unsigned int) SCR_COLS * SCR_ROWS);
}

void scr_row_clear(unsigned char row)
{
    memset(SCR_RAM + (unsigned int) row * SCR_COLS, SCR_BLANK, SCR_COLS);
}

void scr_rows_clear(unsigned char first, unsigned char last)
{
    memset(SCR_RAM + (unsigned int) first * SCR_COLS, SCR_BLANK,
           (unsigned int) (last - first + 1) * SCR_COLS);
}

/* ------------------------------------------------------------------ */
/* Text                                                                */
/* ------------------------------------------------------------------ */

void scr_field(unsigned char row, unsigned char col, const char *s,
               unsigned char width, unsigned char inv)
{
    unsigned char *p = SCR_RAM + (unsigned int) row * SCR_COLS + col;
    unsigned char  v = inv ? 0x40 : 0x00;
    unsigned char  n = 0;

    while (n < width && *s) {
        *p++ = (unsigned char) (sc((unsigned char) *s++) ^ v);
        n++;
    }
    while (n < width) {
        *p++ = (unsigned char) (SCR_BLANK ^ v);
        n++;
    }
}

void scr_text(unsigned char row, unsigned char col, const char *s,
              unsigned char inv)
{
    unsigned char len = (unsigned char) strlen(s);

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

/* ------------------------------------------------------------------ */
/* Raw bytes                                                           */
/* ------------------------------------------------------------------ */

void scr_cell(unsigned char row, unsigned char col, unsigned char v)
{
    SCR_RAM[(unsigned int) row * SCR_COLS + col] = v;
}

void scr_fill(unsigned char row, unsigned char col, unsigned char v,
              unsigned char width)
{
    memset(SCR_RAM + (unsigned int) row * SCR_COLS + col, v, width);
}

/* ------------------------------------------------------------------ */
/* Platform lifecycle                                                  */
/* ------------------------------------------------------------------ */

void plat_init(void)
{
    /* A CoCo 3 may have come up in a 40- or 80-column GIME text mode, which
       has attribute colour but no VDG semigraphics at all. Ask for 32 and we
       are back on the page this backend knows how to draw. */
    width(32);
    scr_clear();
}

/*
 * Quitting cold-starts the machine rather than returning.
 *
 * There is nothing to return to: this program is linked at $1000 and loading
 * it left BASIC only the one line that ran LOADM. coldStart() is the only
 * honest exit.
 */
void plat_shutdown(void)
{
    scr_clear();
    coldStart();
}

/*
 * Nothing to suppress. On the Atari these switch off the display list
 * interrupts, which would otherwise steal cycles from a timing-critical SIO
 * transfer; this backend runs no interrupts of its own, and fujinet-lib masks
 * them itself for the length of a DriveWire transfer.
 *
 * They live here rather than in a timer.c because this client has no wall
 * clock, so there is no timer.c for them to live in.
 */
void plat_net_begin(void)
{
}

void plat_net_end(void)
{
}
