// SPDX-License-Identifier: GPL-2.0
/*
 * android-efi
 * Copyright (C) 2017 lambdadroid
 */

#include <efi.h>
#include <efilib.h>

#include "guid.h"
#include "android.h"
#include "linux.h"
#include "graphics.h"

static EFI_STATUS parse_command_line(EFI_HANDLE loader, EFI_GUID **partition_guid, CHAR16 **path) {
    // Check command line arguments for partition GUID
    CHAR16 **argv;
    INTN argc = GetShellArgcArgv(loader, &argv);
    if (argc < 2) {
        Print(L"Usage: android.efi <Boot Partition GUID/Path>\n");
        return EFI_INVALID_PARAMETER;
    }

    CHAR16 *partition = argv[1];

    // Find path separator
    UINTN i;
    for (i = 0; partition[i]; ++i) {
        if (partition[i] == L'/' || partition[i] == L'\\') {
            *path = &partition[i];

            // Cleanup path (replace forward slashes with backward slashes)
            for (UINTN j = i; partition[j]; ++j) {
                if (partition[j] == L'/') {
                    partition[j] = L'\\';
                }
            }

            break;
        }
    }

    if (i == 0) {
        // File path without partition GUID
        *partition_guid = NULL;
        return EFI_SUCCESS;
    }

    // Parse command line partition GUID
    if (!guid_parse(*partition_guid, partition, i)) {
        Print(L"Expected valid partition GUID, got '%s'\n", argv[1]);
        return EFI_INVALID_PARAMETER;
    }

    return EFI_SUCCESS;
}

static EFI_STATUS load_kernel(EFI_HANDLE loader, EFI_GUID *partition_guid, CHAR16 *path, VOID **boot_params) {
    struct android_image android_image;

    // Open partition
    EFI_STATUS err = image_open(&android_image.image, loader, partition_guid, path);
    if (err) {
        return err;
    }

    // Read Android boot image header
    err = android_open_image(&android_image);
    if (err) {
        return err;
    }

    // Allocate boot params
    err = linux_allocate_boot_params(boot_params);
    if (err) {
        return err;
    }

    struct linux_setup_header *kernel_header = linux_kernel_header(*boot_params);

    // Load kernel header
    err = android_read_kernel(&android_image, LINUX_SETUP_HEADER_OFFSET, (VOID*) kernel_header, sizeof(*kernel_header));
    if (err) {
        goto boot_params_err;
    }

    // Prepare kernel header
    err = linux_check_kernel_header(kernel_header);
    if (err) {
        goto boot_params_err;
    }

    // Allocate kernel memory
    err = linux_allocate_kernel(kernel_header);
    if (err) {
        goto boot_params_err;
    }

    // Load kernel
    err = android_load_kernel(&android_image, linux_kernel_offset(kernel_header), linux_kernel_pointer(kernel_header));
    if (err) {
        goto kernel_err;
    }

    // Prepare command line
    CHAR8 *cmdline;
    err = linux_allocate_cmdline(kernel_header, &cmdline);
    if (err) {
        goto kernel_err;
    }

    // Copy command line
    android_copy_cmdline(&android_image, cmdline);

    // Allocate ramdisk memory
    err = linux_allocate_ramdisk(kernel_header, android_ramdisk_size(&android_image));
    if (err) {
        goto cmdline_err;
    }

    // Load ramdisk
    err = android_load_ramdisk(&android_image, linux_ramdisk_pointer(kernel_header));
    if (err) {
        goto ramdisk_err;
    }

    // Close partition (not needed anymore)
    image_close(&android_image.image, loader);

    return EFI_SUCCESS;

ramdisk_err:
    linux_free_ramdisk(kernel_header);

cmdline_err:
    linux_free_cmdline(kernel_header);

kernel_err:
    linux_free_kernel(kernel_header);

boot_params_err:
    linux_free_boot_params(*boot_params);

    return err;
}

extern struct graphics_image splash_image;

EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *system_table) {
    InitializeLib(image, system_table);

    EFI_GUID guid;

    // Parse command line
    EFI_GUID *partition_guid = &guid;
    CHAR16 *path = NULL;
    EFI_STATUS err = parse_command_line(image, &partition_guid, &path);
    if (err) {
        return err;
    }

    // Display splash image
    graphics_display_image(&splash_image);

    VOID *boot_params;
    err = load_kernel(image, partition_guid, path, &boot_params);
    if (err) {
        Print(L"Failed to load kernel: %r\n", err);
        return err;
    }

    linux_efi_boot(image, boot_params);
    linux_free(boot_params);
    return EFI_SUCCESS;
}