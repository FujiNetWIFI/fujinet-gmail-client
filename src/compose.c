/*
 * The compose screen: one blocking loop, in the house style -- no state
 * table, control flow is the call stack. form.c owns the data and the wire
 * format; the backends own the paint; this file owns the cursor.
 *
 * The editor is modeless. One field is active; printable keys insert at the
 * cursor, E_BS deletes before it, E_UP/E_DOWN/E_ENTER move between fields,
 * and E_DONE leaves -- silently when nothing was touched, through a send?
 * yes/no ask otherwise. Horizontal scrolling lives here, not in the
 * backends: ui_form_row() is handed the visible slice with the cursor
 * column already computed, so a backend paints what it is given and cannot
 * disagree with the engine about where the window starts.
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
static unsigned char msg_up;    /* a ui_form_msg() is on screen */

/* try_send outcomes. */
#define SV_STAY     0           /* validation or network failure -- still here */
#define SV_SENT     1

static void draw_row(unsigned char f)
{
    const char   *s = form_field_ptr(f);
    unsigned char w = ui_form_width(f);
    unsigned char act = (unsigned char) (f == cur_f);
    unsigned char off = 0;
    unsigned char n;

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

    ui_form_row(f, frm.line, act ? (unsigned char) (cpos - off) : 0, act);
}

static void draw_all(void)
{
    unsigned char f;

    for (f = 0; f < FRM_NFIELDS; f++)
        draw_row(f);
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
    if (old != f)
        draw_row(old);
    draw_row(f);
}

static void ins_ch(char c)
{
    char         *s = form_field_ptr(cur_f);
    unsigned char len = (unsigned char) strlen(s);

    if (len >= form_field_max(cur_f))
        return;

    s += cpos;
    memmove(s + 1, s, (unsigned char) (len - cpos + 1));
    *s = c;
    cpos++;

    frm_dirty[cur_f] = 1;
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
