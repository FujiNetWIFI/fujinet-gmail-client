/*
 * ui_form.c -- the COMPOSE bank: the compose form and its on-screen keyboard.
 *
 *   rows  0-1   the app bar: "New message", "Reply" or "Forward"
 *         2-3   To and Subject
 *         4     a rule
 *         5-14  the body, FRM_VBODY (10) lines
 *         15    the form's messages: send?, a missing recipient...
 *         16-25 the on-screen keyboard
 *        26-27  the hint bar
 *
 * compose.c (in R7) owns the editing; this file only paints what it is
 * given and turns the controller into keys. The active field has a marker
 * in column 0 and the cursor is a sprite -- a bar in the gap ahead of the
 * cell it marks -- because colour is two rows deep and To and Subject share
 * a pair.
 *
 * The on-screen keyboard is four rows of sixteen keys and a row of actions,
 * each key one attribute quadrant (four columns, two rows), so moving the
 * cursor is one quadrant changing colour. Three pages: lower case, upper
 * case, and the symbols; between them they hold every printable character.
 * A keyboard on the expansion port types straight past all of this
 * (input.c), and the on-screen one stays up for the controller regardless.
 */

#include <string.h>

#include "../gmail.h"
#include "platform.h"

#pragma code-name (push, "COMPOSE")
#pragma rodata-name (push, "COMPOSERO")

#define ROW_TO      2
#define ROW_SUBJ    3
#define ROW_RULE    4
#define ROW_BODY    5
#define ROW_MSG     15
#define ROW_OSK     16
#define VAL_COL     10          /* To and Subject's windows: cols 10-63 */
#define BODY_COL    1           /* the body's: cols 1-63 */

#define GRP_OSK     (ROW_OSK / 2)
#define OSK_ROWS    5           /* four of keys, one of actions */
#define ACT_ROW     4

#if ROW_BODY + FRM_VBODY != ROW_MSG
#error "the form's body must end above its message row"
#endif

uint8_t form_ask;

static uint8_t page;            /* 0 lower case, 1 upper, 2 symbols */
static uint8_t osk_r, osk_q;    /* the cursor: key row, quadrant column */
static uint8_t caret_row = 0xFF;

static const char keys[3][64] = {
    "1234567890@.-_/:"
    "qwertyuiop,;'?!\""
    "asdfghjkl()&+=*#"
    "zxcvbnm<>[]$%^~|",

    "1234567890@.-_/:"
    "QWERTYUIOP,;'?!\""
    "ASDFGHJKL()&+=*#"
    "ZXCVBNM<>[]$%^~|",

    "!\"#$%&'()*+,-./:"
    ";<=>?@[\\]^_`{|}~"
    "1234567890      "
    "                "
};

/* The action row: first quadrant, quadrants wide, label, what it sends. */
#define A_SPACE     0
#define A_LEFT      1
#define A_RIGHT     2
#define A_DEL       3
#define A_NEXT      4
#define A_PAGE      5
#define A_SEND      6
#define A_DONE      7
#define N_ACT       8

static const uint8_t act_q[N_ACT + 1] = { 0, 4, 5, 6, 8, 10, 12, 14, 16 };
static const char *const act_label[N_ACT] = {
    "SPACE", G_LEFT, G_RIGHT, "DEL", "NEXT", 0, "SEND", "DONE"
};
static const char *const page_label[3] = { "ABC", "#+=", "abc" };

static uint8_t act_at(uint8_t q)
{
    uint8_t a = 0;

    while (act_q[a + 1] <= q)
        ++a;
    return a;
}

/* The highlight on the cursor's key, or off it. */
static void osk_light(uint8_t on)
{
    uint8_t p = on ? PAL_RED : PAL_SEL;
    uint8_t g = (uint8_t) (GRP_OSK + osk_r);
    uint8_t a;

    if (osk_r == ACT_ROW) {
        a = act_at(osk_q);
        scr_attr(g, act_q[a], (uint8_t) (act_q[a + 1] - 1), p);
    } else {
        scr_attr(g, osk_q, osk_q, p);
    }
}

static void osk_labels(void)
{
    uint8_t r, k, a, w, n;
    const char *s;

    for (r = 0; r < 4; ++r) {
        memset(ui_buf, ' ', SCR_COLS);
        for (k = 0; k < 16; ++k)
            ui_buf[(k << 2) + 1] = keys[page][(r << 4) + k];
        ui_buf[SCR_COLS] = '\0';
        scr_put((uint8_t) (ROW_OSK + (r << 1)), 0, ui_buf, SCR_COLS);
    }

    memset(ui_buf, ' ', SCR_COLS);
    for (a = 0; a < N_ACT; ++a) {
        s = act_label[a] ? act_label[a] : page_label[page];
        n = (uint8_t) strlen(s);
        w = (uint8_t) ((act_q[a + 1] - act_q[a]) << 2);
        memcpy(ui_buf + (act_q[a] << 2) + ((w - n) >> 1), s, n);
    }
    ui_buf[SCR_COLS] = '\0';
    scr_put(ROW_OSK + (ACT_ROW << 1), 0, ui_buf, SCR_COLS);
}

