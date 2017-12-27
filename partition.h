// SPDX-License-Identifier: GPL-2.0
/*
 * android-efi
 * Copyright (C) 2017 lambdadroid
 */

#ifndef ANDROID_EFI_PARTITION_H
#define ANDROID_EFI_PARTITION_H

#include <efi.h>

struct efi_partition {
    EFI_HANDLE    partition_handle;
    EFI_BLOCK_IO  *block_io;
    EFI_DISK_IO   *disk_io;
};

EFI_STATUS partition_open(struct efi_partition *partition, EFI_HANDLE image, EFI_GUID *partition_guid);

static inline EFI_STATUS partition_read(const struct efi_partition *partition, UINT64 offset, VOID *buffer, UINTN buffer_size) {
    return uefi_call_wrapper(partition->disk_io->ReadDisk, 5, partition->disk_io, partition->block_io->Media->MediaId,
                             offset, buffer_size, buffer);
}

VOID partition_close(struct efi_partition *partition, EFI_HANDLE image);

#endif //ANDROID_EFI_PARTITION_H
