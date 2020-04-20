/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#include "mouclass_input_injection.h"

#include <kbdmou.h>
#include <ntddmou.h>

#include "debug.h"
#include "log.h"
#include "mouclass.h"
#include "mouhid_hook_manager.h"
#include "mouse_input_validation.h"
#include "nt.h"

#include "../Common/time.h"


//=============================================================================
// Constants
//=============================================================================
#define MODULE_TITLE    "MouClass Input Injection"

#define DEVICE_RESOLUTION_TIMEOUT_SECONDS   15


//=============================================================================
// Private Types
//=============================================================================
/*++

Type Name:

    MOUSE_CLASS_BUTTON_DEVICE

Remarks:

    'Mouse class button device' refers to the mouse class device object which
    receives mouse input data packets that contain button data.

--*/
typedef struct _MOUSE_CLASS_BUTTON_DEVICE {

    //
    // The connect data information used by the MouHid driver to copy mouse
    //  input data packets to the mouse class button device object.
    //
    CONNECT_DATA ConnectData;

    //
    // The 'UnitId' field value of all mouse input data packets sent to the
    //  mouse class button device.
    //
    USHORT UnitId;

} MOUSE_CLASS_BUTTON_DEVICE, *PMOUSE_CLASS_BUTTON_DEVICE;

/*++

Type Name:

    MOUSE_CLASS_MOVEMENT_DEVICE

Remarks:

    'Mouse class movement device' refers to the mouse class device object which
    receives mouse input data packets that contain movement data.

--*/
typedef struct _MOUSE_CLASS_MOVEMENT_DEVICE {

    //
    // The connect data information used by the MouHid driver to copy mouse
    //  input data packets to the mouse class movement device object.
    //
    CONNECT_DATA ConnectData;

    //
    // The 'UnitId' field value of all mouse input data packets sent to the
    //  mouse class movement device.
    //
    USHORT UnitId;

    //
    // These fields reflect the 'Flags' field of all mouse input data packets
    //  sent to the mouse class movement device.
    //
    BOOLEAN AbsoluteMovement;
    BOOLEAN VirtualDesktop;

} MOUSE_CLASS_MOVEMENT_DEVICE, *PMOUSE_CLASS_MOVEMENT_DEVICE;

/*++

Type Name:

    MOUSE_DEVICE_STACK_CONTEXT

Type Description:

    Contains the mouse class button device and the mouse class movement device
    for the HID USB mouse device stack(s).

Remarks:

    This context is effectively a snapshot of the mouse class device objects
    when the context is initialized. PnP events for mouse devices and mouse
    device configuration changes invalidate all mouse device stack contexts
    initialized before the event.

    In a standard Windows environment, the mouse class button device and the
    mouse class movement device are represented by a single device object
    created by the MouClass driver. If a third party mouse filter driver is
    installed in a mouse device stack then one or both of these devices may be
    maintained by the third party driver. In this scenario, the third party
    driver has hooked into the input stream and may be filtering, modifying, or
    injecting input packets in(to) one or both of the class data queues.

--*/
typedef struct _MOUSE_DEVICE_STACK_CONTEXT {
    MOUSE_CLASS_BUTTON_DEVICE ButtonDevice;
    MOUSE_CLASS_MOVEMENT_DEVICE MovementDevice;
} MOUSE_DEVICE_STACK_CONTEXT, *PMOUSE_DEVICE_STACK_CONTEXT;

typedef struct _DEVICE_RESOLUTION_CONTEXT {

    KSPIN_LOCK Lock;

    //
    // Callback statistics.
    //
    _Guarded_by_(Lock) SIZE_T NumberOfPacketsProcessed;

    //
    // The returned ntstatus code of the device resolution callback.
    //
    _Guarded_by_(Lock) NTSTATUS NtStatus;

    //
    // Callback progression bookkeeping.
    //
    _Guarded_by_(Lock) BOOLEAN ButtonDeviceResolved;
    _Guarded_by_(Lock) BOOLEAN MovementDeviceResolved;
    _Guarded_by_(Lock) BOOLEAN ResolutionComplete;

    //
    // Signalled when the device resolution callback is complete, regardless of
    //  the returned ntstatus code.
    //
    _Guarded_by_(Lock) KEVENT CompletionEvent;

    //
    // If the device resolution callback is successful then this pointer
    //  returns the initialized mouse device stack context.
    //
    _Guarded_by_(Lock) PMOUSE_DEVICE_STACK_CONTEXT DeviceStackContext;

} DEVICE_RESOLUTION_CONTEXT, *PDEVICE_RESOLUTION_CONTEXT;

typedef struct _MOUCLASS_INPUT_INJECTION_MANAGER {
    HANDLE MousePnpNotificationHandle;
    POINTER_ALIGNMENT ERESOURCE Resource;
    _Guarded_by_(Resource) PMOUSE_DEVICE_STACK_CONTEXT DeviceStackContext;
} MOUCLASS_INPUT_INJECTION_MANAGER, *PMOUCLASS_INPUT_INJECTION_MANAGER;


//=============================================================================
// Module Globals
//=============================================================================
EXTERN_C static MOUCLASS_INPUT_INJECTION_MANAGER g_MiiManager = {};


