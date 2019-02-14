// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2017 lambdadroid

#ifndef ANDROID_EFI_MALLOC_H
#define ANDROID_EFI_MALLOC_H

#include <efi.h>

EFI_STATUS malloc_high(UINTN size, EFI_PHYSICAL_ADDRESS *addr);
EFI_STATUS malloc_low(UINTN size, UINTN align, EFI_PHYSICAL_ADDRESS *addr);

#endif //ANDROID_EFI_MALLOC_H
