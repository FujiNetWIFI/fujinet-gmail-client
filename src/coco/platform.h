/*
 * Tandy Color Computer backend -- internal interface shared by the files in
 * this directory. The portable half of the program talks to us only through
 * the plat_* / ui_* declarations in ../gmail.h.
 *
 * Two machines are built from this directory: the CoCo 1/2, described below,
 * and the CoCo 3 under -DCOCO3, whose GIME text page has its own notes in the
 * COCO3 block further down. Only one of the two compiles into any binary.
 *
 * The 1/2's screen is the 6847's 32x16 alpha/semigraphics page at $0400, and
 * the whole design turns on one property of it: the VDG decides per byte
 * whether a cell is a character or a 2x2 block of colour, with no mode switch
 * and no second display list.
 *
 *   $00-$3F   glyph (byte & $3F), INV asserted -- green on dark green
 *   $40-$7F   the same glyph, normal video -- dark green on green
 *   $80-$FF   SG4: bit 7 set, bits 6-4 colour, bits 3-0 quadrant mask
 *
 * So text and colour intermix freely, which is how this backend draws the Gmail
 * mark as the mark -- a white envelope with four coloured strokes -- as ordinary
 * bytes in screen RAM. The Atari needs four players and an interrupt for the
 * same thing; the Apple, with one bit per pixel, can only punch the strokes out
 * of an inverse block as holes. The CoCo 3 build gets there a third way, one
 * cell per colour off the attribute plane.
 *
 * Three things about that byte map bite, and all three are load-bearing here:
 *
 *   - The blank byte is $60 (space $20 with bit 6 set), not $00. memset(scr, 0)
 *     paints a screen of inverse '@'.
 *
 *   - Inverse video is XOR $40 -- but only on a character. XOR $40 on an SG4
 *     byte changes its *colour* ($8F green becomes $CF buff), so every routine
 *     that flips a run leaves bytes >= $80 alone. That is why column 0, which
 *     carries the unread chip, sits outside the selection bar. The Atari keeps
 *     its column 0 out because an inverse space is COLPF1 and covers the
 *     player, and the Apple because MouseText has no inverse form -- three
 *     machines, three unrelated reasons, one rule.
 *
 *   - An unlit SG4 quadrant is black, and the text background is green, so a
 *     solid green cell on a text row is invisible. Green is one of the Gmail
 *     four, so the mark needs black beside it wherever it would otherwise meet
 *     the background: see logo.c.
 *
 * A stock 6847 has no lowercase at all -- 64 glyphs, uppercase only -- so sc()
 * folds case and every string literal in this directory is written uppercase.
 */

#ifndef COCO_PLATFORM_H
#define COCO_PLATFORM_H

#include <coco.h>

#ifdef COCO3

/*
 * CoCo 3: the GIME's 80x24 text page, which is a different machine from the
 * VDG page above in every way that matters here.
 *
 * A cell is two bytes -- character then attribute -- and the character is
 * plain ASCII, so sc()'s $3F fold has no counterpart on this build. The
 * attribute is (fg << 3) | bg with bit 6 underline and bit 7 blink; fg indexes
 * palette slots 8-15 and bg slots 0-7, which is why the two fields are
 * separate constants below rather than one color each.
 *
 * The page is not in the CPU map. It lives in MMU block $36, which has to be
 * banked into the $C000 window to be written and unbanked afterwards, with
 * interrupts masked across the pair -- see screen.c. That is the whole reason
 * this backend cannot simply keep pointing memcpy() at SCR_RAM.
 *
 * There are no semigraphics at all, so the mark is drawn as background color
 * on space characters: one cell per color rather than the VDG's four
 * quadrants. At 80x24 that is finer than the 6847 page manages, not coarser.
 */

#define SCR_COLS    80
#define SCR_ROWS    24

/* The $C000 window the text page is banked into, and the block that holds it. */
#define SCR_WIN     ((unsigned char *) 0xC000)
#define SCR_BLOCK   0x36

/* Plain ASCII on this page. */
#define SCR_BLANK   0x20

/* ------------------------------------------------------------------ */
/* Palette                                                             */
/* ------------------------------------------------------------------ */

/*
 * Background slots 0-7. The three chrome colors are the MS-DOS backend's --
 * blue page, light selection bar, red footer -- because that is the one other
 * backend with real attribute color and the client should read the same on
 * both. The other four are Gmail's own, for the mark.
 *
 * PAL_PAPER doubles as the selection bar's background and the envelope, and
 * PAL_RED as the footer bar and the mark's red stroke. Eight slots do not
 * stretch to separate ones, and neither pair ever meets on screen.
 */
