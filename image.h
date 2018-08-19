// SPDX-License-Identifier: GPL-2.0
/*
 * android-efi
 * Copyright (C) 2017 lambdadroid
 */

#ifndef ANDROID_EFI_IMAGE_H
#define ANDROID_EFI_IMAGE_H

#include <efi.h>

struct efi_image_partition {
    EFI_BLOCK_IO  *block_io;
    EFI_DISK_IO   *disk_io;
};

struct efi_image_file {
    EFI_FILE_HANDLE dir;
    EFI_FILE_HANDLE file;
};

struct efi_image {
    EFI_HANDLE partition_handle;

    union {
        struct efi_image_partition partition;
        struct efi_image_file file;
    };
};

EFI_STATUS image_open(struct efi_image *image, EFI_HANDLE loader, EFI_HANDLE loader_device,
                      EFI_GUID *partition_guid, CHAR16 *path);
EFI_STATUS image_read(const struct efi_image *image, UINT64 offset, VOID *buffer, UINTN buffer_size);
VOID image_close(const struct efi_image *image, EFI_HANDLE loader);

#endif //ANDROID_EFI_IMAGE_H
