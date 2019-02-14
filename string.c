// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018 lambdadroid

#include "string.h"
#include <efilib.h>

CHAR16 *StrnDuplicate(const CHAR16 *s, UINTN length) {
    UINTN size = length * sizeof(CHAR16) + sizeof(CHAR16); // Include null terminator
    CHAR16* new = AllocatePool(size);
    if (new) {
        CopyMem(new, s, size);
        new[length] = 0;
    }
    return new;
}

/*
 * Convert an UTF-16 string, not necessarily null terminated, to UTF-8.
 *
 * Taken from the Linux kernel: (GPL-2.0)
 *   drivers/firmware/efi/libstub/efi-stub-helper.c (efi_utf16_to_utf8)
 * Copyright 2011 Intel Corporation
 */
CHAR8 *str_utf16_to_utf8(CHAR8 *dst, const CHAR16 *src, UINTN n) {
    UINTN c;

    while (n--) {
        c = *src++;
        if (n && c >= 0xd800 && c <= 0xdbff && *src >= 0xdc00 && *src <= 0xdfff) {
            c = 0x10000 + ((c & 0x3ff) << 10) + (*src & 0x3ff);
            src++;
            n--;
        }
        if (c >= 0xd800 && c <= 0xdfff) {
            c = 0xfffd; /* Unmatched surrogate */
        }
        if (c < 0x80) {
            *dst++ = c;
            continue;
        }
        if (c < 0x800) {
            *dst++ = 0xc0 + (c >> 6);
            goto t1;
        }
        if (c < 0x10000) {
            *dst++ = 0xe0 + (c >> 12);
            goto t2;
        }
        *dst++ = 0xf0 + (c >> 18);
        *dst++ = 0x80 + ((c >> 12) & 0x3f);
        t2:
        *dst++ = 0x80 + ((c >> 6) & 0x3f);
        t1:
        *dst++ = 0x80 + (c & 0x3f);
    }

    return dst;
}
