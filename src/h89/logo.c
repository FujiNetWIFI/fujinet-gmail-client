/*
 * The Gmail mark.
 *
 * The Apple stencils it: an inverse block for the white envelope, with the
 * M's strokes left in normal video, which on its screen means the M's two
 * diagonals are staircases of whole cells. The H19 can draw them straight.
 * Its graphics set has a triangle filling the upper-left half of a cell and
 * one filling the upper-right, and reverse video turns each into its
 * opposite -- so all four half-cells a diagonal edge can leave are there,
 * and a stroke a cell and a half thick runs smoothly from the top of each
 * leg down to the point of the V.
 */

#include "../gmail.h"
#include "platform.h"

/*
 * Small: the header mark, four cells by three rows. Row 0 of a banded
 * screen is the reverse app bar, so the mark is a notch of normal video in
 * it -- an envelope in line characters, its flap the H19's two diagonals.
 * Four cells is too narrow for the stencil below.
 */
void logo_small(unsigned char row, unsigned char col)
{
    scr_field(row, col, HG_UL HG_HLINE HG_HLINE HG_UR, LOGO_SMALL_COLS, 0);
    scr_field((unsigned char) (row + 1), col,
              HG_VLINE HG_BACKSLASH HG_SLASH HG_VLINE, LOGO_SMALL_COLS, 0);
    scr_field((unsigned char) (row + 2), col, HG_LL HG_HLINE HG_HLINE HG_LR,
              LOGO_SMALL_COLS, 0);
}

/*
 * Large: the flat screens, sixteen by six, the Apple's stencil with the
 * staircases straightened. '.' is envelope (a reverse space), '#' stroke (a
 * plain one), and a-d are the half-cells along the diagonals' edges -- see
 * cell_of().
 */
static const char *const shape_large[LOGO_LARGE_ROWS] = {
    "................",
    ".##a#b....c#d##.",
    ".##.a#b..c#d.##.",
    ".##..a#bc#d..##.",
    ".##...a##d...##.",
    ".##..........##.",
};

/*
 * The stroke is black and the envelope white, and G_TRI_UL / G_TRI_UR light
 * the upper-left / upper-right half of a cell. So:
 *
 *   a  left edge of a '\' stroke: stroke upper right, envelope lower left
 *      -- G_TRI_UR reversed
 *   b  right edge of a '\' stroke: envelope upper right, stroke lower left
 *      -- G_TRI_UR as it is
 *   c  left edge of a '/' stroke: envelope upper left, stroke lower right
 *      -- G_TRI_UL as it is
 *   d  right edge of a '/' stroke: stroke upper left, envelope lower right
 *      -- G_TRI_UL reversed
 */
static unsigned char cell_of(char c)
{
    switch (c) {
    case '.':   return ' ' | REV;
    case 'a':   return G_TRI_UR | REV;
    case 'b':   return G_TRI_UR;
    case 'c':   return G_TRI_UL;
    case 'd':   return G_TRI_UL | REV;
    default:    return ' ';
    }
}

void logo_large(unsigned char row, unsigned char col)
{
    unsigned char r, i;
    const char   *s;

    for (r = 0; r < LOGO_LARGE_ROWS; r++) {
        s = shape_large[r];
        for (i = 0; s[i]; i++)
            scr_cell((unsigned char) (row + r), (unsigned char) (col + i),
                     cell_of(s[i]));
    }
}
