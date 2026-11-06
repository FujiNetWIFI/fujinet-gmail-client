/*
 * ui_reader.c -- the READER bank: one message.
 *
 *   rows  0-1   the app bar: the mark, "Gmail", the clock; "Inbox" and where
 *               in the message the window is
 *         2-5   From and the date, the subject across two rows, a rule
 *         6-25  the body, MSG_ROWS (20) rows of 64
 *        26-27  the hint bar
 *
 * The body is in CHR-RAM (bodystore.c), a row at a time and a frame for each
 * while the screen is on. So the reader works from what it has: a scroll of
 * one row moves the nineteen rows already on the screen -- they are in
 * scr_txt, and scr_put is happy to copy from there -- and fetches only the
 * row that comes into view. A bigger jump fetches the lot with the screen off,
 * where a row costs nothing to read, and the page goes up whole.
 */

#include <stdlib.h>
#include <string.h>

#include "../gmail.h"
#include "platform.h"

#pragma code-name (push, "READER")
#pragma rodata-name (push, "READERRO")

#define INFO_ROW    2
#define BODY_TOP    6
#define DATE_COL    49
#define DATE_W      15

#if BODY_TOP + MSG_ROWS != 26
#error "the reader's body must end on row 25, above the hint bar"
#endif

static unsigned int shown;              /* the top row on the screen */

static void put_right(uint8_t row, uint8_t col, uint8_t w, const char *s)
{
    uint8_t n = (uint8_t) strlen(s);

    if (n > w)
        n = w;
    scr_put(row, col, "", (uint8_t) (w - n));
    scr_put(row, (uint8_t) (col + w - n), s, n);
}

static void body_line(uint8_t i, unsigned int r)
{
    scr_put((uint8_t) (BODY_TOP + i), 0,
            (r < gm_body_rows) ? body_row(r) : "", SCR_COLS);
}

/* "Lines 21-40 of 112", "+" when the body was cut short. */
static void where(unsigned int top)
{
    unsigned int last = top + MSG_ROWS;

    if (last > gm_body_rows)
        last = gm_body_rows;
    if (gm_body_rows <= MSG_ROWS) {
        ui_buf[0] = '\0';
    } else {
        strcpy(ui_buf, "Lines ");
        utoa(top + 1, ui_num, 10);
        strcat(ui_buf, ui_num);
        strcat(ui_buf, "-");
        utoa(last, ui_num, 10);
        strcat(ui_buf, ui_num);
        strcat(ui_buf, " of ");
        utoa(gm_body_rows, ui_num, 10);
        strcat(ui_buf, ui_num);
    }
    if (gm_body_trunc)
        strcat(ui_buf, "+");
    put_right(1, 30, 33, ui_buf);
}

static void full(unsigned int top)
{
    struct entry *e = &gm_index[gm_sel];
    char          date[ENT_DATE_LEN];
    unsigned int  n;
    uint8_t       i;

    ui_screen = SCREEN_READER;
    scr_begin();
    spr_clear();
    scr_cls();
    header_paint("Inbox");

    strcpy(ui_buf, "From: ");
    strcat(ui_buf, e->name);
    scr_put(INFO_ROW, 0, ui_buf, DATE_COL);
    date_fmt(date, e->ts);
    put_right(INFO_ROW, DATE_COL, DATE_W, date);

    n = wrap_text(e->subject, (char *) ui_wrap, 2, SCR_COLS, SCR_COLS + 1);
    for (i = 0; i < 2; ++i)
        scr_put((uint8_t) (INFO_ROW + 1 + i), 0,
                (i < n) ? (const char *) ui_wrap[i] : "", SCR_COLS);

    memset(ui_buf, C_RULE, SCR_COLS);
    ui_buf[SCR_COLS] = '\0';
    scr_put(INFO_ROW + 3, 0, ui_buf, SCR_COLS);

    if (gm_body_rows == 0) {
        scr_put(BODY_TOP, 0, "(no text content)", SCR_COLS);
    } else {
        for (i = 0; i < MSG_ROWS; ++i)
            body_line(i, top + i);
    }

    where(top);
    hint_paint("A Reply    START Forward    B Back    "
               G_UP G_DOWN " Line    " G_LEFT G_RIGHT " Page",
               "R Reply    F Forward    ESC Back    Arrows Scroll");
    shown = top;
    scr_end();
}

void reader_paint(unsigned int top)
{
    uint8_t i;

    if (ui_screen != SCREEN_READER) {
        full(top);
        return;
    }

    if (top == shown + 1) {
        for (i = 0; i < MSG_ROWS - 1; ++i)
            scr_put((uint8_t) (BODY_TOP + i), 0,
                    scr_txt + ((unsigned int) (BODY_TOP + i + 1) << 6), SCR_COLS);
        body_line(MSG_ROWS - 1, top + MSG_ROWS - 1);
    } else if (top + 1 == shown) {
        for (i = MSG_ROWS - 1; i; --i)
            scr_put((uint8_t) (BODY_TOP + i), 0,
                    scr_txt + ((unsigned int) (BODY_TOP + i - 1) << 6), SCR_COLS);
        body_line(0, top);
    } else if (top != shown) {
        scr_begin();
        for (i = 0; i < MSG_ROWS; ++i)
            body_line(i, top + i);
        where(top);
        shown = top;
        scr_end();
        return;
    }

    where(top);
    shown = top;
}

