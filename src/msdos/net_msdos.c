/*
 * fujinet-lib overrides for the network path. Defining the symbols here
 * leaves the library's members unreferenced, the src/adam/ pattern; each
 * carries the reason it exists and all of them should be deleted when
 * fujinet-lib catches up with the bus.
 *
 * The big one is the field descriptor. The current FUJINET.SYS speaks the
 * FujiBusPacket protocol, in which DH describes how the aux bytes become
 * typed parameters -- from the driver's and firmware's own tables,
 *
 *      DH:      0  1  2  3  4  5  6  7
 *      params:  0  1  2  3  4  1  2  1     (numFieldsTable)
 *      size:    -  u8 u8 u8 u8 u16 u16 u32 (fieldSizeTable)
 *
 * -- and fujinet-lib 4.11.2's int_f5_read/int_f5_write always send DH=0: no
 * parameters at all. Commands that need none (status, close, the fuji and
 * clock devices this client uses) work by luck; the two that carry them do
 * not. An open arrives as "Insufficient open paramaters: 0" in the firmware
 * log and a NAK on the wire, which is how this file found out. So the two
 * parameter-carrying calls this client makes get their own bus entry with
 * DH set properly:
 *
 * network_open(): needs DH=2 -- mode and trans as two u8 params -- plus the
 * devicespec as the write payload. The library's version also returned
 * network_error() from its failure path, and *that* returned
 * network_status()'s own return instead of the error byte it carried -- so
 * a refused open reported success and the 212 authorize-in-the-Web-UI
 * error, the one a first-time user is guaranteed to hit, could never name
 * itself. The payload is sent at its real length rather than the library's
 * flat 256: the firmware takes it into a std::string verbatim, and 200
 * bytes of stack garbage after the NUL would become part of the URL.
 *
 * network_read(): needs DH=5 -- the length as one u16 param, aux1 low. The
 * library's common network_read() under __WATCOMC__ additionally passes the
 * unit *number* where the msdos read expects a devicespec *pointer*, and
 * between chunks insists the status error byte be exactly 1 -- zero-as-
 * healthy being precisely the Mailbox quirk st_ok() in ../net.c exists for.
 * Replaced with one exact-length 'R': net.c never asks for more than status
 * said is staged and probes between chunks itself.
 *
 * network_write(): the same DH=5 shape as the read -- the length as one u16
 * param -- with the draft bytes as the write payload. The library's DH=0
 * version NAKs exactly the way the open did.
 *
 * network_error(): returns the device's code, never 0 for a failed call.
 */

#include <dos.h>
#include <stdint.h>
#include <string.h>

#include <fujinet-network.h>

#include "platform.h"

/* Direction, in DL. */
#define F5_NONE     0x00
#define F5_READ     0x40
#define F5_WRITE    0x80

/* Field descriptors, in DH -- see the table above. */
#define FD_NONE     0
#define FD_2xU8     2
#define FD_1xU16    5

/*
 * The library's int_f5_read/int_f5_write minus their DH=0 assumption. Same
 * register protocol otherwise: AL device, AH command, CL/CH aux, ES:BX
 * buffer, DI length, AL back as 'C' complete / 'E' error / 'N' NAK.
 */
static unsigned char int_f5x(unsigned char dir, unsigned char fields,
                             unsigned char dev, unsigned char command,
                             unsigned char aux1, unsigned char aux2,
                             void *buf, unsigned short len)
{
    union REGS r;
    struct SREGS sr;

    memset(&r, 0, sizeof(r));
    memset(&sr, 0, sizeof(sr));

    r.h.dl = dir;
    r.h.dh = fields;
    r.h.al = dev;
    r.h.ah = command;
    r.h.cl = aux1;
    r.h.ch = aux2;
    r.x.si = 0x00;

    sr.es  = FP_SEG(buf);
    r.x.bx = FP_OFF(buf);
    r.x.di = len;

    int86x(0xF5, &r, &r, &sr);

    return r.h.al;
}

uint8_t network_error(const char *devicespec)
{
    uint8_t err = 0;

    if (network_status(devicespec, 0, 0, &err) != FN_ERR_OK)
        return FN_ERR_IO_ERROR;

    /* Never 0 for a failed call: the caller is reporting a failure, and
       FN_ERR_OK from a failure path is how the original bug read. */
    return err ? err : FN_ERR_IO_ERROR;
}

uint8_t network_open(const char *devicespec, uint8_t mode, uint8_t trans)
{
    unsigned char device = (unsigned char) (0x70 + network_unit(devicespec));
    unsigned short len = 0;

    while (devicespec[len])
        len++;

    if (int_f5x(F5_WRITE, FD_2xU8, device, 'O', mode, trans,
                (void *) devicespec, len) != 'C')
        return network_error(devicespec);

    return FN_ERR_OK;
}

int16_t network_read(const char *devicespec, uint8_t *buf, uint16_t len)
{
    unsigned char device = (unsigned char) (0x70 + network_unit(devicespec));

    if (len == 0 || buf == 0)
        return -FN_ERR_BAD_CMD;

    if (int_f5x(F5_READ, FD_1xU16, device, 'R',
                (unsigned char) (len & 0xFF),
                (unsigned char) (len >> 8),
                buf, len) != 'C')
        return -FN_ERR_IO_ERROR;

    return (int16_t) len;
}

uint8_t network_write(const char *devicespec, const uint8_t *buf, uint16_t len)
{
    unsigned char device = (unsigned char) (0x70 + network_unit(devicespec));

    if (len == 0 || buf == 0)
        return FN_ERR_BAD_CMD;

    if (int_f5x(F5_WRITE, FD_1xU16, device, 'W',
                (unsigned char) (len & 0xFF),
                (unsigned char) (len >> 8),
                (void *) buf, len) != 'C')
        return network_error(devicespec);

    return FN_ERR_OK;
}
