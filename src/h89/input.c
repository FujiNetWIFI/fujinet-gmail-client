/*
 * Keyboard, and the screen's delivery.
 *
 * The H19 keyboard is the H89's whole interface, and it has keys no other
 * machine this program runs on has: five function keys and three coloured
 * ones along the top, and a keypad that sends arrows. The coloured keys mean
 * what they mean in the Google Calendar client on the same machine -- BLUE
 * a new one, RED act on this one, GRAY fetch again -- so BLUE composes, RED
 * replies to the message being read, and GRAY (or f5, or HOME, its "today")
 * goes back to the top of the inbox. The 25th line says so on every screen
 * (ui.c). The letters every other backend takes -- C, R, F, Q -- work too.
 *
 * Every read here sends the screen first. The painters only ever write
 * h19.c's copy, so this is the one place the terminal is brought up to
 * date: once per wait, by difference, which is the whole of what makes a
 * 9600-baud screen bearable. The waits poll rather than block, round
 * plat_vsync() and clock_pump(), so the clock on row 2 goes on turning --
 * and each turn flushes again, which costs nothing when nothing changed and
 * five cells when the minute did.
 */

#include "../gmail.h"
#include "platform.h"
#include "kbd.h"
#include "key_codes.h"

static unsigned char map(unsigned char k)
{
    switch (k) {
    case KEY_UP_ARROW:              return K_UP;
    case KEY_DOWN_ARROW:            return K_DOWN;
    case KEY_LEFT_ARROW:
    case KEY_PAGE_UP:               return K_LEFT;
    case KEY_RIGHT_ARROW:
    case KEY_PAGE_DOWN:             return K_RIGHT;

    case KEY_RETURN:                return K_ENTER;
    case KEY_ESCAPE:                return K_BACK;

    case KEY_BLUE: case 'c': case 'C':  return K_COMPOSE;

    /* R is K_REPLY everywhere and the inbox folds it into refresh, as on
       every backend. RED is a reply key and nothing else: in the inbox it
       does nothing, rather than fetch the inbox again. */
    case 'r': case 'R':             return K_REPLY;
    case KEY_RED:
        return (unsigned char) ((ui_screen == UI_READER) ? K_REPLY : K_NONE);

    case 'f': case 'F':             return K_FORWARD;

    case KEY_GRAY: case KEY_F5:
    case KEY_HOME:                  return K_REFRESH;

    case 'q': case 'Q':             return K_QUIT;
    }

    return K_NONE;
}

/* The next key code from the H19, polling: the clock is pumped, and the
   screen sent, once a frame while nothing is pressed. */
static unsigned char key_wait(void)
{
    unsigned char k;

    for (;;) {
        h19_flush();
        k = kbd_get();
        if (k)
            return k;
        plat_vsync();
        clock_pump();
    }
}

unsigned char plat_getkey(void)
{
    unsigned char k;

    for (;;) {
        k = map(key_wait());
        if (k != K_NONE)
            return k;
    }
}

void plat_anykey(void)
{
    key_wait();
}

#ifdef GM_OVERLAYS
#pragma codeseg code_ovl_compose
#endif

/*
 * The compose form's read. BACKSPACE and DELETE both erase before the
 * cursor; BLUE sends without the question; the keypad's arrows move.
 *
 * The flush here is the one that stops between lines when a key is waiting:
 * a typist outruns 9600 baud, and the next key repaints the row anyway.
 */
unsigned char plat_getch(void)
{
    unsigned char k;

    for (;;) {
        h19_flush_poll();
        k = kbd_get();
        switch (k) {
        case 0:
            plat_vsync();
            clock_pump();
            continue;
        case KEY_UP_ARROW:          return E_UP;
        case KEY_DOWN_ARROW:        return E_DOWN;
        case KEY_LEFT_ARROW:        return E_LEFT;
        case KEY_RIGHT_ARROW:       return E_RIGHT;
        case KEY_RETURN:
        case KEY_TAB:               return E_ENTER;
        case KEY_ESCAPE:            return E_DONE;
        case KEY_BACKSPACE:
        case KEY_DELETE:            return E_BS;
        case KEY_BLUE:              return E_SAVE;
        }
        if (k >= 0x20 && k < 0x7F)
            return k;
    }
}

#ifdef GM_OVERLAYS
#pragma codeseg code_compiler
#endif
