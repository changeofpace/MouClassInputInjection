/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#pragma once

#include <Windows.h>

#include "../Common/ioctl.h"

_Check_return_
BOOL
MouInitializeDeviceStackContext(
    _Out_opt_ PMOUSE_DEVICE_STACK_INFORMATION pDeviceStackInformation
);

_Check_return_
BOOL
MouInjectButtonInput(
    _In_ ULONG_PTR ProcessId,
    _In_ USHORT ButtonFlags,
    _In_ USHORT ButtonData
);

_Check_return_
BOOL
MouInjectMovementInput(
    _In_ ULONG_PTR ProcessId,
    _In_ USHORT IndicatorFlags,
    _In_ LONG MovementX,
    _In_ LONG MovementY
);

_Check_return_
BOOL
MouInjectButtonClick(
    _In_ ULONG_PTR ProcessId,
    _In_ USHORT Button,
    _In_ ULONG ReleaseDelayInMilliseconds
);

_Check_return_
BOOL
MouInjectInputPacketUnsafe(
    _In_ ULONG_PTR ProcessId,
    _In_ BOOL UseButtonDevice,
    _In_ PMOUSE_INPUT_DATA pInputPacket
);
