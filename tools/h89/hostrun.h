/* Forced into every file of the host build (tools/h89/hostrun.sh). */
#include <stdint.h>
uint16_t host_tick(void);
char *utoa(unsigned int v, char *buf, int radix);
char *ultoa(unsigned long v, char *buf, int radix);
#define TICKS host_tick()
#define __z88dk_fastcall
