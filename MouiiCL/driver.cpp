/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#include "driver.h"

#include <ntddmou.h>

#include "debug.h"

#include "../Common/ioctl.h"


//=============================================================================
// Private Types
//=============================================================================
typedef struct _DRIVER_CONTEXT
{
    HANDLE DeviceHandle;

} DRIVER_CONTEXT, *PDRIVER_CONTEXT;


//=============================================================================
// Module Globals
//=============================================================================
static DRIVER_CONTEXT g_DriverContext = {};


//=============================================================================
// Meta Interface
//=============================================================================
_Use_decl_annotations_
BOOL
DrvInitialization()
{
    HANDLE hDevice = INVALID_HANDLE_VALUE;
    BOOL status = TRUE;

    hDevice = CreateFileW(
        LOCAL_DEVICE_PATH_U,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (INVALID_HANDLE_VALUE == hDevice)
    {
        status = FALSE;
        goto exit;
    }

    //
    // Initialize the global context.
    //
    g_DriverContext.DeviceHandle = hDevice;

exit:
    if (!status)
    {
        if (INVALID_HANDLE_VALUE != hDevice)
        {
            VERIFY(CloseHandle(hDevice));
        }
    }

    return status;
}


VOID
DrvTermination()
{
    VERIFY(CloseHandle(g_DriverContext.DeviceHandle));
}


//=============================================================================
// Public Interface
//=============================================================================

//
// Suppress signed/unsigned mismatch warnings for Ioctl codes.
//
#pragma warning(push)
#pragma warning(disable:4245)

_Use_decl_annotations_
BOOL
DrvInitializeMouseDeviceStackContext(
    PMOUSE_DEVICE_STACK_INFORMATION pDeviceStackInformation
)
{
    MOUSE_DEVICE_STACK_INFORMATION DeviceStackInformation = {};
    DWORD cbReturned = 0;
    BOOL status = TRUE;

    //
    // Zero out parameters.
    //
    RtlSecureZeroMemory(
        pDeviceStackInformation,
        sizeof(*pDeviceStackInformation));

    status = DeviceIoControl(
        g_DriverContext.DeviceHandle,
        IOCTL_INITIALIZE_MOUSE_DEVICE_STACK_CONTEXT,
        NULL,
        0,
        &DeviceStackInformation,
        sizeof(DeviceStackInformation),
        &cbReturned,
        NULL);
    if (!status)
    {
        goto exit;
    }

    //
    // Set out parameters.
    //
    RtlCopyMemory(
        pDeviceStackInformation,
        &DeviceStackInformation,
        sizeof(MOUSE_DEVICE_STACK_INFORMATION));

exit:
    return status;
}


_Use_decl_annotations_
BOOL
DrvInjectMouseButtonInput(
    ULONG_PTR ProcessId,
    USHORT ButtonFlags,
    USHORT ButtonData
)
{
    INJECT_MOUSE_BUTTON_INPUT_REQUEST Request = {};
    DWORD cbReturned = 0;
    BOOL status = TRUE;

    //
    // Initialize the request.
    //
    Request.ProcessId = ProcessId;
    Request.ButtonFlags = ButtonFlags;
    Request.ButtonData = ButtonData;

    status = DeviceIoControl(
        g_DriverContext.DeviceHandle,
        IOCTL_INJECT_MOUSE_BUTTON_INPUT,
        &Request,
        sizeof(Request),
        NULL,
        0,
        &cbReturned,
        NULL);
    if (!status)
    {
        goto exit;
    }

exit:
    return status;
}


_Use_decl_annotations_
BOOL
DrvInjectMouseMovementInput(
    ULONG_PTR ProcessId,
    USHORT IndicatorFlags,
    LONG MovementX,
    LONG MovementY
)
{
    INJECT_MOUSE_MOVEMENT_INPUT_REQUEST Request = {};
    DWORD cbReturned = 0;
    BOOL status = TRUE;

    //
    // Initialize the request.
    //
    Request.ProcessId = ProcessId;
    Request.IndicatorFlags = IndicatorFlags;
    Request.MovementX = MovementX;
    Request.MovementY = MovementY;

    status = DeviceIoControl(
        g_DriverContext.DeviceHandle,
        IOCTL_INJECT_MOUSE_MOVEMENT_INPUT,
        &Request,
        sizeof(Request),
        NULL,
        0,
        &cbReturned,
        NULL);
    if (!status)
    {
        goto exit;
    }

exit:
    return status;
}


_Use_decl_annotations_
BOOL
DrvInjectMouseInputPacket(
    ULONG_PTR ProcessId,
    BOOL UseButtonDevice,
    PMOUSE_INPUT_DATA pInputPacket
)
{
    INJECT_MOUSE_INPUT_PACKET_REQUEST Request = {};
    DWORD cbReturned = 0;
    BOOL status = TRUE;

    //
    // Initialize the request.
    //
    Request.ProcessId = ProcessId;
    Request.UseButtonDevice = UseButtonDevice ? TRUE : FALSE;
    RtlCopyMemory(
        &Request.InputPacket,
        pInputPacket,
        sizeof(MOUSE_INPUT_DATA));

    status = DeviceIoControl(
        g_DriverContext.DeviceHandle,
        IOCTL_INJECT_MOUSE_INPUT_PACKET,
        &Request,
        sizeof(Request),
        NULL,
        0,
        &cbReturned,
        NULL);
    if (!status)
    {
        goto exit;
    }

exit:
    return status;
}

#pragma warning(pop) // disable:4245
