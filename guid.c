// SPDX-License-Identifier: GPL-2.0
/*
 * android-efi
 * Copyright (C) 2017 lambdadroid
 */

#include "guid.h"
#include <efilib.h>

#define DECLARE_PARSE_HEX(n)\
static BOOLEAN parse_hex##n(const CHAR16 **input, UINT##n *u) {\
    for (unsigned i = 0; i < sizeof(UINT##n) * 2; ++i) {\
        CHAR16 c = *((*input)++);\
        if (c >= '0' && c <= '9')\
            *u = *u << 4 | (c - '0');\
        else if (c >= 'A' && c <= 'F')\
            *u = *u << 4 | (c - 'A' + 10);\
        else if (c >= 'a' && c <= 'f')\
            *u = *u << 4 | (c - 'a' + 10);\
        else{\
            return FALSE;\
        }\
    }\
    return TRUE;\
}

DECLARE_PARSE_HEX(8)
DECLARE_PARSE_HEX(16)
DECLARE_PARSE_HEX(32)

#define GUID_LENGTH  36
#define CHECK_DASH(input) *((input)++) == '-'

BOOLEAN guid_parse(const CHAR16 *input, EFI_GUID *guid) {
    if (StrLen(input) != GUID_LENGTH) {
        return FALSE;
    }

    if (parse_hex32(&input, &guid->Data1)
        && CHECK_DASH(input) && parse_hex16(&input, &guid->Data2)
        && CHECK_DASH(input) && parse_hex16(&input, &guid->Data3)
        && CHECK_DASH(input) && parse_hex8(&input, &guid->Data4[0]) && parse_hex8(&input, &guid->Data4[1])
        && CHECK_DASH(input)) {

        for (int i = 2; i < 8; ++i) {
            if (!parse_hex8(&input, &guid->Data4[i])) {
                return FALSE;
            }
        }

        return TRUE;
    }

    return FALSE;
}
