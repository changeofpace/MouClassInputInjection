/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#pragma once

#include <fltKernel.h>

_IRQL_requires_max_(APC_LEVEL)
_IRQL_requires_same_
_Check_return_
EXTERN_C
NTSTATUS
ObuQueryNameString(
    _In_ PVOID pObject,
    _Outptr_result_nullonfailure_ POBJECT_NAME_INFORMATION* ppObjectNameInfo
);
