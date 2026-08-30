/*
 * CMOC has no <string.h>.
 *
 * It has memcpy, memmove, memset, memcmp, strcpy, strcat, strlen, strchr and
 * the rest -- they are simply all declared in <cmoc.h>, which the compiler
 * pulls in for every translation unit anyway. The portable half of this
 * program is ordinary C89 and includes <string.h> like ordinary C89, so this
 * shim is what lets it stay that way.
 *
 * It is reachable only from the CoCo build, which puts this directory on the
 * include path. Nothing else in the tree sees it, and the directory holds no
 * .c files so the source glob steps over it.
 */

#ifndef COCO_SHIM_STRING_H
#define COCO_SHIM_STRING_H

#include <cmoc.h>

#endif
