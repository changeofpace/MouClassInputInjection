/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#pragma once

#include <fltKernel.h>

#include <ntddmou.h>

#include "../Common/ioctl.h"

//=============================================================================
// Meta Interface
//=============================================================================
_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Check_return_
EXTERN_C
NTSTATUS
MiiDriverEntry();

_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
EXTERN_C
VOID
MiiDriverUnload();

//=============================================================================
// Public Interface
//=============================================================================
_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Check_return_
EXTERN_C
NTSTATUS
MiiInitializeMouseDeviceStackContext(
    _Out_ PMOUSE_DEVICE_STACK_INFORMATION pDeviceStackInformation
);

_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Check_return_
EXTERN_C
NTSTATUS
MiiInjectMouseButtonInput(
    _In_ HANDLE ProcessId,
    _In_ USHORT ButtonFlags,
    _In_ USHORT ButtonData
);

_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Check_return_
EXTERN_C
NTSTATUS
MiiInjectMouseMovementInput(
    _In_ HANDLE ProcessId,
    _In_ USHORT IndicatorFlags,
    _In_ LONG MovementX,
    _In_ LONG MovementY
);

_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Check_return_
EXTERN_C
NTSTATUS
MiiInjectMouseInputPacketUnsafe(
    _In_ HANDLE ProcessId,
    _In_ BOOLEAN fUseButtonDevice,
    _In_ PMOUSE_INPUT_DATA pInputPacket
);
