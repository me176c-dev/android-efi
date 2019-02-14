// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018 lambdadroid

#ifndef ANDROID_EFI_STRING_H
#define ANDROID_EFI_STRING_H

#include <efi.h>

CHAR16 *StrnDuplicate(CHAR16 *s, UINTN length);
CHAR8 *str_utf16_to_utf8(CHAR8 *dst, const CHAR16 *src, UINTN n);

#endif //ANDROID_EFI_STRING_H
