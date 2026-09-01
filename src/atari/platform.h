/*
 * Atari 8-bit backend -- internal interface shared by the files in this
 * directory. The portable half of the program talks to us only through the
 * plat_* / ui_* declarations in ../gmail.h.
 */

#ifndef ATARI_PLATFORM_H
#define ATARI_PLATFORM_H

#include <atari.h>

#define SCR_COLS    40
#define SCR_ROWS    24

/* ------------------------------------------------------------------ */
/* Palette                                                             */
/* ------------------------------------------------------------------ */

/*
 * Gmail's own colors, mapped onto Atari hue/luma.
 *
 * The important GR.0 constraint: a character's pixels take their *hue* from
 * COLPF2 and only their *luminance* from COLPF1. So text is never a different
 * color from its band, just a different brightness. Real color on this screen
 * comes from the player/missile logo, which has its own four registers.
 */
#define C_HDR_BG    _gtia_mkcolor(HUE_BLUE, 7)      /* Gmail's #F6F8FC app bar */
#define C_HDR_FG    _gtia_mkcolor(HUE_GREY, 0)
#define C_LIST_BG   _gtia_mkcolor(HUE_GREY, 7)      /* white message list */
#define C_LIST_FG   _gtia_mkcolor(HUE_GREY, 0)
#define C_FOOT_BG   _gtia_mkcolor(HUE_REDORANGE, 4) /* #EA4335 */
#define C_FOOT_FG   _gtia_mkcolor(HUE_GREY, 7)
#define C_BORDER    _gtia_mkcolor(HUE_REDORANGE, 2)

/* Flat scheme for the unbanded screens (splash, busy, error). */
#define C_FLAT_BG   _gtia_mkcolor(HUE_GREY, 7)
#define C_FLAT_FG   _gtia_mkcolor(HUE_GREY, 0)

/* Logo strokes -- same assignment as the Intellivision original. */
#define C_LOGO_RED      _gtia_mkcolor(HUE_REDORANGE, 5)  /* #EA4335 */
#define C_LOGO_YELLOW   _gtia_mkcolor(HUE_GOLD, 7)       /* #FBBC04 */
#define C_LOGO_BLUE     _gtia_mkcolor(HUE_BLUE, 4)       /* #4285F4 */
#define C_LOGO_GREEN    _gtia_mkcolor(HUE_GREEN, 5)      /* #34A853 */

/* ------------------------------------------------------------------ */
/* Band geometry                                                       */
/* ------------------------------------------------------------------ */

/*
 * Rows 0-2 header, 3-22 list/body, 23 footer. Both banded screens share this
 * split so one DLI chain serves both.
 */
#define HDR_ROWS    3
#define FOOT_ROW    (SCR_ROWS - 1)

/* ------------------------------------------------------------------ */
/* screen.c -- direct blitter over the OS text screen                  */
/* ------------------------------------------------------------------ */

void scr_sync(void);            /* re-read SAVMSC */
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
void scr_row_inv(unsigned char row, unsigned char inv);

/* ------------------------------------------------------------------ */
/* dli.c / dli.s -- color bands                                        */
/* ------------------------------------------------------------------ */

void dli_bands(void);           /* banded screen: shadows + DLIs on */
void dli_flat(unsigned char bg, unsigned char fg);  /* DLIs off, flat colors */
void dli_shutdown(void);
void dli_vbi_install(void);     /* dli.s -- keeps ATRACT at zero */
void dli_vbi_remove(void);

/* ------------------------------------------------------------------ */
/* timer.c -- the frame counter the wall clock runs on                 */
/* ------------------------------------------------------------------ */

/* plat_vsync(), plat_ticks() and plat_fps() are declared in gmail.h: every
   backend owes them now, so the contract lives with the rest of it. */

/* ------------------------------------------------------------------ */
/* pmg.c -- player/missile Gmail logo                                  */
/* ------------------------------------------------------------------ */

#define LOGO_LARGE  0           /* 4 text rows tall */
#define LOGO_SMALL  1           /* 2 text rows tall */
#define LOGO_COLS   11          /* width in character cells */

void pmg_init(void);
void pmg_show(unsigned char variant, unsigned char row, unsigned char col);
void pmg_hide(void);

#endif /* ATARI_PLATFORM_H */
