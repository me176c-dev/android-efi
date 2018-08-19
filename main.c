// SPDX-License-Identifier: GPL-2.0
/*
 * android-efi
 * Copyright (C) 2017 lambdadroid
 */

#include <efi.h>
#include <efilib.h>

#include "string.h"
#include "guid.h"
#include "android.h"
#include "linux.h"
#include "graphics.h"

struct android_efi_options {
    EFI_GUID guid;
    EFI_GUID *partition_guid;
    CHAR16 *path;

    CHAR16 *kernel_parameters;
    UINTN kernel_parameters_length;
};

enum command_line_argument {
    FIRST_ARGUMENT,
    IMAGE,
    END,
    KERNEL_PARAMETERS
};

#define STATE_SKIP_SPACES  (1 << 0)
#define STATE_FOUND_DASH   (1 << 1)
#define STATE_IMAGE_PATH   (1 << 2)

static EFI_STATUS parse_command_line(EFI_LOADED_IMAGE_PROTOCOL *loaded_image, struct android_efi_options *options) {
    CHAR16 *opt = loaded_image->LoadOptions;
    if (!opt) {
        goto out;
    }

    UINTN length = loaded_image->LoadOptionsSize / sizeof(CHAR16);

    CHAR16 c;
    UINT8 state = STATE_SKIP_SPACES;
    enum command_line_argument argument = FIRST_ARGUMENT;

    UINTN start = 0;

    for (UINTN i = 0; i < length; ++i) {
        c = opt[i];

        // Detects two consecutive dashes ("--") that separate arguments from
        // additional kernel parameters
        if (state & STATE_FOUND_DASH) {
            if (c == L'-') {
                argument = KERNEL_PARAMETERS;
                state |= STATE_SKIP_SPACES;
                continue;
            } else {
                state &= ~STATE_FOUND_DASH;
            }
        }

        // Skips redundant spaces between arguments
        if (state & STATE_SKIP_SPACES) {
            if (c == L' ') {
                continue;
            } else {
                state &= ~STATE_SKIP_SPACES;
                start = i;

                if (c == L'-') {
                    state |= STATE_FOUND_DASH;
                    continue;
                }
            }
        }

        if (c && c != L' ') {
            switch (argument) {
                case KERNEL_PARAMETERS:
                    options->kernel_parameters = &opt[i];
                    options->kernel_parameters_length = length - i;
                    goto out; // Break the loop

                case IMAGE:
                    if (!(state & STATE_IMAGE_PATH) && (c == L'/' || c == L'\\')) {
                        break;
                    }
                    // fallthrough
                default:
                    continue;
            }
        }

        // Reached end of argument
        UINTN len = i - start;

        switch (argument) {
            case IMAGE:
                if (state & STATE_IMAGE_PATH) {
                    // Parse file path
                    options->path = StrnDuplicate(&opt[start], len);

                    // Cleanup path (replace forward slashes with backward slashes)
                    for (start = 0; start < len; ++start) {
                        if (options->path[start] == L'/') {
                            options->path[start] = L'\\';
                        }
                    }
                } else if (len) {
                    // Parse command line partition GUID
                    options->partition_guid = &options->guid;
                    if (!guid_parse(options->partition_guid, &opt[start], len)) {
                        CHAR16 *guid = StrnDuplicate(&opt[start], len);
                        Print(L"Expected valid partition GUID, got '%s'\n", guid);
                        FreePool(guid);
                        return EFI_INVALID_PARAMETER;
                    }
                }

                state |= STATE_IMAGE_PATH; // Parse path next
                break;

            case KERNEL_PARAMETERS:
                goto out; // Break the loop

            default:
                break;
        }

        if (!c) {
            break; // Cancel early when encountering a null terminator
        }

        if (c == L' ') {
            if (argument != END) {
                ++argument;
            }
            state |= STATE_SKIP_SPACES;
        }

        start = i;
    }

out:
    if (!options->partition_guid && !options->path) {
        Print(L"Usage: android.efi <Boot Partition GUID/Path> [-- Additional Kernel Parameters...]\n");
        goto err;
    }

    return EFI_SUCCESS;

err:
    if (options->path) {
        FreePool(options->path);
    }
    return EFI_INVALID_PARAMETER;
}

static EFI_STATUS load_kernel(EFI_HANDLE loader, EFI_HANDLE loader_device,
                              struct android_efi_options *options, VOID **boot_params) {
    struct android_image android_image;

    // Open partition
    EFI_STATUS err = image_open(&android_image.image, loader, loader_device, options->partition_guid, options->path);
    if (options->path) {
        FreePool(options->path);
    }
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
    CHAR8 *cmdline, *cmdline_end;
    err = linux_allocate_cmdline(kernel_header, &cmdline, &cmdline_end);
    if (err) {
        goto kernel_err;
    }

    if (options->kernel_parameters) {
        // TODO: Check extra kernel parameters length?

        // Prepend extra kernel parameters
        cmdline = str_utf16_to_utf8(cmdline, options->kernel_parameters, options->kernel_parameters_length);

        if (!cmdline[-1]) {
            // Replace null terminator with space
            cmdline[-1] = ' ';
        } else {
            // Add space
            *cmdline++ = ' ';
        }
    }

    // Copy command line
    err = android_copy_cmdline(&android_image, cmdline, cmdline_end - cmdline);
    if (err) {
        goto cmdline_err;
    }

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

    EFI_LOADED_IMAGE_PROTOCOL *loaded_image;
    EFI_STATUS err = uefi_call_wrapper(BS->OpenProtocol, 6, image, &LoadedImageProtocol, (VOID**) &loaded_image,
                                       image, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if (err) {
        Print(L"Failed to open LoadedImageProtocol\n");
        return err;
    }

    // Parse command line
    struct android_efi_options options = {0};
    err = parse_command_line(loaded_image, &options);
    if (err) {
        return err;
    }

    // Display splash image
    graphics_display_image(&splash_image);

    VOID *boot_params;
    err = load_kernel(image, loaded_image->DeviceHandle, &options, &boot_params);
    if (err) {
        Print(L"Failed to load kernel: %r\n", err);
        uefi_call_wrapper(BS->Stall, 1, 3 * 1000 * 1000);
        return err;
    }

    linux_efi_boot(image, boot_params);
    linux_free(boot_params);
    uefi_call_wrapper(BS->CloseProtocol, 4, image, &LoadedImageProtocol, image, NULL);
    return EFI_SUCCESS;
}
