/*
 * fuji_get_adapter_config_extended() with a guard the library cannot have.
 *
 * Every INT F5 in this program assumes FUJINET.SYS is resident. When it is
 * not, the interrupt vector is null, and int86x through a null vector jumps
 * to 0000:0000 -- not an error return, a crash. This is the one call main.c's
 * have_fujinet() gates everything behind, so checking the vector here
 * protects every later bus call in the program: a missing driver becomes the
 * "FujiNet not found" screen instead of a hang, and ui_notfound() can name
 * CONFIG.SYS with a straight face.
 *
 * Past the guard this is the library member verbatim -- device 0x70, FUJICMD
 * 0xC4, the struct read back whole (msdos/src/fn_fuji/
 * fuji_get_adapter_config_extended.c). Unlike the Adam's fuji_adam.c, the
 * library's bool here is *not* inverted; this override exists purely for the
 * vector check, and defining the symbol leaves the archive's member
 * unreferenced the same way.
 *
 * Delete this file when fujinet-lib checks the vector itself.
 */

#include <dos.h>
#include <stdbool.h>
#include <stdint.h>

#include <fujinet-fuji.h>

#include "platform.h"

bool fuji_get_adapter_config_extended(AdapterConfigExtended *ac)
{
    if (_dos_getvect(0xF5) == 0)
        return false;

    return int_f5_read(0x70, 0xC4, 0x00, 0x00,
                       ac, sizeof(AdapterConfigExtended)) == 'C';
}
