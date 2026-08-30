/*
 * CMOC has no <stdint.h>, and src/gmail.h wants one: the wire record and the
 * timestamp it carries are uint8_t, because that is what they are.
 *
 * <coco.h> already carries the six typedefs, behind its own _CMOC_STDINT_
 * guard, so this shim only has to point at it. What it must NOT do is define
 * them itself:
 *
 *   - Spelled as typedefs, they collide with coco.h's the moment any file in
 *     src/coco/ includes both -- which every file in src/coco/ does.
 *   - Spelled as #defines, they turn coco.h's own block into
 *     "typedef unsigned char unsigned char;" for any translation unit that
 *     reaches this header first. That is what src/coco/logo.c does, via
 *     ../gmail.h, and it is how this file came to be written twice.
 *
 * Deferring to coco.h has neither problem in either order: its include guard
 * makes the second include a no-op, and _CMOC_STDINT_ means fujinet-fuji.h's
 * own "#define uint8_t unsigned char" -- which it does after including coco.h
 * -- lands on a block that has already run and will not run again.
 */

#ifndef COCO_SHIM_STDINT_H
#define COCO_SHIM_STDINT_H

#include <coco.h>

#endif
