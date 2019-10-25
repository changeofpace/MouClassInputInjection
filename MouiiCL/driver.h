/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#pragma once

#include <Windows.h>

#include "../Common/ioctl.h"

//=============================================================================
// Meta Interface
//=============================================================================
_Check_return_
BOOL
MouiiIoInitialization();

VOID
MouiiIoTermination();

//=============================================================================
// Public Interface
//=============================================================================
_Check_return_
BOOL
MouiiIoInitializeMouseDeviceStackContext(
    _Out_ PMOUSE_DEVICE_STACK_INFORMATION pDeviceStackInformation
);

_Check_return_
BOOL
MouiiIoInjectMouseButtonInput(
    _In_ ULONG_PTR ProcessId,
    _In_ USHORT ButtonFlags,
    _In_ USHORT ButtonData
);

_Check_return_
BOOL
MouiiIoInjectMouseMovementInput(
    _In_ ULONG_PTR ProcessId,
    _In_ USHORT IndicatorFlags,
    _In_ LONG MovementX,
    _In_ LONG MovementY
);

_Check_return_
BOOL
MouiiIoInjectMouseInputPacket(
    _In_ ULONG_PTR ProcessId,
    _In_ BOOL UseButtonDevice,
    _In_ PMOUSE_INPUT_DATA pInputPacket
);
