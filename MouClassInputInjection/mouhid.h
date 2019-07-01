/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#pragma once

#include <fltKernel.h>

//=============================================================================
// Constants
//=============================================================================
#define MOUHID_DRIVER_OBJECT_PATH_U     L"\\Driver\\mouhid"

//=============================================================================
// Meta Interface
//=============================================================================
_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Check_return_
EXTERN_C
NTSTATUS
MhdDriverEntry();

//=============================================================================
// Public Interface
//=============================================================================
_Check_return_
EXTERN_C
SIZE_T
MhdGetConnectDataFieldOffset();
