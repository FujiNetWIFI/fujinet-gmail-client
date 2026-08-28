/*
 * Field sanitizer.
 *
 * Everything arriving from the wire passes through here on its way to a
 * display buffer. Two jobs:
 *
 *   - Control bytes (< 32) become spaces. This matters beyond looks: the
 *     Atari's conio treats $9B, $0A and $0D as newlines, so an unsanitized
 *     subject line could scroll the screen out from under us.
 *
 *   - Each *run* of bytes above plain ASCII collapses to a single '?'. The
 *     adapter does no charset conversion, so a UTF-8 name arrives as a
 *     multi-byte sequence; collapsing the run keeps "Jose" from turning into
 *     "Jos??" and keeps column budgets honest.
 */

#include "gmail.h"

void copy_san(char *dst, const char *src, unsigned char dstsize)
{
    unsigned char i = 0;
    unsigned char last = dstsize - 1;
    unsigned char high = 0;         /* inside a run of non-ASCII bytes */
    unsigned char c;

    while (i < last) {
        c = (unsigned char) *src++;
        if (c == 0)
            break;

        if (c > 126) {
            if (high)               /* already emitted the '?' for this run */
                continue;
            high = 1;
            c = '?';
        } else {
            high = 0;
            if (c < 32)
                c = ' ';
        }

        dst[i++] = (char) c;
    }

    dst[i] = '\0';
}
