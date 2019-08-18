/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#include "commands.h"

#include <ntddmou.h>

#include "log.h"
#include "mouse_input_injection.h"
#include "process.h"
#include "string_util.h"


//=============================================================================
// Environment
//=============================================================================

//
// Assert that the hardcoded values in the command descriptions reflect the
//  actual values defined in ntddmou.h. These values are unlikely to change.
//
C_ASSERT(0x0001 == MOUSE_LEFT_BUTTON_DOWN);
C_ASSERT(0x0002 == MOUSE_LEFT_BUTTON_UP);
C_ASSERT(0x0004 == MOUSE_RIGHT_BUTTON_DOWN);
C_ASSERT(0x0008 == MOUSE_RIGHT_BUTTON_UP);
C_ASSERT(0x0010 == MOUSE_MIDDLE_BUTTON_DOWN);
C_ASSERT(0x0020 == MOUSE_MIDDLE_BUTTON_UP);
C_ASSERT(0x0040 == MOUSE_BUTTON_4_DOWN);
C_ASSERT(0x0080 == MOUSE_BUTTON_4_UP);
C_ASSERT(0x0100 == MOUSE_BUTTON_5_DOWN);
C_ASSERT(0x0200 == MOUSE_BUTTON_5_UP);
C_ASSERT(0x0400 == MOUSE_WHEEL);
C_ASSERT(0x0800 == MOUSE_HWHEEL);

C_ASSERT(0x0000 == MOUSE_MOVE_RELATIVE);
C_ASSERT(0x0001 == MOUSE_MOVE_ABSOLUTE);
C_ASSERT(0x0002 == MOUSE_VIRTUAL_DESKTOP);
C_ASSERT(0x0004 == MOUSE_ATTRIBUTES_CHANGED);
C_ASSERT(0x0100 == MOUSE_TERMSRV_SRC_SHADOW);


//=============================================================================
// Command Information
//=============================================================================
#define CMD_INFO_HELP                           "Display commands."
#define CMD_INFO_EXIT_CLIENT                    "Exit the client."

#define CMD_INFO_INITIALIZE_MOUSE_DEVICE_STACK_CONTEXT  \
    "Initialize the mouse device stack context for the driver."
#define CMD_INFO_LOOKUP_PROCESS_ID_BY_NAME      "Query process id by name."
#define CMD_INFO_INJECT_MOUSE_BUTTON_INPUT      "Inject mouse button input."
#define CMD_INFO_INJECT_MOUSE_MOVEMENT_INPUT    "Inject mouse movement input."
#define CMD_INFO_INJECT_MOUSE_BUTTON_CLICK      "Inject a mouse button click."


//=============================================================================
// Private Interface
//=============================================================================
static
VOID
CmdpPrintDeviceReinitializationMessage();


//=============================================================================
// Public Interface
//=============================================================================
_Use_decl_annotations_
BOOL
CmdDisplayCommands()
{
    INF_PRINT("Commands:");
    INF_PRINT("    %-10s  %s", CMD_HELP, CMD_INFO_HELP);
    INF_PRINT("    %-10s  %s", CMD_EXIT_CLIENT, CMD_INFO_EXIT_CLIENT);
    INF_PRINT("    %-10s  %s",
        CMD_INITIALIZE_MOUSE_DEVICE_STACK_CONTEXT,
        CMD_INFO_INITIALIZE_MOUSE_DEVICE_STACK_CONTEXT);
    INF_PRINT("    %-10s  %s",
        CMD_LOOKUP_PROCESS_ID_BY_NAME,
        CMD_INFO_LOOKUP_PROCESS_ID_BY_NAME);
    INF_PRINT("    %-10s  %s",
        CMD_INJECT_MOUSE_BUTTON_INPUT,
        CMD_INFO_INJECT_MOUSE_BUTTON_INPUT);
    INF_PRINT("    %-10s  %s",
        CMD_INJECT_MOUSE_MOVEMENT_INPUT,
        CMD_INFO_INJECT_MOUSE_MOVEMENT_INPUT);
    INF_PRINT("    %-10s  %s",
        CMD_INJECT_MOUSE_BUTTON_CLICK,
        CMD_INFO_INJECT_MOUSE_BUTTON_CLICK);

    return TRUE;
}


#define ARGC_INITIALIZE_MOUSE_DEVICE_STACK_CONTEXT    1

static PCSTR g_pszUsageInitializeMouseDeviceStackContext =
R"(Usage:
    init