//=============================================================================
// Private Prototypes
//=============================================================================
_Requires_lock_not_held_(g_MiiManager.Resource)
EXTERN_C
static
MOUSE_PNP_NOTIFICATION_CALLBACK_ROUTINE
MiipMousePnpNotificationCallbackRoutine;

__drv_allocatesMem(Mem)
_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Check_return_
EXTERN_C
static
PDEVICE_RESOLUTION_CONTEXT
MiipCreateDeviceResolutionContext();

_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
EXTERN_C
VOID
MiipFreeMouseDeviceStackContext(
    _Pre_notnull_ __drv_freesMem(Mem)
        PMOUSE_DEVICE_STACK_CONTEXT pDeviceStackContext
);

_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
EXTERN_C
VOID
MiipFreeDeviceResolutionContext(
    _Pre_notnull_ __drv_freesMem(Mem)
        PDEVICE_RESOLUTION_CONTEXT pDeviceResolutionContext
);

#if defined(DBG)
_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
EXTERN_C
static
VOID
MiipPrintMouseDeviceStackContext(
    _In_ PMOUSE_DEVICE_STACK_CONTEXT pDeviceStackContext
);
#endif

_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
EXTERN_C
static
VOID
MiipInitializeMouseDeviceStackInformation(
    _In_ PMOUSE_DEVICE_STACK_CONTEXT pDeviceStackContext,
    _Out_ PMOUSE_DEVICE_STACK_INFORMATION pDeviceStackInformation
);

EXTERN_C
static
MHK_HOOK_CALLBACK_ROUTINE
MiipHookCallback;

_Requires_shared_lock_held_(g_MiiManager.Resource)
_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Check_return_
EXTERN_C
static
NTSTATUS
MiipAttachProcessInjectInputPacket(
    _In_ HANDLE ProcessId,
    _In_ PCONNECT_DATA pConnectData,
    _In_ PMOUSE_INPUT_DATA pInputPacket
);

_Requires_shared_lock_held_(g_MiiManager.Resource)
_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Check_return_
EXTERN_C
static
ULONG
MiipInjectInputPackets(
    _In_ PCONNECT_DATA pConnectData,
    _In_reads_(nInputPackets) PMOUSE_INPUT_DATA pInputDataStart,
    _In_ ULONG nInputPackets
);


//=============================================================================
// Meta Interface
//=============================================================================
_Use_decl_annotations_
EXTERN_C
NTSTATUS
MiiDriverEntry()
/*++

Routine Description:

    Initializes the MouClass Input Injection module.

Required Modules:

    MouClass Manager

Remarks:

    If successful, the caller must call MiiDriverUnload when the driver is
    unloaded.

--*/
{
    BOOLEAN fResourceInitialized = FALSE;
    HANDLE MousePnpNotificationHandle = NULL;
    BOOLEAN fCallbackRegistered = FALSE;
    NTSTATUS ntstatus = STATUS_SUCCESS;

    DBG_PRINT("Loading %s.", MODULE_TITLE);

    //
    // NOTE We must initialize the resource before registering the mouse
    //  notification callback because the notification callback uses the
    //  resource.
    //
    ntstatus = ExInitializeResourceLite(&g_MiiManager.Resource);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("ExInitializeResourceLite failed: 0x%X", ntstatus);
        goto exit;
    }
    //
    fResourceInitialized = TRUE;

    ntstatus = MclRegisterMousePnpNotificationCallback(
        MiipMousePnpNotificationCallbackRoutine,
        NULL,
        &MousePnpNotificationHandle);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("MclRegisterMousePnpNotificationCallback failed: 0x%X",
            ntstatus);
        goto exit;
    }
    //
    fCallbackRegistered = TRUE;

    //
    // Initialize the global context.
    //
    g_MiiManager.MousePnpNotificationHandle = MousePnpNotificationHandle;

    DBG_PRINT("%s loaded.", MODULE_TITLE);

exit:
    if (!NT_SUCCESS(ntstatus))
    {
        if (fCallbackRegistered)
        {
            MclUnregisterMousePnpNotificationCallback(
                MousePnpNotificationHandle);
        }

        if (fResourceInitialized)
        {
            VERIFY(ExDeleteResourceLite(&g_MiiManager.Resource));
        }
    }

    return ntstatus;
}


_Use_decl_annotations_
EXTERN_C
VOID
MiiDriverUnload()
{
    DBG_PRINT("Unloading %s.", MODULE_TITLE);

    MclUnregisterMousePnpNotificationCallback(
        g_MiiManager.MousePnpNotificationHandle);

    if (g_MiiManager.DeviceStackContext)
    {
        MiipFreeMouseDeviceStackContext(g_MiiManager.DeviceStackContext);
        g_MiiManager.DeviceStackContext = NULL;
    }

    VERIFY(ExDeleteResourceLite(&g_MiiManager.Resource));

    DBG_PRINT("%s unloaded.", MODULE_TITLE);
}


