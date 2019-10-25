/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#include "driver.h"

#include "debug.h"
#include "log.h"
#include "mouclass_input_injection.h"
#include "mouhid.h"
#include "mouhid_hook_manager.h"

#include "../Common/ioctl.h"


//=============================================================================
// Meta Interface
//=============================================================================
_Use_decl_annotations_
EXTERN_C
NTSTATUS
DriverEntry(
    PDRIVER_OBJECT pDriverObject,
    PUNICODE_STRING pRegistryPath
)
{
    PDEVICE_OBJECT pDeviceObject = NULL;
    UNICODE_STRING usDeviceName = {};
    UNICODE_STRING usSymbolicLinkName = {};
    BOOLEAN fSymbolicLinkCreated = FALSE;
    BOOLEAN fMclLoaded = FALSE;
    BOOLEAN fMhkLoaded = FALSE;
    BOOLEAN fMiiLoaded = FALSE;
    NTSTATUS ntstatus = STATUS_SUCCESS;

    UNREFERENCED_PARAMETER(pRegistryPath);

    DBG_PRINT("Loading %ls.", NT_DEVICE_NAME_U);

    usDeviceName = RTL_CONSTANT_STRING(NT_DEVICE_NAME_U);

    ntstatus = IoCreateDevice(
        pDriverObject,
        0,
        &usDeviceName,
        FILE_DEVICE_MOUCLASS_INPUT_INJECTION,
        FILE_DEVICE_SECURE_OPEN,
        TRUE,
        &pDeviceObject);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("IoCreateDevice failed: 0x%X", ntstatus);
        goto exit;
    }
    //
    pDriverObject->MajorFunction[IRP_MJ_CREATE] =
        MouClassInputInjectionDispatchCreate;
    pDriverObject->MajorFunction[IRP_MJ_CLOSE] =
        MouClassInputInjectionDispatchClose;
    pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] =
        MouClassInputInjectionDispatchDeviceControl;
    pDriverObject->DriverUnload = MouClassInputInjectionDriverUnload;

    //
    // Create a symbolic link for the user mode client.
    //
    usSymbolicLinkName = RTL_CONSTANT_STRING(SYMBOLIC_LINK_NAME_U);

    ntstatus = IoCreateSymbolicLink(&usSymbolicLinkName, &usDeviceName);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("IoCreateSymbolicLink failed: 0x%X", ntstatus);
        goto exit;
    }
    //
    fSymbolicLinkCreated = TRUE;

    //
    // Load the driver modules.
    //
    ntstatus = MhdDriverEntry();
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("MhdDriverEntry failed: 0x%X", ntstatus);
        goto exit;
    }

    ntstatus = MclDriverEntry(pDriverObject);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("MclDriverEntry failed: 0x%X", ntstatus);
        goto exit;
    }
    //
    fMclLoaded = TRUE;

    ntstatus = MhkDriverEntry();
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("MhkDriverEntry failed: 0x%X", ntstatus);
        goto exit;
    }
    //
    fMhkLoaded = TRUE;

    ntstatus = MiiDriverEntry();
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("MiiDriverEntry failed: 0x%X", ntstatus);
        goto exit;
    }
    //
    fMiiLoaded = TRUE;

    DBG_PRINT("%ls loaded.", NT_DEVICE_NAME_U);

exit:
    if (!NT_SUCCESS(ntstatus))
    {
        if (fMiiLoaded)
        {
            MiiDriverUnload();
        }

        if (fMhkLoaded)
        {
            MhkDriverUnload();
        }

        if (fMclLoaded)
        {
            MclDriverUnload();
        }

        if (fSymbolicLinkCreated)
        {
            VERIFY(IoDeleteSymbolicLink(&usSymbolicLinkName));
        }

        if (pDeviceObject)
        {
            IoDeleteDevice(pDeviceObject);
        }
    }

    return ntstatus;
}


