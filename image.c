// SPDX-License-Identifier: GPL-2.0
/*
 * android-efi
 * Copyright (C) 2017 lambdadroid
 */

#include "image.h"
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

static EFI_STATUS partition_open(struct efi_image *image, EFI_HANDLE loader) {
    EFI_STATUS err = uefi_call_wrapper(BS->OpenProtocol, 6, image->partition_handle, &BlockIoProtocol,
                            (VOID**) &image->partition.block_io, loader, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if (err) {
        Print(L"Failed to open BlockIO protocol\n");
        return err;
    }

    err = uefi_call_wrapper(BS->OpenProtocol, 6, image->partition_handle, &DiskIoProtocol,
                            (VOID**) &image->partition.disk_io, loader, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if (err) {
        Print(L"Failed to open DiskIO protocol\n");
        return err;
    }

    return EFI_SUCCESS;
}

static inline EFI_STATUS partition_read(const struct efi_image_partition *partition, UINT64 offset, VOID *buffer, UINTN buffer_size) {
    return uefi_call_wrapper(partition->disk_io->ReadDisk, 5, partition->disk_io, partition->block_io->Media->MediaId,
                             offset, buffer_size, buffer);
}

static inline VOID partition_close(const struct efi_image *image, EFI_HANDLE loader) {
    EFI_STATUS err = uefi_call_wrapper(BS->CloseProtocol, 4, image->partition_handle, &DiskIoProtocol, loader, NULL);
    if (err) {
        Print(L"Failed to close DiskIO protocol: %r\n", err);
    }

    err = uefi_call_wrapper(BS->CloseProtocol, 4, image->partition_handle, &BlockIoProtocol, loader, NULL);
    if (err) {
        Print(L"Failed to close BlockIO protocol: %r\n", err);
    }
}

static EFI_STATUS file_open(struct efi_image *image, EFI_HANDLE loader, CHAR16 *path) {
    EFI_STATUS err;
    EFI_HANDLE handle = image->partition_handle;
    if (handle) {
        image->partition_handle = NULL; // Used to differentiate between partition/file
    } else {
        // Load from the partition we were loaded on

        EFI_LOADED_IMAGE *loaded_image;
        err = uefi_call_wrapper(BS->OpenProtocol, 6, loader, &LoadedImageProtocol, (VOID **) &loaded_image,
                                loader, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
        if (err) {
            Print(L"Failed to open LoadedImageProtocol\n");
            return err;
        }

        handle = loaded_image->DeviceHandle;
    }

    image->file.dir = LibOpenRoot(handle);
    if (!image->file.dir) {
        Print(L"Failed to open root directory of partition\n");
        return EFI_VOLUME_CORRUPTED;
    }

    err = uefi_call_wrapper(image->file.dir->Open, 5, image->file.dir, &image->file.file, path, EFI_FILE_MODE_READ, 0);
    if (err) {
        Print(L"Failed to open boot image file\n");
        return err;
    }

    return EFI_SUCCESS;
}

static inline EFI_STATUS file_read(const struct efi_image_file *file, UINT64 offset, VOID *buffer, UINTN buffer_size) {
    EFI_STATUS err = uefi_call_wrapper(file->file->SetPosition, 2, file->file, offset);
    if (err) {
        return err;
    }

    return uefi_call_wrapper(file->file->Read, 3, file->file, &buffer_size, buffer);
}

static inline VOID file_close(const struct efi_image_file *file) {
    EFI_STATUS err = uefi_call_wrapper(file->file->Close, 1, file->file);
    if (err) {
        Print(L"Failed to close image file: %r\n", err);
    }

    err = uefi_call_wrapper(file->dir->Close, 1, file->dir);
    if (err) {
        Print(L"Failed to close image directory: %r\n", err);
    }
}

EFI_STATUS image_open(struct efi_image *image, EFI_HANDLE loader, EFI_GUID *partition_guid, CHAR16 *path) {
    EFI_STATUS err;
    if (partition_guid) {
        err = partition_find(partition_guid, &image->partition_handle);
        if (err) {
            return err;
        }
    } else {
        image->partition_handle = NULL;
    }

    if (path) {
        // File
        return file_open(image, loader, path);
    } else {
        // Partition
        return partition_open(image, loader);
    }
}

EFI_STATUS image_read(const struct efi_image *image, UINT64 offset, VOID *buffer, UINTN buffer_size) {
    if (image->partition_handle) {
        return partition_read(&image->partition, offset, buffer, buffer_size);
    } else {
        return file_read(&image->file, offset, buffer, buffer_size);
    }
}

VOID image_close(const struct efi_image *image, EFI_HANDLE loader) {
    if (image->partition_handle) {
        partition_close(image, loader);
    } else {
        file_close(&image->file);
    }
}
