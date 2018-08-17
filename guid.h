// SPDX-License-Identifier: GPL-2.0
/*
 * android-efi
 * Copyright (C) 2017 lambdadroid
 */

#ifndef ANDROID_EFI_GUID_H
#define ANDROID_EFI_GUID_H

#include <efi.h>

BOOLEAN guid_parse(EFI_GUID *guid, const CHAR16 *input, UINTN length);

#endif //ANDROID_EFI_GUID_H