//=============================================================================
// Public Interface
//=============================================================================
_Use_decl_annotations_
EXTERN_C
NTSTATUS
MiiInitializeMouseDeviceStackContext(
    PMOUSE_DEVICE_STACK_INFORMATION pDeviceStackInformation
)
/*++

Routine Description:

    Initializes the mouse device stack context in the global context.

Parameters:

    pDeviceStackInformation - Returns information about the newly initialized
        mouse device stack context.

Remarks:

    If successful, the 'DeviceStackContext' field of the global context is
    initialized for the active HID USB mouse device stack(s). The caller must
    free the 'DeviceStackContext' field by calling
    MiipFreeMouseDeviceStackContext.

    NOTE This routine must be invoked and succeed before the input injection
    interface can be used.

--*/
{
    PDEVICE_RESOLUTION_CONTEXT pDeviceResolutionContext = NULL;
    BOOLEAN fResourceAcquired = FALSE;
    HANDLE RegistrationHandle = NULL;
    BOOLEAN fCallbacksRegistered = FALSE;
    LARGE_INTEGER TimeoutInterval = {};
    NTSTATUS waitstatus = STATUS_SUCCESS;
    NTSTATUS ntstatus = STATUS_SUCCESS;

    //
    // Zero out parameters.
    //
    RtlSecureZeroMemory(
        pDeviceStackInformation,
        sizeof(*pDeviceStackInformation));

    DBG_PRINT("Initializing the mouse device stack context.");

    pDeviceResolutionContext = MiipCreateDeviceResolutionContext();
    if (!pDeviceResolutionContext)
    {
        ERR_PRINT("MiipCreateDeviceResolutionContext failed: 0x%X", ntstatus);
        ntstatus = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    ExEnterCriticalRegionAndAcquireResourceExclusive(&g_MiiManager.Resource);
    fResourceAcquired = TRUE;

    //
    // Reset the current device stack context if necessary.
    //
    if (g_MiiManager.DeviceStackContext)
    {
        MiipFreeMouseDeviceStackContext(g_MiiManager.DeviceStackContext);
        g_MiiManager.DeviceStackContext = NULL;
    }

    //
    // Install an MHK hook callback to resolve the mouse class device objects
    //  in the HID USB mouse device stack(s).
    //
    // NOTE The user must provide button input and movement input while the
    //  MHK hook callback is active.
    //
    // NOTE We do not specify an MHK notification callback because this module
    //  registers a mouse PnP notification callback directly during module
    //  initialization.
    //
    ntstatus = MhkRegisterCallbacks(
        MiipHookCallback,
        NULL,
        pDeviceResolutionContext,
        &RegistrationHandle);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("MhkRegisterCallbacks failed: 0x%X", ntstatus);
        goto exit;
    }
    //
    fCallbacksRegistered = TRUE;

    //
    // Wait for the user to provide mouse input.
    //
    MakeRelativeIntervalSeconds(
        &TimeoutInterval,
        DEVICE_RESOLUTION_TIMEOUT_SECONDS);

    DBG_PRINT(
        "Waiting %u seconds for user to provide button and movement input.",
        DEVICE_RESOLUTION_TIMEOUT_SECONDS);

    waitstatus = KeWaitForSingleObject(
        &pDeviceResolutionContext->CompletionEvent,
        Executive,
        KernelMode,
        FALSE,
        &TimeoutInterval);

    //
    // Unregister the MHK callback hook immediately after the wait returns so
    //  that the callback cannot modify the device resolution context.
    //
    ntstatus = MhkUnregisterCallbacks(RegistrationHandle);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("MhkUnregisterCallbacks failed: 0x%X", ntstatus);
        goto exit;
    }
    //
    fCallbacksRegistered = FALSE;

    DBG_PRINT("Wait complete. Processed %Iu mouse input data packets.",
        pDeviceResolutionContext->NumberOfPacketsProcessed);

    switch (waitstatus)
    {
        case STATUS_SUCCESS:
            break;

        case STATUS_TIMEOUT:
            ERR_PRINT("Device resolution callback timed out.");
            ntstatus = STATUS_IO_OPERATION_TIMEOUT;
            goto exit;

        default:
            ERR_PRINT("KeWaitForSingleObject failed: 0x%X (Unexpected)");
            ntstatus = STATUS_INTERNAL_ERROR;
            goto exit;
    }
    //
    if (!NT_SUCCESS(pDeviceResolutionContext->NtStatus))
    {
        ERR_PRINT("Device resolution callback failed: 0x%X",
            pDeviceResolutionContext->NtStatus);
        ntstatus = pDeviceResolutionContext->NtStatus;
        goto exit;
    }

#if defined(DBG)
    MiipPrintMouseDeviceStackContext(
        pDeviceResolutionContext->DeviceStackContext);
#endif

    //
    // Set out parameters.
    //
    MiipInitializeMouseDeviceStackInformation(
        pDeviceResolutionContext->DeviceStackContext,
        pDeviceStackInformation);

    //
    // Update the global context.
    //
    g_MiiManager.DeviceStackContext =
        pDeviceResolutionContext->DeviceStackContext;
    pDeviceResolutionContext->DeviceStackContext = NULL;

    DBG_PRINT("Mouse device stack context initialized.");

exit:
    if (fCallbacksRegistered)
    {
        VERIFY(MhkUnregisterCallbacks(RegistrationHandle));
    }

    if (fResourceAcquired)
    {
        ExReleaseResourceAndLeaveCriticalRegion(&g_MiiManager.Resource);
    }

    if (pDeviceResolutionContext)
    {
        MiipFreeDeviceResolutionContext(pDeviceResolutionContext);
    }

    return ntstatus;
}


