/*
 * input.c -- the controller and the Famicom keyboards, as gmail.h's keys.
 *
 * Both are read once a frame and either will do: a keyboard plugged into
 * the expansion port is found at start-up (fuji_nes_kbd_detect), and from
 * then on every key wait takes whichever comes first.
 *
 *              inbox           reader          form (on-screen keyboard)
 *   A          open            reply           type the key under the cursor
 *   B          --              back            delete
 *   START      compose         forward         done (send? / leave)
 *   SELECT     refresh         --              next keyboard page
 *   pad        move / page     line / page     move on the keyboard
 *
 * A keyboard does what the other machines' do: arrows, RETURN to open, ESC
 * (or BS) to go back, C compose, R reply / refresh, F forward, and in the
 * form it types, with RETURN or TAB for the next field and ESC for done.
 * Neither keyboard has a Q key worth spending on quit, and the NES has
 * nowhere to quit to, so nothing produces K_QUIT.
 *
 * Every wait is a loop of plat_vsync() and clock_pump(): the frame is when
 * the screen gets composed (scr.c), and the clock runs on frames.
 */

#include <fujinet-nes.h>

#include "../gmail.h"
#include "platform.h"

#define J_A         0x80
#define J_B         0x40
#define J_SELECT    0x20
#define J_START     0x10
#define J_UP        0x08
#define J_DOWN      0x04
#define J_LEFT      0x02
#define J_RIGHT     0x01
#define J_DPAD      0x0F
#define J_REPEAT    (J_DPAD | J_B)

/* The d-pad and B repeat when held: after REP_FIRST frames, then every
   REP_NEXT -- which is what scrolling a long message wants, and rubbing out
   a word on the on-screen keyboard. */
#define REP_FIRST   20
#define REP_NEXT    5

uint8_t inp_kbd;

static uint8_t pad_prev;
static uint8_t rep_t;

#ifdef GM_FAKE_KEYS
/*
 * Scripted keys for the capture harness (tools/nes-shot.sh): spent before
 * any real input, each one only once the screen has caught up with the one
 * before -- so a capture taken at the end sees every step drawn, not just
 * the last. inp_idle is set when the script has run out, which is what the
 * harness waits for.
 */
static const unsigned char fake_keys[] = { GM_FAKE_KEYS };
static unsigned char fake_idx;
volatile uint8_t inp_idle;

static uint8_t fake_next(uint8_t *k)
{
    uint8_t i;

    if (fake_idx >= sizeof(fake_keys)) {
        inp_idle = 1;
        return 0;
    }
    scr_sync();
    for (i = 0; i < 8; ++i)
        scr_frame();
    *k = fake_keys[fake_idx++];
    return 1;
}
#endif

static uint8_t read_pad(void)
{
    uint8_t i, j = 0;

    JOY1 = 1;
    JOY1 = 0;
    for (i = 0; i < 8; ++i)
        j = (uint8_t) ((j << 1) | ((JOY1 & 3) ? 1 : 0));
    return j;
}

void inp_init(void)
{
    inp_kbd = fuji_nes_kbd_detect();
    pad_prev = read_pad();
}

uint8_t inp_event(void)
{
    static const uint8_t ev_of[8] = {
        EV_RIGHT, EV_LEFT, EV_DOWN, EV_UP, EV_START, EV_SELECT, EV_B, EV_A
    };
    uint8_t j = read_pad();
    uint8_t down = (uint8_t) (j & ~pad_prev);
    uint8_t i;
    char    c;

    pad_prev = j;

    if (j & J_REPEAT) {
        if (down & J_REPEAT)
            rep_t = REP_FIRST;
        else if (--rep_t == 0) {
            rep_t = REP_NEXT;
            down |= (uint8_t) (j & J_REPEAT);
        }
    }

    if (down) {
        for (i = 7; ; --i) {
            if (down & (1 << i))
                return ev_of[i];
            if (i == 0)
                break;
        }
    }

    if (inp_kbd) {
        c = fuji_nes_kbd_getc();
        if (c)
            return (uint8_t) c;
    }
    return EV_NONE;
}

