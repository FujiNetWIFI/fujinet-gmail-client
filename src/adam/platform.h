/*
 * Coleco Adam backend -- internal interface shared by the files in this
 * directory. The portable half of the program talks to us only through the
 * plat_* / ui_* declarations in ../gmail.h.
 *
 * The screen is the TMS9918A's GRAPHICS II page, which z88dk lays out as a
 * linear bitmap: the name table at $1800 is filled with 0..255 three times, so
 * every one of the 768 cells owns its own eight pattern bytes and its own
 * eight colour bytes. That is the property this whole backend turns on.
 *
 *   pattern  $0000 + (row << 8) + (col << 3)      8 bytes, one per scanline
 *   colour   $2000 + (row << 8) + (col << 3)      8 bytes, (fg << 4) | bg
 *
 * So foreground and background are settable per cell -- per scanline within a
 * cell, in fact -- out of fifteen inks. This is the first backend that does not
 * have to approximate Gmail's red: it puts a real red app bar across the top of
 * the screen and a real red chip in the unread column. The Atari has one
 * background and one text luminance per band, the CoCo has eight semigraphics
 * colours and no text colour at all, and the Apple has none.
 *
 * Four things about this screen bite, and all four are load-bearing:
 *
 *   - Rows 21-23 are not ours. smartkeys_clear() is vdp_vfill($1500, 0, 768)
 *     and smartkeys_attrs() writes at MODE2_ATTR + 5376, which are exactly
 *     those three rows. Nothing here ever paints below row 20, and scr_clear()
 *     stops there rather than calling clrscr(), which would wipe the legend.
 *
 *   - That band is also what pays for the extra content. Every other backend
 *     spends its bottom row on a hint bar -- "ENT:READ <>:PAGE R:REFR Q:QUIT"
 *     -- because it has nowhere else to say what the keys do. This machine has
 *     six labelled keys with their captions drawn on the screen, so the hint
 *     bar is not a row we save, it is a row we never had to spend. Twenty-one
 *     rows of 32 is five more than the CoCo has, and it is what lets this
 *     backend keep the portable IDX_MAX of 16 that no other 32-column build
 *     can afford.
 *
 *   - A cell's colour lives in a different plane from its glyph, so writing
 *     text does not set its colour and setting its colour does not disturb the
 *     text. Every scr_ call that writes glyphs repaints the attribute run
 *     afterwards, so a field's appearance never depends on what the console
 *     happened to have selected.
 *
 *   - There is no inverse video. A "selected" row is not a flipped glyph, it is
 *     the same glyph under a different attribute byte, which is why the whole
 *     interface below passes an attr rather than an `inv` flag the way the
 *     other three backends do.
 */

#ifndef ADAM_PLATFORM_H
#define ADAM_PLATFORM_H

#include <video/tms99x8.h>

#define SCR_COLS        32
#define SCR_ROWS        24

/*
 * Rows 0-20. The SmartKeys band owns the last three and smartkeyslib repaints
 * all of them every time a legend changes, so treating them as part of the
 * screen would mean fighting the library for them.
 */
#define SK_ROWS         3
#define OUR_ROWS        (SCR_ROWS - SK_ROWS)
#define OUR_BYTES       ((unsigned int) OUR_ROWS * 256)

#define PAT_BASE        0x0000
#define PAT_ADDR(r, c)  ((unsigned int) (((unsigned int) (r) << 8) + ((unsigned int) (c) << 3)))
#define ATT_ADDR(r, c)  ((unsigned int) (MODE2_ATTR + PAT_ADDR(r, c)))

/* ------------------------------------------------------------------ */
/* Palette                                                             */
/* ------------------------------------------------------------------ */

#define ATTR(fg, bg)    ((unsigned char) (((fg) << 4) | (bg)))

/*
 * Gmail is black on white under a red app bar, and the TMS9918A has a
 * serviceable version of all three.
 *
 * Medium red is #D56F5D against Gmail's #EA4335 -- dark red (#B2495B) is too
 * brown and light red (#FB8F88) is pink, so the middle of the three ramp
 * entries is both the closest hue and the only one white text reads against.
 * It is the app bar, the unread chip and the envelope frame, which is as much
 * of the screen as the brand should own.
 */
#define A_HEADER        ATTR(VDP_INK_WHITE, VDP_INK_MEDIUM_RED)
/* Secondary text on the app bar. Gray on medium red rather than a darker red:
   the ramp's neighbours are too close to the band to read as quieter. */
