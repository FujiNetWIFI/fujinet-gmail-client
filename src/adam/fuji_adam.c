/*
 * fuji_get_adapter_config_extended() for the Adam, replacing fujinet-lib's.
 *
 * The library's Adam version is declared bool and returns fujiError_t codes:
 *
 *     if (err != ADAMNET_OK) return FN_ERR_IO_ERROR;      // 0x01 -> true
 *     ...
 *     return FN_ERR_OK;                                   // 0x00 -> false
 *
 * FN_ERR_OK is zero, so the answer is inverted: the call reports false when it
 * worked and true when it did not. main.c's have_fujinet() probes with this
 * function precisely because it is something only a real adapter can answer, so
 * the symptom is a FujiNet that is present, answering, and logging
 * "Fuji cmd: GET ADAPTER CONFIG EXTENDED" while the client sits on the splash
 * screen insisting it is not there.
 *
 * It is not one function. Thirty-seven of the bool-returning entry points in
 * fujinet-lib's adam/src/fn_fuji/ return FN_ERR_* the same way -- the whole
 * backend was written to the uint8_t convention the network_* half uses and
 * then declared with the fuji_* half's. This client calls exactly one of the
 * thirty-seven; fuji_read_appkey(), fuji_write_appkey() and
 * fuji_set_appkey_details(), which src/hwm.c needs, are among the correct ones
 * and are left to the library.
 *
 * Defining the symbol here keeps the library member unreferenced, so the linker
 * never pulls it and there is no duplicate.
 * *** Delete this file once fujinet-lib's Adam fuji_* return real booleans. ***
 *
 * What this does NOT fix is the behaviour with nothing on the bus. The library
 * version wraps each call in `while (1) { if (err == ADAMNET_TIMEOUT) continue; }`,
 * which looks like the reason a missing adapter hangs the client on the splash
 * screen -- but that loop is unreachable. eoslib spins one level further down:
 *
 *     unsigned char eos_write_character_device(...)
 *     {
 *       while (1) {
 *         r = eos_start_write_character_device(dev, buf, len);
 *         while ((r = eos_end_write_character_device(dev)) < 0x80);
 *         if (r == 0x9B) continue;        // timed out -- start over, forever
 *         else break;
 *       }
 *       return r;
 *     }
 *
 * So ADAMNET_TIMEOUT never reaches any caller on this bus, and ui_notfound() is
 * unreachable without a device-presence check that does not go through
 * eos_write_character_device() at all. That is a separate piece of work and it
 * belongs in eoslib or fujinet-lib, not here. Every Adam FujiNet client shares
 * it.
 */

#include <stdbool.h>
#include <stdint.h>
#include <eos.h>
#include <string.h>

#include <fujinet-fuji.h>

#define FUJI_DEV        0x0F    /* FUJI_DEVICEID::FUJINET on AdamNet */
#define FUJI_CFG_EXT    0xC4    /* fujiCommandID.h */

/* adam/src/bus/response.c -- see the note in clock_adam.c. */
extern unsigned char response[1024];

/*
 * One write, one read, and a real bool. No retry loop: the only way either call
 * returns at all is with a settled status, per the header comment.
 */
bool fuji_get_adapter_config_extended(AdapterConfigExtended *ac)
{
    unsigned char cmd = FUJI_CFG_EXT;

    if (eos_write_character_device(FUJI_DEV, &cmd, 1) != ADAMNET_OK)
        return false;

    if (eos_read_character_device(FUJI_DEV, response, sizeof(response))
            != ADAMNET_OK)
        return false;

    if (ac)
        memcpy(ac, response, sizeof(AdapterConfigExtended));

    return true;
}