_Use_decl_annotations_
EXTERN_C
NTSTATUS
MiiInjectMouseButtonInput(
    HANDLE ProcessId,
    USHORT ButtonFlags,
    USHORT ButtonData
)
/*++

Routine Description:

    Injects a mouse input data packet reflecting the specified input data into
    the input stream in the process context of the specified process id.

Parameters:

    ProcessId - The process id of the process context in which the input
        injection occurs.

    ButtonFlags - The button flags to be injected.

    ButtonData - The button data to be injected.

Remarks:

    This routine validates the specified input data.

--*/
{
    BOOLEAN fResourceAcquired = FALSE;
    MOUSE_INPUT_DATA InputPacket = {};
    NTSTATUS ntstatus = STATUS_SUCCESS;

    DBG_PRINT(
        "Injecting mouse button input data. "
        " (ProcessId = 0x%IX, ButtonFlags = 0x%03hX, ButtonData = 0x%04hX)",
        ProcessId,
        ButtonFlags,
        ButtonData);

    ntstatus = MivValidateButtonInput(ButtonFlags, ButtonData);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("MivValidateMovementInput failed: 0x%X", ntstatus);
        goto exit;
    }

    ExEnterCriticalRegionAndAcquireResourceShared(&g_MiiManager.Resource);
    fResourceAcquired = TRUE;

    if (!g_MiiManager.DeviceStackContext)
    {
        ERR_PRINT("Unexpected mouse device stack context.");
        ntstatus = STATUS_REINITIALIZATION_NEEDED;
        goto exit;
    }

    InputPacket.UnitId = g_MiiManager.DeviceStackContext->ButtonDevice.UnitId;
    InputPacket.ButtonFlags = ButtonFlags;
    InputPacket.ButtonData = ButtonData;

    ntstatus = MiipAttachProcessInjectInputPacket(
        ProcessId,
        &g_MiiManager.DeviceStackContext->ButtonDevice.ConnectData,
        &InputPacket);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("MiipAttachProcessInjectInputPacket failed: 0x%X", ntstatus);
        goto exit;
    }

exit:
    if (fResourceAcquired)
    {
        ExReleaseResourceAndLeaveCriticalRegion(&g_MiiManager.Resource);
    }

    return ntstatus;
}


_Use_decl_annotations_
EXTERN_C
NTSTATUS
MiiInjectMouseMovementInput(
    HANDLE ProcessId,
    USHORT IndicatorFlags,
    LONG MovementX,
    LONG MovementY
)
/*++

Routine Description:

    Injects a mouse input data packet reflecting the specified input data into
    the input stream in the process context of the specified process id.

Parameters:

    ProcessId - The process id of the process context in which the input
        injection occurs.

    IndicatorFlags - The indicator flags to be injected.

    MovementX - The X direction movement data to be injected.

    MovementY - The Y direction movement data to be injected.

Remarks:

    This routine validates the specified input data.

--*/
{
    BOOLEAN fResourceAcquired = FALSE;
    MOUSE_INPUT_DATA InputPacket = {};
    NTSTATUS ntstatus = STATUS_SUCCESS;

    DBG_PRINT(
        "Injecting mouse movement input data. "
        " (ProcessId = 0x%IX, IndicatorFlags = 0x%03hX, MovementX = %d,"
        " MovementY = %d)",
        ProcessId,
        IndicatorFlags,
        MovementX,
        MovementY);

    ntstatus = MivValidateMovementInput(IndicatorFlags, MovementX, MovementY);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("MivValidateMovementInput failed: 0x%X", ntstatus);
        goto exit;
    }

    ExEnterCriticalRegionAndAcquireResourceShared(&g_MiiManager.Resource);
    fResourceAcquired = TRUE;

    if (!g_MiiManager.DeviceStackContext)
    {
        ERR_PRINT("Unexpected mouse device stack context.");
        ntstatus = STATUS_REINITIALIZATION_NEEDED;
        goto exit;
    }

    //
    // Device-specific validation.
    //
    if (g_MiiManager.DeviceStackContext->MovementDevice.AbsoluteMovement)
    {
        if (!(MOUSE_MOVE_ABSOLUTE & IndicatorFlags))
        {
            ERR_PRINT("Unexpected indicator flags. (movement type)");
            ntstatus = STATUS_UNSUCCESSFUL;
            goto exit;
        }
    }
    else
    {
        if (MOUSE_MOVE_ABSOLUTE & IndicatorFlags)
        {
            ERR_PRINT("Unexpected indicator flags. (movement type)");
            ntstatus = STATUS_UNSUCCESSFUL;
            goto exit;
        }
    }

    if (g_MiiManager.DeviceStackContext->MovementDevice.VirtualDesktop)
    {
        if (!(MOUSE_VIRTUAL_DESKTOP & IndicatorFlags))
        {
            ERR_PRINT("Unexpected indicator flags. (virtual desktop)");
            ntstatus = STATUS_UNSUCCESSFUL;
            goto exit;
        }
    }
    else
    {
        if (MOUSE_VIRTUAL_DESKTOP & IndicatorFlags)
        {
            ERR_PRINT("Unexpected indicator flags. (virtual desktop)");
            ntstatus = STATUS_UNSUCCESSFUL;
            goto exit;
        }
    }

    //
    // Initialize the packet.
    //
    InputPacket.UnitId =
        g_MiiManager.DeviceStackContext->MovementDevice.UnitId;
    InputPacket.Flags = IndicatorFlags;
    InputPacket.LastX = MovementX;
    InputPacket.LastY = MovementY;

    ntstatus = MiipAttachProcessInjectInputPacket(
        ProcessId,
        &g_MiiManager.DeviceStackContext->MovementDevice.ConnectData,
        &InputPacket);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("MiipAttachProcessInjectInputPacket failed: 0x%X", ntstatus);
        goto exit;
    }

