// SPDX-License-Identifier: GPL-2.0
/*
 * android-efi
 * Copyright (C) 2017 lambdadroid
 */

#include "partition.h"
#include <efilib.h>

static EFI_STATUS partition_find(EFI_GUID *guid, EFI_HANDLE *handle) {
    UINTN handle_count;
    EFI_HANDLE *handles;
    EFI_STATUS err = LibLocateHandleByDiskSignature(MBR_TYPE_EFI_PARTITION_TABLE_HEADER, SIGNATURE_TYPE_GUID,
                                                    guid, &handle_count, &handles);
    if (err) {
        goto out;
    }

    if (handle_count != 1) {
        if (handle_count == 0) {
            Print(L"Partition not found: %g\n", guid);
            err = EFI_NO_MEDIA;
        } else {
            Print(L"Ambiguous partition GUID: %g\n", guid);
            err = EFI_VOLUME_CORRUPTED;
        }

        goto out;
    }

    *handle = handles[0];

    out:
    FreePool(handles);
    return err;
}

EFI_STATUS partition_open(struct efi_partition *partition, EFI_HANDLE image, EFI_GUID *partition_guid) {
    EFI_STATUS err = partition_find(partition_guid, &partition->partition_handle);
    if (err) {
        return err;
    }

    err = uefi_call_wrapper(BS->OpenProtocol, 6, partition->partition_handle, &BlockIoProtocol,
                            (VOID**) &partition->block_io, image, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if (err) {
        Print(L"Failed to open BlockIO protocol for partition\n");
        return err;
    }

    err = uefi_call_wrapper(BS->OpenProtocol, 6, partition->partition_handle, &DiskIoProtocol,
                            (VOID**) &partition->disk_io, image, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if (err) {
        Print(L"Failed to open DiskIO protocol for partition\n");
        return err;
    }

    return EFI_SUCCESS;
}

VOID partition_close(struct efi_partition *partition, EFI_HANDLE image) {
    EFI_STATUS err = uefi_call_wrapper(BS->CloseProtocol, 4, partition->partition_handle, &DiskIoProtocol, image, NULL);
    if (err) {
        Print(L"Failed to close DiskIO protocol for partition: %r\n", err);
    }

    err = uefi_call_wrapper(BS->CloseProtocol, 4, partition->partition_handle, &BlockIoProtocol, image, NULL);
    if (err) {
        Print(L"Failed to close BlockIO protocol for partition: %r\n", err);
    }
}
