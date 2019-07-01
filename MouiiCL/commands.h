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
// Console Commands
//=============================================================================
#define CMD_HELP                                    "help"
#define CMD_EXIT_CLIENT                             "exit"

#define CMD_INITIALIZE_MOUSE_DEVICE_STACK_CONTEXT   "init"
#define CMD_LOOKUP_PROCESS_ID_BY_NAME               "pid"
#define CMD_INJECT_MOUSE_BUTTON_INPUT               "button"
#define CMD_INJECT_MOUSE_MOVEMENT_INPUT             "move"
#define CMD_INJECT_MOUSE_BUTTON_CLICK               "click"

//=============================================================================
// Public Interface
//=============================================================================
_Check_return_
BOOL
CmdDisplayCommands();

_Check_return_
BOOL
CmdInitializeMouseDeviceStackContext(
    _In_ std::vector<std::string>& Arguments
);

_Check_return_
BOOL
CmdLookupProcessIdByName(
    _In_ std::vector<std::string>& Arguments
);

_Check_return_
BOOL
CmdInjectMouseButtonInput(
    _In_ std::vector<std::string>& Arguments
);

_Check_return_
BOOL
CmdInjectMouseMovementInput(
    _In_ std::vector<std::string>& Arguments
);

_Check_return_
BOOL
CmdInjectMouseButtonClick(
    _In_ std::vector<std::string>& Arguments
);
