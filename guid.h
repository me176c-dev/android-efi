// SPDX-License-Identifier: GPL-2.0
/*
 * android-efi
 * Copyright (C) 2017 lambdadroid
 */

#ifndef ANDROID_EFI_GUID_H
#define ANDROID_EFI_GUID_H

#include <efi.h>

BOOLEAN guid_parse(const CHAR16 *input, EFI_GUID *guid);

#endif //ANDROID_EFI_GUID_H
