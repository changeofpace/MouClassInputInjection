/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#include "string_util.h"

#include <iostream>
#include <iterator>
#include <sstream>


//=============================================================================
// From String Interface
//=============================================================================
_Use_decl_annotations_
BOOL
StrUnsignedShortFromString(
    std::string& Token,
    BOOLEAN IsHex,
    PUSHORT pValue
)
{
    ULONG Value = 0;
    BOOL status = TRUE;

    //
    // Zero out parameters.
    //
    *pValue = 0;

    status = StrUnsignedLongFromString(Token, IsHex, &Value);
    if (!status)
    {
        goto exit;
    }

    //
    // Set out parameters.
    //
    *pValue = (USHORT)Value;

exit:
    return status;
}


_Use_decl_annotations_
BOOL
StrLongFromString(
    std::string& Token,
    BOOLEAN IsHex,
    PLONG pValue
)
{
    int Base = 0;
    LONG Value = 0;
    BOOL status = TRUE;

    //
    // Zero out parameters.
    //
    *pValue = 0;

    Base = IsHex ? 16 : 10;

    try
    {
        Value = std::stol(Token, NULL, Base);
    }
    catch (const std::exception&)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        status = FALSE;
        goto exit;
    }

    //
    // Set out parameters.
    //
    *pValue = Value;

exit:
    return status;
}


_Use_decl_annotations_
BOOL
StrUnsignedLongFromString(
    std::string& Token,
    BOOLEAN IsHex,
    PULONG pValue
)
{
    int Base = 0;
    ULONG Value = 0;
    BOOL status = TRUE;

    //
    // Zero out parameters.
    //
    *pValue = 0;

    Base = IsHex ? 16 : 10;

    try
    {
        Value = std::stoul(Token, NULL, Base);
    }
    catch (const std::exception&)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        status = FALSE;
        goto exit;
    }

    //
    // Set out parameters.
    //
    *pValue = Value;

exit:
    return status;
}


_Use_decl_annotations_
BOOL
StrUnsignedLongLongFromString(
    std::string& Token,
    BOOLEAN IsHex,
    PULONGLONG pValue
)
{
    int Base = 0;
    ULONGLONG Value = 0;
    BOOL status = TRUE;

    //
    // Zero out parameters.
    //
    *pValue = 0;

    Base = IsHex ? 16 : 10;

    try
    {
        Value = std::stoull(Token, NULL, Base);
    }
    catch (const std::exception&)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        status = FALSE;
        goto exit;
    }

    //
    // Set out parameters.
    //
    *pValue = Value;

exit:
    return status;
}


_Use_decl_annotations_
BOOL
StrUnsignedLongPointerFromString(
    std::string& Token,
    BOOLEAN IsHex,
    PULONG_PTR pValue
)
{
    int Base = 0;
    ULONG_PTR Value = 0;
    BOOL status = TRUE;

    //
    // Zero out parameters.
    //
    *pValue = 0;

    Base = IsHex ? 16 : 10;

#if defined(_WIN64)
    status = StrUnsignedLongLongFromString(Token, FALSE, &Value);
#else
    status = StrUnsignedLongFromString(Token, FALSE, &Value);
#endif
    if (!status)
    {
        goto exit;
    }

    //
    // Set out parameters.
    //
    *pValue = Value;

exit:
    return status;
}


//=============================================================================
// Tokenizer Interface
//=============================================================================
_Use_decl_annotations_
SIZE_T
StrSplitStringByWhitespace(
    std::string& Input,
    std::vector<std::string>& Output
)
{
    std::string Token;
    std::stringstream Stream(Input);

    Output = {
        std::istream_iterator<std::string>{Stream},
        std::istream_iterator<std::string>{}
    };

    return Output.size();
}
