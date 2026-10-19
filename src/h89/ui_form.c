/*
 * The compose form, and the word that the message went.
 */

#include <stdlib.h>
#include <string.h>

#include "../gmail.h"
#include "platform.h"
#include "ui_int.h"

/*
 * The Apple's arrangement, framed: full-word labels and wide header
 * windows, and the body in a box like the reader's. An inactive header
 * shows its extent as an underline; the active field is a reverse bar with
 * the H19's own block cursor at the edit point, which the Apple has to fake
 * with a hole in the bar. compose.c owns the cursor and the horizontal
 * scroll; this end paints what it is given.
 */

#define FRM_VAL_COL     11
#define FRM_BOX_TOP     5
#define FRM_BODY_TOP    (FRM_BOX_TOP + 1)
#define FRM_BOX_BOT     (FRM_BODY_TOP + FRM_VBODY)
#define FRM_HINT_ROW    (FRM_BOX_BOT + 1)
#define FRM_MSG_ROW     (FRM_HINT_ROW + 1)

#if FRM_MSG_ROW > LAST_ROW
#error "the form no longer fits -- lower FRM_VBODY"
#endif
#if FRM_BODY_COLS + 1 > SCR_COLS - 2 * MSG_COL
#error "FRM_BODY_COLS is wider than the form's frame"
#endif

/* Where the cursor was put, so it comes back after the send? question. */
static unsigned char frm_cx, frm_cy;

static unsigned char frm_row(unsigned char f)
{
    if (f == F_TO)
        return 3;
    if (f == F_SUBJ)
        return 4;
    return (unsigned char) (FRM_BODY_TOP + (f - F_BODY0));
}

static unsigned char frm_col(unsigned char f)
{
    return (unsigned char) ((f >= F_BODY0) ? MSG_COL : FRM_VAL_COL);
}

/* One cell more than the storage width, so the cursor can sit past a full
   value; both header fields fit their windows whole. */
unsigned char ui_form_width(unsigned char f)
{
    return (unsigned char) ((f >= F_BODY0) ? (FRM_BODY_COLS + 1)
                                           : (FRM_TO_MAX + 1));
}

void ui_form(unsigned char mode)
{
    ui_band((mode == FRM_REPLY) ? "Reply"
          : (mode == FRM_FWD)   ? "Forward"
                                : "New message");
    ui_screen = UI_FORM;

    /* Row 1 says what a reply or a forward is acting on. */
    if (mode != FRM_COMPOSE) {
        scr_text(1, HDR_TEXT_COL,
                 (mode == FRM_REPLY) ? "In reply to" : "Forwarding", 0);
        scr_field(1, HDR_TEXT_COL + 12,
                  (mode == FRM_REPLY) ? gm_index[gm_sel].name
                                      : gm_index[gm_sel].subject,
                  STATUS_W - 12, 0);
    }
    ui_clock();

    scr_text(3, 2, "To", 0);
    scr_text(4, 2, "Subject", 0);

    scr_box(FRM_BOX_TOP, 0, FRM_BOX_BOT, SCR_COLS - 1);
    scr_rule(FRM_BOX_TOP, 1, SCR_COLS - 2, "Message", 1);

    if (mode == FRM_REPLY)
        scr_text(FRM_HINT_ROW, 2,
                 "Blank To and Subject take the reply defaults.", 0);

    scr_keys("[RETURN] Next  [^v] Field  [<>] Cursor  [DEL] Erase  "
             "[BLUE] Send  [ESC] Done");
}

void ui_form_row(unsigned char f, const char *win, unsigned char curx,
                 unsigned char active)
{
    unsigned char w = ui_form_width(f);
    unsigned char row = frm_row(f);
    unsigned char col = frm_col(f);
    unsigned char n;

    scr_field(row, col, win, w, active);

    if (active) {
        frm_cx = (unsigned char) (col + curx);
        frm_cy = row;
        h19_cursor(frm_cx, frm_cy);
        return;
    }

    /* The body is framed already; a header shows how far it goes. */
    if (f >= F_BODY0)
        return;
    n = (unsigned char) strlen(win);
    if (n < w)
        scr_fill(row, (unsigned char) (col + n), G_BAR_BOTTOM,
                 (unsigned char) (w - n));
}

void ui_form_msg(unsigned char msg)
{
    const char *s;

    scr_row_clear(FRM_MSG_ROW);

    switch (msg) {
    case FM_ASK:      s = " Send message? (Y/N) ";     break;
    case FM_NEEDTO:   s = " A recipient is required "; break;
    case FM_NEEDBODY: s = " A message is required ";   break;
    default:
        /* FM_NONE: cleared above, and the cursor back where it was. */
        h19_cursor(frm_cx, frm_cy);
        return;
    }

    /* The question is answered with a key, not typed into a field. */
    if (msg == FM_ASK)
        h19_cursor(H19_NONE, 0);

    scr_center(FRM_MSG_ROW, s, 1);
}

/*
 * In place on the form it was sent from, which is still the best picture of
 * what went: the caller paints its own screen back afterwards whatever this
 * does, so a flat screen in between would be a whole screen for nothing.
 */
void ui_sent(void)
{
    if (ui_banded) {
        h19_cursor(H19_NONE, 0);
        ui_status("Message sent", 1);
        scr_keys("Message sent -- press any key");
    } else {
        ui_flat();
        scr_center(FLAT_TEXT_ROW, "Message sent", 0);
        scr_center(FLAT_TEXT_ROW + 4, "Press any key", 0);
    }
}
