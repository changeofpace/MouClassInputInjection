/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#include "object_util.h"

#include "log.h"
#include "nt.h"


_Use_decl_annotations_
EXTERN_C
NTSTATUS
ObuQueryNameString(
    PVOID pObject,
    POBJECT_NAME_INFORMATION* ppObjectNameInfo
)
/*++

Routine Description:

    This function is a convenience wrapper for ObQueryNameString.

Parameters:

    pObject - Pointer to the object to be queried.

    ppObjectNameInfo - Returns a pointer to an allocated buffer for the object
        name information for the specified object. If the object is unnamed
        then the unicode string object in the returned buffer is zeroed. The
        buffer is allocated from the NonPaged pool.

Remarks:

    If successful, the caller must free the returned object name information
    buffer by calling ExFreePool.

--*/
{
    POBJECT_NAME_INFORMATION pObjectNameInfo = NULL;
    ULONG cbReturnLength = 0;
    NTSTATUS ntstatus = STATUS_SUCCESS;

    //
    // Zero out parameters.
    //
    *ppObjectNameInfo = NULL;

    ntstatus = ObQueryNameString(pObject, NULL, 0, &cbReturnLength);
    if (STATUS_INFO_LENGTH_MISMATCH != ntstatus)
    {
        ERR_PRINT("ObQueryNameString failed: 0x%X (Unexpected)", ntstatus);
        ntstatus = STATUS_UNSUCCESSFUL;
        goto exit;
    }

    pObjectNameInfo = (POBJECT_NAME_INFORMATION)ExAllocatePool(
        NonPagedPool,
        cbReturnLength);
    if (!pObjectNameInfo)
    {
        ntstatus = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }
    //
    RtlSecureZeroMemory(pObjectNameInfo, cbReturnLength);

    ntstatus = ObQueryNameString(
        pObject,
        pObjectNameInfo,
        cbReturnLength,
        &cbReturnLength);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("ObQueryNameString failed: 0x%X", ntstatus);
        goto exit;
    }

    //
    // Set out parameters.
    //
    *ppObjectNameInfo = pObjectNameInfo;

exit:
    if (!NT_SUCCESS(ntstatus))
    {
        if (pObjectNameInfo)
        {
            ExFreePool(pObjectNameInfo);
        }
    }

    return ntstatus;
}
