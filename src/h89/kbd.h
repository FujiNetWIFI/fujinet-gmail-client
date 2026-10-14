/**
 * @file kbd.h
 * @brief H19 keyboard, with its escape sequences decoded - Heathkit H89.
 *
 * From fujinet-config's src/h89 (add-h89, 92cca0e), by way of the
 * Google Calendar client's H89 port (add-h89, c46d930), with kbd_pending()
 * added for the screen flush and kbd_wait() left to input.c.
 */

#ifndef H89_KBD_H
#define H89_KBD_H

#include <stdint.h>

/** The next key (see key_codes.h), or 0 when none is waiting */
uint8_t kbd_get(void);

/** Non-zero when a key is waiting. Reads ahead one byte to find out, and
    keeps it for kbd_get(): HDOS has no call that peeks. */
uint8_t kbd_pending(void);

#endif /* H89_KBD_H */
