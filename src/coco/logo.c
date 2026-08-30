/*
 * The Gmail mark, in semigraphics.
 *
 * This is the only backend that draws the thing itself. The Atari spends four
 * players and a display list interrupt to get the four brand colours on
 * screen; the Apple, at one bit per pixel, can only make the envelope an
 * inverse block and leave the strokes as holes in it. Here the mark is a table
 * of bytes copied into screen RAM, and the strokes are the real colours on a
 * real white envelope.
 *
 * Two constraints shape it, and both are the VDG's.
 *
 * All four quadrants of a cell share one colour, so every boundary between two
 * brand colours falls on a cell edge: the strokes are a whole cell wide -- 8
 * pixels -- and step in whole cells. A partial quadrant mask would not soften
 * that, it would make it worse, because the unlit quadrants of a red cell are
 * *black*, not envelope. So every cell here is Q_ALL.
 *
 * And a solid green cell is invisible against the green text background, which
 * matters because green is one of the four. Wherever the mark would otherwise
 * meet the background it gets black first: a gutter cell to the right of the
 * small one, a full frame around the large one. The frame earns its keep twice
 * -- it also gives the mark an edge on the flat screens, where it is the only
 * thing on the top half of the display.
 *
 * The colour assignment is the one intv/gfx.bas established and src/atari/pmg.c
 * kept: blue pillar, red diagonal, yellow diagonal, green pillar.
 *
 * The tables were drawn from the pictures below and re-rendered from the bytes
 * to check them, which is worth doing again if you edit one: quadrant bit order
 * is TL TR BL BR from bit 3 down, and getting it wrong produces something that
 * still looks deliberate.
 */

#include <string.h>

#include "../gmail.h"
#include "platform.h"

/*
 *   BBRRWWWWYYGG      Six cells on two rows -- 48 x 24 pixels -- which costs
 *   BBWWRRYYWWGG      the header no rows, because the header is two rows
 *                     whatever goes in it.
 *
 * The V meets between cols 2 and 3, the centre line of six. At this size the
 * envelope is two cells and the strokes are four, which is as small as the
 * mark goes and still reads as an M rather than as a smear.
 */
static const unsigned char mark_small[LOGO_SMALL_ROWS][LOGO_SMALL_COLS] = {
    { GM_BLUE, GM_RED,   GM_PAPER, GM_PAPER, GM_YELLOW, GM_GREEN },
    { GM_BLUE, GM_PAPER, GM_RED,   GM_YELLOW, GM_PAPER, GM_GREEN }
};

/*
 *   BBRRWWWWWWWWWWWWYYGG      Ten cells on five rows -- 80 x 60 pixels, the
 *   BBRRWWWWWWWWWWWWYYGG      4:3 field the real mark sits in.
 *   BBWWRRWWWWWWWWYYWWGG
 *   BBWWRRWWWWWWWWYYWWGG      The V descends three cells over three rows and
 *   BBWWWWRRWWWWYYWWWWGG      meets on the centre line between cols 4 and 5,
 *   BBWWWWRRWWWWYYWWWWGG      which is the same 2:3 run-to-drop as the flap
 *   BBWWWWWWRRYYWWWWWWGG      of the real mark. The bottom row is envelope:
 *   BBWWWWWWRRYYWWWWWWGG      the fold stops short of the base, as it does
 *   BBWWWWWWWWWWWWWWWWGG      there too.
 *   BBWWWWWWWWWWWWWWWWGG
 *
 * (Drawn at quadrant resolution, so each cell above is two characters wide and
 * two rows tall -- which is what tools/coco-decode.py's quadrant pane prints,
 * and therefore what a capture can be compared against directly.)
 */
static const unsigned char mark_large[LOGO_LARGE_ROWS][LOGO_LARGE_COLS] = {
    { GM_BLUE, GM_RED,   GM_PAPER, GM_PAPER, GM_PAPER,
      GM_PAPER, GM_PAPER, GM_PAPER, GM_YELLOW, GM_GREEN },
    { GM_BLUE, GM_PAPER, GM_RED,   GM_PAPER, GM_PAPER,
      GM_PAPER, GM_PAPER, GM_YELLOW, GM_PAPER, GM_GREEN },
    { GM_BLUE, GM_PAPER, GM_PAPER, GM_RED,   GM_PAPER,
      GM_PAPER, GM_YELLOW, GM_PAPER, GM_PAPER, GM_GREEN },
    { GM_BLUE, GM_PAPER, GM_PAPER, GM_PAPER, GM_RED,
      GM_YELLOW, GM_PAPER, GM_PAPER, GM_PAPER, GM_GREEN },
    { GM_BLUE, GM_PAPER, GM_PAPER, GM_PAPER, GM_PAPER,
      GM_PAPER, GM_PAPER, GM_PAPER, GM_PAPER, GM_GREEN }
};

/*
 * The header mark, plus the black gutter cell that stands between its green
 * pillar and the text background.
 *
 * The pillar's *bottom* edge still meets the first list row, and there is no
 * row to spare for a rule under a two-row header. Three sides bounded -- the
 * screen border above and to the left, the gutter to the right -- is enough
 * for the mark to read.
 */
void logo_small(unsigned char row, unsigned char col)
{
    unsigned char i;

    for (i = 0; i < LOGO_SMALL_ROWS; i++) {
        memcpy(SCR_RAM + (unsigned int) (row + i) * SCR_COLS + col,
               mark_small[i], LOGO_SMALL_COLS);
        scr_cell((unsigned char) (row + i),
                 (unsigned char) (col + LOGO_SMALL_COLS), SG_BLACK);
    }
}

/*
 * The splash, busy and error mark, inside a one-cell black frame.
 *
 * row and col are the top-left of the *mark*, so both must be at least 1 --
 * the frame is drawn in the ring of cells around them.
 */
void logo_large(unsigned char row, unsigned char col)
{
    unsigned char top  = (unsigned char) (row - 1);
    unsigned char left = (unsigned char) (col - 1);
    unsigned char i;

    scr_fill(top, left, SG_BLACK, LOGO_LARGE_FRAME_COLS);
    scr_fill((unsigned char) (row + LOGO_LARGE_ROWS), left, SG_BLACK,
             LOGO_LARGE_FRAME_COLS);

    for (i = 0; i < LOGO_LARGE_ROWS; i++) {
        scr_cell((unsigned char) (row + i), left, SG_BLACK);
        memcpy(SCR_RAM + (unsigned int) (row + i) * SCR_COLS + col,
               mark_large[i], LOGO_LARGE_COLS);
        scr_cell((unsigned char) (row + i),
                 (unsigned char) (col + LOGO_LARGE_COLS), SG_BLACK);
    }
}
