/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#pragma once

#include <fltKernel.h>

#include "mouclass.h"

//=============================================================================
// Public Types
//=============================================================================
/*++

Type Name:

    MHK_HOOK_CALLBACK_ROUTINE

Type Description:

    An MHK hook callback is invoked each time the MouHid driver invokes a
    mouse class service callback to copy mouse input data packets from a hooked
    MouHid device object to the class data queue of a mouse class device
    object.

Parameters:

    pServiceCallbackOriginal - Pointer to the original class service callback.

    pClassDeviceObject - Pointer to the mouse class device object to receive
        the mouse input data packets.

    pInputDataStart - Pointer to the array of input packets to be copied to the
        class data queue.

    pInputDataEnd - Pointer to the input packet which marks the end of the
        input packet array.

    pnInputDataConsumed - Returns the number of input packets copied to the
        class data queue by the routine.

    pContext - Pointer to caller-defined data to be passed as the context
        parameter each time the callback is invoked. The context data must
        reside in NonPaged memory.

Remarks:

    An MHK hook callback can filter, modify, and inject input packets like a
    standard mouse filter driver. For example, a callback can inject ten mouse
    input data packets using the following strategy:

        1. Allocate storage for ten packets from the NonPaged pool.

        2. Initialize each packet.

        3. Invoke the original class service callback using the pointer to the
            synthesized packets for the 'pInputDataStart' parameter.

            NOTE This effectively drops the original packet(s).

        4. Inspect '*pnInputDataConsumed' to determine how many packets were
            copied to the class data queue by the routine.

--*/
_IRQL_requires_(DISPATCH_LEVEL)
_IRQL_requires_same_
typedef
VOID
NTAPI
MHK_HOOK_CALLBACK_ROUTINE(
    _In_        PMOUSE_SERVICE_CALLBACK_ROUTINE pServiceCallbackOriginal,
    _In_        PDEVICE_OBJECT                  pClassDeviceObject,
    _In_        PMOUSE_INPUT_DATA               pInputDataStart,
    _In_        PMOUSE_INPUT_DATA               pInputDataEnd,
    _Inout_     PULONG                          pnInputDataConsumed,
    _Inout_opt_ PVOID                           pContext
    );

typedef MHK_HOOK_CALLBACK_ROUTINE *PMHK_HOOK_CALLBACK_ROUTINE;

/*++

Type Name:

    MHK_NOTIFICATION_CALLBACK_ROUTINE

Type Description:

    An MHK notification callback is invoked when a mouse related PNP event
    invalidates the hook environment established when the callback is
    registered.

Parameters:

    RegistrationHandle - The opaque registration handle returned from
        MhkRegisterCallbacks.

    Event - The PnP device interface change event which caused the callback to
        be invoked.

    pContext - Pointer to caller-defined data specified at callback
        registration.

Remarks:

    The MouHid Hook Manager unregisters the registration entry specified by
    'RegistrationHandle' before invoking its notification callback.

    WARNING It is not safe for an MHK notification callback to invoke
    MhkRegisterCallbacks or MhkUnregisterCallbacks.

--*/
_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
typedef
VOID
NTAPI
MHK_NOTIFICATION_CALLBACK_ROUTINE(
    _In_        HANDLE                          RegistrationHandle,
    _In_        MOUSE_PNP_NOTIFICATION_EVENT    Event,
    _Inout_opt_ PVOID                           pContext
    );

typedef MHK_NOTIFICATION_CALLBACK_ROUTINE *PMHK_NOTIFICATION_CALLBACK_ROUTINE;

//=============================================================================
// Meta Interface
//=============================================================================
_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Check_return_
EXTERN_C
NTSTATUS
MhkDriverEntry();

_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
EXTERN_C
VOID
MhkDriverUnload();

//=============================================================================
// Public Interface
//=============================================================================
_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Check_return_
EXTERN_C
NTSTATUS
MhkRegisterCallbacks(
    _In_ PMHK_HOOK_CALLBACK_ROUTINE pHookCallback,
    _In_opt_ PMHK_NOTIFICATION_CALLBACK_ROUTINE pNotificationCallback,
    _In_opt_ PVOID pContext,
    _Outptr_result_nullonfailure_ PHANDLE pRegistrationHandle
);

_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Check_return_
EXTERN_C
NTSTATUS
MhkUnregisterCallbacks(
    _In_ HANDLE RegistrationHandle
);