Description:
    Initializes the mouse device stack context required by the kernel input
    injection interface. This command must be invoked and succeed before the
    input injection commands can be used.
)";

_Use_decl_annotations_
BOOL
CmdInitializeMouseDeviceStackContext(
    std::vector<std::string>& Arguments
)
{
    MOUSE_DEVICE_STACK_INFORMATION DeviceStackInformation = {};
    BOOL status = TRUE;

    if (ARGC_INITIALIZE_MOUSE_DEVICE_STACK_CONTEXT != Arguments.size())
    {
        LogPrintDirect(g_pszUsageInitializeMouseDeviceStackContext);
        SetLastError(ERROR_INVALID_PARAMETER);
        status = FALSE;
        goto exit;
    }

    if (CMD_INITIALIZE_MOUSE_DEVICE_STACK_CONTEXT != Arguments[0])
    {
        INF_PRINT("Unexpected command: %s", Arguments[0].c_str());
        SetLastError(ERROR_INVALID_PARAMETER);
        status = FALSE;
        goto exit;
    }

    status = MouInitializeDeviceStackContext(&DeviceStackInformation);
    if (!status)
    {
        ERR_PRINT("MouInitializeDeviceStackContext failed: %u",
            GetLastError());
        goto exit;
    }

    INF_PRINT("Mouse Device Stack Information:");
    INF_PRINT("    Button Device");
    INF_PRINT("        UnitId:              %hu",
        DeviceStackInformation.ButtonDevice.UnitId);
    INF_PRINT("    Movement Device");
    INF_PRINT("        UnitId:              %hu",
        DeviceStackInformation.MovementDevice.UnitId);
    INF_PRINT("        AbsoluteMovement:    %s",
        DeviceStackInformation.MovementDevice.AbsoluteMovement ?
            "TRUE" : "FALSE");
    INF_PRINT("        VirtualDesktop:      %s",
        DeviceStackInformation.MovementDevice.VirtualDesktop ?
            "TRUE" : "FALSE");

exit:
    return status;
}


#define ARGC_LOOKUP_PROCESS_ID_BY_NAME  2

static PCSTR g_pszUsageLookupProcessIdByName =
R"(Usage:
    pid process_name

Description:
    Query the process id of all processes matching the specified name.

Parameters:
    process_name - The target process name including file extension.

Example:
    pid calc.exe
)";

_Use_decl_annotations_
BOOL
CmdLookupProcessIdByName(
    std::vector<std::string>& Arguments
)
{
    PCSTR pszProcessName = NULL;
    std::vector<ULONG_PTR> ProcessIds = {};
    BOOL status = TRUE;

    if (ARGC_LOOKUP_PROCESS_ID_BY_NAME != Arguments.size())
    {
        LogPrintDirect(g_pszUsageLookupProcessIdByName);
        SetLastError(ERROR_INVALID_PARAMETER);
        status = FALSE;
        goto exit;
    }

    if (CMD_LOOKUP_PROCESS_ID_BY_NAME != Arguments[0])
    {
        INF_PRINT("Unexpected command: %s", Arguments[0].c_str());
        SetLastError(ERROR_INVALID_PARAMETER);
        status = FALSE;
        goto exit;
    }

    pszProcessName = Arguments[1].data();

    status = PsuLookupProcessIdByName(pszProcessName, ProcessIds);
    if (!status)
    {
        goto exit;
    }

    if (!ProcessIds.size())
    {
        ERR_PRINT("There are no active processes with the specified name.");
        SetLastError(ERROR_INVALID_NAME);
        status = FALSE;
        goto exit;
    }

    for (SIZE_T i = 0; i < ProcessIds.size(); ++i)
    {
        INF_PRINT("    %Iu  0x%IX", ProcessIds[i], ProcessIds[i]);
    }

exit:
    return status;
}


#define ARGC_INJECT_MOUSE_BUTTON_INPUT  4

static PCSTR g_pszUsageInjectMouseButtonInput =
R"(Usage:
    button process_id button_flag(hex) button_data(hex)

Description:
    Inject mouse button data in the specified process context. The button data
    is validated, converted into a single mouse input data packet, and injected
    into the specified process.