exit:
    if (fResourceAcquired)
    {
        ExReleaseResourceAndLeaveCriticalRegion(&g_MiiManager.Resource);
    }

    return ntstatus;
}


_Use_decl_annotations_
EXTERN_C
NTSTATUS
MiiInjectMouseInputPacketUnsafe(
    HANDLE ProcessId,
    BOOLEAN fUseButtonDevice,
    PMOUSE_INPUT_DATA pInputPacket
)
/*++

Routine Description:

    Injects the specified mouse input data packet into the input stream in the
    process context of the specified process id.

Parameters:

    ProcessId - The process id of the process context in which the input
        injection occurs.

    fUseButtonDevice - Indicates whether the input packet should be injected
        using the mouse button device or the mouse movement device.

    pInputPacket - The mouse input packet to be injected.

Remarks:

    WARNING This routine does not validate the specified input data.

--*/
{
    MOUSE_INPUT_DATA InputPacketNonPaged = {};
    PCONNECT_DATA pConnectData = NULL;
    NTSTATUS ntstatus = STATUS_SUCCESS;

    //
    // Copy the specified input packet to a local variable so that it cannot be
    //  paged out during injection.
    //
    RtlCopyMemory(
        &InputPacketNonPaged,
        pInputPacket,
        sizeof(MOUSE_INPUT_DATA));

    ExEnterCriticalRegionAndAcquireResourceShared(&g_MiiManager.Resource);

    if (!g_MiiManager.DeviceStackContext)
    {
        ERR_PRINT("Unexpected mouse device stack context.");
        ntstatus = STATUS_REINITIALIZATION_NEEDED;
        goto exit;
    }

    if (fUseButtonDevice)
    {
        pConnectData =
            &g_MiiManager.DeviceStackContext->ButtonDevice.ConnectData;
    }
    else
    {
        pConnectData =
            &g_MiiManager.DeviceStackContext->MovementDevice.ConnectData;
    }

    DBG_PRINT(
        "Injecting mouse input data packet. "
        "(SC=%p DO=%p ID=%hu IF=0x%03hX BF=0x%03hX BD=0x%04hX RB=0x%X EX=0x%X"
        " LX=%d LY=%d",
        pConnectData->ClassService,
        pConnectData->ClassDeviceObject,
        pInputPacket->UnitId,
        pInputPacket->Flags,
        pInputPacket->ButtonFlags,
        pInputPacket->ButtonData,
        pInputPacket->RawButtons,
        pInputPacket->ExtraInformation,
        pInputPacket->LastX,
        pInputPacket->LastY);

    ntstatus = MiipAttachProcessInjectInputPacket(
        ProcessId,
        pConnectData,
        &InputPacketNonPaged);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("MiipAttachProcessInjectInputPacket failed: 0x%X", ntstatus);
        goto exit;
    }

exit:
    ExReleaseResourceAndLeaveCriticalRegion(&g_MiiManager.Resource);

    return ntstatus;
}


//=============================================================================
// Private Interface
//=============================================================================
_Use_decl_annotations_
EXTERN_C
static
VOID
MiipMousePnpNotificationCallbackRoutine(
    MOUSE_PNP_NOTIFICATION_EVENT Event,
    PVOID pContext
)
{
    UNREFERENCED_PARAMETER(pContext);

#if defined(DBG)
    if (MousePnpNotificationEventArrival == Event)
    {
        DBG_PRINT("Received mouse PnP notification. (Arrival)");
    }
    else if (MousePnpNotificationEventRemoval == Event)
    {
        DBG_PRINT("Received mouse PnP notification. (Removal)");
    }
    else
    {
        ERR_PRINT("Received mouse PnP notification. (Unknown)");
        DEBUG_BREAK;
    }
#else
    UNREFERENCED_PARAMETER(Event);
#endif

    ExEnterCriticalRegionAndAcquireResourceExclusive(&g_MiiManager.Resource);

    //
    // This PnP event has invalidated our view of the mouse device stack so
    //  reset the device stack context in the global context.
    //
    if (g_MiiManager.DeviceStackContext)
    {
        MiipFreeMouseDeviceStackContext(g_MiiManager.DeviceStackContext);
        g_MiiManager.DeviceStackContext = NULL;

        DBG_PRINT("Mouse device stack context reset. (PnP)");
    }

    ExReleaseResourceAndLeaveCriticalRegion(&g_MiiManager.Resource);
}


