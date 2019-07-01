/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#pragma once

#include <Windows.h>

#include <string>
#include <vector>

//=============================================================================
// From String Interface
//=============================================================================
_Check_return_
BOOL
StrUnsignedShortFromString(
    _In_ std::string& Token,
    _In_ BOOLEAN IsHex,
    _Out_ PUSHORT pValue
);

_Check_return_
BOOL
StrLongFromString(
    _In_ std::string& Token,
    _In_ BOOLEAN IsHex,
    _Out_ PLONG pValue
);

_Check_return_
BOOL
StrUnsignedLongFromString(
    _In_ std::string& Token,
    _In_ BOOLEAN IsHex,
    _Out_ PULONG pValue
);

_Check_return_
BOOL
StrUnsignedLongLongFromString(
    _In_ std::string& Token,
    _In_ BOOLEAN IsHex,
    _Out_ PULONGLONG pValue
);

_Check_return_
BOOL
StrUnsignedLongPointerFromString(
    _In_ std::string& Token,
    _In_ BOOLEAN IsHex,
    _Out_ PULONG_PTR pValue
);

//=============================================================================
// Tokenizer Interface
//=============================================================================
_Check_return_
SIZE_T
StrSplitStringByWhitespace(
    _In_ std::string& Input,
    _Out_ std::vector<std::string>& Output
);
