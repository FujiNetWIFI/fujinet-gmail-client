/*
 * The date column.
 *
 * The wire has always carried a timestamp and the 40-column screen has always
 * thrown it away. At 80 there is room for it, and it is worth the arithmetic:
 * it is the column every real mail client leads with, and it is also the only
 * thing that makes the local read/unread high-water mark legible -- see the
 * unread rule in hwm.c, which is a comparison against exactly this value.
 *
 * The timestamp is epoch SECONDS, not milliseconds. Gmail's own internalDate
 * field is milliseconds and the firmware divides by 1000 before it reaches the
 * wire (GMAIL.cpp, format_index_raw's caller). Getting that wrong renders every
 * row as 1970 and reads like a wire bug rather than a client one.
 *
 * Only the low four bytes are used. The field is a uint64 but the top half
 * stays zero until 2106-02-07, and a 32-bit divide is already a runtime call on
 * a 6502 -- a 64-bit one would be four. A nonzero top half is treated as a
 * corrupt record rather than a date in the far future.
 *
 * Pure: no platform, no network, no device call. tests/hosttest.c covers it.
 */

#include <string.h>

#include "gmail.h"

static const char mon3[12][4] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

/* Two digits, zero padded, straight into dst[0..1]. */
static void num2(char *dst, unsigned char v)
{
    dst[0] = (char) ('0' + (v / 10) % 10);
    dst[1] = (char) ('0' + v % 10);
}

/*
 * Days since 1970-01-01 to a civil date.
 *
 * Howard Hinnant's civil_from_days, shifted to an era beginning on 0000-03-01
 * so that the leap day lands at the end of a 146097-day era and every month
 * length except the last is regular. That is what lets the whole thing be four
 * divisions and no table, and what makes 2100 come out correctly as not a leap
 * year without the rule appearing anywhere.
 *
 * The 719468 is the offset from that era's start to the Unix epoch. Everything
 * stays unsigned: days is bounded by the uint32 epoch, so the era can never go
 * negative and 1970 is comfortably inside the first one.
 *
 * int is sixteen bits here, so the two quantities that do not fit in one are
 * long and stay long: doe runs to 146096, and 365 * yoe to 145635. Truncating
 * either gives a date that is plausible, wrong, and wrong only some of the
 * time -- which is the worst kind of wrong to go looking for.
 */
static void civil_from_days(unsigned long days, unsigned int *y,
                            unsigned char *mo, unsigned char *d)
{
    unsigned long era;
    unsigned long doe;          /* day of era,   0..146096 */
    unsigned int  yoe;          /* year of era,  0..399    */
    unsigned int  doy;          /* day of year, from March */
    unsigned char mp;           /* month, March = 0        */

    days += 719468UL;

    era = days / 146097UL;
    doe = days - era * 146097UL;

    yoe = (unsigned int) ((doe - doe / 1460UL + doe / 36524UL
                               - doe / 146096UL) / 365UL);
    doy = (unsigned int) (doe - (365UL * yoe + yoe / 4 - yoe / 100));
    mp  = (unsigned char) ((5 * doy + 2) / 153);

    *d  = (unsigned char) (doy - (153 * mp + 2) / 5 + 1);
    *mo = (unsigned char) (mp < 10 ? mp + 3 : mp - 9);
    *y  = (unsigned int) (yoe + (unsigned int) (era * 400UL) + (*mo <= 2));
}

/*
 * Render ts into ENT_DATE_LEN bytes.
 *
 * "Aug 28 14:32" for a message in the current year, "Aug 28  2024" otherwise --
 * the same trade every mail client makes, because the time of a message from
 * two years ago tells you nothing and the year tells you everything. With no
 * clock (gm_year == 0) there is no current year to compare against, so
 * everything gets the time form.
 *
 * A rejected timestamp gives an empty string rather than a wrong date. Callers
 * pad the column, so that reads as a blank.
 */
void date_fmt(char *dst, const uint8_t ts[8])
{
    unsigned long secs;
    unsigned long days;
    unsigned long rem;          /* seconds into the day, 0..86399 */
    unsigned int  y;
    unsigned char mo, d, h, mi;
    long          off;

    dst[0] = '\0';

    /* Above the low four bytes is a date past 2106, which this wire cannot
       legitimately carry -- so it is a corrupt record, not the future. */
    if (ts[4] || ts[5] || ts[6] || ts[7])
        return;

    secs = (unsigned long) ts[0]
         | ((unsigned long) ts[1] << 8)
         | ((unsigned long) ts[2] << 16)
         | ((unsigned long) ts[3] << 24);

    if (secs == 0)
        return;

    /*
     * Into local time, in unsigned arithmetic throughout. Going via a signed
     * long would be shorter and would overflow in 2038, which is exactly the
     * kind of bug that ships. A west-of-UTC offset can only carry the date
     * below the epoch for a timestamp in the first hours of 1970, which is the
     * same "not a real message" case as above.
     */
    off = (long) gm_tzoff * 60L;
    if (off < 0) {
        if (secs < (unsigned long) -off)
            return;
        secs -= (unsigned long) -off;
    } else {
        secs += (unsigned long) off;
    }

    days = secs / 86400UL;
    rem  = secs - days * 86400UL;
    h    = (unsigned char) (rem / 3600UL);
    mi   = (unsigned char) ((rem - (unsigned long) h * 3600UL) / 60UL);

    civil_from_days(days, &y, &mo, &d);

    if (mo < 1 || mo > 12)
        return;

    memcpy(dst, mon3[mo - 1], 3);
    dst[3] = ' ';
    num2(dst + 4, d);
    dst[6] = ' ';

    if (gm_year && y != gm_year) {
        dst[7] = ' ';
        num2(dst + 8, (unsigned char) (y / 100));
        num2(dst + 10, (unsigned char) (y % 100));
        dst[12] = '\0';
        return;
    }

    num2(dst + 7, h);
    dst[9] = ':';
    num2(dst + 10, mi);
    dst[12] = '\0';
}
