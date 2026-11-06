/*
 * sprites.c -- the Gmail mark, the busy dots and the form's cursor.
 *
 * Everything on the screen that is in more than two colours is a sprite: the
 * text bitmap has a paper and an ink per quadrant, and the mark needs blue,
 * red, yellow and green at once. The patterns are assets/nes/sprites.txt,
 * copied into CHR bank 16 at start-up; tall (8 x 16) sprites, all from the
 * $1000 table, all below $1C00 -- R5's window there belongs to the body
 * store (bodystore.c).
 *
 * The OAM slots are fixed, one block to each use, so showing one thing never
 * disturbs another:
 *
 *    0-3    the header's mark: the M's two halves in front, the white
 *           square behind them
 *    4-11   the flat screens' mark, the same M doubled
 *   12-15   the busy dots (crt0.s bobs them while spin_on is set)
 *   16      the form's cursor
 *
 * Eight to a line is the hardware's limit; the most this program ever puts
 * on one line is four.
 */

#include <string.h>

#include "../gmail.h"
#include "platform.h"
#include "art.h"

#define OAM_MARK    0
#define OAM_BIG     4
#define OAM_SPIN    12
#define OAM_CARET   16
#define OAM_USED    17

/* Text row r's first line, as a sprite's y: the line above its top row. */
#define ROW_Y(r)    ((uint8_t) (8 * (r) + 8 - 1))

static void put(uint8_t slot, uint8_t y, uint8_t tile, uint8_t attr, uint8_t x)
{
    uint8_t *o = oam + (slot << 2);

    o[0] = y;
    o[1] = tile;
    o[2] = attr;
    o[3] = x;
}

static void hide(uint8_t first, uint8_t n)
{
    for (; n; --n, ++first)
        oam[first << 2] = 0xF8;
}

void spr_init(void)
{
    PPU_ADDR = 0x10;
    PPU_ADDR = 0x00;
    ppu_copy_far(BANK_ART, spr_chr, SPR_CHR_SIZE);
    spr_clear();
}

void spr_clear(void)
{
    spin_on = 0;
    hide(0, OAM_USED);
}

/* Rows 0-1, columns 1-4 of the app bar. */
void spr_mark(void)
{
    hide(OAM_BIG, OAM_USED - OAM_BIG);
    spin_on = 0;
    put(OAM_MARK + 0, ROW_Y(0), SPR_MARK_0, 0, 4);
    put(OAM_MARK + 1, ROW_Y(0), SPR_MARK_1, 1, 12);
    put(OAM_MARK + 2, ROW_Y(0), SPR_PAPER_0, 2, 4);
    put(OAM_MARK + 3, ROW_Y(0), SPR_PAPER_1, 2, 12);
}

/* 32 x 32, centred, its top at text row `row`. */
void spr_mark_big(uint8_t row)
{
    static const uint8_t tile[8] = {
        SPR_MARK_BIG_0, SPR_MARK_BIG_1, SPR_MARK_BIG_2, SPR_MARK_BIG_3,
        SPR_MARK_BIG_4, SPR_MARK_BIG_5, SPR_MARK_BIG_6, SPR_MARK_BIG_7
    };
    uint8_t i, y = ROW_Y(row);

    hide(OAM_MARK, 4);
    for (i = 0; i < 8; ++i)
        put((uint8_t) (OAM_BIG + i), (uint8_t) (y + ((i & 4) << 2)),
            tile[i], (uint8_t) ((i & 2) ? 1 : 0), (uint8_t) (112 + ((i & 3) << 3)));
}

/* Blue, red, yellow, green -- Google's loading dots, under text row `row`. */
void spr_spin(uint8_t row)
{
    static const uint8_t tile[4] = { SPR_DOT1, SPR_DOT2, SPR_DOT3, SPR_DOT1 };
    static const uint8_t pal[4]  = { 0, 0, 1, 1 };
    uint8_t i;

    spin_y = ROW_Y(row);
    for (i = 0; i < 4; ++i)
        put((uint8_t) (OAM_SPIN + i), spin_y, tile[i], pal[i],
            (uint8_t) (104 + i * 12));
    spin_on = 1;
}

/* A bar down the gap ahead of text cell (row, col). The pattern's bar is in
   its lower tile, so the sprite starts a text row higher. */
void spr_caret(uint8_t row, uint8_t col)
{
    put(OAM_CARET, (uint8_t) (8 * row - 1), SPR_CARET, 3,
        (uint8_t) (col ? (col << 2) - 1 : 0));
}

void spr_caret_off(void)
{
    hide(OAM_CARET, 1);
}
