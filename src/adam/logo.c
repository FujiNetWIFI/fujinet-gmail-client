/*
 * The Gmail mark, in hardware sprites.
 *
 * Every other backend draws this mark out of whatever its screen already had:
 * the Atari steers four players with a display list interrupt, the CoCo lays
 * down semigraphics bytes, the Apple punches the strokes out of an inverse
 * block as holes. The Adam has thirty-two real sprites, so here the four
 * strokes are four of them and the envelope they sit on is character cells.
 *
 * The stroke assignment is intv/gfx.bas's, which src/atari/pmg.c and
 * src/coco/logo.c both kept: a blue pillar, a red diagonal descending to the
 * right, a yellow diagonal descending to the left to meet it, and a green
 * pillar. The fold where the two diagonals meet is the mark.
 *
 *      BBRR........YYGG      blue   #4285F4  left pillar
 *      BB.RR......YY.GG      red    #EA4335  descending right
 *      BB..RR....YY..GG      yellow #FBBC04  descending left
 *      BB...RR..YY...GG      green  #34A853  right pillar
 *      BB....RRYY....GG
 *
 * Four is not an arbitrary number. A TMS9918A displays at most four sprites on
 * any one scanline and silently drops the fifth, and a sprite occupies every
 * scanline its 16-pixel box covers whether or not it has a lit pixel there. All
 * four of these are stacked on one spot, so they cover the same sixteen lines
 * and sit exactly on the limit -- and because they do, *the mark can never be
 * wider than one sprite*. A fifth colour, or a second sprite alongside to widen
 * it, would cost whichever stroke the hardware got to last.
 *
 * That is why the envelope is not a sprite. It is painted into the attribute
 * plane underneath, which costs no sprite and no scanline, and gives the same
 * picture the CoCo draws: a white envelope with four coloured strokes on it.
 *
 * The two sizes are one pattern set. logo_small() runs the sprites unmagnified
 * at 16x16, two cells square, which is what lets the app bar carry the mark
 * without spending a row on it; logo_large() sets the VDP's global MAG bit and
 * gets 32x32 -- four cells square -- out of the same thirty-two bytes per
 * stroke. The bit is global, but the two are never on screen together: the flat
 * screens have no app bar and the app bar has no flat screen.
 *
 * A 16x16 sprite's 32 pattern bytes are four 8x8 quadrants in the order
 * top-left, bottom-left, top-right, bottom-right -- not the raster order the
 * picture above is drawn in. Re-render from the bytes if you edit a stroke.
 * No two strokes light the same pixel, so sprite priority never arises.
 */

#include <video/tms99x8.h>

#include "../gmail.h"
#include "platform.h"

/* Sprite generator handles. Each 16x16 sprite eats four 8x8 pattern slots,
   which vdp_set_sprite_16() accounts for itself. */
#define H_BLUE      0
#define H_RED       1
#define H_YELLOW    2
#define H_GREEN     3

/* Sprite attribute slots. The four are always used together. */
#define ID_BLUE     0
#define ID_RED      1
#define ID_YELLOW   2
#define ID_GREEN    3

/*
 * The sprite attribute table, and the y value that ends it.
 *
 * A y of 208 tells the VDP to stop scanning the list there, and ending it
 * matters more than it looks. vdp_set_mode(2) clears VRAM, so slots 4 to 31 are
 * left reading y=0 -- and a sprite counts against the four-per-scanline budget
 * whether or not its colour is transparent. Unterminated, twenty-eight
 * invisible sprites sit across scanlines 1 to 16 and the mark loses whichever
 * of its four strokes the hardware gets to last.
 *
 * The address is z88dk's, from libsrc/classic/video/tms9918/__vdp_mode2.asm.
 * tools/adam-decode.py checks VDP reg5 against the same number and says so if
 * it ever moves.
 */
#define SPR_ATTR    0x1B00
#define SPR_END     208

/* vdp_set_sprite_mode() writes (reg1 & 0xFC) | mode, so bit 1 is SIZE and bit 0
   is MAG. The enum in <video/tms99x8.h> names 16x16 unmagnified and stops; the
   magnified form is the same register write with both bits set. */
#define SPR_16          sprite_large            /* 2: 16x16          */
#define SPR_16_MAG      ((enum sprite_mode) 3)  /* 3: 16x16, doubled */

static const unsigned char sp_blue[32] = {
    0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0,     /* TL */
    0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0,     /* BL */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* TR */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00      /* BR */
};