#define A_HDR_DIM       ATTR(VDP_INK_GRAY, VDP_INK_MEDIUM_RED)

#define A_BODY          ATTR(VDP_INK_BLACK, VDP_INK_WHITE)
#define A_DIM           ATTR(VDP_INK_GRAY, VDP_INK_WHITE)

/*
 * The rule row and the selection bar are the same black on gray, which is what
 * Gmail's own hover highlight looks like and leaves red to mean "unread" and
 * nothing else. They are never adjacent -- the rule is row 2 and the bar is
 * somewhere in the list -- so sharing an attribute costs no legibility.
 *
 * Not white on gray: the TMS9918A's gray is #CCCCCC, far closer to its white
 * than to its black, and white text on it is barely there.
 */
#define A_RULE          ATTR(VDP_INK_BLACK, VDP_INK_GRAY)
#define A_SEL           ATTR(VDP_INK_BLACK, VDP_INK_GRAY)

/* A solid block of `ink`: no lit pixels, and the ink is the cell background. */
#define A_BLOCK(ink)    ATTR(VDP_INK_BLACK, ink)

/*
 * Gmail's four brand colours, for the sprite mark, in the stroke assignment
 * intv/gfx.bas established and src/atari/pmg.c and src/coco/logo.c both kept:
 * a blue pillar, a red diagonal down to the right, a yellow diagonal down to
 * the left, and a green pillar.
 *
 * Three of the four differ from the calendar client's choices for the same hex,
 * because these sit on a white envelope rather than over a page:
 *
 *   blue   #4285F4 -> light blue  #5C80FC   (nearer than dark blue #5455ED)
 *   red    #EA4335 -> medium red  #D56F5D
 *   yellow #FBBC04 -> dark yellow #D5C154   (light yellow washes out on white)
 *   green  #34A853 -> dark green  #21B03B   (nearer than medium green #21C842)
 */
#define G_BLUE          VDP_INK_LIGHT_BLUE
#define G_RED           VDP_INK_MEDIUM_RED
#define G_YELLOW        VDP_INK_DARK_YELLOW
#define G_GREEN         VDP_INK_DARK_GREEN

/* The envelope the mark sits on, and the chip in the unread column. */
#define G_PAPER         VDP_INK_WHITE
#define GM_RED          VDP_INK_MEDIUM_RED

/* ------------------------------------------------------------------ */
/* Band geometry                                                       */
/* ------------------------------------------------------------------ */

/*
 * Inbox: rows 0-1 app bar, 2 rule, 3-18 list, 19-20 panel.
 *
 * The app bar is two rows because the mark is two rows. The rule carries the
 * selected message's date and the page indicator -- the two things that are
 * about the list rather than in it. The panel is two rows because a 12-column
 * From and a 17-column Subject truncate hard enough that the selection has to
 * be spelled out somewhere, which is the same trade the CoCo makes.
 */
#define HDR_ROWS        2
#define RULE_ROW        2
#define LIST_TOP        3                       /* rows 3..3+IDX_MAX-1 = 3..18 */
#define PANEL_ROW       19                      /* rows 19-20 */
#define LAST_ROW        (OUR_ROWS - 1)          /* 20 */

/*
 * Reader: row 0 sender and date, 1-2 subject, 3 rule, 4-20 body.
 *
 * The rule row is the separator every mail reader draws between the envelope
 * and the letter, and it is where the page indicator goes now that there is no
 * footer to put it in. It costs the body one row -- MSG_ROWS is 17 rather than
 * the portable 18 -- which is the whole price of the layout.
 */
#define MSG_HDR_ROW     0
#define MSG_SUBJ_ROW    1                       /* rows 1-2 */
#define MSG_RULE_ROW    3
#define MSG_TOP         4                       /* rows 4..4+MSG_ROWS-1 = 4..20 */

/* Header columns. The mark occupies cols 0-1 on both app bar rows and the text
   starts clear of it. */
#define LOGO_ROW        0
#define LOGO_COL        0
#define HDR_TEXT_COL    3
#define RIGHT_COL       (SCR_COLS - 1)

/* ------------------------------------------------------------------ */
/* screen.c -- the blitter                                             */
/* ------------------------------------------------------------------ */

void scr_clear(void);
void scr_row_clear(unsigned char row);
void scr_rows_clear(unsigned char first, unsigned char last);

