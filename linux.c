// SPDX-License-Identifier: GPL-2.0
/*
 * android-efi
 * Copyright (C) 2017 lambdadroid
 */

#include "linux.h"
#include "malloc.h"
#include <efilib.h>

#define SETUP_BOOT_FLAG        0xAA55
#define SETUP_HEADER_MAGIC     0x53726448  /* HdrS */
#define MIN_KERNEL_VERSION     0x20c       /* For EFI Handover protocol */
#define ANDROID_EFI_LOADER_ID  0xFF

#ifdef __x86_64__
#define XLF_EFI_HANDOVER       (1<<3)      /* XLF_EFI_HANDOVER_64 */
#else
#define XLF_EFI_HANDOVER       (1<<2)      /* XLF_EFI_HANDOVER_32 */
#endif

EFI_STATUS linux_allocate_boot_params(VOID **boot_params) {
    EFI_PHYSICAL_ADDRESS addr;
    EFI_STATUS err = malloc_low(LINUX_BOOT_PARAMS_SIZE, 0x1, &addr);
    if (err) {
        return err;
    }

    ZeroMem((VOID*) addr, LINUX_BOOT_PARAMS_SIZE);
    *boot_params = (VOID*) addr;
    return EFI_SUCCESS;
}

EFI_STATUS linux_check_kernel_header(const struct linux_setup_header *header) {
    // Check that this is actually a kernel header
    if (header->boot_flag != SETUP_BOOT_FLAG || header->header != SETUP_HEADER_MAGIC) {
        Print(L"Kernel does not contain a valid setup header. Boot flag: 0x%x, header: 0x%x\n",
              header->boot_flag, header->header);
        return EFI_VOLUME_CORRUPTED;
    }

    // Check setup header version
    if (header->version < MIN_KERNEL_VERSION) {
        Print(L"Kernel version too old: 0x%x\n", header->version);
        return EFI_UNSUPPORTED;
    }

    // Check that kernel supports the EFI handover protocol
    if (!(header->xloadflags & XLF_EFI_HANDOVER)) {
        Print(L"Kernel does not support the EFI handover protocol. xloadflags: 0x%x\n", header->xloadflags);
        return EFI_UNSUPPORTED;
    }

    return EFI_SUCCESS;
}

EFI_STATUS linux_allocate_kernel(struct linux_setup_header *header) {
    // Attempt to allocate preferred address
    EFI_PHYSICAL_ADDRESS addr = header->pref_address;
    EFI_STATUS err = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAddress, EfiLoaderData,
                                       EFI_SIZE_TO_PAGES(header->init_size), &addr);
    if (err) {
        if (!header->relocatable_kernel) {
            Print(L"Failed to allocate preferred kernel address for non relocatable kernel: 0x%x\n", header->pref_address);
            return err;
        }

        // Allocate as low as possible with the given alignment
        err = malloc_low(header->init_size, header->kernel_alignment, &addr);
        if (err) {
            return err;
        }
    }

    header->code32_start = (UINT32) addr;
    return EFI_SUCCESS;
}

EFI_STATUS linux_allocate_ramdisk(struct linux_setup_header *header, UINT32 size) {
    EFI_PHYSICAL_ADDRESS addr = header->initrd_addr_max;
    EFI_STATUS err = malloc_high(size, &addr);
    if (err) {
        return err;
    }

    header->ramdisk_image = (UINT32) addr;
    header->ramdisk_size = size;
    return EFI_SUCCESS;
}

EFI_STATUS linux_allocate_cmdline(struct linux_setup_header *header, CHAR8 **cmdline) {
    EFI_PHYSICAL_ADDRESS addr = UINT32_MAX;
    EFI_STATUS err = malloc_high(EFI_PAGE_SIZE, &addr);
    if (err) {
        return err;
    }

    *cmdline = (CHAR8*) addr;
    header->cmd_line_ptr = (UINT32) addr;
    return EFI_SUCCESS;
}

// EFI handover protocol

#ifdef __x86_64__
typedef VOID(*handover_f)(EFI_HANDLE image, EFI_SYSTEM_TABLE *table, VOID *boot_params);
#else
typedef VOID(*handover_f)(EFI_HANDLE image, EFI_SYSTEM_TABLE *table, VOID *boot_params) __attribute__((regparm(0)));
#endif

VOID linux_efi_boot(EFI_HANDLE image, VOID *boot_params) {
    struct linux_setup_header *header = linux_kernel_header(boot_params);
    header->type_of_loader = ANDROID_EFI_LOADER_ID;

    handover_f handover;

#ifdef __x86_64__
    asm volatile ("cli");
    handover = (handover_f) ((UINTN) header->code32_start + 512 + header->handover_offset);
#else
    handover = (handover_f) ((UINTN) header->code32_start + header->handover_offset);
#endif

    handover(image, ST, boot_params);
}

inline VOID linux_free_boot_params(VOID *boot_params) {
    EFI_STATUS err = uefi_call_wrapper(BS->FreePages, 2, (EFI_PHYSICAL_ADDRESS) boot_params,
                                       EFI_SIZE_TO_PAGES(LINUX_BOOT_PARAMS_SIZE));
    if (err) {
        Print(L"Failed to free boot params: %r\n", err);
    }
}

inline VOID linux_free_kernel(const struct linux_setup_header *header) {
    EFI_STATUS err = uefi_call_wrapper(BS->FreePages, 2, (EFI_PHYSICAL_ADDRESS) header->code32_start,
                                       EFI_SIZE_TO_PAGES(header->init_size));
    if (err) {
        Print(L"Failed to free kernel: %r\n", err);
    }
}

inline VOID linux_free_ramdisk(const struct linux_setup_header *header) {
    EFI_STATUS err = uefi_call_wrapper(BS->FreePages, 2, (EFI_PHYSICAL_ADDRESS) header->ramdisk_image,
                                       EFI_SIZE_TO_PAGES(header->ramdisk_size));
    if (err) {
        Print(L"Failed to free ramdisk: %r\n", err);
    }
}

inline VOID linux_free_cmdline(const struct linux_setup_header *header) {
    EFI_STATUS err = uefi_call_wrapper(BS->FreePages, 2, (EFI_PHYSICAL_ADDRESS) header->cmd_line_ptr,
                                       EFI_SIZE_TO_PAGES(header->cmdline_size));
    if (err) {
        Print(L"Failed to free cmdline: %r\n", err);
    }
}

VOID linux_free(VOID *boot_params) {
    struct linux_setup_header *header = linux_kernel_header(boot_params);

    linux_free_kernel(header);
    linux_free_ramdisk(header);
    linux_free_cmdline(header);
    linux_free_boot_params(boot_params);
}
