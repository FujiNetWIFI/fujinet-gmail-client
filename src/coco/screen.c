/*
 * Text and semigraphics straight into the 32x16 page at $0400, or -- on the
 * CoCo 3 build -- character/attribute pairs into the GIME's 80x24 page.
 *
 * There is no conio here to fight with: printf() and putchar() go out through
 * Color BASIC's console hook at $A002, which scrolls the screen when it runs
 * off the last row and would wreck a full-width footer -- and which costs 1.1K
 * to link. Nothing in this backend uses either.
 *
 * scr_field() is the primitive everything else is built on. scr_text(),
 * scr_right() and scr_center() are written once against it and are the same
 * code on both machines; only the primitives below the line differ.
 */

#include <string.h>
#include <coco.h>

#include "../gmail.h"
#include "platform.h"

#ifdef COCO3

/* ------------------------------------------------------------------ */
/* CoCo 3 -- the GIME text page                                        */
/* ------------------------------------------------------------------ */

/*
 * The page is not in the CPU map. Block $36 has to be banked into the $C000
 * window, written, and put back, and an interrupt taken in between would run
 * the BASIC ROM's handler with a screen where its own ROM should be. So the
 * window is opened and closed around each painter rather than each byte: the
 * longest hold is a full clear, which is 3,840 bytes, and this client has no
 * interrupt work of its own that a few missed 60Hz ticks would spoil.
 */

static unsigned char saved_bank;

