/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#pragma once

#if defined(_KERNEL_MODE)
#include <fltKernel.h>
#else
#include <Windows.h>
#endif

_Check_return_
EXTERN_C
BOOLEAN
MivValidateButtonInput(
    _In_ USHORT ButtonFlags,
    _In_ USHORT ButtonData
);

_Check_return_
EXTERN_C
BOOLEAN
MivValidateMovementInput(
    _In_ USHORT IndicatorFlags,
    _In_ LONG MovementX,
    _In_ LONG MovementY
);
