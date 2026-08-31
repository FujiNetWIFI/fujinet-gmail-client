/*
 * MS-DOS backend -- internal interface shared by the files in this directory.
 * The portable half of the program talks to us only through the plat_* / ui_*
 * declarations in ../gmail.h.
 *
 * Target is Open Watcom's 8086 small model, BIOS text modes only, which is
 * what lets the same GMAIL.EXE run on everything from a PCjr to a 486: no
 * instruction newer than the 8088 has, no video access fancier than the text
 * page every adapter in the family exposes.
 *
 * The one thing no other backend has to deal with: the screen width is not
 * known until the program is running. A PC inherits whatever video mode it
 * was started in -- 40x25 in modes 0/1, 80x25 in modes 2/3, and the MDA's
 * mode 7 -- so SCR_COLS cannot be a macro here. screen.c probes the mode in
 * plat_init() and exports the geometry as variables; ui.c picks its layout
 * off scr_wide, and body.c wraps to the runtime width through the GM_RT_COLS
 * hook in gmail.h.
 */

#ifndef MSDOS_PLATFORM_H
#define MSDOS_PLATFORM_H

#ifndef GM_RT_COLS
#error "the MS-DOS backend requires -DGM_RT_COLS (see CFLAGS_EXTRA_MSDOS)"
#endif

/* Every mode this backend runs in is 25 rows. EGA and VGA can be talked into
   43 or 50, and a program started there is put back into mode 3 by the probe
   rather than taught a fourth geometry -- see plat_init(). */
#define SCR_ROWS    25

extern unsigned char scr_cols;      /* 40 or 80, probed at plat_init()      */
extern unsigned char scr_wide;      /* scr_cols >= 80: the layout switch    */

/* ------------------------------------------------------------------ */
/* Attributes                                                          */
/* ------------------------------------------------------------------ */

/*
 * Painters name a role, not a byte. screen.c resolves the role through one of
 * three tables picked at init -- colour (modes 1/3), black-and-white (modes
 * 0/2, or /MONO), and MDA (mode 7) -- so a painter never knows whether the
 * selection bar it just drew is light-grey-on-blue or the MDA's reverse
 * video. This is the same job the other backends' `inv` flag does, grown to
 * fit hardware that has more than one bit of emphasis: the MDA's underline
 * and intensity are real attributes here, not glyph tricks.
 */
#define A_TEXT      0   /* list and body text                            */
#define A_EMPH      1   /* unread rows; intensity on MDA                 */
#define A_SEL       2   /* the selection bar                             */
#define A_BAR       3   /* the app bar, row 0                            */
#define A_FOOT      4   /* the hint bar, row 24                          */
#define A_UNDER     5   /* the reader's subject; underline on MDA        */
#define N_ATTRS     6

/* ------------------------------------------------------------------ */
/* Character set                                                       */
/* ------------------------------------------------------------------ */

/*
 * Code page 437. Unlike every other backend there is no sc() mapping at all:
 * the byte in the string is the glyph in the cell. copy_san() clamps every
 * wire field to $20-$7E and body.c applies the same rule to message text, so
 * the control range and the high range are ours for chrome -- and CP437
 * fills both with exactly the furniture a mail client wants.
 */
#define GL_UNREAD   "\x04"      /* diamond, the unread marker            */
#define GL_UP       "\x18"
#define GL_DOWN     "\x19"
#define GL_RIGHT    "\x1A"
#define GL_LEFT     "\x1B"
#define GL_RULE     0xC4        /* single horizontal line, for scr_fill  */

/* ------------------------------------------------------------------ */
/* Band geometry                                                       */
/* ------------------------------------------------------------------ */

/* Rows 0-2 header, 3-23 content, 24 footer -- the same three-region split
   every backend makes, with the extra row a 25-line screen has over the
   Apple's 24 spent on the content band. */
#define HDR_ROWS        3
#define FOOT_ROW        (SCR_ROWS - 1)
#define CONTENT_TOP     HDR_ROWS

/* ------------------------------------------------------------------ */
/* screen.c -- direct writes into the B000/B800 text page              */
/* ------------------------------------------------------------------ */

void scr_clear(void);
void scr_row_clear(unsigned char row);
void scr_rows_clear(unsigned char first, unsigned char last);

/* Write s into [col, col+width), space padded and truncated to fit. */
void scr_field(unsigned char row, unsigned char col, const char *s,
               unsigned char width, unsigned char attr);
void scr_text(unsigned char row, unsigned char col, const char *s,
              unsigned char attr);
void scr_right(unsigned char row, unsigned char rcol, const char *s,
               unsigned char attr);
void scr_center(unsigned char row, const char *s, unsigned char attr);

/* A run of one repeated glyph -- rules and chrome bands. */
void scr_fill(unsigned char row, unsigned char col, unsigned char glyph,
              unsigned char width, unsigned char attr);

/* One cell with an explicit attribute byte, sidestepping the role tables --
   logo.c paints its coloured strokes with this. */
void scr_cell(unsigned char row, unsigned char col, unsigned char glyph,
              unsigned char rawattr);

/* The byte a role resolves to, and whether the table in force is the colour
   one -- what logo.c needs to build stroke attributes that sit on the right
   background. */
unsigned char scr_attr_byte(unsigned char attr);
extern unsigned char scr_color;

/* ui.c's layout choice, made once the geometry is known. Called at the end
   of plat_init(), which is the one place that knows when that is. */
void ui_geom(void);

#ifdef GM_SHOT
/* tools/msdos-shot.sh's capture: the text page, verbatim, into SCREEN.BIN
   in the current directory -- two geometry bytes then cols x rows char/attr
   pairs. input.c calls it where the program would otherwise block. */
void scr_snapshot(void);
#endif

/* ------------------------------------------------------------------ */
/* logo.c -- the Gmail mark                                            */
/* ------------------------------------------------------------------ */

#define LOGO_SMALL_COLS 6
#define LOGO_SMALL_ROWS 3
#define LOGO_LARGE_COLS 16
#define LOGO_LARGE_ROWS 6

void logo_small(unsigned char row, unsigned char col);
void logo_large(unsigned char row, unsigned char col);

/* ------------------------------------------------------------------ */
/* The bus, for the shims                                              */
/* ------------------------------------------------------------------ */

/*
 * fujinet-lib's INT F5 entry point: DL=0x40 read, AL=device, AH=command,
 * CL/CH=aux1/2, ES:BX=buffer, DI=length; returns AL, 'C' for complete. It is
 * a public member of the shipped archive, but its declaration lives in the
 * library's internal fujinet-fuji-msdos.h, which the release zip does not
 * carry -- so the shims declare it here instead of growing a bus layer of
 * their own.
 */
unsigned char int_f5_read(unsigned char dev, unsigned char command,
                          unsigned char aux1, unsigned char aux2,
                          void *buf, unsigned short len);

#endif /* MSDOS_PLATFORM_H */
