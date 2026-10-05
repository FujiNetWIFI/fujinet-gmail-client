/*
 * tramp.c -- gmail.h's names, in the fixed half, for code that lives in the
 * banks.
 *
 * The portable core calls ui_inbox(), gm_fetch_index() and the rest by their
 * plain names and knows nothing of banks. Each of those names is defined
 * here as a one-line call through a wrapped-call prototype (platform.h, and
 * the NET block below), which is what maps the right bank on the way in and
 * puts the caller's back on the way out (bank_tramp, crt0.s).
 *
 * src/net.c, clock.c and hwm.c are compiled into the NET bank with their
 * entry points renamed nb_* by the Makefile (NET_RENAMES), so these
 * definitions are the only ones the linker sees under the real names.
 */

#include <string.h>

#include <fujinet-nes.h>

#include "../gmail.h"
#include "platform.h"

/* ------------------------------------------------------------------ */
/* NET: src/net.c, clock.c, hwm.c                                      */
/* ------------------------------------------------------------------ */

#pragma wrapped-call (push, bank_tramp, BANK_NET)
unsigned char nb_fetch_index(unsigned long range);
unsigned char nb_fetch_body(const char *msgnum);
unsigned char nb_send_begin(unsigned char reply, const char *msgnum);
void          nb_send_put(const char *line);
unsigned int  nb_send_room(void);
unsigned char nb_send_end(void);
void          nb_clock_load(void);
void          nb_hwm_load(void);
void          nb_hwm_flags(void);
void          nb_hwm_update(const uint8_t *ts);
#pragma wrapped-call (pop)

unsigned char gm_fetch_index(unsigned long range)  { return nb_fetch_index(range); }
unsigned char gm_fetch_body(const char *msgnum)    { return nb_fetch_body(msgnum); }
unsigned char gm_send_begin(unsigned char reply, const char *msgnum)
{
    return nb_send_begin(reply, msgnum);
}
void          gm_send_put(const char *line)         { nb_send_put(line); }
unsigned int  gm_send_room(void)                    { return nb_send_room(); }
unsigned char gm_send_end(void)                     { return nb_send_end(); }
void          clock_load(void)                      { nb_clock_load(); }
void          hwm_load(void)                        { nb_hwm_load(); }
void          hwm_flags(void)                       { nb_hwm_flags(); }
void          hwm_update(const uint8_t *ts)         { nb_hwm_update(ts); }

/* ------------------------------------------------------------------ */
/* The screens                                                         */
/* ------------------------------------------------------------------ */

void plat_init(void)        { boot_init(); }

/*
 * The splash is up while the cartridge brings its link to the FujiNet up,
 * which takes a moment after power-on -- and main()'s first question to the
 * device would otherwise go out before it can be answered, and put up "No
 * FujiNet found" for a FujiNet that is about to be there. So it waits here,
 * dots bobbing, for the link bit in the cartridge's status byte (a read: the
 * mailbox only minds writes), up to LINK_WAIT seconds. On a cartridge that
 * is not a FujiNet the status is open bus, and fuji_nes_present() says so
 * at once.
 */
#define FN_STATUS       (*(volatile uint8_t *) 0x5401)
#define FN_STATUS_LINK  0x01
#define LINK_WAIT       15

void ui_splash(void)
{
    unsigned int n;

    boot_flat(FLAT_SPLASH, 0, 0);
    if (!fuji_nes_present())
        return;
    for (n = LINK_WAIT * 60; n && !(FN_STATUS & FN_STATUS_LINK); --n)
        scr_frame();
}
void ui_notfound(void)      { boot_flat(FLAT_NOTFOUND, 0, 0); }
void ui_busy(unsigned char reason) { boot_flat(reason, 0, 0); }
void ui_sent(void)          { boot_flat(FLAT_SENT, 0, 0); }

/*
 * gm_stage points at a literal in net.c, which is in the NET bank -- and the
 * error screen is drawn from BOOT, in the same window. So the words come
 * across here first, with NET mapped for as long as it takes to copy them.
 */
void ui_error(unsigned char code)
{
    static char stage[12];
    uint8_t     was = cur_bank;

    prg_r6(BANK_NET);
    strncpy(stage, gm_stage ? gm_stage : "", sizeof(stage) - 1);
    prg_r6(was);
    boot_flat(FLAT_ERROR, code, stage);
}

void ui_inbox(void)                                 { inbox_paint(); }
void ui_inbox_sel(unsigned char from, unsigned char to) { inbox_sel(from, to); }
void ui_message(unsigned int top)                   { reader_paint(top); }

void ui_form(unsigned char mode)                    { form_paint(mode); }
void ui_form_msg(unsigned char msg)                 { form_msg(msg); }
void ui_form_row(unsigned char f, const char *win,
                 unsigned char curx, unsigned char active)
{
    form_row(f, win, curx, active);
}

/* The form's windows: TO and SUBJECT after their labels, the body the whole
   width but the marker column. A function rather than a table so compose.c
   (in R7) can call it without a bank switch. */
unsigned char ui_form_width(unsigned char f)
{
    return (unsigned char) ((f < F_BODY0) ? 54 : 63);
}

/* There is nowhere to go back to: the NES has no shell. Quitting starts the
   program over, which is what the console's own RESET does too. */
void plat_shutdown(void)
{
}
