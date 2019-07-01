/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#pragma once

#include <fltKernel.h>

_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Check_return_
EXTERN_C
NTSTATUS
IouEnumerateDeviceObjectList(
    _In_ PDRIVER_OBJECT pDriverObject,
    _Outptr_result_nullonfailure_ PDEVICE_OBJECT** pppDeviceObjectList,
    _Out_ PULONG pnDeviceObjectList
);

_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
EXTERN_C
VOID
IouFreeDeviceObjectList(
    _Pre_notnull_ __drv_freesMem(Mem) PDEVICE_OBJECT* ppDeviceObjectList,
    _In_ ULONG nDeviceObjectList
);

_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Check_return_
EXTERN_C
PDEVICE_OBJECT
IouGetUpperDeviceObject(
    _In_ PDEVICE_OBJECT pDeviceObject
);

#if defined(DBG)
_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
EXTERN_C
VOID
IouPrintDeviceObjectList(
    _In_ PWSTR pwzDriverName,
    _In_ PDEVICE_OBJECT* ppDeviceObjectList,
    _In_ ULONG nDeviceObjectList
);
#endif
