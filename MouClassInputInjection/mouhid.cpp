/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#include "mouhid.h"

#include <kbdmou.h>

#include "io_util.h"
#include "log.h"
#include "nt.h"
#include "pe.h"


//=============================================================================
// Constants
//=============================================================================
#define MODULE_TITLE    "MouHid Context"

//
// We choose a search limit by adding a small delta to the actual connect data
//  offset. This delta accounts for the field offset changing in future
//  versions of the MouHid driver.
//
// The actual offset can be found in the IOCTL_INTERNAL_MOUSE_CONNECT case of
//  the MouHid IRP_MJ_INTERNAL_DEVICE_CONTROL handler, mouhid!MouHid_IOCTL:
//
//  Platform:   Windows 7 x64
//  File:       mouhid.sys
//  Version:    6.1.7600.16385 (win7_rtm.090713-1255)
//  Ioctl Code: 0xF0203 (IOCTL_INTERNAL_MOUSE_CONNECT)
//
//  Annotated Assembly:
//
//      mov     rax, [rsi+_IO_STACK_LOCATION.Parameters.DeviceIoControl.Type3InputBuffer]
//      movdqu  xmm0, xmmword ptr [rax+CONNECT_DATA.ClassDeviceObject]
//      movdqu  xmmword ptr [r12+MOUHID_DEVICE_EXTENSION.ConnectData_B8.ClassDeviceObject], xmm0
//
//  Unannotated Assembly:
//
//      mov     rax, [rsi+20h]
//      movdqu  xmm0, xmmword ptr [rax]
//      movdqu  xmmword ptr [r12+0B8h], xmm0
//
#define DEVICE_EXTENSION_SEARCH_SIZE    0x100


//=============================================================================
// Private Types
//=============================================================================
typedef struct _MOUHID_CONTEXT
{
    //
    // The field offset, in bytes, of the CONNECT_DATA object in the device
    //  extension of a MouHid device object.
    //
    SIZE_T ConnectDataFieldOffset;

} MOUHID_CONTEXT, *PMOUHID_CONTEXT;


//=============================================================================
// Module Globals
//=============================================================================
EXTERN_C static MOUHID_CONTEXT g_MhdContext = {};


//=============================================================================
// Private Prototypes
//=============================================================================
_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Check_return_
EXTERN_C
static
NTSTATUS
MhdpResolveConnectDataFieldOffsetForDevice(
    _In_ PDEVICE_OBJECT pDeviceObject,
    _Out_ PSIZE_T pcbConnectDataFieldOffset
);

_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Check_return_
EXTERN_C
static
NTSTATUS
MhdpResolveConnectDataFieldOffset(
    _Out_ PSIZE_T pcbConnectDataFieldOffset
);


//=============================================================================
// Meta Interface
//=============================================================================
_Use_decl_annotations_
EXTERN_C
NTSTATUS
MhdDriverEntry()
/*++

Routine Description:

    Initializes the MouHid Context module.

Required Modules:

    None

--*/
{
    SIZE_T cbConnectDataFieldOffset = 0;
    NTSTATUS ntstatus = STATUS_SUCCESS;

    DBG_PRINT("Loading %s.\n", MODULE_TITLE);

    ntstatus = MhdpResolveConnectDataFieldOffset(&cbConnectDataFieldOffset);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("MhdpResolveConnectDataFieldOffset failed: 0x%X\n",
            ntstatus);
        goto exit;
    }

    //
    // Initialize the global context.
    //
    g_MhdContext.ConnectDataFieldOffset = cbConnectDataFieldOffset;

    DBG_PRINT("%s loaded:\n", MODULE_TITLE);
    DBG_PRINT("    ConnectDataFieldOffset:      0x%IX\n",
        g_MhdContext.ConnectDataFieldOffset);

exit:
    return ntstatus;
}


//=============================================================================
// Public Interface
//=============================================================================
_Use_decl_annotations_
EXTERN_C
SIZE_T
MhdGetConnectDataFieldOffset()
{
    return g_MhdContext.ConnectDataFieldOffset;
}


