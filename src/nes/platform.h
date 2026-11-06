/*
 * platform.h -- the NES backend's private interface.
 *
 * 64 columns by 28 rows of 4 x 8 text on a bitmap, colour in two-row bands
 * four columns at a time (scr.c), sprites for the Gmail mark, the busy dots
 * and the form's cursor (sprites.c), the controller and the Famicom
 * keyboards (input.c), and the message body in CHR-RAM (bodystore.c).
 *
 * The screen painters live in the PRG banks, one bank to a screen; tramp.c
 * is the fixed-half face of them that gmail.h's ui_* names call.
 */
#ifndef NES_PLATFORM_H
#define NES_PLATFORM_H

#include <stdint.h>

#include "neshw.h"

/* ------------------------------------------------------------------ */
/* Glyphs of our own (assets/nes/font4x8.txt)                          */
/* ------------------------------------------------------------------ */

#define G_DOT       "\x01"
#define G_LEFT      "\x02"
#define G_RIGHT     "\x03"
#define G_UP        "\x04"
#define G_DOWN      "\x05"
#define G_VBAR      "\x06"
#define G_RULE      "\x07"
#define C_RULE      0x07
#define C_RULE_L    0x08
#define C_RULE_R    0x09
#define C_DOT       0x01

/* ------------------------------------------------------------------ */
/* scr.c                                                               */
/* ------------------------------------------------------------------ */

/* s into row at col, padded with spaces (or cut) to w cells. */
void scr_put(uint8_t row, uint8_t col, const char *s, uint8_t w);
void scr_text(uint8_t row, uint8_t col, const char *s);
void scr_right(uint8_t row, uint8_t end, const char *s);   /* ends before end */
void scr_center(uint8_t row, const char *s);
void scr_blank(uint8_t row0, uint8_t row1);

/* Group g's (rows 2g, 2g+1) quadrants q0..q1 (columns 4q..4q+3) to palette
   p; scr_band does the whole width. */
void scr_attr(uint8_t g, uint8_t q0, uint8_t q1, uint8_t p);
void scr_band(uint8_t g, uint8_t p);

void scr_cls(void);                     /* spaces, palette 0 */
void scr_frame(void);                   /* compose a row, wait a frame */
void scr_sync(void);                    /* everything on the screen */
void scr_begin(void);                   /* screen off */
void scr_end(void);                     /* everything across, screen on */
void ppu_copy_far(uint8_t bank, const uint8_t *src, unsigned int n);

/* ------------------------------------------------------------------ */
/* sprites.c                                                           */
/* ------------------------------------------------------------------ */

void spr_init(void);                    /* the patterns; screen off */
void spr_clear(void);                   /* every sprite off the screen */
void spr_mark(void);                    /* the header's small mark */
void spr_mark_big(uint8_t row);         /* the flat screens' mark, at row */
void spr_spin(uint8_t row);             /* the busy dots, under row */
void spr_caret(uint8_t row, uint8_t col);
void spr_caret_off(void);

/* ------------------------------------------------------------------ */
/* input.c                                                             */
/* ------------------------------------------------------------------ */

/* One input event: a keyboard character as fuji_nes_kbd_getc() gives it,
   or a controller button going down (EV_*). */
#define EV_NONE     0
#define EV_A        0xF0
#define EV_B        0xF1
#define EV_SELECT   0xF2
#define EV_START    0xF3
#define EV_UP       0xF4
#define EV_DOWN     0xF5
#define EV_LEFT     0xF6
#define EV_RIGHT    0xF7

extern uint8_t inp_kbd;                 /* FUJI_NES_KBD_*, from inp_init */
void    inp_init(void);
uint8_t inp_event(void);                /* this frame's, or EV_NONE */

/* Which screen is up, for the controller's mapping: A opens in the inbox
   but replies in the reader. Set by the painters. */
#define SCREEN_FLAT     0
#define SCREEN_INBOX    1
#define SCREEN_READER   2
#define SCREEN_FORM     3
extern uint8_t ui_screen;

/* The form's on-screen keyboard and its answers (ui_form.c, in COMPOSE). */
extern uint8_t form_ask;                /* the send? question is up */

/* ------------------------------------------------------------------ */
/* The painters, in their banks (tramp.c calls them)                   */
/* ------------------------------------------------------------------ */

#pragma wrapped-call (push, bank_tramp, BANK_BOOT)
void boot_init(void);
void boot_flat(uint8_t what, uint8_t code, const char *stage);
#pragma wrapped-call (pop)

/* boot_flat's what: gmail.h's BUSY_* (1-3), or one of these. */
#define FLAT_SPLASH     10
#define FLAT_NOTFOUND   11
#define FLAT_ERROR      12
#define FLAT_SENT       13

#pragma wrapped-call (push, bank_tramp, BANK_INBOX)
void inbox_paint(void);
void inbox_sel(uint8_t from, uint8_t to);
#pragma wrapped-call (pop)

#pragma wrapped-call (push, bank_tramp, BANK_READER)
void reader_paint(unsigned int top);
#pragma wrapped-call (pop)

#pragma wrapped-call (push, bank_tramp, BANK_COMPOSE)
void form_paint(uint8_t mode);
void form_row(uint8_t f, const char *win, uint8_t curx, uint8_t active);
void form_msg(uint8_t msg);
uint8_t form_key(uint8_t ev);
#pragma wrapped-call (pop)

/* The header every full screen shares (header.c, fixed): the red bar, the
   mark, "Gmail", the title on the right of row 1, and the clock. */
void header_paint(const char *title);
void header_off(void);                  /* a flat screen: no bar, no clock */

/* The painters' shared scratch (header.c): only one screen is painted at a
   time. ui_buf holds a "From: " line or a whole 64-column row. */
#define UI_BUF  (ENT_NAME_LEN + ENT_SUBJ_LEN + 8)
extern char ui_buf[UI_BUF];
extern char ui_wrap[2][SCR_COLS + 1];
extern char ui_num[12];

/* The hint bar, rows 26-27: one line for the controller, one for a
   keyboard if there is one. */
void hint_paint(const char *pad, const char *kbd);

#endif
