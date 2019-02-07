// SPDX-License-Identifier: GPL-2.0
/*
 * android-efi
 * Copyright (C) 2017 lambdadroid
 */

#ifndef ANDROID_EFI_LINUX_H
#define ANDROID_EFI_LINUX_H

#include <efi.h>

#define LINUX_BOOT_PARAMS_SIZE     0x4000
#define LINUX_SETUP_HEADER_OFFSET  0x01f1
#define LINUX_SETUP_SECT_SIZE      512
#define LINUX_CMDLINE_SIZE         EFI_PAGE_SIZE

/*
 * Taken from the Linux kernel: (GPL-2.0)
 *   arch/x86/include/uapi/asm/bootparam.h (struct setup_header)
 */
struct linux_setup_header {
    UINT8	setup_sects;
    UINT16	root_flags;
    UINT32	syssize;
    UINT16	ram_size;
    UINT16	vid_mode;
    UINT16	root_dev;
    UINT16	boot_flag;
    UINT16	jump;
    UINT32	header;
    UINT16	version;
    UINT32	realmode_swtch;
    UINT16	start_sys_seg;
    UINT16	kernel_version;
    UINT8	type_of_loader;
    UINT8	loadflags;
    UINT16	setup_move_size;
    UINT32	code32_start;
    UINT32	ramdisk_image;
    UINT32	ramdisk_size;
    UINT32	bootsect_kludge;
    UINT16	heap_end_ptr;
    UINT8	ext_loader_ver;
    UINT8	ext_loader_type;
    UINT32	cmd_line_ptr;
    UINT32	initrd_addr_max;
    UINT32	kernel_alignment;
    UINT8	relocatable_kernel;
    UINT8	min_alignment;
    UINT16	xloadflags;
    UINT32	cmdline_size;
    UINT32	hardware_subarch;
    UINT64	hardware_subarch_data;
    UINT32	payload_offset;
    UINT32	payload_length;
    UINT64	setup_data;
    UINT64	pref_address;
    UINT32	init_size;
    UINT32	handover_offset;
} __attribute__((packed));

EFI_STATUS linux_allocate_boot_params(VOID **boot_params);
EFI_STATUS linux_check_kernel_header(const struct linux_setup_header *header);
EFI_STATUS linux_allocate_kernel(struct linux_setup_header *header);
EFI_STATUS linux_allocate_ramdisk(struct linux_setup_header *header, UINT32 size);
EFI_STATUS linux_allocate_cmdline(struct linux_setup_header *header);
VOID linux_efi_boot(EFI_HANDLE image, VOID *boot_params);

static inline UINT64 linux_kernel_offset(const struct linux_setup_header *header) {
    return (UINT64) header->setup_sects * LINUX_SETUP_SECT_SIZE + LINUX_SETUP_SECT_SIZE;
}

static inline struct linux_setup_header* linux_kernel_header(VOID *boot_params) {
    return (VOID*) ((UINTN) boot_params + LINUX_SETUP_HEADER_OFFSET);
}

static inline VOID* linux_kernel_pointer(const struct linux_setup_header *header) {
    return (VOID*) (UINTN) header->code32_start;
}

static inline VOID* linux_ramdisk_pointer(const struct linux_setup_header *header) {
    return (VOID*) (UINTN) header->ramdisk_image;
}

static inline CHAR8* linux_cmdline_pointer(const struct linux_setup_header *header) {
    return (CHAR8*) (UINTN) header->cmd_line_ptr;
}

VOID linux_free(VOID *boot_params);

#endif //ANDROID_EFI_LINUX_H
