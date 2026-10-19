/*
 * Heathkit H89 backend -- internal interface shared by the files in this
 * directory. The portable half of the program talks to us only through the
 * plat_* / ui_* declarations in ../gmail.h.
 *
 * The H89's screen is its H19 terminal: 80x24, a 25th line that never
 * scrolls, reverse video, and 33 graphics characters -- lines, corners and
 * tees that join cell to cell, diagonals, halves, a checkerboard, a bullet.
 * The layout is the Apple //e backend's (src/apple2enh) where it can be, and
 * the chrome is the Google Calendar client's H89 port: an app bar across row
 * 0, a status row with the clock, framed tables and a frame round the
 * message, and a line of its own for the keys.
 *
 * The one thing that shapes everything else: the terminal is on the far end
 * of a 9600-baud line, about a thousand characters a second, and the program
 * cannot read what is on it. So nothing here writes to the terminal. Every
 * painter writes cells into h19.c's copy of the screen, and the copy is sent
 * by difference when the program next waits for a key -- see input.c.
 */

#ifndef H89_PLATFORM_H
#define H89_PLATFORM_H

#include "h19.h"

#define SCR_COLS    80
#define SCR_ROWS    24          /* the page; the 25th line is KEY_ROW */
#define KEY_ROW     H19_STATUS_ROW

/* ------------------------------------------------------------------ */
/* Glyphs in strings                                                   */
/* ------------------------------------------------------------------ */

/*
 * A cell holds the H19's ROM index, and the graphics characters are indices
 * 0x00-0x1F and 0x7F -- so a string can carry them as those bytes, the way
 * the Apple backend carries MouseText. Nothing from the wire can collide:
 * copy_san() clamps every field to 0x20-0x7E before a painter sees it, and
 * wrap_text() only ever copies what body.c already sanitised.
 *
 * Index 0x00 is the vertical line, which cannot be in a C string, so it
 * travels as 0x1E -- the pilcrow's index, which nothing here draws -- and
 * screen.c puts it back.
 *
 * Spelled as three-digit octal escapes on purpose: "\x1B" followed by a hex
 * digit is one escape, not two characters.
 */
#define HG_VLINE        "\036"
#define HG_HLINE        "\001"
#define HG_CROSS        "\002"
#define HG_UR           "\003"  /* corners: upper right... */
#define HG_LR           "\004"
#define HG_LL           "\005"
#define HG_UL           "\006"
#define HG_ARROW_R      "\010"
#define HG_CHECKER      "\011"
#define HG_ARROW_D      "\013"
#define HG_HALF_TOP     "\020"
#define HG_TEE_DOWN     "\023"  /* tees, named for the way the stem points */
#define HG_TEE_LEFT     "\024"
#define HG_TEE_UP       "\025"
#define HG_TEE_RIGHT    "\026"
#define HG_X            "\027"
#define HG_SLASH        "\030"
#define HG_BACKSLASH    "\031"
#define HG_TRI_UR       "\037"
#define HG_BULLET       "\177"

#define HG_VLINE_C      0x1E    /* the same, as a byte */
#define HG_HLINE_C      0x01

/* ------------------------------------------------------------------ */
/* Band geometry                                                       */
/* ------------------------------------------------------------------ */

/*
 * Rows 0-2 header, 3-23 content, and the 25th line for the keys. The Apple
 * spends row 23 on its hints; here they have a line the page cannot scroll
 * into, so row 23 is content.
 */
#define HDR_ROWS        3
#define CONTENT_TOP     HDR_ROWS
#define LAST_ROW        (SCR_ROWS - 1)

/* The right-hand edge of the text area, leaving column 79 clear. */
#define RIGHT_COL       78

/* ------------------------------------------------------------------ */
/* screen.c -- cells into h19.c's copy of the screen                   */
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

/* One cell, already coded: a ROM index, | REV for reverse. */
void scr_cell(unsigned char row, unsigned char col, unsigned char cell);

/* A run of one cell -- lines, the progress bar, bands. */
void scr_fill(unsigned char row, unsigned char col, unsigned char cell,
              unsigned char width);

/* A horizontal line from col to col+width-1 with a label set into it,
   "--| label |-----", the label starting lcol cells in. A NULL or empty
   label is a plain line. */
void scr_rule(unsigned char row, unsigned char col, unsigned char width,
              const char *label, unsigned char lcol);

/* The four sides of a box, the corners included; the inside is untouched. */
void scr_box(unsigned char top, unsigned char left, unsigned char bottom,
             unsigned char right);

/*
 * The 25th line: a legend of the keys the screen takes. Text in [brackets]
 * is a keycap -- drawn in reverse, the brackets dropped -- so a screen names
 * a key the way it is printed on the H19's keyboard.
 */
void scr_keys(const char *legend);

/* ------------------------------------------------------------------ */
/* logo.c -- the Gmail mark                                            */
/* ------------------------------------------------------------------ */

#define LOGO_SMALL_COLS 4
#define LOGO_SMALL_ROWS 3
#define LOGO_LARGE_COLS 16
#define LOGO_LARGE_ROWS 6

void logo_small(unsigned char row, unsigned char col);
void logo_large(unsigned char row, unsigned char col);

/* ------------------------------------------------------------------ */
/* ui.c -- what input.c needs to know of the screen                    */
/* ------------------------------------------------------------------ */

/*
 * The screen that is up. The key map is the one place that asks: RED is
 * reply in the reader and nothing in the inbox, where the portable loop
 * would otherwise fold it into a refresh nobody asked RED for.
 */
#define UI_FLAT     0
#define UI_INBOX    1
#define UI_READER   2
#define UI_FORM     3

extern unsigned char ui_screen;

#endif /* H89_PLATFORM_H */
