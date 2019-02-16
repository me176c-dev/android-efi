// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2017 lambdadroid

#include "graphics.h"
#include <efilib.h>

static inline UINTN center(UINTN i, UINTN max) {
    return i < max ? (max - i) / 2 : 0;
}

EFI_STATUS graphics_display_image(const struct graphics_image *image) {
    EFI_GRAPHICS_OUTPUT_PROTOCOL *graphics_output;
    EFI_STATUS err = LibLocateProtocol(&GraphicsOutputProtocol, (VOID**) &graphics_output);
    if (err) {
        return err;
    }

    // Clear screen before displaying image
    err = uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut);
    if (err) {
        return err;
    }

    // Calculate position of image
    UINTN x = center(image->width, graphics_output->Mode->Info->HorizontalResolution);
    UINTN y = center(image->height, graphics_output->Mode->Info->VerticalResolution);
    return uefi_call_wrapper(graphics_output->Blt, 10, graphics_output, image->blt, EfiBltBufferToVideo,
                             0, 0, x, y, image->width, image->height, 0);
}
