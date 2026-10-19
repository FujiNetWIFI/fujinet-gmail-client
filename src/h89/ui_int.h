/*
 * What the screen painters share. The painters are split by screen -- ui.c
 * the chrome and the flat screens, ui_fetch.c, ui_inbox.c, ui_msg.c and
 * ui_form.c the rest -- so that under HDOS each can ride in the overlay of
 * the screen it paints (see Makefile).
 */

#ifndef H89_UI_INT_H
#define H89_UI_INT_H

#include "../gmail.h"
#include "platform.h"

/* Header: the mark at cols 1-4 rows 0-2, everything else right of it. */
#define LOGO_ROW        0
#define LOGO_COL        1
#define HDR_TEXT_COL    6
#define CLOCK_COL       (RIGHT_COL - 4)
#define STATUS_W        (CLOCK_COL - 1 - HDR_TEXT_COL)

/* Flat screens: the large mark, centred, and the text under it. */
#define SPLASH_LOGO_ROW 4
#define SPLASH_LOGO_COL ((SCR_COLS - LOGO_LARGE_COLS) / 2)
#define FLAT_TEXT_ROW   13
#define FLAT_PROG_ROW   17

/* The reader's and the form's frames: two columns in from each side. */
#define MSG_COL         2

extern char          ui_sbuf[96];
extern char          ui_wrapped[2][SCR_COLS + 1];
extern char          ui_datebuf[ENT_DATE_LEN];

/* A banded screen is up, so a busy message can go on it and the clock has
   somewhere to be. */
extern unsigned char ui_banded;

void ui_num(char *dst, unsigned int v);
void ui_cat_num(unsigned int v);
void ui_band(const char *title);
void ui_status(const char *s, unsigned char inv);
void ui_flat(void);

#endif
