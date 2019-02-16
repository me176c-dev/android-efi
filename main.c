// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2017 lambdadroid

#include <efi.h>
#include <efilib.h>

#include "string.h"
#include "guid.h"
#include "android.h"
#include "linux.h"
#include "graphics.h"

#ifndef ANDROID_EFI_VERSION
#define ANDROID_EFI_VERSION  "unknown"
#endif

struct android_efi_options {
    EFI_GUID guid;
    EFI_GUID *partition_guid;
    CHAR16 *path;

    const CHAR16 *kernel_parameters;
    UINTN kernel_parameters_length;
};

enum command_line_argument {
    INVALID,
    FLAG,
    KERNEL_PARAMETERS,

    // Positional
    FIRST_ARGUMENT,
    IMAGE,
};

#define is_flag(flag, len, f) ((len) == STRING_LENGTH(f) && CompareMem(flag, f, STRING_LENGTH(f)) == 0)

static EFI_STATUS parse_flag(const CHAR16 *flag, UINTN len) {
    if (is_flag(flag, len, L"--version")) {
        Print(L"android-efi version " ANDROID_EFI_VERSION "\n");
        return EFI_ABORTED;
    }

    CHAR16 *copy = StrnDuplicate(flag, len);
    Print(L"Unknown command line flag: %s\n", copy);
    FreePool(copy);
    return EFI_INVALID_PARAMETER;
}

static EFI_STATUS parse_image(struct android_efi_options *options, const CHAR16 *opt, UINTN len, UINTN path) {
    if (path) {
        options->path = StrnDuplicate(opt, len);

        // Clean path (replace forward slashes with backward slashes)
        for (UINTN i = 0; i < len; ++i) {
            if (options->path[i] == L'/') {
                options->path[i] = L'\\';
            }
        }
    } else if (len) {
        options->partition_guid = &options->guid;
        if (!guid_parse(options->partition_guid, opt, len)) {
            CHAR16 *guid = StrnDuplicate(opt, len);
            Print(L"Expected valid partition GUID, got '%s'\n", guid);
            FreePool(guid);
            return EFI_INVALID_PARAMETER;
        }
    }

    return EFI_SUCCESS;
}

#define STATE_DASHES       3
#define STATE_SKIP_SPACES  (1 << 2)
#define STATE_IMAGE_PATH   (1 << 3)

static EFI_STATUS parse_command_line(EFI_LOADED_IMAGE_PROTOCOL *loaded_image, struct android_efi_options *options) {
    const CHAR16 *opt = loaded_image->LoadOptions;
    if (!opt) {
        goto out;
    }

    UINTN state = STATE_SKIP_SPACES;
    enum command_line_argument argument = FIRST_ARGUMENT;
    enum command_line_argument last_argument = INVALID;
    UINTN start = 0;

    EFI_STATUS err = EFI_SUCCESS;
    for (UINTN i = 0, length = loaded_image->LoadOptionsSize / sizeof(*opt); i < length; ++i) {
        CHAR16 c = opt[i];

        if (state & STATE_DASHES) {
            if (c == L'-' && (++state & STATE_DASHES) < STATE_DASHES) {
                continue;
            }

            // Detect two consecutive dashes ("--") that
            // separate positional arguments from additional
            // kernel parameters (or are part of command line flags)
            if ((state & STATE_DASHES) == 2) {
                if (c == L' ') {
                    argument = KERNEL_PARAMETERS;
                    state |= STATE_SKIP_SPACES;
                } else if (c) {
                    last_argument = argument;
                    argument = FLAG;
                }
            }

            state &= ~STATE_DASHES;
        }

        if (state & STATE_SKIP_SPACES) {
            // Skip redundant spaces between arguments
            if (c == L' ') {
                continue;
            }

            state &= ~STATE_SKIP_SPACES;
            start = i;

            if (c == L'-') {
                ++state; // Count dash
                continue;
            }
        }

        if (c && c != L' ') {
            switch (argument) {
                case IMAGE:
                    if (!(state & STATE_IMAGE_PATH) && (c == L'/' || c == L'\\')) {
                        break;
                    }
                    continue;

                case KERNEL_PARAMETERS:
                    options->kernel_parameters = &opt[i];
                    options->kernel_parameters_length = length - i;
                    goto out; // Break the loop

                default:
                    continue;
            }
        }

        // Reached end of argument
        UINTN len = i - start;

        switch (argument) {
            case IMAGE:
                err = parse_image(options, &opt[start], len, state & STATE_IMAGE_PATH);
                if (err) {
                    goto end;
                }

                state |= STATE_IMAGE_PATH; // Parse path next
                break;

            case KERNEL_PARAMETERS:
                goto out; // Break the loop

            case FLAG:
                err = parse_flag(&opt[start], len);
                if (err) {
                    goto end;
                }
                break;

            default:
                break;
        }

        if (!c) {
            break; // Cancel early when encountering a null terminator
        }

        if (c == L' ') {
            if (argument == FLAG) {
                argument = last_argument;
            } else {
                ++argument;
            }

            state |= STATE_SKIP_SPACES;
        }

        start = i;
    }

out:
    if (!options->partition_guid && !options->path) {
        err = EFI_INVALID_PARAMETER;
        Print(L"Usage: android.efi <Boot Partition GUID/Path> [-- Additional Kernel Parameters...]\n");
    }

end:
    if (err) {
        FreePool(options->path);
    }
    return err;
}

