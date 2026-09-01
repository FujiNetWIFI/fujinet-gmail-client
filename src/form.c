/*
 * The compose form model: field storage, validation, and the exact draft
 * bytes the GMAIL adapter's parser takes.
 *
 * Pure -- no platform, no network. compose.c owns the screen loop and the
 * cursor; net.c owns the channel; this file owns what is *in* the form and
 * what goes out on the wire, which is the half a host test can pin down to
 * the byte.
 *
 * Three modes share the one form. A compose sends TO, SUBJECT and the body.
 * A reply may leave TO and SUBJECT blank -- the adapter prefills the
 * original's sender and a "Re:" subject at commit time, and only a typed
 * value overrides that, so a blank field here means "take the default", not
 * "send nothing". A forward is a compose with the original message appended
 * after the typed body: the adapter has no forward operation, so the
 * separator block and the quoted rows are built here, straight out of
 * gm_index[gm_sel] and gm_body.
 *
 * There is no overlay trick (compare the calendar client's GC_FORM_OVERLAY):
 * that forward emission is exactly why. The form is alive at the same time
 * as the body buffer it would otherwise borrow.
 */

#include <string.h>

#include "gmail.h"

struct frmbuf frm;
unsigned char frm_dirty[FRM_NFIELDS];
unsigned char frm_mode;

char *form_field_ptr(unsigned char f)
{
    if (f == F_TO)
        return frm.to;
    if (f == F_SUBJ)
        return frm.subj;
    return frm.body[f - F_BODY0];
}

unsigned char form_field_max(unsigned char f)
{
    if (f == F_TO)
        return FRM_TO_MAX;
    if (f == F_SUBJ)
        return FRM_SUBJ_MAX;
    return FRM_BODY_COLS;
}

/* Does s already start with "fwd:", any case? The adapter guards "Re:" the
   same way for replies; the forward prefix is this client's to build, so the
   guard against doubling it lives here too. */
static unsigned char has_fwd(const char *s)
{
    return (unsigned char) ((s[0] == 'f' || s[0] == 'F') &&
                            (s[1] == 'w' || s[1] == 'W') &&
                            (s[2] == 'd' || s[2] == 'D') &&
                            s[3] == ':');
}

void form_init(unsigned char mode)
{
    unsigned char f;

    memset(&frm, 0, sizeof(struct frmbuf));
    for (f = 0; f < FRM_NFIELDS; f++)
        frm_dirty[f] = 0;

    frm_mode = mode;

    /* A forward gets its subject built now, where the user can still edit
       it; everything else starts blank. Prefilling does not dirty the
       field -- an untouched form still exits without the send ask. The
       copy is hand-rolled because it truncates by design and not every
       toolchain in this family ships strncpy. */
    if (mode == FRM_FWD) {
        const char   *s = gm_index[gm_sel].subject;
        unsigned char i, n;

        if (!has_fwd(s))
            strcpy(frm.subj, "Fwd: ");
        n = (unsigned char) strlen(frm.subj);
        for (i = 0; s[i] && n < FRM_SUBJ_MAX; i++, n++)
            frm.subj[n] = s[i];
        frm.subj[n] = '\0';
    }
}

unsigned char form_any_dirty(void)
{
    unsigned char f;

    for (f = 0; f < FRM_NFIELDS; f++)
        if (frm_dirty[f])
            return 1;

    return 0;
}

/* One past the last body line holding anything: the emission bound, so
   trailing blank lines never go out while interior ones survive. */
static unsigned char body_end(void)
{
    unsigned char i, end = 0;

    for (i = 0; i < FRM_NBODY; i++)
        if (frm.body[i][0])
            end = (unsigned char) (i + 1);

    return end;
}

/*
 * Catch before sending what the adapter would only bounce as an opaque 132
 * -- it collapses every draft error to one code, so the field name in the
 * message here is the only diagnosis the user will ever get. Returns
 * FM_NONE when the draft is sound, else the message code, with *bad the
 * field to put the cursor back on.
 *
 * A reply needs no TO (the adapter defaults it) but does need a body --
 * an empty draft is a clean abort at the adapter, and "nothing happened"
 * is not what send should mean. A forward needs a recipient and nothing
 * else: the original message is the body.
 */
unsigned char form_validate(unsigned char *bad)
{
    if (frm_mode != FRM_REPLY && !frm.to[0]) {
        *bad = F_TO;
        return FM_NEEDTO;
    }

    if (frm_mode != FRM_FWD && body_end() == 0) {
        *bad = F_BODY0;
        return FM_NEEDBODY;
    }

    return FM_NONE;
}

/* ------------------------------------------------------------------ */
/* Emission                                                            */
/* ------------------------------------------------------------------ */

/* '\n' below is deliberate: each toolchain's charmap turns it into that
   platform's native terminator -- $9B on the Atari, CR on the Apple, LF
   elsewhere -- and the adapter's line splitter takes all of them. */
static void put_line(const char *key, const char *val)
{
    strcpy(frm.line, key);
    strcat(frm.line, val);
    strcat(frm.line, "\n");
    gm_send_put(frm.line);
}

/*
 * The forward block: a separator, the original's headers as far as this
 * client kept them, then the body rows as they were wrapped for display --
 * which is all the client has, so a forward re-flows at this screen's
 * width.
 *
 * Budget-checked against gm_send_room() row by row, because the worst case
 * genuinely overflows: the wide screens keep more body than the adapter's
 * cap takes, and tripping the device-side 162 would poison the whole
 * draft. The 32-byte reserve is what the truncation notice needs.
 */
static void emit_forward(unsigned char had_intro)
{
    char          date[ENT_DATE_LEN];
    unsigned int  r;
    unsigned char cut = gm_body_trunc;

    if (had_intro)
        gm_send_put("\n");
    gm_send_put("---------- Forwarded message ----------\n");
    put_line("From: ", gm_index[gm_sel].name);
    date_fmt(date, gm_index[gm_sel].ts);
    put_line("Date: ", date);
    put_line("Subject: ", gm_index[gm_sel].subject);
    gm_send_put("\n");

    for (r = 0; r < gm_body_rows; r++) {
        if (gm_send_room() < strlen(gm_body[r]) + 33) {
            cut = 1;
            break;
        }
        put_line("", gm_body[r]);
    }

    if (cut)
        gm_send_put("[forwarded message truncated]\n");
}

/*
 * Send the draft through gm_send_put(), one line at a time. The caller has
 * already validated; this only formats. A failed put is gm_send_put's own
 * to remember: the channel still has to be closed, and gm_send_end() is
 * where the failure reports, so there is nothing useful to do here but
 * keep going.
 *
 * The order is headers, one blank line, body -- the blank line goes out
 * even when there are no headers, because it is what ends the header
 * section, and a bare-body reply draft is exactly "\n" + body. A blank TO
 * or SUBJECT is omitted entirely: on a reply that is how the adapter's
 * defaults are asked for.
 */
void form_emit(void)
{
    unsigned char i;
    unsigned char end = body_end();

    if (frm.to[0])
        put_line("TO: ", frm.to);
    if (frm.subj[0])
        put_line("SUBJECT: ", frm.subj);
    gm_send_put("\n");

    for (i = 0; i < end; i++)
        put_line("", frm.body[i]);

    if (frm_mode == FRM_FWD)
        emit_forward(end);
}