_Use_decl_annotations_
EXTERN_C
VOID
MouClassInputInjectionDriverUnload(
    PDRIVER_OBJECT pDriverObject
)
{
    UNICODE_STRING usSymbolicLinkName = {};

    DBG_PRINT("Unloading %ls.", NT_DEVICE_NAME_U);

    //
    // Unload the driver modules.
    //
    MiiDriverUnload();
    MhkDriverUnload();
    MclDriverUnload();

    //
    // Release driver resources.
    //
    usSymbolicLinkName = RTL_CONSTANT_STRING(SYMBOLIC_LINK_NAME_U);

    VERIFY(IoDeleteSymbolicLink(&usSymbolicLinkName));

    if (pDriverObject->DeviceObject)
    {
        IoDeleteDevice(pDriverObject->DeviceObject);
    }

    DBG_PRINT("%ls unloaded.", NT_DEVICE_NAME_U);
}


//=============================================================================
// Public Interface
//=============================================================================
_Use_decl_annotations_
EXTERN_C
NTSTATUS
MouClassInputInjectionDispatchCreate(
    PDEVICE_OBJECT pDeviceObject,
    PIRP pIrp
)
{
    UNREFERENCED_PARAMETER(pDeviceObject);
    DBG_PRINT("Processing IRP_MJ_CREATE.");
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}


_Use_decl_annotations_
EXTERN_C
NTSTATUS
MouClassInputInjectionDispatchClose(
    PDEVICE_OBJECT pDeviceObject,
    PIRP pIrp
)
{
    UNREFERENCED_PARAMETER(pDeviceObject);
    DBG_PRINT("Processing IRP_MJ_CLOSE.");
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}


