/*
 * Tandy Color Computer backend -- internal interface shared by the files in
 * this directory. The portable half of the program talks to us only through
 * the plat_* / ui_* declarations in ../gmail.h.
 *
 * The screen is the 6847's 32x16 alpha/semigraphics page at $0400, and the
 * whole design turns on one property of it: the VDG decides per byte whether a
 * cell is a character or a 2x2 block of colour, with no mode switch and no
 * second display list.
 *
 *   $00-$3F   glyph (byte & $3F), INV asserted -- green on dark green
 *   $40-$7F   the same glyph, normal video -- dark green on green
 *   $80-$FF   SG4: bit 7 set, bits 6-4 colour, bits 3-0 quadrant mask
 *
 * So text and colour intermix freely. This is the only backend that draws the
 * Gmail mark as the mark -- a white envelope with four coloured strokes -- as
 * ordinary bytes in screen RAM. The Atari needs four players and an interrupt
 * for the same thing; the Apple, with one bit per pixel, can only punch the
 * strokes out of an inverse block as holes.
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

#define SCR_COLS    32
#define SCR_ROWS    16
#define SCR_RAM     ((unsigned char *) 0x0400)

/* Space, normal video. Not zero -- see the header comment. */
#define SCR_BLANK   0x60

/* ------------------------------------------------------------------ */
/* Semigraphics-4                                                      */
/* ------------------------------------------------------------------ */

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
