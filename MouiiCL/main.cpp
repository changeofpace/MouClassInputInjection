/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#include <Windows.h>

#include <iostream>
#include <string>
#include <vector>

#include "commands.h"
#include "debug.h"
#include "driver.h"
#include "log.h"
#include "mouse_input_injection.h"
#include "string_util.h"


static
VOID
PrintProgramBanner()
{
    INF_PRINT("Type 'help' for a list of commands.");
    INF_PRINT("");
}


static
VOID
ProcessCommands()
{
    for (;;)
    {
        std::string Input;
        std::string Command;
        std::vector<std::string> Arguments;

        //
        // Prompt and tokenize input.
        //
        std::cout << "> ";
        std::getline(std::cin, Input);

        //
        // Skip empty lines.
        //
        if (!StrSplitStringByWhitespace(Input, Arguments))
        {
            std::cin.clear();
            continue;
        }

        Command = Arguments[0];

        //
        // Dispatch commands.
        //
        if (CMD_HELP == Command)
        {
            (VOID)CmdDisplayCommands();
        }
        else if (CMD_EXIT_CLIENT == Command)
        {
            break;
        }
        else if (CMD_INITIALIZE_MOUSE_DEVICE_STACK_CONTEXT == Command)
        {
            (VOID)CmdInitializeMouseDeviceStackContext(Arguments);
        }
        else if (CMD_LOOKUP_PROCESS_ID_BY_NAME == Command)
        {
            (VOID)CmdLookupProcessIdByName(Arguments);
        }
        else if (CMD_INJECT_MOUSE_BUTTON_INPUT == Command)
        {
            (VOID)CmdInjectMouseButtonInput(Arguments);
        }
        else if (CMD_INJECT_MOUSE_MOVEMENT_INPUT == Command)
        {
            (VOID)CmdInjectMouseMovementInput(Arguments);
        }
        else if (CMD_INJECT_MOUSE_BUTTON_CLICK == Command)
        {
            (VOID)CmdInjectMouseButtonClick(Arguments);
        }
        else
        {
            ERR_PRINT("Invalid command. Type 'help' for a list of commands.");
        }

        //
        // Prepare the next line.
        //
        std::cout << std::endl;
        std::cin.clear();
    }
}


int
main(
    _In_ int argc,
    _In_ char* argv[]
)
{
    BOOL fDriverInitialized = FALSE;
    int mainstatus = EXIT_SUCCESS;

    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);

    if (!LogInitialization(LOG_CONFIG_STDOUT))
    {
        ERR_PRINT("LogInitialization failed: %u", GetLastError());
        mainstatus = EXIT_FAILURE;
        goto exit;
    }

    if (!DrvInitialization())
    {
        ERR_PRINT("DrvInitialization failed: %u", GetLastError());
        mainstatus = EXIT_FAILURE;
        goto exit;
    }
    //
    fDriverInitialized = TRUE;

    if (!MouInitializeDeviceStackContext(NULL))
    {
        ERR_PRINT("MouInitializeDeviceStackContext failed: %u",
            GetLastError());
        mainstatus = EXIT_FAILURE;
        goto exit;
    }

    PrintProgramBanner();

    ProcessCommands();

exit:
    if (fDriverInitialized)
    {
        DrvTermination();
    }

    return mainstatus;
}