_Use_decl_annotations_
EXTERN_C
static
PDEVICE_RESOLUTION_CONTEXT
MiipCreateDeviceResolutionContext()
/*++

Routine Description:

    Returns an allocated and initialized device resolution context.

Remarks:

    If successful, the caller must free the returned device resolution context
    by calling MiipFreeDeviceResolutionContext.

--*/
{
    PMOUSE_DEVICE_STACK_CONTEXT pDeviceStackContext = NULL;
    PDEVICE_RESOLUTION_CONTEXT pDeviceResolutionContext = NULL;

    pDeviceStackContext = (PMOUSE_DEVICE_STACK_CONTEXT)ExAllocatePool(
        NonPagedPool,
        sizeof(*pDeviceStackContext));
    if (!pDeviceStackContext)
    {
        goto exit;
    }
    //
    RtlSecureZeroMemory(pDeviceStackContext, sizeof(*pDeviceStackContext));

    pDeviceResolutionContext = (PDEVICE_RESOLUTION_CONTEXT)ExAllocatePool(
        NonPagedPool,
        sizeof(*pDeviceResolutionContext));
    if (!pDeviceResolutionContext)
    {
        goto exit;
    }
    //
    RtlSecureZeroMemory(
        pDeviceResolutionContext,
        sizeof(*pDeviceResolutionContext));

    //
    // Initialize the device resolution context.
    //
    KeInitializeSpinLock(&pDeviceResolutionContext->Lock);
    pDeviceResolutionContext->NtStatus = STATUS_INTERNAL_ERROR;
    KeInitializeEvent(
        &pDeviceResolutionContext->CompletionEvent,
        SynchronizationEvent,
        FALSE);
    pDeviceResolutionContext->DeviceStackContext = pDeviceStackContext;

exit:
    if (!pDeviceResolutionContext)
    {
        if (pDeviceStackContext)
        {
            ExFreePool(pDeviceStackContext);
        }
    }

    return pDeviceResolutionContext;
}


_Use_decl_annotations_
EXTERN_C
VOID
MiipFreeMouseDeviceStackContext(
    PMOUSE_DEVICE_STACK_CONTEXT pDeviceStackContext
)
{
    if (pDeviceStackContext->ButtonDevice.ConnectData.ClassDeviceObject)
    {
        ObDereferenceObject(
            pDeviceStackContext->ButtonDevice.ConnectData.ClassDeviceObject);
    }

    if (pDeviceStackContext->MovementDevice.ConnectData.ClassDeviceObject)
    {
        ObDereferenceObject(
            pDeviceStackContext->MovementDevice.ConnectData.ClassDeviceObject);
    }

    ExFreePool(pDeviceStackContext);
}


_Use_decl_annotations_
EXTERN_C
VOID
MiipFreeDeviceResolutionContext(
    PDEVICE_RESOLUTION_CONTEXT pDeviceResolutionContext
)
{
    if (pDeviceResolutionContext->DeviceStackContext)
    {
        MiipFreeMouseDeviceStackContext(
            pDeviceResolutionContext->DeviceStackContext);
    }

    ExFreePool(pDeviceResolutionContext);
}


#if defined(DBG)
_Use_decl_annotations_
EXTERN_C
static
VOID
MiipPrintMouseDeviceStackContext(
    PMOUSE_DEVICE_STACK_CONTEXT pDeviceStackContext
)
{
    DBG_PRINT("Mouse Device Stack Context:");

    DBG_PRINT("    Button Device");
    DBG_PRINT("        ConnectData");
    DBG_PRINT("            ClassDeviceObject:   %p",
        pDeviceStackContext->ButtonDevice.ConnectData.ClassDeviceObject);
    DBG_PRINT("            ClassService:        %p",
        pDeviceStackContext->ButtonDevice.ConnectData.ClassService);
    DBG_PRINT("        UnitId:                  %hu",
        pDeviceStackContext->ButtonDevice.UnitId);

    DBG_PRINT("    Movement Device");
    DBG_PRINT("        ConnectData");
    DBG_PRINT("            ClassDeviceObject:   %p",
        pDeviceStackContext->MovementDevice.ConnectData.ClassDeviceObject);
    DBG_PRINT("            ClassService:        %p",
        pDeviceStackContext->MovementDevice.ConnectData.ClassService);
    DBG_PRINT("        UnitId:                  %hu",
        pDeviceStackContext->MovementDevice.UnitId);
    DBG_PRINT("        AbsoluteMovement:        %s",
        pDeviceStackContext->MovementDevice.AbsoluteMovement ?
        "TRUE" : "FALSE");
    DBG_PRINT("        VirtualDesktop:          %s",
        pDeviceStackContext->MovementDevice.VirtualDesktop ? "TRUE" : "FALSE");
}
#endif


