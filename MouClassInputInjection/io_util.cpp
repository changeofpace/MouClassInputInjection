/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#include "io_util.h"

#include "log.h"
#include "object_util.h"


_Use_decl_annotations_
EXTERN_C
NTSTATUS
IouEnumerateDeviceObjectList(
    PDRIVER_OBJECT pDriverObject,
    PDEVICE_OBJECT** pppDeviceObjectList,
    PULONG pnDeviceObjectList
)
/*++

Routine Description:

    A convenience wrapper for IoEnumerateDeviceObjectList.

Parameters:

    pDriverObject - Pointer to the target driver object.

    pppDeviceObjectList - Returns a pointer to an allocated device object list
        for the target driver object. This function increments the reference
        count of every device object in the list. The list is allocated from
        the NonPaged pool.

    pnDeviceObjectList - Returns the number of elements in the allocated list.

Remarks:

    If successful, the caller must free the returned list by calling
    IouFreeDeviceObjectList.

    NOTE The returned device object list is a snapshot of the devices which
    were attached to the target driver object at the time of the call.

--*/
{
    ULONG NumberOfElements = 0;
    PDEVICE_OBJECT* ppDeviceObjectList = NULL;
    ULONG cbDeviceObjectList = 0;
    ULONG nDeviceObjectList = 0;
    NTSTATUS ntstatus = STATUS_SUCCESS;

    //
    // Zero out parameters.
    //
    *pppDeviceObjectList = NULL;
    *pnDeviceObjectList = 0;

    //
    // NOTE We start with an initial list size of one so that we do not attempt
    //  to allocate zero memory.
    //
    for (NumberOfElements = 1;; NumberOfElements = nDeviceObjectList)
    {
        cbDeviceObjectList = NumberOfElements * sizeof(*ppDeviceObjectList);

        ppDeviceObjectList = (PDEVICE_OBJECT*)ExAllocatePool(
            NonPagedPool,
            cbDeviceObjectList);
        if (!ppDeviceObjectList)
        {
            ntstatus = STATUS_INSUFFICIENT_RESOURCES;
            goto exit;
        }
        //
        RtlSecureZeroMemory(ppDeviceObjectList, cbDeviceObjectList);

        ntstatus = IoEnumerateDeviceObjectList(
            pDriverObject,
            ppDeviceObjectList,
            cbDeviceObjectList,
            &nDeviceObjectList);
        if (NT_SUCCESS(ntstatus))
        {
            break;
        }
        else if (STATUS_BUFFER_TOO_SMALL != ntstatus)
        {
            ERR_PRINT("IoEnumerateDeviceObjectList failed: 0x%X", ntstatus);
            nDeviceObjectList = NumberOfElements;
            goto exit;
        }

        IouFreeDeviceObjectList(ppDeviceObjectList, NumberOfElements);
    }

    //
    // Set out parameters.
    //
    *pppDeviceObjectList = ppDeviceObjectList;
    *pnDeviceObjectList = nDeviceObjectList;

exit:
    if (!NT_SUCCESS(ntstatus))
    {
        if (ppDeviceObjectList)
        {
            NT_ASSERT(nDeviceObjectList);

            IouFreeDeviceObjectList(ppDeviceObjectList, nDeviceObjectList);
        }
    }

    return ntstatus;
}


_Use_decl_annotations_
EXTERN_C
VOID
IouFreeDeviceObjectList(
    PDEVICE_OBJECT* ppDeviceObjectList,
    ULONG nDeviceObjectList
)
{
    ULONG i = 0;

    for (i = 0; i < nDeviceObjectList; ++i)
    {
        if (ppDeviceObjectList[i])
        {
            ObDereferenceObject(ppDeviceObjectList[i]);
        }
    }

    ExFreePool(ppDeviceObjectList);
}


//
// KeReleaseQueuedSpinLock SAL annotations do not mirror
//  KeAcquireQueuedSpinLock annotations.
//
#pragma warning(suppress: 28166)
//
_Use_decl_annotations_
EXTERN_C
PDEVICE_OBJECT
IouGetUpperDeviceObject(
    PDEVICE_OBJECT pDeviceObject
)
/*++

Routine Description:

    Returns a referenced pointer to the next upper-level device object on the
    driver stack or NULL if there is no upper-level device.

Parameters:

    pDeviceObject - Pointer to the target device object.

Remarks:

    If successful, the caller must dereference the returned device object
    pointer by calling ObDereferenceObject.

--*/
{
    KIRQL PreviousIrql = 0;
    PDEVICE_OBJECT pAttachedDevice = NULL;

    PreviousIrql = KeAcquireQueuedSpinLock(LockQueueIoDatabaseLock);

    pAttachedDevice = pDeviceObject->AttachedDevice;
    if (pAttachedDevice)
    {
        ObReferenceObject(pAttachedDevice);
    }

    KeReleaseQueuedSpinLock(LockQueueIoDatabaseLock, PreviousIrql);

    return pAttachedDevice;
}


#if defined(DBG)
_Use_decl_annotations_
EXTERN_C
VOID
IouPrintDeviceObjectList(
    PWSTR pwzDriverName,
    PDEVICE_OBJECT* ppDeviceObjectList,
    ULONG nDeviceObjectList
)
{
    POBJECT_NAME_INFORMATION pObjectNameInfo = NULL;
    NTSTATUS ntstatus = STATUS_SUCCESS;

    DBG_PRINT("Device Object List (DriverName = %ls):", pwzDriverName);

    for (ULONG i = 0; i < nDeviceObjectList; ++i)
    {
        ntstatus = ObuQueryNameString(ppDeviceObjectList[i], &pObjectNameInfo);
        if (!NT_SUCCESS(ntstatus))
        {
            ERR_PRINT("ObuQueryNameString failed: 0x%X (%u)", ntstatus, i);
            continue;
        }

        if (pObjectNameInfo->Name.Buffer)
        {
            DBG_PRINT("    %u: DeviceObject = %p, Name = %wZ",
                i,
                ppDeviceObjectList[i],
                pObjectNameInfo->Name);
        }
        else
        {
            DBG_PRINT("    %u: DeviceObject = %p, Name = (unnamed)",
                i,
                ppDeviceObjectList[i]);
        }

        ExFreePool(pObjectNameInfo);
    }
}
#endif
