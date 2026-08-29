/*
 * Read/unread high-water mark.
 *
 * The Gmail adapter exposes no read/unread state, so this is entirely local: a
 * single 8-byte little-endian timestamp persisted in a FujiNet appkey. A
 * message is unread when its timestamp is strictly newer than the mark.
 *
 * The consequence is inherited from the Intellivision version and is
 * deliberate rather than a bug: opening an older message marks everything
 * older than it read too, including newer messages you skipped past.
 */

#include <string.h>

#include <fujinet-fuji.h>

#include "gmail.h"

#define AK_CREATOR  0x474D      /* "GM" */
#define AK_APP      1
#define AK_KEY      0

static uint8_t hwm[8];

/*
 * 66 for an 8-byte key, and both buses have their own reason for it.
 *
 * SIO: fuji_read_appkey reads keysize + 2 and then memmoves the 2-byte count
 * prefix down over the payload, so a 64-byte buffer corrupts two bytes past its
 * end. SmartPort: it does an unconditional memset(data, 0, 64) before copying
 * whatever the device returned, so anything under 64 corrupts regardless of how
 * short the key is. 66 satisfies both, and fuji_write_appkey reads the full 64
 * either way. Do not shrink this to fit the key.
 */
static uint8_t akbuf[66];

void hwm_load(void)
{
    uint16_t count = 0;

    memset(hwm, 0, sizeof(hwm));

#ifdef GM_FAKE_DATA
    return;                     /* no adapter to read from; start all unread */
#endif

    plat_net_begin();
    fuji_set_appkey_details(AK_CREATOR, AK_APP, DEFAULT);
    if (fuji_read_appkey(AK_KEY, &count, akbuf) && count >= 8)
        memcpy(hwm, akbuf, 8);
    plat_net_end();
}

static void hwm_save(void)
{
    memset(akbuf, 0, 64);
    memcpy(akbuf, hwm, 8);

#ifdef GM_FAKE_DATA
    return;
#endif

    plat_net_begin();
    fuji_set_appkey_details(AK_CREATOR, AK_APP, DEFAULT);
    fuji_write_appkey(AK_KEY, 8, akbuf);
    plat_net_end();
}

/* Strictly newer than the mark, comparing the wire's little-endian bytes from
   the most significant down. Equal is not newer. */
static unsigned char hwm_newer(const uint8_t *ts)
{
    signed char i;

    for (i = 7; i >= 0; i--) {
        if (ts[i] > hwm[i])
            return 1;
        if (ts[i] < hwm[i])
            return 0;
    }
    return 0;
}

void hwm_flags(void)
{
    unsigned char i;

    for (i = 0; i < gm_count; i++)
        gm_index[i].unread = hwm_newer(gm_index[i].ts);
}

void hwm_update(const uint8_t *ts)
{
    if (!hwm_newer(ts))
        return;

    memcpy(hwm, ts, 8);
    hwm_save();
    hwm_flags();
}
