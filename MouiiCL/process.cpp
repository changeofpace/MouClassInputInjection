/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#include "process.h"

#include "debug.h"
#include "log.h"
#include "ntdll.h"


//=============================================================================
// Public Interface
//=============================================================================
_Use_decl_annotations_
BOOL
PsuLookupProcessIdByName(
    PCSTR pszProcessName,
    std::vector<ULONG_PTR>& ProcessIds
)
/*++

Routine Description:

    Returns the process ids of every active process whose name matches the
    specified process name.

Parameters:

    pszProcessName - The process name, including file extension, to be matched
        against.

    ProcessIds - Returns a vector of process ids whose names match the
        specified process names.

Remarks:

    This routine uses a case-insensitive comparison.

--*/
{
    PSYSTEM_PROCESS_INFORMATION pSystemProcessInfo = NULL;
    ULONG cbSystemProcessInfo = NULL;
    ANSI_STRING asProcessName = {};
    UNICODE_STRING usProcessName = {};
    BOOLEAN fStringAllocated = FALSE;
    PSYSTEM_PROCESS_INFORMATION pEntry = NULL;
    NTSTATUS ntstatus = STATUS_SUCCESS;
    BOOL status = TRUE;

    //
    // Zero out parameters.
    //
    ProcessIds.clear();

    for (cbSystemProcessInfo = sizeof(*pSystemProcessInfo);;)
    {
        pSystemProcessInfo = (PSYSTEM_PROCESS_INFORMATION)HeapAlloc(
            GetProcessHeap(),
            HEAP_ZERO_MEMORY,
            cbSystemProcessInfo);
        if (!pSystemProcessInfo)
        {
            status = FALSE;
            goto exit;
        }

        ntstatus = NtQuerySystemInformation(
            SystemProcessInformation,
            pSystemProcessInfo,
            cbSystemProcessInfo,
            &cbSystemProcessInfo);
        if (NT_SUCCESS(ntstatus))
        {
            break;
        }
        else if (STATUS_INFO_LENGTH_MISMATCH != ntstatus)
        {
            ERR_PRINT("NtQuerySystemInformation failed: 0x%X", ntstatus);
            status = FALSE;
            goto exit;
        }

        status = HeapFree(GetProcessHeap(), 0, pSystemProcessInfo);
        if (!status)
        {
            ERR_PRINT("HeapFree failed: %u", GetLastError());
            goto exit;
        }
    }

    //
    // Initialize a unicode string for the specified process name.
    //
    RtlInitAnsiString(&asProcessName, pszProcessName);

    ntstatus = RtlAnsiStringToUnicodeString(
        &usProcessName,
        &asProcessName,
        TRUE);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("RtlAnsiStringToUnicodeString failed: 0x%X", ntstatus);
        status = FALSE;
        goto exit;
    }
    //
    fStringAllocated = TRUE;

    //
    // Search the process snapshot for matching process names.
    //
    for (pEntry = pSystemProcessInfo;
        pEntry->NextEntryOffset;
        pEntry = OFFSET_POINTER(
            pEntry,
            pEntry->NextEntryOffset,
            SYSTEM_PROCESS_INFORMATION))
    {
        if (!RtlCompareUnicodeString(&usProcessName, &pEntry->ImageName, TRUE))
        {
            ProcessIds.emplace_back((ULONG_PTR)pEntry->UniqueProcessId);
        }
    }

exit:
    if (fStringAllocated)
    {
        RtlFreeUnicodeString(&usProcessName);
    }

    if (pSystemProcessInfo)
    {
        VERIFY(HeapFree(GetProcessHeap(), 0, pSystemProcessInfo));
    }

    return status;
}
