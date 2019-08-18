/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#include "mouse_input_injection.h"

#include <ntddmou.h>

#include "driver.h"
#include "log.h"
#include "ntdll.h"

#include "../Common/mouse_input_validation.h"
#include "../Common/time.h"


//=============================================================================
// Public Interface
//=============================================================================
_Use_decl_annotations_
BOOL
MouInitializeDeviceStackContext(
    PMOUSE_DEVICE_STACK_INFORMATION pDeviceStackInformation
)
/*++

Routine Description:

    Initializes the mouse device stack context required by the kernel input
    injection interface.

Parameters:

    pDeviceStackInformation - Returns information about the newly initialized
        mouse device stack context.

Remarks:

    This routine must be invoked and succeed before the Mouse Input Injection
    interface can be used.

--*/
{
    MOUSE_DEVICE_STACK_INFORMATION DeviceStackInformation = {};
    BOOL status = TRUE;

    //
    // Zero out parameters.
    //
    if (ARGUMENT_PRESENT(pDeviceStackInformation))
    {
        RtlSecureZeroMemory(
            pDeviceStackInformation,
            sizeof(*pDeviceStackInformation));
    }

    INF_PRINT("Initializing the mouse device stack context for the driver.");
    INF_PRINT("Waiting for user to provide mouse button and movement input.");

    status = DrvInitializeMouseDeviceStackContext(&DeviceStackInformation);
    if (!status)
    {
        ERR_PRINT("DrvInitializeMouseDeviceStackContext failed: %u",
            GetLastError());
        goto exit;
    }

    //
    // Set out parameters.
    //
    if (ARGUMENT_PRESENT(pDeviceStackInformation))
    {
        RtlCopyMemory(
            pDeviceStackInformation,
            &DeviceStackInformation,
            sizeof(MOUSE_DEVICE_STACK_INFORMATION));
    }

exit:
    return status;
}


_Use_decl_annotations_
BOOL
MouInjectButtonInput(
    ULONG_PTR ProcessId,
    USHORT ButtonFlags,
    USHORT ButtonData
)
/*++

Routine Description:

    Injects a mouse input data packet reflecting the specified input data into
    the input stream in the process context of the specified process id.

Parameters:

    ProcessId - The process id of the process context in which the input
        injection occurs.

    ButtonFlags - The button flags to be injected.

    ButtonData - The button data to be injected.

Remarks:

    This routine validates the specified input data.

    If this routine fails and GetLastError() returns
    ERROR_DEVICE_REINITIALIZATION_NEEDED then the caller must invoke
    MouInitializeDeviceStackContext to initialize the mouse device stack
    context for the driver.

--*/
{
    BOOL status = TRUE;

    status = MivValidateButtonInput(ButtonFlags, ButtonData);
    if (!status)
    {
        ERR_PRINT("MivValidateButtonInput failed.");
        goto exit;
    }

    status = DrvInjectMouseButtonInput(ProcessId, ButtonFlags, ButtonData);
    if (!status)
    {
        ERR_PRINT("DrvInjectMouseButtonInput failed: %u", GetLastError());
        goto exit;
    }

exit:
    return status;
}


_Use_decl_annotations_
BOOL
MouInjectMovementInput(
    ULONG_PTR ProcessId,
    USHORT IndicatorFlags,
    LONG MovementX,
    LONG MovementY
)
/*++

Routine Description:

    Injects a mouse input data packet reflecting the specified input data into
    the input stream in the process context of the specified process id.

Parameters:

    ProcessId - The process id of the process context in which the input
        injection occurs.

    IndicatorFlags - The indicator flags to be injected.

    MovementX - The X direction movement data to be injected.

    MovementY - The Y direction movement data to be injected.

Remarks:

    This routine validates the specified input data.

    If this routine fails and GetLastError() returns
    ERROR_DEVICE_REINITIALIZATION_NEEDED then the caller must invoke
    MouInitializeDeviceStackContext to initialize the mouse device stack
    context for the driver.

--*/
{
    BOOL status = TRUE;

    status = MivValidateMovementInput(IndicatorFlags, MovementX, MovementY);
    if (!status)
    {
        ERR_PRINT("MivValidateMovementInput failed.");
        goto exit;
    }

    status = DrvInjectMouseMovementInput(
        ProcessId,
        IndicatorFlags,
        MovementX,
        MovementY);
    if (!status)
    {
        ERR_PRINT("DrvInjectMouseMovementInput failed: %u", GetLastError());
        goto exit;
    }

exit:
    return status;
}


