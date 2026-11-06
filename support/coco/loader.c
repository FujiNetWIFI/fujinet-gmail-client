/*
 * The model picker for the combined disk.
 *
 * LOADM"GMAIL":EXEC lands here rather than in the client, and this hands off to
 * GMAIL1 on a CoCo 1/2 or GMAIL3 on a CoCo 3. It is the whole reason one disk
 * can carry both binaries without asking the user which machine they are on.
 *
 * runm() is fujinet-fujirkle's: poke BASIC's direct-mode buffer with M"<name>",
 * point $A6 at it and enter the ROM's RUNM at $AE75. There is no LOADM callable
 * from C, and RUNM is what leaves the loader's own memory free for the client
 * that replaces it.
 */

#include <cmoc.h>
#include <coco.h>

void runm(const char *filename)
{
    *((unsigned short *) 0x02DD) = 0x4D22;      /* M"                       */
    strcpy((char *) 0x02DF, filename);
    *((unsigned short *) 0x00A6) = 0x02DD;      /* BASIC's input pointer    */

    asm
    {
        ldd     #$4D1C
        jmp     $AE75
    }
}

int main(void)
{
    initCoCoSupport();
    runm(isCoCo3 ? "GMAIL3" : "GMAIL1");
    return 0;
}