static uint8_t next_event(void)
{
    uint8_t ev;

    for (;;) {
        plat_vsync();
        clock_pump();
        ev = inp_event();
        if (ev)
            return ev;
    }
}

/* ------------------------------------------------------------------ */
/* The plat_ contract                                                  */
/* ------------------------------------------------------------------ */

static uint8_t key_of(uint8_t ev)
{
    uint8_t reader = (uint8_t) (ui_screen == SCREEN_READER);

    switch (ev) {
    case EV_UP:     case (uint8_t) FUJI_NES_KEY_UP:     return K_UP;
    case EV_DOWN:   case (uint8_t) FUJI_NES_KEY_DOWN:   return K_DOWN;
    case EV_LEFT:   case (uint8_t) FUJI_NES_KEY_LEFT:   return K_LEFT;
    case EV_RIGHT:  case (uint8_t) FUJI_NES_KEY_RIGHT:  return K_RIGHT;
    case EV_A:      return reader ? K_REPLY : K_ENTER;
    case EV_START:  return reader ? K_FORWARD : K_COMPOSE;
    case EV_B:      return reader ? K_BACK : K_NONE;
    case EV_SELECT: return reader ? K_NONE : K_REFRESH;

    case FUJI_NES_KEY_ENTER:                    return K_ENTER;
    case FUJI_NES_KEY_ESC:
    case FUJI_NES_KEY_BS:                       return K_BACK;
    case (uint8_t) FUJI_NES_KEY_HOME:           return K_REFRESH;
    case 'c': case 'C':                         return K_COMPOSE;
    case 'r': case 'R':                         return K_REPLY;
    case 'f': case 'F':                         return K_FORWARD;
    }
    return K_NONE;
}

unsigned char plat_getkey(void)
{
    uint8_t k;

#ifdef GM_FAKE_KEYS
    if (fake_next(&k))
        return k;
#endif

    for (;;) {
        k = key_of(next_event());
        if (k != K_NONE)
            return k;
    }
}

void plat_anykey(void)
{
#ifdef GM_FAKE_KEYS
    uint8_t k;

    if (fake_next(&k))
        return;
#endif

    (void) next_event();
}

/*
 * The form's read. A keyboard types straight in; the controller drives the
 * on-screen keyboard, which lives with the rest of the form in the COMPOSE
 * bank -- form_key() answers 0 for a press that only moved its cursor.
 */
unsigned char plat_getch(void)
{
    uint8_t ev;

#ifdef GM_FAKE_KEYS
    if (fake_next(&ev))
        return ev;
#endif

    for (;;) {
        ev = next_event();

        if (ev >= EV_A) {
            ev = form_key(ev);
            if (ev)
                return ev;
            continue;
        }

        switch (ev) {
        case (uint8_t) FUJI_NES_KEY_UP:     return E_UP;
        case (uint8_t) FUJI_NES_KEY_DOWN:   return E_DOWN;
        case (uint8_t) FUJI_NES_KEY_LEFT:   return E_LEFT;
        case (uint8_t) FUJI_NES_KEY_RIGHT:  return E_RIGHT;
        case FUJI_NES_KEY_ENTER:
        case FUJI_NES_KEY_TAB:              return E_ENTER;
        case FUJI_NES_KEY_ESC:              return E_DONE;
        case FUJI_NES_KEY_BS:
        case 0x7F:                          return E_BS;
        }
        if (ev >= 0x20 && ev < 0x7F)
            return ev;
    }
}

/* ------------------------------------------------------------------ */
/* Frames                                                              */
/* ------------------------------------------------------------------ */

/* A frame: the next dirty row composed (scr.c), then the wait. */
void plat_vsync(void)
{
    scr_frame();
}

unsigned long plat_ticks(void)
{
    unsigned long a, b;

    /* The NMI can land between the four bytes; read until two agree. */
    do {
        a = frame_count;
        b = frame_count;
    } while (a != b);
    return a;
}

unsigned char plat_fps(void)
{
    return 60;
}

/* Nothing to bracket: the interrupts never touch the mailbox (crt0.s). */
void plat_net_begin(void)
{
}

void plat_net_end(void)
{
}
