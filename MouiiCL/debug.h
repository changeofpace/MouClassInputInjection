/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#pragma once

#include <Windows.h>

#include "log.h"

/*++

Macro Name:

    VERIFY

Macro Description:

    A validation macro which ASSERTs in debug build configurations and logs
    failures in release build configurations.

--*/
#if defined(VERIFY)
#error "Unexpected identifier conflict. (VERIFY)"
#endif

#if defined(_DEBUG)
#define VERIFY(Expression)  (_ASSERT(Expression))
#else
#define VERIFY(Expression)                                              \
{                                                                       \
    if (!(Expression))                                                  \
    {                                                                   \
        ERR_PRINT("\'" #Expression "\' failed: %u\n", GetLastError());  \
    }                                                                   \
}
#endif
