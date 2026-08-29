/*
 * The Gmail mark.
 *
 * The Atari draws it with four players -- a red diagonal, a yellow diagonal and
 * two pillars -- with the white envelope showing through as playfield, and
 * priority ordered so the diagonals cross over the pillars where they meet.
 * None of that survives here: no sprites, and no colour to put in them.
 *
 * What is left is the part that actually identifies the thing. The mark is a
 * white envelope with an M on it whose two diagonals are the flap, and on a
 * one-bit screen a white envelope is an inverse block and the M is the cells
 * left in normal video -- the strokes become the holes. The fold where the red
 * and yellow diagonals cross is the one thing that cannot survive, because it
 * was made of two colours.
 *
 * The shapes are pattern strings rather than coordinates so that the M is
 * legible in the source, which is the only place anyone will ever debug it.
 * ' ' is envelope and '#' is stroke.
 */

#include "../gmail.h"
#include "platform.h"

static const char *const shape_large[LOGO_LARGE_ROWS] = {
    "                ",
    " ##          ## ",
    " ## ##    ## ## ",
    " ##  ##  ##  ## ",
    " ##   ####   ## ",
    " ##          ## "
};

/*
 * Paint one pattern row as runs of like video, rather than a field per cell.
 * Six rows of sixteen would otherwise be ninety-six blits, each of them a pair
 * of bank switches, for a screen that is up while nothing else is happening.
 */
static void shape_row(unsigned char row, unsigned char col, const char *pat)
{
    unsigned char start = 0;
    unsigned char i = 0;

    for (;;) {
        /* Walk to the end of the current run. The NUL ends the last one. */
        while (pat[i] && pat[i] == pat[start])
            i++;

        scr_field(row, (unsigned char) (col + start), "",
                  (unsigned char) (i - start),
                  (unsigned char) (pat[start] == ' '));

        if (!pat[i])
            return;
        start = i;
    }
}

/*
 * Small: the header mark, six cells by three rows.
 *
 * Row 0 of the inbox is the inverse app bar, so the mark's top row is painted
 * in *normal* video -- a dark notch in the white bar, standing in for the red
 * flap. The two rows below it sit on ordinary background, where inverse is the
 * white envelope. At six columns the M is the letter M and not a drawn shape;
 * there is no room for a stroke to be two cells wide and still be an M.
 */
void logo_small(unsigned char row, unsigned char col)
{
    scr_field(row, col, "", LOGO_SMALL_COLS, 0);
    scr_field((unsigned char) (row + 1), col, "  MM", LOGO_SMALL_COLS, 1);
    scr_field((unsigned char) (row + 2), col, "", LOGO_SMALL_COLS, 1);
}

/* Large: the splash, busy and error screens, sixteen by six. */
void logo_large(unsigned char row, unsigned char col)
{
    unsigned char i;

    for (i = 0; i < LOGO_LARGE_ROWS; i++)
        shape_row((unsigned char) (row + i), col, shape_large[i]);
}