static void osk_paint(void)
{
    uint8_t g;

    for (g = 0; g < OSK_ROWS; ++g)
        scr_band((uint8_t) (GRP_OSK + g), PAL_SEL);
    for (g = 0; g < OSK_ROWS; ++g)
        scr_blank((uint8_t) (ROW_OSK + (g << 1) + 1), (uint8_t) (ROW_OSK + (g << 1) + 1));
    osk_labels();
    osk_light(1);
}

/* ------------------------------------------------------------------ */
/* gmail.h's form hooks                                                */
/* ------------------------------------------------------------------ */

void form_paint(uint8_t mode)
{
    ui_screen = SCREEN_FORM;
    form_ask = 0;
    caret_row = 0xFF;
    scr_begin();
    spr_clear();
    scr_cls();
    header_paint((mode == FRM_REPLY) ? "Reply"
               : (mode == FRM_FWD)   ? "Forward"
                                     : "New message");
    if (mode == FRM_REPLY)
        scr_put(1, 26, "Blank To and Subject reply to the sender", 37);

    scr_put(ROW_TO, 1, "To:", VAL_COL - 1);
    scr_put(ROW_SUBJ, 1, "Subject:", VAL_COL - 1);
    memset(ui_buf, C_RULE, SCR_COLS);
    ui_buf[SCR_COLS] = '\0';
    scr_put(ROW_RULE, 0, ui_buf, SCR_COLS);

    osk_paint();
    hint_paint("A Type    B Delete    SELECT Letters/symbols    START Done",
               "RETURN Next field    ESC Done    Arrows Move");
    scr_end();
}

static uint8_t row_of(uint8_t f)
{
    if (f == F_TO)
        return ROW_TO;
    if (f == F_SUBJ)
        return ROW_SUBJ;
    return (uint8_t) (ROW_BODY + (f - F_BODY0));
}

void form_row(uint8_t f, const char *win, uint8_t curx, uint8_t active)
{
    uint8_t row = row_of(f);
    uint8_t col = (uint8_t) ((f < F_BODY0) ? VAL_COL : BODY_COL);

    scr_put(row, col, win, ui_form_width(f));

    if (active) {
        if (caret_row != 0xFF && caret_row != row)
            scr_put(caret_row, 0, "", 1);
        scr_put(row, 0, G_RIGHT, 1);
        caret_row = row;
        spr_caret(row, (uint8_t) (col + curx));
    } else if (row == caret_row) {
        scr_put(row, 0, "", 1);
        caret_row = 0xFF;
        spr_caret_off();
    }
}

void form_msg(uint8_t msg)
{
    const char *s = "";

    form_ask = (uint8_t) (msg == FM_ASK);
    switch (msg) {
    case FM_ASK:
        s = inp_kbd ? "Send it?   Y or A: send   N or START: discard   B: keep editing"
                    : "Send it?   A: send    START: discard    B: keep editing";
        break;
    case FM_NEEDTO:
        s = "Who is it to? Fill in To first.";
        break;
    case FM_NEEDBODY:
        s = "There is nothing to send yet: type a message.";
        break;
    }
    scr_put(ROW_MSG, 1, s, SCR_COLS - 1);
}

/* ------------------------------------------------------------------ */
/* The controller                                                      */
/* ------------------------------------------------------------------ */

static void move(int8_t dr, int8_t dq)
{
    uint8_t a;

    osk_light(0);
    if (dr) {
        osk_r = (uint8_t) ((osk_r + OSK_ROWS + dr) % OSK_ROWS);
    } else if (osk_r == ACT_ROW) {
        a = act_at(osk_q);
        a = (uint8_t) ((a + N_ACT + dq) % N_ACT);
        osk_q = act_q[a];
    } else {
        osk_q = (uint8_t) ((osk_q + 16 + dq) & 15);
    }
    osk_light(1);
}

static void next_page(void)
{
    page = (uint8_t) ((page + 1) % 3);
    osk_labels();
}

/*
 * One controller press in the form: the key to hand compose.c, or 0 when the
 * press only moved the cursor or turned the page. While the send? question
 * is up, A sends, START discards and B (or anything else) goes back to
 * editing -- compose.c treats any key but yes and no as "not now".
 */
uint8_t form_key(uint8_t ev)
{
    char c;

    if (form_ask) {
        if (ev == EV_A)
            return 'y';
        if (ev == EV_START)
            return E_DONE;
        return E_LEFT;
    }

    switch (ev) {
    case EV_UP:     move(-1, 0); return 0;
    case EV_DOWN:   move(1, 0);  return 0;
    case EV_LEFT:   move(0, -1); return 0;
    case EV_RIGHT:  move(0, 1);  return 0;
    case EV_B:      return E_BS;
    case EV_START:  return E_DONE;
    case EV_SELECT: next_page(); return 0;
    }

    /* EV_A: the key under the cursor */
    if (osk_r != ACT_ROW) {
        c = keys[page][(osk_r << 4) + osk_q];
        return (uint8_t) ((c == ' ') ? 0 : c);
    }

    switch (act_at(osk_q)) {
    case A_SPACE:   return ' ';
    case A_LEFT:    return E_LEFT;
    case A_RIGHT:   return E_RIGHT;
    case A_DEL:     return E_BS;
    case A_NEXT:    return E_ENTER;
    case A_PAGE:    next_page(); return 0;
    case A_SEND:    return E_SAVE;
    }
    return E_DONE;
}