static EFI_STATUS prepare_cmdline(struct linux_setup_header *kernel_header, const struct android_image *android_image,
        const struct android_efi_options *options) {
    EFI_STATUS err = linux_allocate_cmdline(kernel_header);
    if (err) {
        return err;
    }

    CHAR8 *cmdline = linux_cmdline_pointer(kernel_header);
    CHAR8 *cmdline_end = cmdline + LINUX_CMDLINE_SIZE;

    if (options->kernel_parameters) {
        // TODO: Check extra kernel parameters length?

        // Prepend extra kernel parameters
        cmdline = str_utf16_to_utf8(cmdline, options->kernel_parameters, options->kernel_parameters_length);

        if (!cmdline[-1]) {
            // Replace null terminator with space
            cmdline[-1] = ' ';
        } else {
            *cmdline++ = ' ';
        }
    }

    return android_copy_cmdline(android_image, cmdline, cmdline_end - cmdline);
}

#define RAMDISK_OPTION       "initrd="
#define MAX_RAMDISK_COUNT    4
#define MAX_PATH_LENGTH      256

static const CHAR8 *find_initrd_option(const CHAR8 *cmdline) {
    const CHAR8 *tail = cmdline;
    for (const CHAR8 *first = cmdline + STRING_LENGTH(RAMDISK_OPTION); cmdline < first && *cmdline; ++cmdline);
    for (; *cmdline; ++cmdline, ++tail) {
        if (CompareMem(tail, RAMDISK_OPTION, STRING_LENGTH(RAMDISK_OPTION)) == 0) {
            return cmdline;
        }
    }
    return NULL;
}