static const unsigned char sp_red[32] = {
    0x30, 0x30, 0x30, 0x30, 0x18, 0x18, 0x18, 0x0C,     /* TL */
    0x0C, 0x0C, 0x06, 0x06, 0x06, 0x03, 0x03, 0x03,     /* BL */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* TR */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00      /* BR */
};

static const unsigned char sp_yellow[32] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* TL */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* BL */
    0x0C, 0x0C, 0x0C, 0x0C, 0x18, 0x18, 0x18, 0x30,     /* TR */
    0x30, 0x30, 0x60, 0x60, 0x60, 0xC0, 0xC0, 0xC0      /* BR */
};

static const unsigned char sp_green[32] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* TL */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     /* BL */
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,     /* TR */
    0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03      /* BR */
};

/*
 * Once, from plat_init(), and only after smartkeys_set_mode() -- vdp_set_mode()
 * clears VRAM, so patterns written before it would go with everything else.
 */
void logo_init(void)
{
    vdp_set_sprite_16(H_BLUE,   (void *) sp_blue);
    vdp_set_sprite_16(H_RED,    (void *) sp_red);
    vdp_set_sprite_16(H_YELLOW, (void *) sp_yellow);
    vdp_set_sprite_16(H_GREEN,  (void *) sp_green);

    /* Four sprites and no more, for the whole run. */
    vdp_vpoke(SPR_ATTR + 4 * 4, SPR_END);

    logo_hide();
}

/*
 * Hiding is ending the list one slot earlier rather than moving four sprites
 * off the bottom: one write instead of sixteen, and it cannot be undone by
 * accident. logo_small() and logo_large() both write slot 0's real y, which
 * puts the list back.
 *
 * It has to be called explicitly wherever a screen changes, because sprites
 * live outside the character planes and scr_clear() does not touch them --
 * without it the large mark from ui_busy() sits over the inbox.
 */
void logo_hide(void)
{
    vdp_vpoke(SPR_ATTR, SPR_END);
}

/* The four strokes, stacked, at a pixel position. */
static void strokes(unsigned char x, unsigned char y)
{
    vdp_put_sprite_16(ID_BLUE,   x, y, H_BLUE,   G_BLUE);
    vdp_put_sprite_16(ID_RED,    x, y, H_RED,    G_RED);
    vdp_put_sprite_16(ID_YELLOW, x, y, H_YELLOW, G_YELLOW);
    vdp_put_sprite_16(ID_GREEN,  x, y, H_GREEN,  G_GREEN);
}

/*
 * Two cells by two, which is what lets the app bar keep the mark without
 * spending a row on it -- the bar is two rows whatever goes in it.
 *
 * The envelope has to be painted as well as the strokes: the bar is red and the
 * mark is a white envelope with the M on it, so without the white cells the
 * strokes would sit on the brand colour and the red one would vanish.
 */
void logo_small(unsigned char row, unsigned char col)
{
    unsigned char r;

    for (r = 0; r < LOGO_SMALL_ROWS; r++)
        scr_fill((unsigned char) (row + r), col, G_PAPER, LOGO_SMALL_COLS);

    vdp_set_sprite_mode(SPR_16);
    strokes((unsigned char) ((col << 3) + LOGO_SMALL_XPAD),
            (unsigned char) (row << 3));
}

/*
 * The flat screens' mark: the same four strokes doubled to four cells square,
 * on a six-by-five white envelope with a one-cell red frame around it.
 *
 * The frame is what makes it an envelope rather than a mark floating on the
 * page, and red is the one colour it can be -- it is the brand's, and it is
 * already on screen two rows up on every other screen. row and col are the
 * top-left of the frame, so the whole thing occupies ENV_FRAME_ROWS by
 * ENV_FRAME_COLS.
 */
void logo_large(unsigned char row, unsigned char col)
{
    unsigned char r;

    for (r = 0; r < ENV_FRAME_ROWS; r++)
        scr_fill((unsigned char) (row + r), col, GM_RED, ENV_FRAME_COLS);

    for (r = 0; r < ENV_ROWS; r++)
        scr_fill((unsigned char) (row + 1 + r), (unsigned char) (col + 1),
                 G_PAPER, ENV_COLS);

    /* Centred left to right in the envelope, and one white row left under it:
       the mark is four rows in a five-row field, which is where the fold of a
       real envelope's flap would put it. */
    vdp_set_sprite_mode(SPR_16_MAG);
    strokes((unsigned char) ((col + 2) << 3), (unsigned char) ((row + 1) << 3));
}