_Use_decl_annotations_
EXTERN_C
static
VOID
MiipInitializeMouseDeviceStackInformation(
    PMOUSE_DEVICE_STACK_CONTEXT pDeviceStackContext,
    PMOUSE_DEVICE_STACK_INFORMATION pDeviceStackInformation
)
{
    RtlSecureZeroMemory(
        pDeviceStackInformation,
        sizeof(*pDeviceStackInformation));

    pDeviceStackInformation->ButtonDevice.UnitId =
        pDeviceStackContext->ButtonDevice.UnitId;

    pDeviceStackInformation->MovementDevice.UnitId =
        pDeviceStackContext->MovementDevice.UnitId;
    pDeviceStackInformation->MovementDevice.AbsoluteMovement =
        pDeviceStackContext->MovementDevice.AbsoluteMovement;
    pDeviceStackInformation->MovementDevice.VirtualDesktop =
        pDeviceStackContext->MovementDevice.VirtualDesktop;
}


_Use_decl_annotations_
EXTERN_C
static
VOID
NTAPI
MiipHookCallback(
    PMOUSE_SERVICE_CALLBACK_ROUTINE pServiceCallbackOriginal,
    PDEVICE_OBJECT pClassDeviceObject,
    PMOUSE_INPUT_DATA pInputDataStart,
    PMOUSE_INPUT_DATA pInputDataEnd,
    PULONG pnInputDataConsumed,
    PVOID pContext
)
/*++

Routine Description:

    Resolves the mouse class device objects in the HID USB mouse device
    stack(s) by analyzing the contents of the input data packets.

Parameters:

    pServiceCallbackOriginal - Pointer to the original mouse service callback.

    pClassDeviceObject - Pointer to the mouse class device object to receive
        the mouse input data packets.

    pInputDataStart - Pointer to the array of input packets to be copied to the
        class data queue.

    pInputDataEnd - Pointer to the input packet which marks the end of the
        input packet array.

    pnInputDataConsumed - Returns the number of input packets copied to the
        class data queue by the routine.

    pContext - Pointer to a device resolution context.

Remarks:

    NOTE The user must provide mouse button input and mouse movement input by
    interacting with the physical mouse while this MHK hook callback is active.

--*/
{
    PDEVICE_RESOLUTION_CONTEXT pDeviceResolutionContext = NULL;
    PMOUSE_INPUT_DATA pInputPacket = NULL;
    BOOLEAN fResolutionComplete = FALSE;
    PMOUSE_CLASS_BUTTON_DEVICE pButtonDevice = NULL;
    PMOUSE_CLASS_MOVEMENT_DEVICE pMovementDevice = NULL;
    LONG EventState = 0;
    NTSTATUS ntstatus = STATUS_SUCCESS;

    pDeviceResolutionContext = (PDEVICE_RESOLUTION_CONTEXT)pContext;

    KeAcquireSpinLockAtDpcLevel(&pDeviceResolutionContext->Lock);

    pDeviceResolutionContext->NumberOfPacketsProcessed++;

    //
    // If device resolution is complete then there is no work to be done.
    //
    if (pDeviceResolutionContext->ResolutionComplete)
    {
        goto exit;
    }

    for (pInputPacket = pInputDataStart;
        pInputPacket < pInputDataEnd;
        ++pInputPacket)
    {
        //
        // Reject unexpected packet data.
        //
        if (!pInputPacket->UnitId)
        {
            ERR_PRINT("Unexpected input packet data. (UnitId = 0)");
            fResolutionComplete = TRUE;
            ntstatus = STATUS_INVALID_ID_AUTHORITY;
            DEBUG_BREAK;
            goto exit;
        }

        //
        // Mouse attribute changes may invalidate the device information
        //  obtained in previous callback invocations.
        //
        if (MOUSE_ATTRIBUTES_CHANGED & pInputPacket->Flags)
        {
            ERR_PRINT("Mouse attributes changed during device resolution.");
            fResolutionComplete = TRUE;
            ntstatus = STATUS_REPARSE_ATTRIBUTE_CONFLICT;
            DEBUG_BREAK;
            goto exit;
        }

        //
        // If this packet contains button data then assume that the class
        //  device object is a valid mouse class button device.
        //
        if (!pDeviceResolutionContext->ButtonDeviceResolved &&
            pInputPacket->Buttons)
        {
            ObReferenceObject(pClassDeviceObject);

            pButtonDevice =
                &pDeviceResolutionContext->DeviceStackContext->ButtonDevice;

            pButtonDevice->ConnectData.ClassDeviceObject = pClassDeviceObject;
            pButtonDevice->ConnectData.ClassService = pServiceCallbackOriginal;
            pButtonDevice->UnitId = pInputPacket->UnitId;

            pDeviceResolutionContext->ButtonDeviceResolved = TRUE;
        }

        //
        // If this packet contains movement data then assume that the class
        //  device object is a valid mouse class movement device.
        //
        if (!pDeviceResolutionContext->MovementDeviceResolved &&
            (pInputPacket->LastX || pInputPacket->LastY))
        {
            ObReferenceObject(pClassDeviceObject);

            pMovementDevice =
                &pDeviceResolutionContext->DeviceStackContext->MovementDevice;

            pMovementDevice->ConnectData.ClassDeviceObject =
                pClassDeviceObject;
            pMovementDevice->ConnectData.ClassService =
                pServiceCallbackOriginal;
            pMovementDevice->UnitId = pInputPacket->UnitId;
            pMovementDevice->AbsoluteMovement =
                (MOUSE_MOVE_ABSOLUTE & pInputPacket->Flags) ? TRUE : FALSE;
            pMovementDevice->VirtualDesktop =
                (MOUSE_VIRTUAL_DESKTOP & pInputPacket->Flags) ? TRUE : FALSE;

            pDeviceResolutionContext->MovementDeviceResolved = TRUE;
        }

        if (pDeviceResolutionContext->ButtonDeviceResolved &&
            pDeviceResolutionContext->MovementDeviceResolved)
        {
            fResolutionComplete = TRUE;
            break;
        }
    }

exit:
    if (fResolutionComplete)
    {
        pDeviceResolutionContext->NtStatus = ntstatus;
        pDeviceResolutionContext->ResolutionComplete = TRUE;
    }

    KeReleaseSpinLockFromDpcLevel(&pDeviceResolutionContext->Lock);

    if (fResolutionComplete)
    {
        EventState = KeSetEvent(
            &pDeviceResolutionContext->CompletionEvent,
            EVENT_INCREMENT,
            FALSE);
        if (EventState)
        {
            ERR_PRINT("Unexpected completion event state. (EventState = %d)",
                EventState);
            DEBUG_BREAK;
        }
    }

    //
    // Invoke the original service callback.
    //
    pServiceCallbackOriginal(
        pClassDeviceObject,
        pInputDataStart,
        pInputDataEnd,
        pnInputDataConsumed);
}


