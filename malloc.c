// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2017 lambdadroid

#include "malloc.h"
#include <efilib.h>

#define EFI_PAGE_ALIGN(a) (((a) + EFI_PAGE_MASK) & ~EFI_PAGE_MASK)

static EFI_STATUS load_memory_map(EFI_MEMORY_DESCRIPTOR **buf, UINTN *map_size, UINTN *desc_size) {
    EFI_STATUS err;
    UINTN map_key;
    UINT32 desc_version;

    *map_size = sizeof(**buf) * 32;

get_map:
    err = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, *map_size, (VOID**) buf);
    if (err) {
        return err;
    }

    err = uefi_call_wrapper(BS->GetMemoryMap, 5, map_size, *buf, &map_key, desc_size, &desc_version);
    if (err) {
        FreePool(*buf);
        if (err == EFI_BUFFER_TOO_SMALL) {
            // We might create a new descriptor by allocating memory
            *map_size += sizeof(**buf);
            goto get_map;
        }
    }

    return err;
}

/*
 * The following methods were originally based on parts of the Linux kernel: (GPL-2.0)
 *   drivers/firmware/efi/libstub/efi-stub-helper.c (efi_high_alloc/efi_low_alloc)
 *
 * Simplified quite a bit for use in android-efi.
 */

EFI_STATUS malloc_high(UINTN size, EFI_PHYSICAL_ADDRESS *addr) {
    EFI_MEMORY_DESCRIPTOR *buf;
    UINTN map_size, desc_size;
    EFI_STATUS err = load_memory_map(&buf, &map_size, &desc_size);
    if (err) {
        return err;
    }

    // Align size to EFI_PAGE_SIZE
    size = EFI_PAGE_ALIGN(size);
    UINTN nr_pages = size / EFI_PAGE_SIZE;

    // Align max to EFI_PAGE_SIZE (round down) and subtract aligned size
    EFI_PHYSICAL_ADDRESS max = (*addr & ~EFI_PAGE_MASK) - size;

    err = EFI_NOT_FOUND;

    for (UINTN d = (UINTN) buf + map_size; d >= (UINTN) buf; d -= desc_size) {
        EFI_MEMORY_DESCRIPTOR *desc = (EFI_MEMORY_DESCRIPTOR*) d;
        if (desc->Type != EfiConventionalMemory || desc->NumberOfPages < nr_pages) {
            continue;
        }

        *addr = desc->PhysicalStart + (desc->NumberOfPages * EFI_PAGE_SIZE) - size;
        if (*addr > max) {
            *addr = max;
        }

        // Make sure we're still in the memory region
        if (*addr && *addr >= desc->PhysicalStart) {
            err = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAddress, EfiLoaderData, nr_pages, addr);
            if (err) {
                Print(L"Cannot allocate at %d: %r\n", *addr, err);
            }

            if (err == EFI_SUCCESS) {
                goto done;
            }
        }
    }

done:
    FreePool(buf);
    return err;
}

EFI_STATUS malloc_low(UINTN size, UINTN align, EFI_PHYSICAL_ADDRESS *addr) {
    EFI_MEMORY_DESCRIPTOR *buf;
    UINTN map_size, desc_size;
    EFI_STATUS err = load_memory_map(&buf, &map_size, &desc_size);
    if (err) {
        return err;
    }

    UINTN align_mask;
    if (align < EFI_PAGE_SIZE) {
        // Need to align to at least the EFI page size
        align = EFI_PAGE_SIZE;
        align_mask = EFI_PAGE_MASK;
    } else {
        align_mask = align - 1;
    }

    // Align size to EFI_PAGE_SIZE
    size = EFI_PAGE_ALIGN(size);
    UINTN nr_pages = size / EFI_PAGE_SIZE;

    err = EFI_NOT_FOUND;

    for (UINTN d = (UINTN) buf, map_end = d + map_size; d < map_end; d += desc_size) {
        EFI_MEMORY_DESCRIPTOR *desc = (EFI_MEMORY_DESCRIPTOR*) d;
        if (desc->Type != EfiConventionalMemory || desc->NumberOfPages < nr_pages) {
            continue;
        }

        EFI_PHYSICAL_ADDRESS max = desc->PhysicalStart + (desc->NumberOfPages * EFI_PAGE_SIZE) - size;
        *addr = desc->PhysicalStart;

        // Avoid allocating at 0x0
        if (*addr == 0x0) {
            *addr = align;
        } else {
            // Align address
            *addr = (*addr + align_mask) & ~align_mask;
        }

        // Make sure we're still in the memory region
        if (*addr <= max) {
            err = uefi_call_wrapper(BS->AllocatePages, 4, AllocateAddress, EfiLoaderData, nr_pages, addr);
            if (err) {
                Print(L"Cannot allocate at %d: %r\n", *addr, err);
            }

            if (err == EFI_SUCCESS) {
                goto done;
            }
        }
    }

done:
    FreePool(buf);
    return err;
}
