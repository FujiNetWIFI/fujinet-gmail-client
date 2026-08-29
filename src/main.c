/*
 * FujiNet Gmail client.
 *
 * A read-only inbox browser. There is no authentication anywhere in this
 * program: the FujiNet GMAIL adapter uses the Google grant stored in FujiNet
 * config, the user authorizes once in the FujiNet Web UI, and the firmware
 * handles token refresh. The console never sees a credential -- a 212 back
 * from a fetch simply means "not authorized yet".
 *
 * Ported from the IntyBASIC original in intv/.
 */

#include <fujinet-fuji.h>

#include "gmail.h"

/* UI-owned state; the fetch layer owns the rest (see net.c). */
unsigned char gm_sel;
unsigned char gm_list_valid;

static AdapterConfigExtended ace;
static unsigned int          msg_top;

/*
 * network_init() is a no-op on the Atari and cannot tell us whether a FujiNet
 * is present, so ask the device something only a real one can answer.
 */
static unsigned char have_fujinet(void)
{
#ifdef GM_FAKE_DATA
    return 1;
#else
    unsigned char ok;

    plat_net_begin();
    ok = fuji_get_adapter_config_extended(&ace) ? 1 : 0;
    plat_net_end();
    return ok;
#endif
}

static void read_message(void)
{
    unsigned int maxtop;
    unsigned char k;

    ui_busy(BUSY_BODY);
    if (!gm_fetch_body(gm_index[gm_sel].num)) {
        ui_error(gm_ecode);
        plat_anykey();
        return;
    }

    /* Mark read before rendering, so a reset or a power cut mid-read still
       counts the message as seen. */
    hwm_update(gm_index[gm_sel].ts);

    msg_top = 0;
    ui_message(msg_top);

    for (;;) {
        k = plat_getkey();
        maxtop = (gm_body_rows > MSG_ROWS) ? (gm_body_rows - MSG_ROWS) : 0;

        switch (k) {
        case K_UP:
            if (msg_top) {
                msg_top--;
                ui_message(msg_top);
            }
            break;

        case K_DOWN:
            if (msg_top < maxtop) {
                msg_top++;
                ui_message(msg_top);
            }
            break;

        case K_LEFT:
            if (msg_top) {
                msg_top = (msg_top > MSG_ROWS) ? (msg_top - MSG_ROWS) : 0;
                ui_message(msg_top);
            }
            break;

        case K_RIGHT:
            if (msg_top < maxtop) {
                msg_top += MSG_ROWS;
                if (msg_top > maxtop)
                    msg_top = maxtop;   /* the last page sits flush */
                ui_message(msg_top);
            }
            break;

        case K_BACK:
        case K_ENTER:
            return;                     /* back to the inbox, no refetch */
        }
    }
}

int main(void)
{
    unsigned char land_last = 0;
    unsigned char refetch;
    unsigned char old;
    unsigned char k;

    plat_init();
    ui_splash();

    while (!have_fujinet()) {
        ui_notfound();
        plat_anykey();
        ui_splash();
    }

    /* Before hwm_load() only because both are boot-time device calls and this
       one decides how every row is labelled. Neither can fail the boot: with no
       clock the dates read as UTC, and with no appkey nothing is marked read. */
    clock_load();
    hwm_load();

    gm_range = 0;
    gm_list_valid = 0;

    for (;;) {
        ui_busy(BUSY_INDEX);

        if (gm_fetch_index(gm_range)) {
            hwm_flags();
            gm_list_valid = 1;
            gm_sel = (land_last && gm_count) ? (gm_count - 1) : 0;
        } else {
            ui_error(gm_ecode);
            plat_anykey();
            /* A failed refresh keeps the listing that is still on screen; only
               a cold failure is worth retrying. */
            if (!gm_list_valid)
                continue;
        }
        land_last = 0;

        ui_inbox();

        refetch = 0;
        while (!refetch) {
            k = plat_getkey();
            old = gm_sel;

            switch (k) {
            case K_UP:
                if (gm_sel > 0) {
                    gm_sel--;
                    ui_inbox_sel(old, gm_sel);
                } else if (gm_range >= IDX_MAX) {
                    gm_range -= IDX_MAX;
                    land_last = 1;      /* came off the top -- land on the end */
                    refetch = 1;
                }
                break;

            case K_DOWN:
                if (gm_count && gm_sel + 1 < gm_count) {
                    gm_sel++;
                    ui_inbox_sel(old, gm_sel);
                } else if (gm_next) {
                    gm_range += IDX_MAX;
                    refetch = 1;
                }
                break;

            case K_LEFT:
                if (gm_range >= IDX_MAX) {
                    gm_range -= IDX_MAX;
                    refetch = 1;
                }
                break;

            case K_RIGHT:
                if (gm_next) {
                    gm_range += IDX_MAX;
                    refetch = 1;
                }
                break;

            case K_REFRESH:
                gm_range = 0;
                refetch = 1;
                break;

            case K_ENTER:
                if (gm_count) {
                    read_message();
                    hwm_flags();
                    ui_inbox();
                }
                break;

            case K_QUIT:
                plat_shutdown();
                return 0;
            }
        }
    }
}