_Use_decl_annotations_
EXTERN_C
static
NTSTATUS
MiipAttachProcessInjectInputPacket(
    HANDLE ProcessId,
    PCONNECT_DATA pConnectData,
    PMOUSE_INPUT_DATA pInputPacket
)
{
    PEPROCESS pProcess = NULL;
    BOOLEAN fHasProcessReference = FALSE;
    BOOLEAN fHasProcessExitSynchronization = FALSE;
    KAPC_STATE ApcState = {};
    ULONG nInputPackets = 0;
    ULONG nPacketsConsumed = 0;
    NTSTATUS ntstatus = STATUS_SUCCESS;

    ntstatus = PsLookupProcessByProcessId(ProcessId, &pProcess);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("PsLookupProcessByProcessId failed: 0x%X", ntstatus);
        goto exit;
    }
    //
    fHasProcessReference = TRUE;

    ntstatus = PsAcquireProcessExitSynchronization(pProcess);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("PsAcquireProcessExitSynchronization failed: 0x%X",
            ntstatus);
        goto exit;
    }
    //
    fHasProcessExitSynchronization = TRUE;

    nInputPackets = 1;

    __try
    {
        __try
        {
            KeStackAttachProcess(pProcess, &ApcState);

            nPacketsConsumed = MiipInjectInputPackets(
                pConnectData,
                pInputPacket,
                nInputPackets);
        }
        __finally
        {
            KeUnstackDetachProcess(&ApcState);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ERR_PRINT("Unexpected exception: 0x%X", GetExceptionCode());
        ntstatus = STATUS_UNSUCCESSFUL;
        goto exit;
    }

    //
    // Verify that the mouse class service callback consumed every packet that
    //  we injected.
    //
    if (nPacketsConsumed != nInputPackets)
    {
        ERR_PRINT("Unexpected number of consumed packets.");
        ntstatus = STATUS_UNSUCCESSFUL;
        goto exit;
    }

exit:
    if (fHasProcessExitSynchronization)
    {
        PsReleaseProcessExitSynchronization(pProcess);
    }

    if (fHasProcessReference)
    {
        ObDereferenceObject(pProcess);
    }

    return ntstatus;
}


_Use_decl_annotations_
EXTERN_C
static
ULONG
MiipInjectInputPackets(
    PCONNECT_DATA pConnectData,
    PMOUSE_INPUT_DATA pInputDataStart,
    ULONG nInputPackets
)
/*++

Routine Description:

    Injects the specified number of mouse input packets into the class data
    queue of the effective mouse class device object by invoking the effective
    mouse class service callback.

Parameters:

    pConnectData - Pointer to connect data information which specifies the
        effective mouse class device object and mouse class service callback.

    pInputDataStart - Pointer to the buffer of mouse input data packets to be
        injected.

    nInputPackets - The number of packets in the specified input packet buffer.

Return Value:

    Returns the number of mouse input data packets consumed by the effective
    mouse class service callback.

--*/
{
    PMOUSE_SERVICE_CALLBACK_ROUTINE pClassService = NULL;
    PDEVICE_OBJECT pClassDeviceObject = NULL;
    ULONG nInputDataConsumed = 0;
    PMOUSE_INPUT_DATA pInputDataEnd = NULL;
    KIRQL PreviousIrql = 0;

    pClassService =
        (PMOUSE_SERVICE_CALLBACK_ROUTINE)pConnectData->ClassService;
    pClassDeviceObject = pConnectData->ClassDeviceObject;
    pInputDataEnd = pInputDataStart + nInputPackets;

    KeRaiseIrql(DISPATCH_LEVEL, &PreviousIrql);

    //
    // NOTE We initialize nInputDataConsumed to zero to follow the convention
    //  used by mouhid!MouHid_ReadComplete.
    //
    pClassService(
        pClassDeviceObject,
        pInputDataStart,
        pInputDataEnd,
        &nInputDataConsumed);

    KeLowerIrql(PreviousIrql);

    return nInputDataConsumed;
}
