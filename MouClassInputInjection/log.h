/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#pragma once

#include <fltKernel.h>

//=============================================================================
// Enumerations
//=============================================================================
typedef enum _LOG_LEVEL
{
    LogLevelDebug,
    LogLevelInfo,
    LogLevelWarning,
    LogLevelError,

} LOG_LEVEL, *PLOG_LEVEL;

//=============================================================================
// Private Interface
//=============================================================================
_IRQL_requires_same_
EXTERN_C
NTSTATUS
LogpPrint(
    _In_ LOG_LEVEL LogLevel,
    _In_z_ _Printf_format_string_ PCSTR pszFormat,
    ...
);

//=============================================================================
// Public Interface
//=============================================================================
#if defined(DBG)
#define DBG_PRINT(Format, ...) \
    (LogpPrint(LogLevelDebug, (Format), __VA_ARGS__))
#else
//
// Debug level messages are disabled in release builds.
//
#define DBG_PRINT(Format, ...)
#endif

#define INF_PRINT(Format, ...) (LogpPrint(LogLevelInfo, (Format), __VA_ARGS__))
#define WRN_PRINT(Format, ...) \
    (LogpPrint(LogLevelWarning, (Format), __VA_ARGS__))
#define ERR_PRINT(Format, ...) \
    (LogpPrint(LogLevelError, (Format), __VA_ARGS__))
