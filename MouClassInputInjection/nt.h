/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#pragma once

#include <fltKernel.h>

//=============================================================================
// Macros
//=============================================================================
#define OFFSET_POINTER(Pointer, Offset, Type) \
    ((Type*)(((PUCHAR)(Pointer)) + (Offset)))

#define POINTER_OFFSET(Offset, Base) \
    ((SIZE_T)(((ULONG_PTR)(Offset)) - ((ULONG_PTR)(Base))))

//=============================================================================
// Globals
//=============================================================================
EXTERN_C POBJECT_TYPE* IoDriverObjectType;

//=============================================================================
// Object Interface
//=============================================================================
EXTERN_C
NTSTATUS
NTAPI
ObReferenceObjectByName(
    _In_        PUNICODE_STRING ObjectName,
    _In_        ULONG           Attributes,
    _In_opt_    PACCESS_STATE   AccessState,
    _In_opt_    ACCESS_MASK     DesiredAccess,
    _In_        POBJECT_TYPE    ObjectType,
    _In_        KPROCESSOR_MODE AccessMode,
    _Inout_opt_ PVOID           ParseContext,
    _Out_       PVOID*          Object
);

//=============================================================================
// Process Interface
//=============================================================================
EXTERN_C
PUCHAR
NTAPI
PsGetProcessImageFileName(
    _In_ PEPROCESS Process
);

EXTERN_C
NTSTATUS
NTAPI
PsAcquireProcessExitSynchronization(
    _In_ PEPROCESS Process
);

EXTERN_C
VOID
NTAPI
PsReleaseProcessExitSynchronization(
    _In_ PEPROCESS Process
);

//=============================================================================
// Rtl Interface
//=============================================================================
EXTERN_C
PIMAGE_NT_HEADERS
NTAPI
RtlImageNtHeader(
    _In_ PVOID ImageBase
);

EXTERN_C
PVOID
NTAPI
RtlPcToFileHeader(
    _In_ PVOID PcValue,
    _Out_ PVOID* BaseOfImage
);