//=============================================================================
// Private Interface
//=============================================================================
_Use_decl_annotations_
EXTERN_C
static
NTSTATUS
MhdpResolveConnectDataFieldOffsetForDevice(
    PDEVICE_OBJECT pDeviceObject,
    PSIZE_T pcbConnectDataFieldOffset
)
/*++

Routine Description:

    Dynamically resolves the field offset of the CONNECT_DATA object in the
    device extension of the specified MouHid device object.

Parameters:

    pDeviceObject - Referenced pointer to a MouHid device object.

    pcbConnectDataFieldOffset - Returns the field offset of the CONNECT_DATA
        for the specified MouHid device object.

Remarks:

    This routine utilizes knowledge of the MouClass communication protocol as a
    heuristic for resolving the CONNECT_DATA field offset.

    Heuristic:

        1. Obtain a list of all the MouHid device objects.

        2. For each MouHid device object:

            2.a Get the device object attached to the MouHid device object.

            2.b Get the address range of every executable image section in the
                driver of the attached device object.

            2.c Search the device extension of the MouHid device object for a
                valid CONNECT_DATA object by interpreting each pointer-aligned
                address as a CONNECT_DATA candidate. A candidate is valid if it
                meets the following criteria:

                    i. The 'ClassDeviceObject' field matches the attached
                        device object.

                    ii. The 'ClassService' field points to an address contained
                        in one of the executable image sections from (2.b).

    NOTE This heuristic may be applicable to other types of mouse device
    stacks, e.g., PS/2.

--*/
{
    PDEVICE_OBJECT pAttachedDevice = NULL;
    PVOID pDriverStart = NULL;
    ULONG_PTR ImageBase = 0;
    PIMAGE_SECTION_HEADER* ppExecutableSections = NULL;
    ULONG nExecutableSections = 0;
    PVOID pDeviceExtension = NULL;
    SIZE_T Span = 0;
    ULONG_PTR SearchEnd = 0;
    PCONNECT_DATA pConnectData = NULL;
    ULONG i = 0;
    ULONG_PTR SectionBase = 0;
    BOOLEAN fClassServiceValidated = FALSE;
    SIZE_T cbConnectDataFieldOffset = 0;
    NTSTATUS ntstatus = STATUS_SUCCESS;

    //
    // Zero out parameters.
    //
    *pcbConnectDataFieldOffset = 0;

    DBG_PRINT(
        "Resolving MouHid connect data field offset for device."
        " (DeviceObject = %p)\n",
        pDeviceObject);

    //
    // Get a referenced pointer to the device object attached to the current
    //  MouHid device object.
    //
    pAttachedDevice = IouGetUpperDeviceObject(pDeviceObject);
    if (!pAttachedDevice)
    {
        ERR_PRINT("Unexpected AttachedDevice. (DeviceObject = %p)\n",
            pDeviceObject);
        ntstatus = STATUS_UNSUCCESSFUL;
        goto exit;
    }

    //
    // Get the executable image sections for the driver of the attached device.
    //
    pDriverStart = pAttachedDevice->DriverObject->DriverStart;
    if (!pDriverStart)
    {
        ERR_PRINT("Unexpected DriverStart. (DriverObject = %p)\n",
            pAttachedDevice->DriverObject);
        ntstatus = STATUS_UNSUCCESSFUL;
        goto exit;
    }

    if (!RtlPcToFileHeader(pDriverStart, (PVOID*)&ImageBase))
    {
        ERR_PRINT("RtlPcToFileHeader failed. (PcValue = %p)\n", pDriverStart);
        ntstatus = STATUS_UNSUCCESSFUL;
        goto exit;
    }

    ntstatus = PeGetExecutableSections(
        ImageBase,
        &ppExecutableSections,
        &nExecutableSections);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("PeGetExecutableSections failed: 0x%X\n", ntstatus);
        goto exit;
    }

    pDeviceExtension = pDeviceObject->DeviceExtension;

    //
    // Clamp the search range.
    //
    Span = ADDRESS_AND_SIZE_TO_SPAN_PAGES(
        pDeviceExtension,
        DEVICE_EXTENSION_SEARCH_SIZE);
    if (1 == Span)
    {
        SearchEnd =
            (ULONG_PTR)pDeviceExtension + DEVICE_EXTENSION_SEARCH_SIZE;
    }
    else
    {
        SearchEnd = (ULONG_PTR)(
            PAGE_ALIGN((ULONG_PTR)pDeviceExtension + PAGE_SIZE));
    }

    //
    // Search the device extension for a valid connect data object.
    //
    for (pConnectData = (PCONNECT_DATA)pDeviceExtension;
        (ULONG_PTR)pConnectData + sizeof(*pConnectData) <= SearchEnd;
        pConnectData =
            OFFSET_POINTER(pConnectData, sizeof(ULONG_PTR), CONNECT_DATA))
    {
        //
        // Filter invalid ClassDeviceObject values.
        //
        if (pConnectData->ClassDeviceObject != pAttachedDevice)
        {
            continue;
        }

        //
        // Filter ClassService values which do not point to an address inside
        //  an executable image section in the driver of the attached device
        //  object.
        //
        for (i = 0; i < nExecutableSections; ++i)
        {
            SectionBase = ImageBase + ppExecutableSections[i]->VirtualAddress;

            if ((ULONG_PTR)pConnectData->ClassService >= SectionBase &&
                (ULONG_PTR)pConnectData->ClassService <
                    SectionBase + ppExecutableSections[i]->Misc.VirtualSize)
            {
                fClassServiceValidated = TRUE;
                break;
            }
        }
        //
        if (!fClassServiceValidated)
        {
            continue;
        }

        //
        // If we found multiple matches then our heuristic is flawed.
        //
        if (cbConnectDataFieldOffset)
        {
            ERR_PRINT("Found multiple field offset candidates.\n");
            ntstatus = STATUS_INTERNAL_ERROR;
            goto exit;
        }

        cbConnectDataFieldOffset =
            POINTER_OFFSET(pConnectData, pDeviceExtension);
    }
    //
    if (!cbConnectDataFieldOffset)
    {
        ERR_PRINT(
            "Failed to resolve connect data field offset for device."
            " (DeviceObject = %p)\n",
            pDeviceObject);
        ntstatus = STATUS_UNSUCCESSFUL;
        goto exit;
    }

    //
    // Set out parameters.
    //
    *pcbConnectDataFieldOffset = cbConnectDataFieldOffset;

