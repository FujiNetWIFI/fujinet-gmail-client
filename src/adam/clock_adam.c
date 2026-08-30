/*
 * clock_get_time() for the Adam.
 *
 * fujinet-lib has no fn_clock at all on this bus. The CoCo archive carries
 * clock_get_time and not clock_get_tz; the Adam archive carries neither --
 * adam/src/ has fn_network/ and fn_fuji/ and nothing else. src/clock.c makes
 * exactly one call, and this is it.
 *
 * The firmware does answer it. lib/device/adamnet/adamClock.cpp registers
 * platformClock at FUJI_DEVICEID::CLOCK, which on AdamNet is device 0x03, and
 * fujiClock::processCommand() dispatches APETIME_GET_ISO_LOCAL (0x49, 'I') to
 * get_iso_local() -> send_string(get_current_time_iso(...)). send_string()
 * appends a NUL, so what comes back is
 *
 *      YYYY-MM-DDTHH:MM:SS+HHMM\0
 *
 * resolved through the FujiNet's [General] timezone -- twenty-five bytes, and
 * exactly the buffer src/clock.c already parses and validates. So the Adam gets
 * real local dates rather than the "UTC without a clock" fallback, and the
 * portable half needs no #ifdef.
 *
 * The sibling calendar client asks the *Fuji* device for FUJI_GET_TIME (0xD2)
 * instead, because SIMPLE_BINARY is all its src/clock.c wants. That form is
 * local wall-clock with no offset in it, which is no use here: this client
 * needs the trailing +HHMM to turn the wire's UTC epoch seconds into a local
 * date column, and there is nothing to difference it against.
 *
 * Two bytes go out, not one. APETIME_GET_ISO_LOCAL is in
 * fujiClock::command_takes_alt(), so the device reads param 0 to decide whether
 * an alternate timezone was requested; sending the zero explicitly is cheaper
 * than reasoning about what it reads when the payload is one byte long.
 *
 * No retry on ADAMNET_TIMEOUT, which is what fujinet-lib's own adam/ calls wrap
 * these in. It cannot happen: eos_write_character_device() restarts itself
 * internally until the device settles and only ever returns a settled status,
 * so the retry is unreachable and only reads as though a missing adapter were
 * handled. See the header comment in fuji_adam.c.
 *
 * Defining the symbol here leaves nothing in the archive to collide with, the
 * same way fuji_adam.c leaves the library's member unreferenced.
 */

#include <stdint.h>
#include <eos.h>
#include <string.h>

#include <fujinet-network.h>
#include <fujinet-clock.h>

#define CLOCK_DEV       0x03    /* FUJI_DEVICEID::CLOCK on AdamNet */
#define APETIME_ISO_LOC 0x49    /* fujiCommandID.h, 'I' */

/* 24 characters and the NUL send_string() appends. */
#define ISO_LEN         25

/* adam/src/bus/response.c. Not in the shipped headers, but it is in the archive
   and every fuji_* call in it uses this buffer -- borrowing it is both correct
   and a kilobyte cheaper than a second one. */
extern unsigned char response[1024];

uint8_t clock_get_time(uint8_t *time_data, TimeFormat format)
{
    unsigned char cmd[2];
    uint8_t err;

    /*
     * Only the ISO form is answered. The other TimeFormat values are separate
     * clock-device commands, and claiming to support them would mean handing
     * back 0x49's reply under a different name.
     */
    if (format != TZ_ISO_STRING)
        return FN_ERR_BAD_CMD;

    cmd[0] = APETIME_ISO_LOC;
    cmd[1] = 0x00;              /* use_alt: the system timezone, not an override */

    err = eos_write_character_device(CLOCK_DEV, cmd, sizeof(cmd));
    if (err != ADAMNET_OK)
        return FN_ERR_IO_ERROR;

    err = eos_read_character_device(CLOCK_DEV, response, sizeof(response));
    if (err != ADAMNET_OK)
        return FN_ERR_IO_ERROR;

    memcpy(time_data, response, ISO_LEN);

    return FN_ERR_OK;
}