Parameters:
    process_id - The process id of the process context in which the input
        injection occurs.

    button_flag (hex) - The system defined value of the button input:

        0x001   Left mouse button down.         (Mouse button 1)
        0x002   Left mouse button up.           (Mouse button 1)
        0x004   Right mouse button down.        (Mouse button 2)
        0x008   Right mouse button up.          (Mouse button 2)
        0x010   Middle mouse button down.       (Mouse button 3)
        0x020   Middle mouse button up.         (Mouse button 4)
        0x040   Mouse button 4 down.
        0x080   Mouse button 4 up.
        0x100   Mouse button 5 down.
        0x200   Mouse button 5 up.
        0x400   Vertical mouse wheel scroll.
        0x800   Horizontal mouse wheel scroll.

    button_data (hex) - The mouse wheel delta for scrolling. This value is only
        valid if 'button_flag' specifies a mouse wheel scroll value.

Example:
    The following command injects a middle mouse button press into the process
    whose process id is 1234:

        button 1234 0x10 0
)";

_Use_decl_annotations_
BOOL
CmdInjectMouseButtonInput(
    std::vector<std::string>& Arguments
)
{
    ULONG_PTR ProcessId = 0;
    USHORT ButtonFlags = 0;
    USHORT ButtonData = 0;
    BOOL status = TRUE;

    if (ARGC_INJECT_MOUSE_BUTTON_INPUT != Arguments.size())
    {
        LogPrintDirect(g_pszUsageInjectMouseButtonInput);
        SetLastError(ERROR_INVALID_PARAMETER);
        status = FALSE;
        goto exit;
    }

    if (CMD_INJECT_MOUSE_BUTTON_INPUT != Arguments[0])
    {
        INF_PRINT("Unexpected command: %s", Arguments[0].c_str());
        SetLastError(ERROR_INVALID_PARAMETER);
        status = FALSE;
        goto exit;
    }

    status = StrUnsignedLongPointerFromString(Arguments[1], FALSE, &ProcessId);
    if (!status)
    {
        goto exit;
    }

    status = StrUnsignedShortFromString(Arguments[2], TRUE, &ButtonFlags);
    if (!status)
    {
        ERR_PRINT("Invalid button flag: %s", Arguments[2].c_str());
        goto exit;
    }

    status = StrUnsignedShortFromString(Arguments[3], TRUE, &ButtonData);
    if (!status)
    {
        ERR_PRINT("Invalid button data: %s", Arguments[3].c_str());
        goto exit;
    }

    status = MouInjectButtonInput(ProcessId, ButtonFlags, ButtonData);
    if (!status)
    {
        if (ERROR_DEVICE_REINITIALIZATION_NEEDED == GetLastError())
        {
            CmdpPrintDeviceReinitializationMessage();
        }

        goto exit;
    }

exit:
    return status;
}


#define ARGC_INJECT_MOUSE_MOVEMENT_INPUT    5

static PCSTR g_pszUsageInjectMouseMovementInput =
R"(Usage:
    move process_id indicator_flags(hex) x y

Description:
    Inject mouse movement data in the specified process context. The movement
    data is validated, converted into a single mouse input data packet, and
    injected into the specified process.

Parameters:
    process_id - The process id of the process context in which the input
        injection occurs.

    indicator_flags (hex) - The system defined mouse indicator flags:

        0x000   'x' and 'y' specify relative mouse movement.
        0x001   'x' and 'y' specify absolute mouse movement.
        0x002   The coordinates are mapped to the virtual desktop.
        0x004   Requery for mouse attributes. (Untested)
        0x008   Do not coalesce WM_MOUSEMOVEs. (Untested)
        0x100   MOUSE_TERMSRV_SRC_SHADOW. (Restricted/untested)

    x - The motion delta to be applied in the x direction.

    y - The motion delta to be applied in the y direction.

Example:
    The following command injects a movement packet which moves the mouse left
    by 3 relative units and up by 10 relative units:

        move 1234 0 -3 10
)";

