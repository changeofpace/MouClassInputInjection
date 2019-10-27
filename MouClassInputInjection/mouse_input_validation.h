/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#pragma once

#include <fltKernel.h>

_Check_return_
EXTERN_C
NTSTATUS
MivValidateButtonInput(
    _In_ USHORT ButtonFlags,
    _In_ USHORT ButtonData
);

_Check_return_
EXTERN_C
NTSTATUS
MivValidateMovementInput(
    _In_ USHORT IndicatorFlags,
    _In_ LONG MovementX,
    _In_ LONG MovementY
);