exit:
    if (ppExecutableSections)
    {
        ExFreePool(ppExecutableSections);
    }

    if (pAttachedDevice)
    {
        ObDereferenceObject(pAttachedDevice);
    }

    return ntstatus;
}


_Use_decl_annotations_
EXTERN_C
static
NTSTATUS
MhdpResolveConnectDataFieldOffset(
    PSIZE_T pcbConnectDataFieldOffset
)
{
    UNICODE_STRING usMouHidDriverObject = {};
    PDRIVER_OBJECT pMouHidDriverObject = NULL;
    BOOLEAN fHasDriverObjectReference = FALSE;
    PDEVICE_OBJECT* ppMouHidDeviceObjectList = NULL;
    ULONG nMouHidDeviceObjectList = 0;
    ULONG i = 0;
    SIZE_T cbFieldOffset = 0;
    SIZE_T cbFieldOffsetCandidate = 0;
    NTSTATUS ntstatus = STATUS_SUCCESS;

    //
    // Zero out parameters.
    //
    *pcbConnectDataFieldOffset = 0;

    DBG_PRINT("Resolving MouHid connect data field offset.\n");

    //
    // Open the MouHid driver object.
    //
    usMouHidDriverObject = RTL_CONSTANT_STRING(MOUHID_DRIVER_OBJECT_PATH_U);

    ntstatus = ObReferenceObjectByName(
        &usMouHidDriverObject,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL,
        0,
        *IoDriverObjectType,
        KernelMode,
        NULL,
        (PVOID*)&pMouHidDriverObject);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("ObReferenceObjectByName failed: 0x%X\n", ntstatus);
        goto exit;
    }
    //
    fHasDriverObjectReference = TRUE;

    //
    // Get a snapshot of all the MouHid device objects.
    //
    ntstatus = IouEnumerateDeviceObjectList(
        pMouHidDriverObject,
        &ppMouHidDeviceObjectList,
        &nMouHidDeviceObjectList);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("IouEnumerateDeviceObjectList failed: 0x%X\n", ntstatus);
        goto exit;
    }

#if defined(DBG)
    IouPrintDeviceObjectList(
        MOUHID_DRIVER_OBJECT_PATH_U,
        ppMouHidDeviceObjectList,
        nMouHidDeviceObjectList);
#endif

    //
    // Apply the connect data heuristic to every MouHid device object to verify
    //  that each device object yields the same offset.
    //
    for (i = 0; i < nMouHidDeviceObjectList; ++i)
    {
        ntstatus = MhdpResolveConnectDataFieldOffsetForDevice(
            ppMouHidDeviceObjectList[i],
            &cbFieldOffset);
        if (!NT_SUCCESS(ntstatus))
        {
            ERR_PRINT(
                "MhdpResolveConnectDataFieldOffsetForDevice failed: 0x%X\n",
                ntstatus);
            goto exit;
        }

        if (cbFieldOffsetCandidate)
        {
            if (cbFieldOffsetCandidate != cbFieldOffset)
            {
                ERR_PRINT("Found multiple field offset candidates.\n");
                ntstatus = STATUS_INTERNAL_ERROR;
                goto exit;
            }
        }
        else
        {
            cbFieldOffsetCandidate = cbFieldOffset;
        }
    }
    //
    if (!cbFieldOffsetCandidate)
    {
        ERR_PRINT("Failed to resolve connect data field offset.\n");
        ntstatus = STATUS_UNSUCCESSFUL;
        goto exit;
    }

    //
    // Set out parameters.
    //
    *pcbConnectDataFieldOffset = cbFieldOffsetCandidate;

exit:
    if (ppMouHidDeviceObjectList)
    {
        IouFreeDeviceObjectList(
            ppMouHidDeviceObjectList,
            nMouHidDeviceObjectList);
    }

    if (fHasDriverObjectReference)
    {
        ObDereferenceObject(pMouHidDriverObject);
    }

    return ntstatus;
}
