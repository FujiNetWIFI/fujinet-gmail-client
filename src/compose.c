/*
 * The compose screen: one blocking loop, in the house style -- no state
 * table, control flow is the call stack. form.c owns the data and the wire
 * format; the backends own the paint; this file owns the cursor.
 *
 * The editor is modeless. One field is active; printable keys insert at the
 * cursor, E_BS deletes before it, E_UP/E_DOWN/E_ENTER move between fields,
 * and E_DONE leaves -- silently when nothing was touched, through a send?
 * yes/no ask otherwise. Scrolling lives here in both directions, not in the
 * backends: ui_form_row() is handed the visible slice with the cursor
 * column already computed, so a backend paints what it is given and cannot
 * disagree with the engine about where the window starts.
 *
 * Vertically that means the form holds FRM_NBODY body lines and shows
 * FRM_VBODY of them, starting at btop. A backend lays out FRM_VBODY rows and
 * is told which *slot* to paint, never which line -- so growing the body
 * costs a constant in the Makefile and nothing anywhere else.
 *
 * The network is touched only at send time. A cancel costs nothing, and a
 * reply's target is resolved by the adapter at that open, as late as
 * possible. A failed send comes back to the form with every field intact --
 * nothing the user typed is ever thrown away on an error.
 */

#include <string.h>

#include "gmail.h"

static unsigned char cur_f;     /* the active field */
static unsigned char cpos;      /* cursor position within it */
static unsigned char btop;      /* first body line in the window */
static unsigned char msg_up;    /* a ui_form_msg() is on screen */

/* try_send outcomes. */
#define SV_STAY     0           /* validation or network failure -- still here */
#define SV_SENT     1

/*
 * Field f's screen slot, or FRM_NFIELDS when it is scrolled out of the body
 * window. The headers are always on screen and always their own slot; a body
 * line is a slot only while it is inside [btop, btop + FRM_VBODY).
 */
static unsigned char slot_of(unsigned char f)
{
    unsigned char b;

    if (f < F_BODY0)
        return f;

    b = (unsigned char) (f - F_BODY0);
    if (b < btop || (unsigned char) (b - btop) >= FRM_VBODY)
        return FRM_NFIELDS;

    return (unsigned char) (F_BODY0 + (b - btop));
}

static void draw_row(unsigned char f)
{
    const char   *s = form_field_ptr(f);
    unsigned char slot = slot_of(f);
    unsigned char w;
    unsigned char act = (unsigned char) (f == cur_f);
    unsigned char off = 0;
    unsigned char n;

    if (slot == FRM_NFIELDS)
        return;                 /* scrolled out -- nothing to paint it into */

    w = ui_form_width(slot);

    if (act && cpos >= w)
        off = (unsigned char) (cpos - w + 1);

    /* frm.line doubles as the echo scratch: emission and echo never
       overlap, and it is comfortably longer than the widest window. */
    s += off;
    n = (unsigned char) strlen(s);
    if (n > w)
        n = w;
    memcpy(frm.line, s, n);
    frm.line[n] = '\0';

    ui_form_row(slot, frm.line, act ? (unsigned char) (cpos - off) : 0, act);
}

/* Every visible body row. A scroll moves all of them and a spill can rewrite
   several, so there is nothing to be gained by tracking which ones changed. */
static void draw_body(void)
{
    unsigned char i;

    for (i = 0; i < FRM_VBODY; i++)
        draw_row((unsigned char) (F_BODY0 + btop + i));
}

static void draw_all(void)
{
    draw_row(F_TO);
    draw_row(F_SUBJ);
    draw_body();
}

/*
 * Bring field f into the body window. Returns 1 when the window moved, which
 * is when the whole body has to be repainted rather than the row or two the
 * cursor touched. btop is clamped so the window never runs off the end of
 * storage, which is what lets draw_body() paint FRM_VBODY rows unconditionally.
 */
static unsigned char scroll_to(unsigned char f)
{
    unsigned char b;
    unsigned char top = btop;

    if (f < F_BODY0)
        return 0;               /* the headers are always on screen */

    b = (unsigned char) (f - F_BODY0);
    if (b < top)
        top = b;
    else if ((unsigned char) (b - top) >= FRM_VBODY)
        top = (unsigned char) (b - FRM_VBODY + 1);

    if (top == btop)
        return 0;

    btop = top;
    return 1;
}

static void clear_msg(void)
{
    if (msg_up) {
        ui_form_msg(FM_NONE);
        msg_up = 0;
    }
}

/* Activate a field, cursor at the end -- which is where a prefilled value
   wants to be appended to and an empty one is the same as the start. */
static void set_field(unsigned char f)
{
    unsigned char old = cur_f;

    cur_f = f;
    cpos = (unsigned char) strlen(form_field_ptr(f));

    if (scroll_to(f)) {
        /* The window moved, so every body row is wrong. A header the cursor
           just left is not in it and still has an inverse bar to lose. */
        draw_body();
        if (old < F_BODY0)
            draw_row(old);
        return;
    }

    if (old != f)
        draw_row(old);
    draw_row(f);
}