_Use_decl_annotations_
BOOL
CmdInjectMouseMovementInput(
    std::vector<std::string>& Arguments
)
{
    ULONG_PTR ProcessId = 0;
    USHORT IndicatorFlags = 0;
    LONG MovementX = 0;
    LONG MovementY = 0;
    BOOL status = TRUE;

    if (ARGC_INJECT_MOUSE_MOVEMENT_INPUT != Arguments.size())
    {
        LogPrintDirect(g_pszUsageInjectMouseMovementInput);
        SetLastError(ERROR_INVALID_PARAMETER);
        status = FALSE;
        goto exit;
    }

    if (CMD_INJECT_MOUSE_MOVEMENT_INPUT != Arguments[0])
    {
        INF_PRINT("Unexpected command: %s", Arguments[0].c_str());
        SetLastError(ERROR_INVALID_PARAMETER);
        status = FALSE;
        goto exit;
    }

    status = StrUnsignedLongPointerFromString(Arguments[1], FALSE, &ProcessId);
    if (!status)
    {
        goto exit;
    }

    status = StrUnsignedShortFromString(Arguments[2], TRUE, &IndicatorFlags);
    if (!status)
    {
        ERR_PRINT("Invalid indicator flags: %s", Arguments[2].c_str());
        goto exit;
    }

    status = StrLongFromString(Arguments[3], FALSE, &MovementX);
    if (!status)
    {
        ERR_PRINT("Invalid x: %s", Arguments[3].c_str());
        goto exit;
    }

    status = StrLongFromString(Arguments[4], FALSE, &MovementY);
    if (!status)
    {
        ERR_PRINT("Invalid y: %s", Arguments[4].c_str());
        goto exit;
    }

    status = MouInjectMovementInput(
        ProcessId,
        IndicatorFlags,
        MovementX,
        MovementY);
    if (!status)
    {
        if (ERROR_DEVICE_REINITIALIZATION_NEEDED == GetLastError())
        {
            CmdpPrintDeviceReinitializationMessage();
        }

        goto exit;
    }

exit:
    return status;
}


#define ARGC_INJECT_MOUSE_BUTTON_CLICK  4

static PCSTR g_pszUsageInjectMouseButtonClick =
R"(Usage:
    click process_id button(hex) release_delay

Description:
    Inject a mouse button press into the specified process then inject the
    corresponding mouse button release after the specified delay.

Parameters:
    process_id - The process id of the process context in which the input
        injection occurs.

    button (hex) - One of the following system defined values:

        0x001   Left mouse button.      (Mouse button 1)
        0x004   Right mouse button.     (Mouse button 2)
        0x010   Middle mouse button.    (Mouse button 3)
        0x040   Mouse button 4.
        0x100   Mouse button 5.

    release_delay - The approximate duration in milliseconds between when the
        button press is injected and when the button release is injected.

Example:
    The following command injects a left mouse button click where the button is
    activated (i.e., held down) for approximately 777 milliseconds.

        click 1234 0x1 777
)";

_Use_decl_annotations_
BOOL
CmdInjectMouseButtonClick(
    std::vector<std::string>& Arguments
)
{
    ULONG_PTR ProcessId = 0;
    USHORT Button = 0;
    ULONG DurationInMilliseconds = 0;
    BOOL status = TRUE;

    if (ARGC_INJECT_MOUSE_BUTTON_CLICK != Arguments.size())
    {
        LogPrintDirect(g_pszUsageInjectMouseButtonClick);
        SetLastError(ERROR_INVALID_PARAMETER);
        status = FALSE;
        goto exit;
    }

    if (CMD_INJECT_MOUSE_BUTTON_CLICK != Arguments[0])
    {
        INF_PRINT("Unexpected command: %s", Arguments[0].c_str());
        SetLastError(ERROR_INVALID_PARAMETER);
        status = FALSE;
        goto exit;
    }

    status = StrUnsignedLongPointerFromString(Arguments[1], FALSE, &ProcessId);
    if (!status)
    {
        goto exit;
    }

    status = StrUnsignedShortFromString(Arguments[2], TRUE, &Button);
    if (!status)
    {
        ERR_PRINT("Invalid button flag: %s", Arguments[2].c_str());
        goto exit;
    }

    status = StrUnsignedLongFromString(
        Arguments[3],
        FALSE,
        &DurationInMilliseconds);
    if (!status)
    {
        ERR_PRINT("Invalid duration: %s", Arguments[3].c_str());
        goto exit;
    }

    status = MouInjectButtonClick(ProcessId, Button, DurationInMilliseconds);
    if (!status)
    {
        if (ERROR_DEVICE_REINITIALIZATION_NEEDED == GetLastError())
        {
            CmdpPrintDeviceReinitializationMessage();
        }

        goto exit;
    }

exit:
    return status;
}


//=============================================================================
// Private Interface
//=============================================================================
static
VOID
CmdpPrintDeviceReinitializationMessage()
{
    INF_PRINT(
        "The mouse device stack context for the driver is not initialized"
        " or a change in the HID USB mouse device stack(s) has invalidated the"
        " previous context. Execute the '%s' command to initialize a new"
        " device stack context.",
        CMD_INITIALIZE_MOUSE_DEVICE_STACK_CONTEXT);
}
