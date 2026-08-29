/*
 * Everything date.c needs from the device, read once at boot.
 *
 * Two values: the local UTC offset, and the current year. Neither changes often
 * enough to be worth a second device call, and both are cosmetic -- a mailbox
 * with no clock still browses perfectly, it just labels everything in UTC and
 * gives every message a time rather than a year. That is why this never fails
 * the boot the way the calendar client's clock does; there, every device spec
 * names a date and there is no sensible fallback for "today".
 *
 * The offset comes from TZ_ISO_STRING, which hands back
 *
 *      YYYY-MM-DDTHH:MM:SS+HHMM
 *
 * already resolved through the FujiNet's [General] timezone -- so the trailing
 * +HHMM *is* the offset, including whatever DST rule was in force, and there is
 * no POSIX TZ parser anywhere in this program. clock_get_tz() would give us the
 * rule instead of the answer ("CST6CDT"), which is strictly more work for
 * strictly less; it also copies a length byte's worth of bytes without
 * terminating them.
 */

#include <fujinet-clock.h>

#include "gmail.h"

int          gm_tzoff;          /* minutes east of UTC */
unsigned int gm_year;           /* 0 = unknown */

/*
 * 26, not 25.
 *
 * The device returns 25 bytes (24 characters and a NUL) and clock_get_time's
 * copy loop does not clamp to the caller's buffer -- worse, it compares only
 * the low byte of a two-byte count, so a count of 0 or 256 walks Y all the way
 * around and stores 256 bytes. Nothing can be done about that from C, so the
 * buffer is a file-scope array with the shape check below standing between it
 * and anything that trusts the contents.
 */
static unsigned char iso[26];

static unsigned char digit(unsigned char c)
{
    return (unsigned char) (c - '0');
}

static unsigned int num2(const unsigned char *p)
{
    return (unsigned int) digit(p[0]) * 10 + digit(p[1]);
}

/* Every byte the parse below relies on, in the one place it can be checked. */
static unsigned char well_formed(void)
{
    unsigned char i;

    if (iso[4] != '-' || iso[7] != '-' || iso[10] != 'T' ||
        iso[13] != ':' || iso[16] != ':')
        return 0;

    if (iso[19] != '+' && iso[19] != '-')
        return 0;

    /* The digit positions, so a NAK body of plausible punctuation cannot get
       this far and come out as a real offset. */
    for (i = 0; i < 24; i++) {
        if (i == 4 || i == 7 || i == 10 || i == 13 || i == 16 || i == 19)
            continue;
        if (iso[i] < '0' || iso[i] > '9')
            return 0;
    }

    return 1;
}

void clock_load(void)
{
    unsigned int y, off;
    unsigned char i;

    gm_tzoff = 0;
    gm_year = 0;

    for (i = 0; i < sizeof(iso); i++)
        iso[i] = 0;

#ifdef GM_FAKE_DATA
    /* A headless run has no device, and a date column of nothing but blanks
       would make the one new screen element untestable. */
    {
        static const char canned[] = "2026-08-29T10:15:00-0500";
        for (i = 0; i < sizeof(canned) - 1; i++)
            iso[i] = (unsigned char) canned[i];
    }
#else
    plat_net_begin();
    if (clock_get_time(iso, TZ_ISO_STRING) != FN_ERR_OK) {
        plat_net_end();
        return;
    }
    plat_net_end();
#endif

    if (!well_formed())
        return;

    y = num2(iso) * 100 + num2(iso + 2);
    if (y < 2000 || y > 2199)
        return;

    off = num2(iso + 20) * 60 + num2(iso + 22);

    /* Fourteen hours is the widest real offset there is (Kiritimati). Anything
       past it is a misparse, and the year is worth keeping even then. */
    if (off <= 14 * 60)
        gm_tzoff = (iso[19] == '-') ? -(int) off : (int) off;

    gm_year = y;
}
