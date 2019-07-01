/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#pragma once

#include <Windows.h>

#include <cstdio>

#if defined(_DEBUG)
#define DBG_PRINT   printf
#else
//
// Debug level messages are disabled in release builds.
//
#define DBG_PRINT
#endif

#define INF_PRINT   printf
#define WRN_PRINT   printf
#define ERR_PRINT   printf