#define PAL_PAGE        0       /* page background, blue                */
#define PAL_PAPER       1       /* selection bar and the envelope       */
#define PAL_RED         2       /* footer bar and the mark's red        */
#define PAL_BLUE        3       /* the mark's blue pillar               */
#define PAL_YELLOW      4
#define PAL_GREEN       5
#define PAL_BLACK       6       /* frame, gutter, and the read chip     */
#define PAL_EMPH        7       /* unread rows                          */

/* Foreground indices. These are 0-7 in the attribute byte and land on palette
   slots 8-15, which is why they are not the same numbers as the backgrounds. */
#define FG_WHITE        0
#define FG_BLACK        1
#define FG_BRIGHT       2

#define ATTR(f, b)      ((unsigned char) (((f) << 3) | (b)))
#define ATTR_UNDER      0x40

/* ------------------------------------------------------------------ */
/* Attribute roles                                                     */
/* ------------------------------------------------------------------ */

/*
 * Painters name a role, not a color, exactly as the MS-DOS backend does. A
 * color picker would only have to rewrite these six and repaint; nothing that
 * draws needs to know a palette slot.
 */
#define A_TEXT      ATTR(FG_WHITE,  PAL_PAGE)
#define A_EMPH      ATTR(FG_BRIGHT, PAL_PAGE)
#define A_SEL       ATTR(FG_BLACK,  PAL_PAPER)
#define A_BAR       ATTR(FG_BLACK,  PAL_PAPER)
#define A_FOOT      ATTR(FG_BRIGHT, PAL_RED)
#define A_UNDER     (ATTR(FG_BRIGHT, PAL_PAGE) | ATTR_UNDER)

/* What the mark is made of: a space on a colored ground. */
#define GM_BLUE     ATTR(FG_WHITE, PAL_BLUE)
#define GM_RED      ATTR(FG_WHITE, PAL_RED)
#define GM_YELLOW   ATTR(FG_WHITE, PAL_YELLOW)
#define GM_GREEN    ATTR(FG_WHITE, PAL_GREEN)
#define GM_PAPER    ATTR(FG_WHITE, PAL_PAPER)
#define SG_BLACK    ATTR(FG_WHITE, PAL_BLACK)

#else

#define SCR_COLS    32
#define SCR_ROWS    16
#define SCR_RAM     ((unsigned char *) 0x0400)

/* Space, normal video. Not zero -- see the header comment. */
#define SCR_BLANK   0x60

#endif /* COCO3 */

/* ------------------------------------------------------------------ */
/* Semigraphics-4                                                      */
/* ------------------------------------------------------------------ */

#ifndef COCO3

#define SG_GREEN        0
#define SG_YELLOW       1
#define SG_BLUE         2
#define SG_RED          3
#define SG_BUFF         4
#define SG_CYAN         5
#define SG_MAGENTA      6
#define SG_ORANGE       7

/* Quadrant mask bits, in the order the VDG reads them. */
#define Q_TL            0x08
#define Q_TR            0x04
#define Q_BL            0x02
#define Q_BR            0x01
#define Q_TOP           (Q_TL | Q_TR)
#define Q_BOT           (Q_BL | Q_BR)
#define Q_LEFT          (Q_TL | Q_BL)
#define Q_RIGHT         (Q_TR | Q_BR)
#define Q_ALL           0x0F

#define SG4(c, m)       ((unsigned char) (0x80 | ((c) << 4) | (m)))
#define SG_SOLID(c)     SG4(c, Q_ALL)

/* Every colour's empty cell looks the same, so the black gutter, the mark's
   frame and the read/unread column's "read" state are one constant. */
#define SG_BLACK        SG4(SG_GREEN, 0)

/* Gmail's four brand colours land on four of the eight the VDG has, and the
   envelope lands on buff. Nothing is missing and nothing is approximated. */
#define GM_BLUE         SG_SOLID(SG_BLUE)
#define GM_RED          SG_SOLID(SG_RED)
#define GM_YELLOW       SG_SOLID(SG_YELLOW)
#define GM_GREEN        SG_SOLID(SG_GREEN)
#define GM_PAPER        SG_SOLID(SG_BUFF)

#endif /* !COCO3 */

/* ------------------------------------------------------------------ */
/* Band geometry                                                       */
/* ------------------------------------------------------------------ */