/*
 * A full field is where the wrap lives. The headers scroll instead: an address
 * or a subject is one line by definition and there is nowhere for a second one
 * to go. A body line pushes its trailing word down and, if the cursor was in
 * that word, follows it -- which is what makes typing straight through the
 * right margin do the obvious thing.
 *
 * The loop is for the case where the word lands on a line that is itself now
 * full: each pass either frees room on the line the cursor is on or moves the
 * cursor one line further down, so it always reaches a line with room or the
 * bottom of the body.
 */
static void ins_ch(char c)
{
    char         *s = form_field_ptr(cur_f);
    unsigned char len = (unsigned char) strlen(s);
    unsigned char spilled = 0;

    while (len >= form_field_max(cur_f)) {
        unsigned char ln;

        if (cur_f < F_BODY0)
            return;

        ln = (unsigned char) (cur_f - F_BODY0);
        if (!form_body_spill(&ln, &cpos)) {
            if (spilled)
                draw_body();    /* an earlier pass did move text */
            return;
        }

        frm_dirty[cur_f] = 1;   /* the line the word left changed too */
        cur_f = (unsigned char) (F_BODY0 + ln);
        (void) scroll_to(cur_f);
        spilled = 1;

        s = form_field_ptr(cur_f);
        len = (unsigned char) strlen(s);
    }

    s += cpos;
    memmove(s + 1, s, (unsigned char) (len - cpos + 1));
    *s = c;
    cpos++;

    frm_dirty[cur_f] = 1;

    if (spilled)
        draw_body();
    else
        draw_row(cur_f);
}

static void del_ch(void)
{
    char *s;

    if (cpos == 0)
        return;

    s = form_field_ptr(cur_f) + cpos;
    memmove(s - 1, s, (unsigned char) (strlen(s) + 1));
    cpos--;

    frm_dirty[cur_f] = 1;
    draw_row(cur_f);
}

static unsigned char try_send(void)
{
    unsigned char bad;
    unsigned char code = form_validate(&bad);

    if (code != FM_NONE) {
        set_field(bad);
        ui_form_msg(code);
        msg_up = 1;
        return SV_STAY;
    }

    ui_busy(BUSY_SEND);

    if (!gm_send_begin((unsigned char) (frm_mode == FRM_REPLY),
                       gm_index[gm_sel].num))
        goto failed;

    /* Validation guarantees the draft is worth a commit, so there is no
       nothing-to-send branch here: emit, then close -- the close is the
       send. Write failures are latched in net.c and report from the end. */
    form_emit();

    if (!gm_send_end())
        goto failed;

    ui_sent();
    plat_anykey();
    return SV_SENT;

failed:
    ui_error(gm_ecode);
    plat_anykey();
    ui_form(frm_mode);
    draw_all();
    return SV_STAY;
}

static void runform(unsigned char mode)
{
    unsigned char c;

    form_init(mode);

    /* A reply's TO and SUBJECT default at the adapter, so the body is where
       typing starts there; everywhere else the recipient comes first. */
    cur_f = (unsigned char) ((mode == FRM_REPLY) ? F_BODY0 : F_TO);
    cpos = (unsigned char) strlen(form_field_ptr(cur_f));
    btop = 0;
    msg_up = 0;

    ui_form(mode);
    draw_all();

    for (;;) {
        c = plat_getch();

        if (c >= 0x20 && c < 0x7F) {
            clear_msg();
            ins_ch((char) c);
            continue;
        }

        switch (c) {
        case E_BS:
            clear_msg();
            del_ch();
            break;

        case E_LEFT:
            if (cpos) {
                cpos--;
                draw_row(cur_f);
            }
            break;

        case E_RIGHT:
            if (cpos < strlen(form_field_ptr(cur_f))) {
                cpos++;
                draw_row(cur_f);
            }
            break;

        case E_UP:
            clear_msg();
            set_field((unsigned char)
                      (cur_f ? cur_f - 1 : FRM_NFIELDS - 1));
            break;

        case E_DOWN:
        case E_ENTER:
            clear_msg();
            set_field((unsigned char)
                      (cur_f + 1 < FRM_NFIELDS ? cur_f + 1 : 0));
            break;

        case E_SAVE:
            clear_msg();
            if (try_send() == SV_SENT)
                return;
            break;

        case E_DONE:
            if (!form_any_dirty())
                return;

            ui_form_msg(FM_ASK);
            msg_up = 1;

            for (;;) {
                c = plat_getch();

                if (c == 'y' || c == 'Y' || c == E_SAVE) {
                    clear_msg();
                    if (try_send() == SV_SENT)
                        return;
                    break;      /* failed -- back to editing */
                }
                if (c == 'n' || c == 'N' || c == E_DONE)
                    return;

                /* Anything else: the ask was a misfire, keep editing. */
                clear_msg();
                break;
            }
            break;
        }
    }
}

void compose_new(void)
{
    runform(FRM_COMPOSE);
}

void compose_reply(void)
{
    runform(FRM_REPLY);
}

void compose_forward(void)
{
    runform(FRM_FWD);
}
