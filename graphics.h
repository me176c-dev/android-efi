// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2017 lambdadroid

#ifndef ANDROID_EFI_GRAPHICS_H
#define ANDROID_EFI_GRAPHICS_H

#include <efi.h>

struct graphics_image {
    UINTN width;
    UINTN height;
    EFI_GRAPHICS_OUTPUT_BLT_PIXEL *blt;
};

EFI_STATUS graphics_display_image(const struct graphics_image *image);

#endif //ANDROID_EFI_GRAPHICS_H