static EFI_STATUS load_ramdisk_cmdline(EFI_HANDLE loader_device, const CHAR8 *cmdline,
        struct linux_setup_header *kernel_header, const struct android_image *android_image) {
    EFI_FILE_HANDLE dir = LibOpenRoot(loader_device);
    if (!dir) {
        Print(L"Failed to open root directory\n");
        return EFI_VOLUME_CORRUPTED;
    }

    UINT32 size = android_ramdisk_size(android_image);

    EFI_STATUS err;
    struct {
        EFI_FILE_HANDLE handle;
        UINTN size;
    } files[MAX_RAMDISK_COUNT];
    UINTN n;
    for (n = 0; n < MAX_RAMDISK_COUNT; ++n) {
        // Skip leading slashes
        while (*cmdline == '/' || *cmdline == '\\')
            ++cmdline;

        // Convert to long char
        CHAR16 path[MAX_PATH_LENGTH];
        CHAR16 *p = path, *p_last = path + MAX_PATH_LENGTH - 1;
        for (; *cmdline && *cmdline != ' ' && *cmdline != '\n'; ++cmdline) {
            if (p >= p_last) {
                continue;
            }

            if (*cmdline == '/') {
                *p++ = '\\';
            } else {
                *p++ = *cmdline;
            }
        }
        *p = 0;

        err = uefi_call_wrapper(dir->Open, 5, dir, &files[n].handle, path, EFI_FILE_MODE_READ, 0);
        if (err) {
            --n;
            Print(L"Failed to open initrd '%s'\n", path);
            goto err;
        }

        EFI_FILE_INFO *info = LibFileInfo(files[n].handle);
        if (!info) {
            Print(L"Failed to get file info for initrd '%s'\n", path);
            goto err;
        }
        files[n].size = info->FileSize;
        size += info->FileSize;
        FreePool(info);

        cmdline = find_initrd_option(cmdline);
        if (!cmdline) {
            break;
        }
    }
    if (n == MAX_RAMDISK_COUNT) {
        --n;
        Print(L"Too many ramdisks listed as 'initrd=' option. Maximum supported are: %d\n", MAX_RAMDISK_COUNT);
        err = EFI_OUT_OF_RESOURCES;
        goto err;
    }

    err = linux_allocate_ramdisk(kernel_header, size);
    if (err) {
        goto err;
    }

    // Load cmdline ramdisks
    UINTN ramdisk = (UINTN) linux_ramdisk_pointer(kernel_header);
    for (UINTN i = 0; i <= n; ++i) {
        err = uefi_call_wrapper(files[i].handle->Read, 3, files[i].handle, &files[i].size, (VOID*) ramdisk);
        if (err) {
            Print(L"Failed to read initrd %d\n", i);
            goto err;
        }

        ramdisk += files[i].size;
    }

    err = android_load_ramdisk(android_image, (VOID*) ramdisk);

err:
    for (UINTN i = 0; i <= n; ++i) {
        uefi_call_wrapper(files[i].handle->Close, 1, files[i].handle);
    }
    uefi_call_wrapper(dir->Close, 1, dir);
    return err;
}

static EFI_STATUS load_ramdisk(EFI_HANDLE loader_device,
        struct linux_setup_header *kernel_header, const struct android_image *android_image) {
    const CHAR8 *initrd = find_initrd_option(linux_cmdline_pointer(kernel_header));
    if (initrd) {
        return load_ramdisk_cmdline(loader_device, initrd, kernel_header, android_image);
    }

    EFI_STATUS err = linux_allocate_ramdisk(kernel_header, android_ramdisk_size(android_image));
    if (err) {
        return err;
    }

    return android_load_ramdisk(android_image, linux_ramdisk_pointer(kernel_header));
}

static EFI_STATUS load_kernel(EFI_HANDLE loader, EFI_HANDLE loader_device,
                              const struct android_efi_options *options, VOID **boot_params) {
    struct android_image android_image;
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

    err = linux_allocate_boot_params(boot_params);
    if (err) {
        return err;
    }

    struct linux_setup_header *kernel_header = linux_kernel_header(*boot_params);
    err = android_read_kernel(&android_image, LINUX_SETUP_HEADER_OFFSET, (VOID*) kernel_header, sizeof(*kernel_header));
    if (err) {
        goto err;
    }

    err = linux_check_kernel_header(kernel_header);
    if (err) {
        goto err;
    }

    err = linux_allocate_kernel(kernel_header);
    if (err) {
        goto err;
    }

    err = android_load_kernel(&android_image, linux_kernel_offset(kernel_header), linux_kernel_pointer(kernel_header));
    if (err) {
        goto err;
    }

    err = prepare_cmdline(kernel_header, &android_image, options);
    if (err) {
        goto err;
    }

    err = load_ramdisk(loader_device, kernel_header, &android_image);
    if (err) {
        goto err;
    }

    // Close image (not needed anymore)
    image_close(&android_image.image, loader);

    return EFI_SUCCESS;

err:
    linux_free(*boot_params);
    return err;
}

extern const struct graphics_image splash_image;

EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *system_table) {
    InitializeLib(image, system_table);

    EFI_LOADED_IMAGE_PROTOCOL *loaded_image;
    EFI_STATUS err = uefi_call_wrapper(BS->OpenProtocol, 6, image, &LoadedImageProtocol, (VOID**) &loaded_image,
                                       image, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if (err) {
        Print(L"Failed to open LoadedImageProtocol\n");
        return err;
    }

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
