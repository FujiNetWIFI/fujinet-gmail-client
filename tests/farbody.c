/*
 * The far body store, as the NES keeps it, on the host.
 *
 * GM_FAR_BODY builds hand every wrapped row to body_put_rows() and read them
 * back through body_row(); src/nes/bodystore.c does that through CHR-RAM.
 * This is the same contract with the hardware taken out, made as unforgiving
 * as the real one:
 *
 *   - rows are stored BODY_COLS bytes wide with no terminator, so a full-width
 *     row comes back only if body_row() terminates its copy itself;
 *   - the store is fenced with canaries, so a put that writes a stride where
 *     it should write a row width shows up;
 *   - body_row() alternates two buffers and poisons the one it handed out
 *     before, so a caller holding two rows at once reads garbage here instead
 *     of getting away with it on the host and failing on the NES.
 */

#include <string.h>

#include "../src/gmail.h"

#ifdef GM_FAR_BODY

#define CANARY  0x5A
#define POISON  '#'

static unsigned char fence_lo[16];
static char          store[BODY_ROWS][BODY_COLS];
static unsigned char fence_hi[16];

static char          copy[2][BODY_STRIDE];
static unsigned char which;

void farbody_reset(void)
{
    memset(fence_lo, CANARY, sizeof fence_lo);
    memset(fence_hi, CANARY, sizeof fence_hi);
    memset(store, POISON, sizeof store);
}

int farbody_fences_ok(void)
{
    unsigned char i;

    for (i = 0; i < 16; i++)
        if (fence_lo[i] != CANARY || fence_hi[i] != CANARY)
            return 0;
    return 1;
}

void body_put_rows(unsigned int r, const char *rows, unsigned char n)
{
    unsigned char i;
    size_t        len;

    for (i = 0; i < n; i++, r++, rows += BODY_STRIDE) {
        if (r >= BODY_ROWS) {
            fence_hi[0] = 0;            /* a put past the end: fail the fence */
            return;
        }
        /* Exactly what the NES does: BODY_COLS bytes, zero-padded, so a short
           row ends at its NUL and a full one has none. */
        len = strlen(rows);
        if (len > BODY_COLS)
            len = BODY_COLS;
        memset(store[r], 0, BODY_COLS);
        memcpy(store[r], rows, len);
    }
}

const char *body_row(unsigned int r)
{
    char *d;

    memset(copy[which], POISON, BODY_STRIDE);   /* the one handed out last */
    which ^= 1;
    d = copy[which];
    if (r >= BODY_ROWS) {
        d[0] = '\0';
        return d;
    }
    memcpy(d, store[r], BODY_COLS);
    d[BODY_COLS] = '\0';
    return d;
}

#endif /* GM_FAR_BODY */
