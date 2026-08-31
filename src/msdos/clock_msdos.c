/*
 * clock_get_time() for MS-DOS.
 *
 * fujinet-lib has no fn_clock at all on this bus. msdos/src/ has bus/,
 * fn_fuji/ and fn_network/ and nothing else, so without this file the link
 * fails on the one call src/clock.c makes -- the same hole src/adam/
 * clock_adam.c fills for AdamNet, and the same fix: defining the symbol here
 * leaves nothing in the archive to collide with.
 *
 * The firmware does answer it. The clock is FUJI_DEVICEID::CLOCK -- device
 * 0x45, the byte FUJINET.SYS passes through in AL untouched, and the same
 * device fujinet-msdos's own FUJITIME.EXE reads binary time from -- and
 * APETIME_GET_ISO_LOCAL (0x49, 'I') answers with
 *
 *      YYYY-MM-DDTHH:MM:SS+HHMM\0
 *
 * resolved through the FujiNet's [General] timezone: twenty-five bytes, and
 * exactly the buffer src/clock.c already parses and validates. The command
 * byte and reply length are the atari fn_clock's own table for TimeFormat
 * index 3 (clock_get_time.s), which is the byte the Adam shim sends too.
 * aux1 is the use_alt flag: zero asks for the system timezone rather than an
 * override, sent explicitly for the same reason the Adam sends its second
 * command byte.
 *
 * FUJITIME.EXE is why this program does not read the DOS clock instead: the
 * driver disk sets DOS time *from* the FujiNet, so INT 21h would hand back
 * the same wall clock -- but with the +HHMM offset stripped, and the offset
 * is the entire point. The wire's timestamps are UTC epoch seconds and
 * date.c needs the local offset to place them; local wall-clock alone has
 * nothing to difference against.
 *
 * If the RS-232 firmware build has no clock registered, the driver times the
 * call out and this returns an error -- which clock_load() treats as "no
 * clock": dates render in UTC and everything else works. That is the
 * degradation src/clock.c was designed around, not a failure path added
 * here.
 *
 * Delete this file when fujinet-lib grows msdos/src/fn_clock/.
 */

#include <stdint.h>

#include <fujinet-network.h>
#include <fujinet-clock.h>

#include "platform.h"

#define CLOCK_DEV       0x45    /* FUJI_DEVICEID::CLOCK                  */
#define APETIME_ISO_LOC 0x49    /* fujiCommandID.h, 'I'                  */

/* 24 characters and the NUL the firmware appends. */
#define ISO_LEN         25

uint8_t clock_get_time(uint8_t *time_data, TimeFormat format)
{
    /*
     * Only the ISO form is answered. The other TimeFormat values are
     * separate clock-device commands, and claiming to support them would
     * mean handing back 0x49's reply under a different name.
     */
    if (format != TZ_ISO_STRING)
        return FN_ERR_BAD_CMD;

    if (int_f5_read(CLOCK_DEV, APETIME_ISO_LOC, 0x00, 0x00,
                    time_data, ISO_LEN) != 'C')
        return FN_ERR_IO_ERROR;

    return FN_ERR_OK;
}
