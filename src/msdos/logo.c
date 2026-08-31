/*
 * The Gmail mark.
 *
 * The M as solid-block cells, one stroke per colour the way the real mark is
 * drawn: blue left leg, red and yellow diagonals, green right leg. A CP437
 * full block covers the whole cell with the foreground colour, so on a
 * colour adapter each stroke is simply a cell of its colour sitting on the
 * desktop background. Black-and-white and MDA get every stroke in intensity
 * white -- the Apple's one-bit rendering, arrived at from the other
 * direction.
 *
 * Blank cells are not painted at all. The header bar and the flat screens
 * have already put their own background where the logo lands, and skipping
 * the blanks is what lets the small mark sit across the app bar (row 0) and
 * the desktop (rows 1-2) without carrying either's colour around.
 *
 * Two sizes, matching every other backend's two: the small one in the inbox
 * header, the large one on the splash, busy and error screens.
 */

#include "../gmail.h"
#include "platform.h"

#define STROKE_GLYPH    0xDB    /* full block */

/*
 * 'B'lue, 'R'ed, 'Y'ellow, 'G'reen strokes; space is not painted. The
 * diagonals step two columns per row and meet two-thirds down, which is as
 * close to the mark's proportions as 8x8 cells get.
 */
static const char * const small_rows[LOGO_SMALL_ROWS] = {
    "BR  YG",
    "B RY G",
    "B    G",
};

static const char * const large_rows[LOGO_LARGE_ROWS] = {
    "BBRR        YYGG",
    "BB  RR    YY  GG",
    "BB    RRYY    GG",
    "BB            GG",
    "BB            GG",
    "BB            GG",
};

static unsigned char stroke_attr(char c)
{
    unsigned char bg;

    if (!scr_color)
        return scr_attr_byte(A_EMPH);

    /* The desktop's background nibble, minus the blink bit, under the
       stroke's own foreground. The block glyph hides the background anyway;
       carrying it keeps the cell honest if the glyph ever changes. */
    bg = (unsigned char) (scr_attr_byte(A_TEXT) & 0x70);

    switch (c) {
    case 'B':   return (unsigned char) (bg | 0x09);
    case 'R':   return (unsigned char) (bg | 0x0C);
    case 'Y':   return (unsigned char) (bg | 0x0E);
    case 'G':   return (unsigned char) (bg | 0x0A);
    }

    return scr_attr_byte(A_TEXT);
}

static void draw(const char * const *rows, unsigned char nrows,
                 unsigned char row, unsigned char col)
{
    unsigned char r, x;
    const char *s;

    for (r = 0; r < nrows; r++) {
        s = rows[r];
        for (x = 0; s[x]; x++) {
            if (s[x] == ' ')
                continue;
            scr_cell((unsigned char) (row + r), (unsigned char) (col + x),
                     STROKE_GLYPH, stroke_attr(s[x]));
        }
    }
}

void logo_small(unsigned char row, unsigned char col)
{
    draw(small_rows, LOGO_SMALL_ROWS, row, col);
}

void logo_large(unsigned char row, unsigned char col)
{
    draw(large_rows, LOGO_LARGE_ROWS, row, col);
}