/* Write s into [col, col+width), space padded and truncated to fit, then paint
   the attribute run. */
void scr_field(unsigned char row, unsigned char col, const char *s,
               unsigned char width, unsigned char attr);
void scr_text(unsigned char row, unsigned char col, const char *s,
              unsigned char attr);
void scr_right(unsigned char row, unsigned char rcol, const char *s,
               unsigned char attr);
void scr_center(unsigned char row, const char *s, unsigned char attr);

/* Colour without glyphs: scr_cell paints one solid block, scr_fill a run.
   This is what draws the unread chips and the envelope behind the mark. */
void scr_cell(unsigned char row, unsigned char col, unsigned char ink);
void scr_fill(unsigned char row, unsigned char col, unsigned char ink,
              unsigned char width);

/* Repaint an attribute run without touching the glyphs under it. */
void scr_attr(unsigned char row, unsigned char col, unsigned char width,
              unsigned char attr);

/* ------------------------------------------------------------------ */
/* logo.c -- the Gmail mark, in hardware sprites                        */
/* ------------------------------------------------------------------ */

/*
 * Four 16x16 sprites stacked on one spot, one per stroke of the "M", each
 * carrying disjoint pixels of the same shape: a blue pillar, a red diagonal
 * descending right, a yellow diagonal descending left to meet it, and a green
 * pillar. That fold is the mark.
 *
 * Four is the hardware ceiling, not a design choice. A TMS9918A shows at most
 * four sprites on any one scanline and silently drops the fifth, and a sprite
 * occupies every line its 16-pixel box covers whether or not it has a lit pixel
 * there. Stacked sprites all cover the same scanlines, so *the mark can never
 * be wider than one sprite* -- which is why the white envelope around the
 * strokes is painted in character cells rather than drawn in a fifth sprite.
 * That is the same picture the CoCo draws in semigraphics bytes, and it costs
 * nothing.
 *
 * The two sizes are one pattern set. The small mark runs the sprites
 * unmagnified at 16x16 (two cells square); the large one sets the VDP's
 * global MAG bit and gets 32x32 (four cells square) out of the same bytes. The
 * bit is global, but the two marks are never on screen together -- the flat
 * screens have no app bar and the app bar has no flat screen.
 */
/* Three cells wide for a 16-pixel mark: the sprite is centred in them, which
   leaves four pixels of envelope either side. At two cells the strokes ran
   edge to edge and the blue and green pillars bled straight into the red app
   bar. HDR_TEXT_COL is 3, so the extra column was already spare. */
#define LOGO_SMALL_COLS 3
#define LOGO_SMALL_ROWS 2
#define LOGO_SMALL_XPAD 4
#define LOGO_LARGE_COLS 4
#define LOGO_LARGE_ROWS 4

/* The envelope painted around the large mark, and its red frame: a 6x5 white
   field with the 4x4 mark inside it, framed one cell all round. */
#define ENV_COLS        6
#define ENV_ROWS        5
#define ENV_FRAME_COLS  (ENV_COLS + 2)
#define ENV_FRAME_ROWS  (ENV_ROWS + 2)

void logo_init(void);                           /* patterns into VRAM, once */
void logo_small(unsigned char row, unsigned char col);
void logo_large(unsigned char row, unsigned char col);
void logo_hide(void);

/* ------------------------------------------------------------------ */
/* input.c -- SmartKeys                                                */
/* ------------------------------------------------------------------ */

/*
 * SmartKey I..VI arrive as 0x81..0x86 and mean whatever the screen currently on
 * display says they mean, so the legend and the key map are set together and
 * never separately: a legend that has drifted from its map is a key that lies
 * about what it does.
 */
struct sk_set {
    const char   *label[6];     /* NULL leaves the slot as yellow status */
    unsigned char key[6];
};

void sk_bind(const struct sk_set *s);           /* legend + map, together */

/*
 * The frame wait the key poll spins in lives in timer.c now, with the counter
 * the wall clock runs on -- plat_vsync(), plat_ticks() and plat_fps() are all
 * declared in gmail.h, because every backend owes them. It is still a HALT
 * rather than a counter read: the VDP raises an NMI once per frame and z88dk's
 * coleco crt installs a handler for it unconditionally (TAR__crt_enable_nmi),
 * so a HALT wakes once per frame with no interrupt of our own.
 */

#endif /* ADAM_PLATFORM_H */
