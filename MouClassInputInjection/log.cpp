/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#include "log.h"

#define NTSTRSAFE_NO_CB_FUNCTIONS

#include <ntstrsafe.h>

#include "debug.h"
#include "nt.h"


//=============================================================================
// Constants
//=============================================================================
#define OUTPUT_BUFFER_CCH_MAX   512
#define TIME_BUFFER_CCH_MAX     20
#define MESSAGE_BUFFER_CCH_MAX  \
    (OUTPUT_BUFFER_CCH_MAX - TIME_BUFFER_CCH_MAX - 80)


//=============================================================================
// Private Interface
//=============================================================================
_Use_decl_annotations_
EXTERN_C
NTSTATUS
LogpPrint(
    LOG_LEVEL LogLevel,
    PCSTR pszFormat,
    ...
)
{
    PCHAR pszLogLevel = NULL;
    LARGE_INTEGER SystemTime = {};
    LARGE_INTEGER LocalTime = {};
    TIME_FIELDS TimeFields = {};
    CHAR szTimeBuffer[TIME_BUFFER_CCH_MAX] = {};
    va_list VarArgs = {};
    CHAR szMessageBuffer[MESSAGE_BUFFER_CCH_MAX] = {};
    CHAR szOutputBuffer[OUTPUT_BUFFER_CCH_MAX] = {};
    NTSTATUS ntstatus = STATUS_SUCCESS;

    //
    // Set the log level prefix.
    //
    switch (LogLevel)
    {
        case LogLevelDebug:     pszLogLevel = "DBG"; break;
        case LogLevelInfo:      pszLogLevel = "INF"; break;
        case LogLevelWarning:   pszLogLevel = "WRN"; break;
        case LogLevelError:     pszLogLevel = "ERR"; break;
        default:
            ntstatus = STATUS_INVALID_PARAMETER_1;
            DEBUG_BREAK;
            goto exit;
    }

    //
    // Query the current local time.
    //
    KeQuerySystemTime(&SystemTime);
    ExSystemTimeToLocalTime(&SystemTime, &LocalTime);
    RtlTimeToTimeFields(&LocalTime, &TimeFields);

    ntstatus = RtlStringCchPrintfA(
        szTimeBuffer,
        RTL_NUMBER_OF(szTimeBuffer),
        "%02hd:%02hd:%02hd.%03hd",
        TimeFields.Hour,
        TimeFields.Minute,
        TimeFields.Second,
        TimeFields.Milliseconds);
    if (!NT_SUCCESS(ntstatus))
    {
        DEBUG_BREAK;
        goto exit;
    }

    va_start(VarArgs, pszFormat);
    ntstatus = RtlStringCchVPrintfA(
        szMessageBuffer,
        RTL_NUMBER_OF(szMessageBuffer),
        pszFormat,
        VarArgs);
    va_end(VarArgs);
    if (!NT_SUCCESS(ntstatus))
    {
        DEBUG_BREAK;
        goto exit;
    }

    ntstatus = RtlStringCchPrintfA(
        szOutputBuffer,
        RTL_NUMBER_OF(szOutputBuffer),
        "%s  %s  %04Iu:%04Iu  %-15s  %s",
        szTimeBuffer,
        pszLogLevel,
        (ULONG_PTR)PsGetProcessId(PsGetCurrentProcess()),
        (ULONG_PTR)PsGetCurrentThreadId(),
        PsGetProcessImageFileName(PsGetCurrentProcess()),
        szMessageBuffer);
    if (!NT_SUCCESS(ntstatus))
    {
        DEBUG_BREAK;
        goto exit;
    }

    ntstatus = DbgPrintEx(
        DPFLTR_DEFAULT_ID,
        DPFLTR_ERROR_LEVEL,
        "%s",
        szOutputBuffer);
    if (!NT_SUCCESS(ntstatus))
    {
        DEBUG_BREAK;
        goto exit;
    }

exit:
    return ntstatus;
}