static void win_open(void)
{
    asm { orcc #$50 }
    saved_bank = *((unsigned char *) 0xFFA6);
    *((unsigned char *) 0xFFA6) = SCR_BLOCK;
}

static void win_close(void)
{
    *((unsigned char *) 0xFFA6) = saved_bank;
    asm { andcc #$AF }
}

/* Cell (row, col) inside the open window. Two bytes per cell: char then
   attribute. */
static unsigned char *cell_at(unsigned char row, unsigned char col)
{
    return SCR_WIN + ((unsigned int) row * SCR_COLS + col) * 2;
}

static void blank_run(unsigned char *p, unsigned int cells)
{
    while (cells--) {
        *p++ = SCR_BLANK;
        *p++ = A_TEXT;
    }
}

void scr_clear(void)
{
    win_open();
    blank_run(SCR_WIN, (unsigned int) SCR_COLS * SCR_ROWS);
    win_close();
}

void scr_row_clear(unsigned char row)
{
    win_open();
    blank_run(cell_at(row, 0), SCR_COLS);
    win_close();
}

void scr_rows_clear(unsigned char first, unsigned char last)
{
    win_open();
    blank_run(cell_at(first, 0),
              (unsigned int) (last - first + 1) * SCR_COLS);
    win_close();
}

/*
 * The character byte is the byte in the string. copy_san() and body_ingest()
 * have already clamped everything from the wire to $20-$7E, which is exactly
 * what this font draws, so unlike the VDG build there is no case fold and no
 * $3F mapping -- lowercase arrives and is shown.
 */
void scr_field(unsigned char row, unsigned char col, const char *s,
               unsigned char width, unsigned char inv)
{
    unsigned char *p;
    unsigned char  a = inv ? A_SEL : A_TEXT;
    unsigned char  n = 0;

    win_open();
    p = cell_at(row, col);

    while (n < width && *s) {
        *p++ = (unsigned char) *s++;
        *p++ = a;
        n++;
    }
    while (n < width) {
        *p++ = SCR_BLANK;
        *p++ = a;
        n++;
    }
    win_close();
}

/* One cell of solid color: a space on the given ground. This is what the
   mark and the unread chips are made of, in place of the VDG's SG4 bytes. */
void scr_cell(unsigned char row, unsigned char col, unsigned char v)
{
    unsigned char *p;

    win_open();
    p = cell_at(row, col);
    p[0] = SCR_BLANK;
    p[1] = v;
    win_close();
}

void scr_fill(unsigned char row, unsigned char col, unsigned char v,
              unsigned char width)
{
    unsigned char *p;

    win_open();
    p = cell_at(row, col);
    while (width--) {
        *p++ = SCR_BLANK;
        *p++ = v;
    }
    win_close();
}

void scr_cells(unsigned char row, unsigned char col,
               const unsigned char *v, unsigned char n)
{
    unsigned char *p;

    win_open();
    p = cell_at(row, col);
    while (n--) {
        *p++ = SCR_BLANK;
        *p++ = *v++;
    }
    win_close();
}

/*
 * Recolor cells without disturbing their text: the footer band, and the
 * brighter foreground that marks an unread row.
 *
 * The GIME has no bold bit -- the attribute byte is blink, underline, three
 * bits of foreground and three of background -- so emphasis here is a
 * different foreground entry, which is the same thing the MS-DOS backend does
 * with A_EMPH against A_TEXT.
 */
void scr_attr_run(unsigned char row, unsigned char col, unsigned char width,
                  unsigned char attr)
{
    unsigned char *p;

    win_open();
    p = cell_at(row, col) + 1;
    while (width--) {
        *p = attr;
        p += 2;
    }
    win_close();
}

/*
 * Palette. Slots 0-7 are the backgrounds the attribute byte's low field
 * indexes, 8-15 the foregrounds its high field does.
 *
 * paletteRGB() takes 0-3 per channel, which is the GIME's real depth -- the
 * register is six bits, two per gun -- so these are the brand colors at the
 * resolution the hardware has rather than approximations of them.
 */
static void set_palette(void)
{
    paletteRGB(PAL_PAGE,   0, 0, 1);    /* dark blue page               */
    paletteRGB(PAL_PAPER,  3, 3, 3);    /* white: bar and envelope      */
    paletteRGB(PAL_RED,    3, 1, 1);    /* Gmail red                    */
    paletteRGB(PAL_BLUE,   1, 2, 3);    /* Gmail blue                   */
    paletteRGB(PAL_YELLOW, 3, 3, 0);    /* Gmail yellow                 */
    paletteRGB(PAL_GREEN,  1, 2, 1);    /* Gmail green                  */
    paletteRGB(PAL_BLACK,  0, 0, 0);
    paletteRGB(PAL_EMPH,   2, 2, 3);

    paletteRGB(8  + FG_WHITE,  2, 2, 2);
    paletteRGB(8  + FG_BLACK,  0, 0, 0);
    paletteRGB(8  + FG_BRIGHT, 3, 3, 3);

    setBorderColor(0x00);
}

void plat_init(void)
{
    width(80);
    set_palette();
    scr_clear();
}

#else

/* ------------------------------------------------------------------ */
/* CoCo 1/2 -- the VDG page                                            */
/* ------------------------------------------------------------------ */

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

void scr_cell(unsigned char row, unsigned char col, unsigned char v)
{
    SCR_RAM[(unsigned int) row * SCR_COLS + col] = v;
}

void scr_fill(unsigned char row, unsigned char col, unsigned char v,
              unsigned char width)
{
    memset(SCR_RAM + (unsigned int) row * SCR_COLS + col, v, width);
}

void scr_cells(unsigned char row, unsigned char col,
               const unsigned char *v, unsigned char n)
{
    memcpy(SCR_RAM + (unsigned int) row * SCR_COLS + col, v, n);
}

void plat_init(void)
{
    /* A CoCo 3 may have come up in a 40- or 80-column GIME text mode, which
       has attribute color but no VDG semigraphics at all. Ask for 32 and we
       are back on the page this backend knows how to draw. */
    width(32);
    scr_clear();
}

#endif /* COCO3 */

/* ------------------------------------------------------------------ */
/* Text, on both machines                                              */
/* ------------------------------------------------------------------ */

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
/* Platform lifecycle                                                  */
/* ------------------------------------------------------------------ */

/*
 * Quitting cold-starts the machine rather than returning.
 *
 * There is nothing to return to: this program is linked above BASIC and
 * loading it left BASIC only the one line that ran LOADM. coldStart() is the
 * only honest exit.
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
