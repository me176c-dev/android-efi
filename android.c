// SPDX-License-Identifier: GPL-2.0
/*
 * android-efi
 * Copyright (C) 2017 lambdadroid
 */

#include "android.h"
#include <efilib.h>

EFI_STATUS android_open_image(struct android_image *image) {
    EFI_STATUS err = image_read(&image->image, 0, &image->header, sizeof(image->header));
    if (err) {
        return err;
    }

    if (CompareMem(image->header.magic, ANDROID_BOOT_MAGIC, ANDROID_BOOT_MAGIC_SIZE)) {
        Print(L"Partition does not appear to be an Android boot partition\n");
        return EFI_VOLUME_CORRUPTED;
    }

    return EFI_SUCCESS;
}

EFI_STATUS android_copy_cmdline(struct android_image *image, CHAR8* cmdline, UINTN max_length) {
    if (max_length <= ANDROID_BOOT_ARGS_SIZE + ANDROID_BOOT_EXTRA_ARGS_SIZE) {
        return EFI_BUFFER_TOO_SMALL;
    }

    // Note: Command line is *not* null-terminated
    CopyMem(cmdline, image->header.cmdline, ANDROID_BOOT_ARGS_SIZE);

    if (cmdline[ANDROID_BOOT_ARGS_SIZE - 1]) {
        // Copy extra command line as well
        CopyMem(&cmdline[ANDROID_BOOT_ARGS_SIZE], image->header.extra_cmdline, ANDROID_BOOT_EXTRA_ARGS_SIZE);

        if (image->header.extra_cmdline[ANDROID_BOOT_EXTRA_ARGS_SIZE - 1]) {
            // Ensure to add null terminator
            cmdline[ANDROID_BOOT_ARGS_SIZE + ANDROID_BOOT_EXTRA_ARGS_SIZE] = 0;
        }
    }

    return EFI_SUCCESS;
}
