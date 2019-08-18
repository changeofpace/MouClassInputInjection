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
#define LOG_OPTION_APPEND_CRLF          0x00000001

//=============================================================================
// Enumerations
//=============================================================================
typedef enum _LOG_LEVEL {
    LogLevelDebug,
    LogLevelInfo,
    LogLevelWarning,
    LogLevelError,
} LOG_LEVEL, *PLOG_LEVEL;

//=============================================================================
// Public Interface
//=============================================================================
_IRQL_requires_same_
EXTERN_C
NTSTATUS
LogPrint(
    _In_ LOG_LEVEL Level,
    _In_ ULONG Options,
    _In_z_ _Printf_format_string_ PCHAR pszFormat,
    ...
);

#if defined(DBG)
#define DBG_PRINT(Format, ...) \
    LogPrint(LogLevelDebug, LOG_OPTION_APPEND_CRLF, (Format), __VA_ARGS__)
#else
//
// Debug level messages are disabled in release builds.
//
#define DBG_PRINT(Format, ...)
#endif

#define INF_PRINT(Format, ...) \
    LogPrint(LogLevelInfo, LOG_OPTION_APPEND_CRLF, (Format), __VA_ARGS__)

#define WRN_PRINT(Format, ...)  \
    LogPrint(                   \
        LogLevelWarning,        \
        LOG_OPTION_APPEND_CRLF, \
        (Format),               \
        __VA_ARGS__)

#define ERR_PRINT(Format, ...) \
    LogPrint(LogLevelError, LOG_OPTION_APPEND_CRLF, (Format), __VA_ARGS__)
