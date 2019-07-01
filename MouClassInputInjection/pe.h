/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#pragma once

#include <fltKernel.h>

#include <ntimage.h>

_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Check_return_
EXTERN_C
NTSTATUS
PeGetSectionsByCharacteristics(
    _In_ ULONG_PTR ImageBase,
    _In_ ULONG Characteristics,
    _Outptr_result_nullonfailure_ PIMAGE_SECTION_HEADER** pppSectionHeaders,
    _Out_ PULONG pnSectionHeaders
);

_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Check_return_
EXTERN_C
NTSTATUS
PeGetExecutableSections(
    _In_ ULONG_PTR ImageBase,
    _Outptr_result_nullonfailure_ PIMAGE_SECTION_HEADER** pppSectionHeaders,
    _Out_ PULONG pnSectionHeaders
);