_Use_decl_annotations_
EXTERN_C
NTSTATUS
MouClassInputInjectionDispatchDeviceControl(
    PDEVICE_OBJECT pDeviceObject,
    PIRP pIrp
)
{
    PIO_STACK_LOCATION pIrpStack = IoGetCurrentIrpStackLocation(pIrp);
    PVOID pSystemBuffer = pIrp->AssociatedIrp.SystemBuffer;
    ULONG cbInput = pIrpStack->Parameters.DeviceIoControl.InputBufferLength;
    ULONG cbOutput = pIrpStack->Parameters.DeviceIoControl.OutputBufferLength;
    PINITIALIZE_MOUSE_DEVICE_STACK_CONTEXT_REPLY
        pInitMouseDeviceStackContextReply = NULL;
    PINJECT_MOUSE_BUTTON_INPUT_REQUEST pInjectMouseButtonInputRequest = NULL;
    PINJECT_MOUSE_MOVEMENT_INPUT_REQUEST pInjectMouseMovementInputRequest =
        NULL;
    PINJECT_MOUSE_INPUT_PACKET_REQUEST pInjectMouseInputPacketRequest = NULL;
    ULONG_PTR Information = 0;
    NTSTATUS ntstatus = STATUS_SUCCESS;

    UNREFERENCED_PARAMETER(pDeviceObject);

    switch (pIrpStack->Parameters.DeviceIoControl.IoControlCode)
    {
        case IOCTL_INITIALIZE_MOUSE_DEVICE_STACK_CONTEXT:
            DBG_PRINT(
                "Processing IOCTL_INITIALIZE_MOUSE_DEVICE_STACK_CONTEXT.");

            if (cbInput)
            {
                ntstatus = STATUS_INVALID_PARAMETER_4;
                goto exit;
            }

            pInitMouseDeviceStackContextReply =
                (PINITIALIZE_MOUSE_DEVICE_STACK_CONTEXT_REPLY)pSystemBuffer;
            if (!pInitMouseDeviceStackContextReply)
            {
                ntstatus = STATUS_INVALID_PARAMETER_5;
                goto exit;
            }

            if (sizeof(*pInitMouseDeviceStackContextReply) != cbOutput)
            {
                ntstatus = STATUS_INVALID_PARAMETER_6;
                goto exit;
            }

            ntstatus = MiiInitializeMouseDeviceStackContext(
                &pInitMouseDeviceStackContextReply->DeviceStackInformation);
            if (!NT_SUCCESS(ntstatus))
            {
                ERR_PRINT("MiiInitializeMouseDeviceStackContext failed: 0x%X",
                    ntstatus);
                goto exit;
            }

            Information = sizeof(*pInitMouseDeviceStackContextReply);

            break;

        case IOCTL_INJECT_MOUSE_BUTTON_INPUT:
            DBG_PRINT("Processing IOCTL_INJECT_MOUSE_BUTTON_INPUT.");

            pInjectMouseButtonInputRequest =
                (PINJECT_MOUSE_BUTTON_INPUT_REQUEST)pSystemBuffer;
            if (!pInjectMouseButtonInputRequest)
            {
                ntstatus = STATUS_INVALID_PARAMETER_3;
                goto exit;
            }

            if (sizeof(*pInjectMouseButtonInputRequest) != cbInput)
            {
                ntstatus = STATUS_INVALID_PARAMETER_4;
                goto exit;
            }

            if (cbOutput)
            {
                ntstatus = STATUS_INVALID_PARAMETER_6;
                goto exit;
            }

            ntstatus = MiiInjectMouseButtonInput(
                (HANDLE)pInjectMouseButtonInputRequest->ProcessId,
                pInjectMouseButtonInputRequest->ButtonFlags,
                pInjectMouseButtonInputRequest->ButtonData);
            if (!NT_SUCCESS(ntstatus))
            {
                ERR_PRINT("MiiInjectMouseButtonInput failed: 0x%X", ntstatus);
                goto exit;
            }

            break;

        case IOCTL_INJECT_MOUSE_MOVEMENT_INPUT:
            DBG_PRINT("Processing IOCTL_INJECT_MOUSE_MOVEMENT_INPUT.");

            pInjectMouseMovementInputRequest =
                (PINJECT_MOUSE_MOVEMENT_INPUT_REQUEST)pSystemBuffer;
            if (!pInjectMouseMovementInputRequest)
            {
                ntstatus = STATUS_INVALID_PARAMETER_3;
                goto exit;
            }

            if (sizeof(*pInjectMouseMovementInputRequest) != cbInput)
            {
                ntstatus = STATUS_INVALID_PARAMETER_4;
                goto exit;
            }

            if (cbOutput)
            {
                ntstatus = STATUS_INVALID_PARAMETER_6;
                goto exit;
            }

            ntstatus = MiiInjectMouseMovementInput(
                (HANDLE)pInjectMouseMovementInputRequest->ProcessId,
                pInjectMouseMovementInputRequest->IndicatorFlags,
                pInjectMouseMovementInputRequest->MovementX,
                pInjectMouseMovementInputRequest->MovementY);
            if (!NT_SUCCESS(ntstatus))
            {
                ERR_PRINT("MiiInjectMouseMovementInput failed: 0x%X",
                    ntstatus);
                goto exit;
            }

            break;

        case IOCTL_INJECT_MOUSE_INPUT_PACKET:
            DBG_PRINT("Processing IOCTL_INJECT_MOUSE_INPUT_PACKET.");

            pInjectMouseInputPacketRequest =
                (PINJECT_MOUSE_INPUT_PACKET_REQUEST)pSystemBuffer;
            if (!pInjectMouseInputPacketRequest)
            {
                ntstatus = STATUS_INVALID_PARAMETER_3;
                goto exit;
            }

            if (sizeof(*pInjectMouseInputPacketRequest) != cbInput)
            {
                ntstatus = STATUS_INVALID_PARAMETER_4;
                goto exit;
            }

            if (cbOutput)
            {
                ntstatus = STATUS_INVALID_PARAMETER_6;
                goto exit;
            }

            ntstatus = MiiInjectMouseInputPacketUnsafe(
                (HANDLE)pInjectMouseInputPacketRequest->ProcessId,
                pInjectMouseInputPacketRequest->UseButtonDevice,
                &pInjectMouseInputPacketRequest->InputPacket);
            if (!NT_SUCCESS(ntstatus))
            {
                ERR_PRINT("MiiInjectMouseInputPacketUnsafe failed: 0x%X",
                    ntstatus);
                goto exit;
            }

            break;

        default:
            ERR_PRINT(
                "Unhandled IOCTL."
                " (MajorFunction = %hhu, MinorFunction = %hhu)",
                pIrpStack->MajorFunction,
                pIrpStack->MinorFunction);
            ntstatus = STATUS_UNSUCCESSFUL;
            goto exit;
    }

exit:
    pIrp->IoStatus.Information = Information;
    pIrp->IoStatus.Status = ntstatus;

    IoCompleteRequest(pIrp, IO_NO_INCREMENT);

    return ntstatus;
}