_Use_decl_annotations_
BOOL
MouInjectButtonClick(
    ULONG_PTR ProcessId,
    USHORT Button,
    ULONG ReleaseDelayInMilliseconds
)
/*++

Routine Description:

    Emulates a mouse-button-click action using the following strategy:

        1. Inject a mouse input data packet for a mouse-button-down action.

        2. Wait for the specified duration.

        3. Inject a mouse input data packet for the corresponding
            mouse-button-up action.

    Both injections occur in the process context of the specified process id.

Parameters:

    ProcessId - The process id of the process context in which the input
        injection occurs.

    Button - The button state indicator value to be used for the emulated
        click. This must be one of the MOUSE_*_DOWN flags defined in ntddmou.h.

    ReleaseDelayInMilliseconds - The approximate duration in milliseconds
        to wait between the two input injections.

Remarks:

    This routine validates the specified input data.

    If this routine fails and GetLastError() returns
    ERROR_DEVICE_REINITIALIZATION_NEEDED then the caller must invoke
    MouInitializeDeviceStackContext to initialize the mouse device stack
    context for the driver.

--*/
{
    LARGE_INTEGER DelayInterval = {};
    USHORT ReleaseButton = 0;
    NTSTATUS ntstatus = STATUS_SUCCESS;
    BOOL status = TRUE;

    //
    // Validate in parameters.
    //
    if (INFINITE == ReleaseDelayInMilliseconds)
    {
        ERR_PRINT("Invalid release delay.");
        SetLastError(ERROR_INVALID_PARAMETER);
        status = FALSE;
        goto exit;
    }

    //
    // Determine the release button flag.
    //
    switch (Button)
    {
        case MOUSE_LEFT_BUTTON_DOWN:
            ReleaseButton = MOUSE_LEFT_BUTTON_UP;
            break;

        case MOUSE_RIGHT_BUTTON_DOWN:
            ReleaseButton = MOUSE_RIGHT_BUTTON_UP;
            break;

        case MOUSE_MIDDLE_BUTTON_DOWN:
            ReleaseButton = MOUSE_MIDDLE_BUTTON_UP;
            break;

        case MOUSE_BUTTON_4_DOWN:
            ReleaseButton = MOUSE_BUTTON_4_UP;
            break;

        case MOUSE_BUTTON_5_DOWN:
            ReleaseButton = MOUSE_BUTTON_5_UP;
            break;

        default:
            ERR_PRINT("Unexpected Button: 0x%hX", Button);
            status = FALSE;
            goto exit;
    }

    MakeRelativeIntervalMilliseconds(
        &DelayInterval,
        ReleaseDelayInMilliseconds);

    status = DrvInjectMouseButtonInput(ProcessId, Button, 0);
    if (!status)
    {
        ERR_PRINT("DrvInjectMouseButtonInput failed: %u", GetLastError());
        goto exit;
    }

    ntstatus = NtDelayExecution(FALSE, &DelayInterval);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("NtDelayExecution failed: 0x%X", ntstatus);
        status = FALSE;
        goto exit;
    }

    status = DrvInjectMouseButtonInput(ProcessId, ReleaseButton, 0);
    if (!status)
    {
        ERR_PRINT("DrvInjectMouseButtonInput failed: %u", GetLastError());
        goto exit;
    }

exit:
    return status;
}


_Use_decl_annotations_
BOOL
MouInjectInputPacketUnsafe(
    ULONG_PTR ProcessId,
    BOOL UseButtonDevice,
    PMOUSE_INPUT_DATA pInputPacket
)
/*++

Routine Description:

    Injects the specified mouse input data packet into the input stream in the
    process context of the specified process id.

Parameters:

    ProcessId - The process id of the process context in which the input
        injection occurs.

    UseButtonDevice - Indicates whether the input packet should be injected
        using the mouse button device or the mouse movement device.

    pInputPacket - The mouse input packet to be injected.

Remarks:

    WARNING This routine does not validate the specified input data.

    If this routine fails and GetLastError() returns
    ERROR_DEVICE_REINITIALIZATION_NEEDED then the caller must invoke
    MouInitializeDeviceStackContext to initialize the mouse device stack
    context for the driver.

--*/
{
    BOOL status = TRUE;

    status = DrvInjectMouseInputPacket(
        ProcessId,
        UseButtonDevice,
        pInputPacket);
    if (!status)
    {
        ERR_PRINT("DrvInjectMouseInputPacket failed: %u", GetLastError());
        goto exit;
    }

exit:
    return status;
}