/*
 * Inbox: rows 0-1 header, 2-12 list, 13-14 panel, 15 footer.
 *
 * Sixteen rows is two-thirds of the Atari's and every one of them is spoken
 * for. The header is two rows because the mark is; the panel is two because a
 * 12-column From and a 17-column Subject truncate hard enough that the
 * selected entry has to be spelled out somewhere, which is the same trade the
 * Atari makes with forty columns to play with.
 */
#define HDR_ROWS        2
#define FOOT_ROW        (SCR_ROWS - 1)
#define LIST_TOP        HDR_ROWS                /* rows 2..2+IDX_MAX-1 */
#define PANEL_ROW       (FOOT_ROW - 2)          /* rows 13-14 */

/* Message reader: subject on rows 1-2, body from row 3 down to the footer. */
#define MSG_TOP         3

/* Header columns. The mark occupies cols 0-5 on both header rows, col 6 is its
   black gutter, and the text starts clear of both. */
#define LOGO_ROW        0
#define LOGO_COL        0
#define HDR_GUTTER_COL  LOGO_SMALL_COLS         /* 6 */
#define HDR_TEXT_COL    8
#define RIGHT_COL       (SCR_COLS - 1)

/* ------------------------------------------------------------------ */
/* screen.c -- the blitter                                             */
/* ------------------------------------------------------------------ */

void scr_clear(void);
void scr_row_clear(unsigned char row);
void scr_rows_clear(unsigned char first, unsigned char last);

/* Write s into [col, col+width), space padded and truncated to fit. */
void scr_field(unsigned char row, unsigned char col, const char *s,
               unsigned char width, unsigned char inv);
void scr_text(unsigned char row, unsigned char col, const char *s,
              unsigned char inv);
void scr_right(unsigned char row, unsigned char rcol, const char *s,
               unsigned char inv);
void scr_center(unsigned char row, const char *s, unsigned char inv);

/* Raw byte access, for SG4. scr_fill writes one byte across a run, which is
   what draws the mark's frame and the black gutter.
   (The obvious parameter name `byte` is a typedef in <coco.h>.) */
void scr_cell(unsigned char row, unsigned char col, unsigned char v);
void scr_fill(unsigned char row, unsigned char col, unsigned char v,
              unsigned char width);

/* A run of color cells from a table -- one row of the mark. On the VDG this
   is a memcpy of SG4 bytes; on the GIME each entry is an attribute worn by a
   space, which is why logo.c goes through this rather than touching memory. */
void scr_cells(unsigned char row, unsigned char col,
               const unsigned char *v, unsigned char n);

#ifdef COCO3
/* Recolor a run of cells' attribute bytes, leaving their text alone: the
   footer band, and the brighter foreground an unread row gets. */
void scr_attr_run(unsigned char row, unsigned char col, unsigned char width,
                  unsigned char attr);
#endif

/* ------------------------------------------------------------------ */
/* logo.c -- the Gmail mark                                            */
/* ------------------------------------------------------------------ */

/*
 * An SG4 quadrant is 4 pixels wide and 6 scanlines tall, so a cell is 8 x 12
 * and the large mark's 10 x 5 cells are 80 x 60 pixels -- the 4:3 field the
 * real mark sits in.
 *
 * The sizes below are the mark itself. logo_large() paints a one-cell black
 * frame outside it as well, so it needs 12 x 7 on screen; logo_small() paints
 * one black gutter cell to its right and needs 7 x 2.
 */
#define LOGO_SMALL_COLS 6
#define LOGO_SMALL_ROWS 2
#define LOGO_LARGE_COLS 10
#define LOGO_LARGE_ROWS 5

#define LOGO_LARGE_FRAME_COLS (LOGO_LARGE_COLS + 2)
#define LOGO_LARGE_FRAME_ROWS (LOGO_LARGE_ROWS + 2)

/* Both take the top-left of the *mark*; logo_large() draws its frame in the
   cells around that, so row and col must both be at least 1. */
void logo_small(unsigned char row, unsigned char col);
void logo_large(unsigned char row, unsigned char col);

/* ------------------------------------------------------------------ */
/* input.c                                                             */
/* ------------------------------------------------------------------ */

/*
 * The frame wait the key poll spins in lives in timer.c now, with the counter
 * the wall clock runs on; plat_vsync(), plat_ticks() and plat_fps() are all
 * declared in gmail.h, because every backend owes them.
 *
 * It is still the address range tools/coco-shot.sh aims at: it needs somewhere
 * the program provably comes to rest, and a tight self-contained spin is the
 * only kind a sampled PC lands in reliably. Polling inkey() alone would leave
 * the CPU in the BASIC ROM's keyboard scan most of the time, where no symbol
 * names it.
 */

#endif /* COCO_PLATFORM_H */
