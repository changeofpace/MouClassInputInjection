/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#include "mouse_input_validation.h"

#include <ntddmou.h>


//=============================================================================
// Constants
//=============================================================================
#define VALID_INDICATOR_FLAGS_MASK  \
    (MOUSE_MOVE_RELATIVE |          \
    MOUSE_MOVE_ABSOLUTE |           \
    MOUSE_VIRTUAL_DESKTOP |         \
    MOUSE_ATTRIBUTES_CHANGED |      \
    MOUSE_MOVE_NOCOALESCE |         \
    MOUSE_TERMSRV_SRC_SHADOW)


//=============================================================================
// Public Interface
//=============================================================================
_Use_decl_annotations_
EXTERN_C
NTSTATUS
MivValidateButtonInput(
    USHORT ButtonFlags,
    USHORT ButtonData
)
{
    NTSTATUS ntstatus = STATUS_SUCCESS;

    switch (ButtonFlags)
    {
        case MOUSE_LEFT_BUTTON_DOWN:
        case MOUSE_LEFT_BUTTON_UP:
        case MOUSE_RIGHT_BUTTON_DOWN:
        case MOUSE_RIGHT_BUTTON_UP:
        case MOUSE_MIDDLE_BUTTON_DOWN:
        case MOUSE_MIDDLE_BUTTON_UP:
        case MOUSE_BUTTON_4_DOWN:
        case MOUSE_BUTTON_4_UP:
        case MOUSE_BUTTON_5_DOWN:
        case MOUSE_BUTTON_5_UP:
            if (ButtonData)
            {
                ntstatus = STATUS_INVALID_PARAMETER_1;
                goto exit;
            }

            break;

        case MOUSE_WHEEL:
        case MOUSE_HWHEEL:
            if (!ButtonData)
            {
                ntstatus = STATUS_INVALID_PARAMETER_2;
                goto exit;
            }

            break;

        default:
            ntstatus = STATUS_INVALID_PARAMETER;
            goto exit;
    }

exit:
    return ntstatus;
}


_Use_decl_annotations_
EXTERN_C
NTSTATUS
MivValidateMovementInput(
    USHORT IndicatorFlags,
    LONG MovementX,
    LONG MovementY
)
{
    NTSTATUS ntstatus = STATUS_SUCCESS;

    UNREFERENCED_PARAMETER(MovementX);
    UNREFERENCED_PARAMETER(MovementY);

    if ((~VALID_INDICATOR_FLAGS_MASK) & IndicatorFlags)
    {
        ntstatus = STATUS_INVALID_PARAMETER_1;
        goto exit;
    }

    //
    // RELATIVE movement is an exclusive flag.
    //
    if (MOUSE_MOVE_RELATIVE & IndicatorFlags &&
        (~MOUSE_MOVE_RELATIVE) & IndicatorFlags)
    {
        ntstatus = STATUS_INVALID_PARAMETER_1;
        goto exit;
    }

    //
    // RELATIVE/ABSOLUTE are mutually exclusive flags.
    //
    if (MOUSE_MOVE_RELATIVE & IndicatorFlags &&
        MOUSE_MOVE_ABSOLUTE & IndicatorFlags)
    {
        ntstatus = STATUS_INVALID_PARAMETER_1;
        goto exit;
    }

    //
    // When the attributes changed flag is set the other fields are ignored.
    //
    if (MOUSE_ATTRIBUTES_CHANGED & IndicatorFlags &&
        (~MOUSE_ATTRIBUTES_CHANGED) & IndicatorFlags)
    {
        ntstatus = STATUS_INVALID_PARAMETER_1;
        goto exit;
    }

    //
    // Reject this flag because its purpose is unknown.
    //
    if (MOUSE_TERMSRV_SRC_SHADOW & IndicatorFlags)
    {
        ntstatus = STATUS_INVALID_PARAMETER_1;
        goto exit;
    }

exit:
    return ntstatus;
}
