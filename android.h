// SPDX-License-Identifier: GPL-2.0
/*
 * android-efi
 * Copyright (C) 2017 lambdadroid
 */

#ifndef ANDROID_EFI_ANDROID_H
#define ANDROID_EFI_ANDROID_H

#include <efi.h>
#include <efilib.h>
#include "image.h"

#define ANDROID_BOOT_MAGIC "ANDROID!"
#define ANDROID_BOOT_MAGIC_SIZE 8
#define ANDROID_BOOT_NAME_SIZE 16
#define ANDROID_BOOT_ARGS_SIZE 512
#define ANDROID_BOOT_EXTRA_ARGS_SIZE 1024

struct android_boot_image_header {
    CHAR8   magic[ANDROID_BOOT_MAGIC_SIZE];
    UINT32  kernel_size;
    UINT32  kernel_addr;
    UINT32  ramdisk_size;
    UINT32  ramdisk_addr;
    UINT32  second_size;
    UINT32  second_addr;
    UINT32  tags_addr;
    UINT32  page_size;
    UINT32  unused;
    UINT32  os_version;
    CHAR8   name[ANDROID_BOOT_NAME_SIZE];
    CHAR8   cmdline[ANDROID_BOOT_ARGS_SIZE];
    UINT32  id[8];
    CHAR8   extra_cmdline[ANDROID_BOOT_EXTRA_ARGS_SIZE];
} __attribute__((packed));

struct android_image {
    struct efi_image image;
    struct android_boot_image_header header;
};

static inline EFI_STATUS android_open_image(struct android_image *image) {
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

static inline EFI_STATUS android_read_kernel(struct android_image *image, UINT64 offset, VOID *kernel, UINTN size) {
    return image_read(&image->image, image->header.page_size + offset, kernel, size);
}

static inline EFI_STATUS android_load_kernel(struct android_image *image, UINT64 offset, VOID *kernel) {
    return android_read_kernel(image, offset, kernel, image->header.kernel_size - offset);
}

static inline UINT64 android_ramdisk_offset(struct android_image *image) {
    UINTN mask = image->header.page_size - 1;
    return image->header.page_size + ((image->header.kernel_size + mask) & ~mask);
}

static inline UINT32 android_ramdisk_size(struct android_image *image) {
    return image->header.ramdisk_size;
}

static inline EFI_STATUS android_load_ramdisk(struct android_image *image, VOID *ramdisk) {
    return image_read(&image->image, android_ramdisk_offset(image), ramdisk, android_ramdisk_size(image));
}

static inline VOID android_copy_cmdline(struct android_image *image, CHAR8* cmdline) {
    // Note: Command line is *not* null-terminated
    CopyMem(cmdline, image->header.cmdline, ANDROID_BOOT_ARGS_SIZE);

    if (cmdline[ANDROID_BOOT_ARGS_SIZE-1]) {
        // Copy extra command line as well
        CopyMem(&cmdline[ANDROID_BOOT_ARGS_SIZE], image->header.extra_cmdline, ANDROID_BOOT_EXTRA_ARGS_SIZE);

        if (image->header.extra_cmdline[ANDROID_BOOT_EXTRA_ARGS_SIZE-1]) {
            // Ensure to add null terminator
            cmdline[ANDROID_BOOT_EXTRA_ARGS_SIZE] = 0;
        }
    }
}

#endif //ANDROID_EFI_ANDROID_H
