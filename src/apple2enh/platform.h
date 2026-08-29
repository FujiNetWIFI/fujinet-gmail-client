/*
 * Apple //e (enhanced) backend -- internal interface shared by the files in
 * this directory. The portable half of the program talks to us only through the
 * plat_* / ui_* declarations in ../gmail.h.
 *
 * Target is cc65's apple2enh: 80-column text, the alternate character set, and
 * MouseText. There is no colour here at all -- 80-column text is one bit per
 * pixel -- so everything the Atari backend does with player/missile graphics
 * and display list interrupts is done with inverse video and glyphs instead.
 *
 * apple2 is not a build of this. It is the unenhanced machine, with no
 * MouseText and a character generator that would render the marker column as
 * random inverse capitals.
 */

#ifndef APPLE2ENH_PLATFORM_H
#define APPLE2ENH_PLATFORM_H

#define SCR_COLS    80
#define SCR_ROWS    24

/* ------------------------------------------------------------------ */
/* Character set                                                       */
/* ------------------------------------------------------------------ */

/*
 * With ALTCHARSET on -- plat_init() sets it rather than inheriting it -- the
 * enhanced //e character generator reads:
 *
 *   $00-$1F inverse uppercase    $20-$3F inverse symbols
 *   $40-$5F MouseText            $60-$7F inverse lowercase
 *   $80-$FF normal ASCII + $80
 *
 * so an inverse space is $20, a solid white cell. That is what the selection
 * bar and the chrome bands are made of, exactly as the Atari backend uses its
 * own inverse spaces.
 *
 * MouseText has no ASCII to sit on, so the blitter steals the control range:
 * a byte $01-$1F in a string means MouseText glyph $40 + byte. Nothing from the
 * wire can collide with that. copy_san() clamps every index field to $20-$7E
 * before a painter sees it, and body.c applies the same rule byte by byte to
 * message text -- so only a string literal in this directory can produce one.
 *
 * Spelled as octal escapes on purpose: "\x1B" followed by a hex digit is one
 * escape, not two characters, and these end up inside longer hint strings.
 */
#define MT_HOURGLASS    "\003"
#define MT_CHECK        "\004"
#define MT_LEFT         "\010"
#define MT_DOTS         "\011"
#define MT_DOWN         "\012"
#define MT_UP           "\013"
#define MT_TOPRULE      "\014"
#define MT_RETURN       "\015"
#define MT_BLOCK        "\016"
#define MT_RULE         "\023"
#define MT_CORNER_BL    "\024"
#define MT_RIGHT        "\025"
#define MT_DITHER_A     "\026"
#define MT_DITHER_B     "\027"
#define MT_FOLDER_L     "\030"
#define MT_FOLDER_R     "\031"
#define MT_VRULE_R      "\032"
#define MT_DIAMOND      "\033"
#define MT_TWORULES     "\034"
#define MT_VRULE_L      "\037"

/* The unread marker. The Atari's is an asterisk because a 40-column row has no
   spare column to put a glyph in and column 1 is inside the selection bar; here
   it gets column 0 to itself, outside the bar, and can be a real mark. */
#define UNREAD_GLYPH    MT_DIAMOND

/* ------------------------------------------------------------------ */
/* Band geometry                                                       */
/* ------------------------------------------------------------------ */

/*
 * Rows 0-2 header, 3-22 content, 23 footer -- the same split the Atari uses, so
 * main.c does not know the difference. There the three bands are three colours
 * from a display list interrupt; here rows 0 and 23 carry the chrome and the
 * content between them is ordinary white on black.
 */
#define HDR_ROWS        3
#define FOOT_ROW        (SCR_ROWS - 1)
#define CONTENT_TOP     HDR_ROWS
#define CONTENT_ROWS    (FOOT_ROW - HDR_ROWS)

/* The right-hand edge of the text area, leaving column 79 clear. */
#define RIGHT_COL       78

/* ------------------------------------------------------------------ */
/* screen.c / blit.s -- direct blitter over the 80-column text page    */
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

/* A run of one repeated glyph -- rules and chrome bands. */
void scr_fill(unsigned char row, unsigned char col, const char *glyph,
              unsigned char width, unsigned char inv);

/* ------------------------------------------------------------------ */
/* logo.c -- the Gmail mark                                            */
/* ------------------------------------------------------------------ */

/*
 * The envelope, which on a one-bit screen is an inverse block with the M's
 * strokes punched through it. Two sizes, matching the Atari's two: the small
 * one sits in the header, the large one on the splash, busy and error screens.
 */
#define LOGO_SMALL_COLS 6
#define LOGO_SMALL_ROWS 3
#define LOGO_LARGE_COLS 16
#define LOGO_LARGE_ROWS 6

void logo_small(unsigned char row, unsigned char col);
void logo_large(unsigned char row, unsigned char col);

#endif /* APPLE2ENH_PLATFORM_H */
