/*
 * CMOC has no <stdlib.h> either, and for the same reason: everything is in
 * <cmoc.h>.
 *
 * src/net.c and the backend's painters want utoa() and ultoa(). CMOC spells
 * both as macros over utoa10() and ultoa10() that ignore the base argument, so
 * base 10 -- the only base this program asks for -- comes out right and any
 * other base would silently not. Nothing here asks for another.
 */

#ifndef COCO_SHIM_STDLIB_H
#define COCO_SHIM_STDLIB_H

#include <cmoc.h>

#endif
