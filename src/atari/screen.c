/*
 * Text output straight into the OS screen RAM.
 *
 * cc65's Atari conio (libsrc/atari/cputc.s, clrscr.s) also writes directly
 * through SAVMSC rather than going out through CIO, so nothing here fights
 * with it -- but cputc() wraps at column 40 and scrolls the whole screen when
 * it runs off row 23, which would wreck a full-width footer. Writing screen
 * codes ourselves avoids that entirely and is faster besides.
 */

#include <string.h>
#include <conio.h>

#include "../gmail.h"
#include "platform.h"

static unsigned char *scr;

void scr_sync(void)
{
    scr = OS.savmsc;
}

/*
 * ASCII to Atari screen code. Same mapping cputc() performs:
 *   $00-$1F -> +$40      $20-$5F -> -$20      $60-$7F -> unchanged
 * Everything reaching here has been through copy_san(), so it is $20-$7E.
 */
static unsigned char sc(unsigned char c)
{
    if (c < 0x20)
        return (unsigned char) (c + 0x40);
    if (c < 0x60)
        return (unsigned char) (c - 0x20);
    return c;
}

void scr_clear(void)
{
    memset(scr, 0, (unsigned int) SCR_COLS * SCR_ROWS);
}

void scr_row_clear(unsigned char row)
{
    memset(scr + (unsigned int) row * SCR_COLS, 0, SCR_COLS);
}

void scr_rows_clear(unsigned char first, unsigned char last)
{
    memset(scr + (unsigned int) first * SCR_COLS, 0,
           (unsigned int) (last - first + 1) * SCR_COLS);
}

void scr_field(unsigned char row, unsigned char col, const char *s,
               unsigned char width, unsigned char inv)
{
    unsigned char *p = scr + (unsigned int) row * SCR_COLS + col;
    unsigned char  v = inv ? 0x80 : 0x00;
    unsigned char  n = 0;

    while (n < width && *s) {
        *p++ = sc((unsigned char) *s++) | v;
        n++;
    }
    while (n < width) {
        *p++ = v;                       /* screen code 0 is a space */
        n++;
    }
}

void scr_text(unsigned char row, unsigned char col, const char *s,
              unsigned char inv)
{
    unsigned char len = (unsigned char) strlen(s);

    if (col + len > SCR_COLS)
        len = SCR_COLS - col;
    scr_field(row, col, s, len, inv);
}

/* Right-align s so that its last character lands on column rcol. */
void scr_right(unsigned char row, unsigned char rcol, const char *s,
               unsigned char inv)
{
    unsigned char len = (unsigned char) strlen(s);

    if (len > rcol + 1)
        len = rcol + 1;
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

void scr_row_inv(unsigned char row, unsigned char inv)
{
    unsigned char *p = scr + (unsigned int) row * SCR_COLS;
    unsigned char  i;

    if (inv)
        for (i = 0; i < SCR_COLS; i++)
            p[i] |= 0x80;
    else
        for (i = 0; i < SCR_COLS; i++)
            p[i] &= 0x7F;
}

/* ------------------------------------------------------------------ */
/* Platform lifecycle                                                  */
/* ------------------------------------------------------------------ */

void plat_init(void)
{
    clrscr();
    cursor(0);
    scr_sync();

    OS.color4 = C_BORDER;
    pmg_init();
    dli_vbi_install();
}

void plat_shutdown(void)
{
    pmg_hide();
    dli_shutdown();
    cursor(1);
    clrscr();
}
