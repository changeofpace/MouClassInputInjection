/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#pragma once

#include <fltKernel.h>

#include <ntddmou.h>

//=============================================================================
// Constants
//=============================================================================
#define MOUCLASS_DRIVER_OBJECT_PATH_U   L"\\Driver\\mouclass"

//=============================================================================
// Enumerations
//=============================================================================
typedef enum _MOUSE_PNP_NOTIFICATION_EVENT
{
    MousePnpNotificationEventInvalid = 0,
    MousePnpNotificationEventArrival,
    MousePnpNotificationEventRemoval,

} MOUSE_PNP_NOTIFICATION_EVENT, *PMOUSE_PNP_NOTIFICATION_EVENT;

//=============================================================================
// Public Types
//=============================================================================
/*++

Type Name:

    MOUSE_SERVICE_CALLBACK_ROUTINE

Type Description:

    The mouse class service callback routine.

Parameters:

    pDeviceObject - Pointer to the mouse class device object to receive the
        mouse input data packets.

    pInputDataStart - Pointer to the array of input packets to be copied to the
        class data queue.

    pInputDataEnd - Pointer to the input packet which marks the end of the
        input packet array.

    pnInputDataConsumed - Returns the number of input packets copied to the
        class data queue by the routine.

Remarks:

    This is the mouse class implementation of a PSERVICE_CALLBACK_ROUTINE.

--*/
_IRQL_requires_(DISPATCH_LEVEL)
_IRQL_requires_same_
typedef
VOID
NTAPI
MOUSE_SERVICE_CALLBACK_ROUTINE(
    _In_    PDEVICE_OBJECT      pDeviceObject,
    _In_    PMOUSE_INPUT_DATA   pInputDataStart,
    _In_    PMOUSE_INPUT_DATA   pInputDataEnd,
    _Inout_ PULONG              pnInputDataConsumed
    );

typedef MOUSE_SERVICE_CALLBACK_ROUTINE *PMOUSE_SERVICE_CALLBACK_ROUTINE;

/*++

Type Name:

    MOUSE_PNP_NOTIFICATION_CALLBACK_ROUTINE

Type Description:

    A callback which is invoked each time a mouse device is added to or removed
    from the system.

Parameters:

    Event - The PnP device interface change event which caused the callback to
        be invoked.

    pContext - Pointer to caller-defined data specified at callback
        registration.

Remarks:

    WARNING Mouse PnP notification callbacks are invoked as part of the PnP
    notification chain so they must not block or execute for a significant
    amount of time.

    WARNING It is not safe for a mouse PnP notification callback to invoke
    MclRegisterMousePnpNotificationCallback or
    MclUnregisterMousePnpNotificationCallback.

--*/
_IRQL_requires_max_(PASSIVE_LEVEL)
typedef
VOID
NTAPI
MOUSE_PNP_NOTIFICATION_CALLBACK_ROUTINE(
    _In_        MOUSE_PNP_NOTIFICATION_EVENT    Event,
    _Inout_opt_ PVOID                           pContext
    );

typedef MOUSE_PNP_NOTIFICATION_CALLBACK_ROUTINE
    *PMOUSE_PNP_NOTIFICATION_CALLBACK_ROUTINE;

//=============================================================================
// Meta Interface
//=============================================================================
_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Check_return_
EXTERN_C
NTSTATUS
MclDriverEntry(
    _In_ PDRIVER_OBJECT pDriverObject
);

_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
EXTERN_C
VOID
MclDriverUnload();

//=============================================================================
// Public Interface
//=============================================================================
_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Check_return_
EXTERN_C
NTSTATUS
MclRegisterMousePnpNotificationCallback(
    _In_ PMOUSE_PNP_NOTIFICATION_CALLBACK_ROUTINE pCallback,
    _In_opt_ PVOID pContext,
    _Outptr_result_nullonfailure_ PHANDLE pRegistrationHandle
);

_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
EXTERN_C
VOID
MclUnregisterMousePnpNotificationCallback(
    _Inout_ HANDLE RegistrationHandle
);

_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
EXTERN_C
VOID
MclPrintInputPacket(
    _In_ ULONG64 PacketId,
    _In_ PMOUSE_SERVICE_CALLBACK_ROUTINE pServiceCallback,
    _In_ PDEVICE_OBJECT pDeviceObject,
    _In_ PMOUSE_INPUT_DATA pInputPacket
);

#if defined(DBG)
_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Check_return_
EXTERN_C
NTSTATUS
MclPrintMouClassDeviceObjects();
#endif
