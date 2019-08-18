/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#include "mouclass.h"

#include <wdmguid.h>

#include "debug.h"
#include "log.h"

#if defined(DBG)
#include "io_util.h"
#include "nt.h"
#endif


//=============================================================================
// Constants
//=============================================================================
#define MODULE_TITLE    "MouClass Manager"


//=============================================================================
// Private Types
//=============================================================================
typedef struct _PNP_NOTIFICATION_CONTEXT
{
    _Interlocked_ volatile POINTER_ALIGNMENT LONG64 ArrivalEvents;
    _Interlocked_ volatile POINTER_ALIGNMENT LONG64 RemovalEvents;

} PNP_NOTIFICATION_CONTEXT, *PPNP_NOTIFICATION_CONTEXT;

typedef struct _MCL_REGISTRATION_ENTRY
{
    LIST_ENTRY ListEntry;
    PMOUSE_PNP_NOTIFICATION_CALLBACK_ROUTINE Callback;
    PVOID Context;

} MCL_REGISTRATION_ENTRY, *PMCL_REGISTRATION_ENTRY;

typedef struct _MOUCLASS_MANAGER
{
    PVOID PnpNotificationHandle;
    PNP_NOTIFICATION_CONTEXT PnpNotificationContext;

    POINTER_ALIGNMENT ERESOURCE Resource;
    _Guarded_by_(Resource) LIST_ENTRY RegisteredCallbackListHead;

} MOUCLASS_MANAGER, *PMOUCLASS_MANAGER;


//=============================================================================
// Module Globals
//=============================================================================
EXTERN_C static MOUCLASS_MANAGER g_MclManager = {};


//=============================================================================
// Private Prototypes
//=============================================================================
_Requires_lock_not_held_(g_MclManager.Resource)
EXTERN_C
static
DRIVER_NOTIFICATION_CALLBACK_ROUTINE
MclpPnpNotificationCallbackRoutine;


//=============================================================================
// Meta Interface
//=============================================================================
_Use_decl_annotations_
EXTERN_C
NTSTATUS
MclDriverEntry(
    PDRIVER_OBJECT pDriverObject
)
/*++

Routine Description:

    Initializes the MouClass Manager module.

Parameters:

    pDriverObject - Pointer to the driver object for this driver.

Required Modules:

    None

Remarks:

    If successful, the caller must call MclDriverUnload when the driver is
    unloaded.

--*/
{
    PVOID PnpNotificationHandle = NULL;
    BOOLEAN fPnpCallbackRegistered = FALSE;
    BOOLEAN fResourceInitialized = FALSE;
    NTSTATUS ntstatus = STATUS_SUCCESS;

    DBG_PRINT("Loading %s.", MODULE_TITLE);

#if defined(DBG)
    VERIFY(MclPrintMouClassDeviceObjects());
#endif

    //
    // Register for mouse device interface notifications.
    //
    ntstatus = IoRegisterPlugPlayNotification(
        EventCategoryDeviceInterfaceChange,
        0,
        (PVOID)&GUID_DEVINTERFACE_MOUSE,
        pDriverObject,
        MclpPnpNotificationCallbackRoutine,
        &g_MclManager.PnpNotificationContext,
        &PnpNotificationHandle);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("IoRegisterPlugPlayNotification failed: 0x%X", ntstatus);
        goto exit;
    }
    //
    fPnpCallbackRegistered = TRUE;

    ntstatus = ExInitializeResourceLite(&g_MclManager.Resource);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("ExInitializeResourceLite failed: 0x%X", ntstatus);
        goto exit;
    }
    //
    fResourceInitialized = TRUE;

    //
    // Initialize the global context.
    //
    g_MclManager.PnpNotificationHandle = PnpNotificationHandle;
    InitializeListHead(&g_MclManager.RegisteredCallbackListHead);

    DBG_PRINT("%s loaded.", MODULE_TITLE);

exit:
    if (!NT_SUCCESS(ntstatus))
    {
        if (fResourceInitialized)
        {
            VERIFY(ExDeleteResourceLite(&g_MclManager.Resource));
        }

        if (fPnpCallbackRegistered)
        {
            VERIFY(IoUnregisterPlugPlayNotificationEx(PnpNotificationHandle));
        }
    }

    return ntstatus;
}


_Use_decl_annotations_
EXTERN_C
VOID
MclDriverUnload()
{
    DBG_PRINT("Unloading %s.", MODULE_TITLE);

    NT_ASSERT(IsListEmpty(&g_MclManager.RegisteredCallbackListHead));

    VERIFY(IoUnregisterPlugPlayNotificationEx(
        g_MclManager.PnpNotificationHandle));

    VERIFY(ExDeleteResourceLite(&g_MclManager.Resource));

    DBG_PRINT("%s unloaded.", MODULE_TITLE);
}


//=============================================================================
// Public Interface
//=============================================================================
_Use_decl_annotations_
EXTERN_C
NTSTATUS
MclRegisterMousePnpNotificationCallback(
    PMOUSE_PNP_NOTIFICATION_CALLBACK_ROUTINE pCallback,
    PVOID pContext,
    PHANDLE pRegistrationHandle
)
/*++

Routine Description:

    Registers a mouse PnP notification callback.

Parameters:

    pCallback - Pointer to the callback to be registered.

    pContext - Pointer to caller-defined data to be passed as the context
        parameter each time the callback is invoked.

    pRegistrationHandle - Returns an opaque registration handle to be used to
        unregister the callback.

Remarks:

    If successful, the caller must unregister the callback by calling
    MclUnregisterMousePnpNotificationCallback.

--*/
{
    PMCL_REGISTRATION_ENTRY pEntry = NULL;
    NTSTATUS ntstatus = STATUS_SUCCESS;

    //
    // Zero out parameters.
    //
    *pRegistrationHandle = NULL;

    //
    // Allocate and initialize a new registration entry.
    //
    pEntry = (PMCL_REGISTRATION_ENTRY)ExAllocatePool(
        NonPagedPool,
        sizeof(*pEntry));
    if (!pEntry)
    {
        ntstatus = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }
    //
    RtlSecureZeroMemory(pEntry, sizeof(*pEntry));

    pEntry->Callback = pCallback;
    pEntry->Context = pContext;

    //
    // Insert the new entry into the registered callback list.
    //
    ExEnterCriticalRegionAndAcquireResourceExclusive(&g_MclManager.Resource);

    InsertTailList(
        &g_MclManager.RegisteredCallbackListHead,
        &pEntry->ListEntry);

    ExReleaseResourceAndLeaveCriticalRegion(&g_MclManager.Resource);

    DBG_PRINT(
        "Registered mouse PnP notification callback."
        " (Callback = %p, Context = %p, RegistrationHandle = %p)",
        pCallback,
        pContext,
        pEntry);

    //
    // Set out parameters.
    //
    *pRegistrationHandle = (HANDLE)pEntry;

exit:
    return ntstatus;
}


_Use_decl_annotations_
EXTERN_C
VOID
MclUnregisterMousePnpNotificationCallback(
    HANDLE RegistrationHandle
)
{
    PMCL_REGISTRATION_ENTRY pEntry = NULL;

    pEntry = (PMCL_REGISTRATION_ENTRY)RegistrationHandle;

    ExEnterCriticalRegionAndAcquireResourceExclusive(&g_MclManager.Resource);

    RemoveEntryList(&pEntry->ListEntry);

    ExReleaseResourceAndLeaveCriticalRegion(&g_MclManager.Resource);

    ExFreePool(pEntry);

    DBG_PRINT(
        "Unregistered mouse PnP notification callback."
        " (RegistrationHandle = %p)",
        RegistrationHandle);
}


_Use_decl_annotations_
EXTERN_C
VOID
MclPrintInputPacket(
    ULONG64 PacketId,
    PMOUSE_SERVICE_CALLBACK_ROUTINE pServiceCallback,
    PDEVICE_OBJECT pDeviceObject,
    PMOUSE_INPUT_DATA pInputPacket
)
{
    INF_PRINT(
        "Mouse Packet %I64u: SC=%p DO=%p ID=%hu IF=0x%03hX BF=0x%03hX"
        " BD=0x%04hX RB=0x%X EX=0x%X LX=%d LY=%d",
        PacketId,
        pServiceCallback,
        pDeviceObject,
        pInputPacket->UnitId,
        pInputPacket->Flags,
        pInputPacket->ButtonFlags,
        pInputPacket->ButtonData,
        pInputPacket->RawButtons,
        pInputPacket->ExtraInformation,
        pInputPacket->LastX,
        pInputPacket->LastY);
}


//=============================================================================
// Private Interface
//=============================================================================
_Use_decl_annotations_
EXTERN_C
static
NTSTATUS
MclpPnpNotificationCallbackRoutine(
    PVOID pNotificationStructure,
    PVOID pNotificationContext
)
{
    PDEVICE_INTERFACE_CHANGE_NOTIFICATION pNotification = NULL;
    PPNP_NOTIFICATION_CONTEXT pContext = NULL;
    MOUSE_PNP_NOTIFICATION_EVENT Event = {};
    ULONG64 nEvents = 0;
    BOOLEAN fResourceAcquired = FALSE;
    PLIST_ENTRY pListEntry = NULL;
    PMCL_REGISTRATION_ENTRY pEntry = NULL;
    NTSTATUS ntstatus = STATUS_SUCCESS;

    pNotification =
        (PDEVICE_INTERFACE_CHANGE_NOTIFICATION)pNotificationStructure;
    pContext = (PPNP_NOTIFICATION_CONTEXT)pNotificationContext;

    if (IsEqualGUID(pNotification->Event, GUID_DEVICE_INTERFACE_ARRIVAL))
    {
        Event = MousePnpNotificationEventArrival;

        nEvents = InterlockedIncrement64(&pContext->ArrivalEvents);

        DBG_PRINT("Device interface arrival (%I64u): %wZ",
            nEvents,
            pNotification->SymbolicLinkName);
    }
    else if (IsEqualGUID(pNotification->Event, GUID_DEVICE_INTERFACE_REMOVAL))
    {
        Event = MousePnpNotificationEventRemoval;

        nEvents = InterlockedIncrement64(&pContext->RemovalEvents);

        DBG_PRINT("Device interface removal (%I64u): %wZ",
            nEvents,
            pNotification->SymbolicLinkName);
    }
    else
    {
        ERR_PRINT(
            "Unexpected device interface change GUID."
            " (SymbolicLinkName = %wZ)",
            pNotification->SymbolicLinkName);
        DEBUG_BREAK;
        goto exit;
    }

    //
    // Invoke each registered callback.
    //
    ExEnterCriticalRegionAndAcquireResourceShared(&g_MclManager.Resource);
    fResourceAcquired = TRUE;

    for (pListEntry = g_MclManager.RegisteredCallbackListHead.Flink;
        pListEntry != &g_MclManager.RegisteredCallbackListHead;
        pListEntry = pListEntry->Flink)
    {
        pEntry = CONTAINING_RECORD(
            pListEntry,
            MCL_REGISTRATION_ENTRY,
            ListEntry);

        pEntry->Callback(Event, pEntry->Context);
    }

exit:
    if (fResourceAcquired)
    {
        ExReleaseResourceAndLeaveCriticalRegion(&g_MclManager.Resource);
    }

    return ntstatus;
}


#if defined(DBG)
_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Check_return_
EXTERN_C
NTSTATUS
MclPrintMouClassDeviceObjects()
{
    UNICODE_STRING usDriverObject = {};
    PDRIVER_OBJECT pDriverObject = NULL;
    BOOLEAN fHasDriverObjectReference = FALSE;
    PDEVICE_OBJECT* ppDeviceObjectList = NULL;
    ULONG nDeviceObjectList = 0;
    NTSTATUS ntstatus = STATUS_SUCCESS;

    usDriverObject = RTL_CONSTANT_STRING(MOUCLASS_DRIVER_OBJECT_PATH_U);

    ntstatus = ObReferenceObjectByName(
        &usDriverObject,
        OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
        NULL,
        0,
        *IoDriverObjectType,
        KernelMode,
        NULL,
        (PVOID*)&pDriverObject);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("ObReferenceObjectByName failed: 0x%X", ntstatus);
        goto exit;
    }
    //
    fHasDriverObjectReference = TRUE;

    //
    // Get a snapshot of all the  device objects.
    //
    ntstatus = IouEnumerateDeviceObjectList(
        pDriverObject,
        &ppDeviceObjectList,
        &nDeviceObjectList);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("IouEnumerateDeviceObjectList failed: 0x%X", ntstatus);
        goto exit;
    }

    IouPrintDeviceObjectList(
        MOUCLASS_DRIVER_OBJECT_PATH_U,
        ppDeviceObjectList,
        nDeviceObjectList);

exit:
    if (ppDeviceObjectList)
    {
        IouFreeDeviceObjectList(ppDeviceObjectList, nDeviceObjectList);
    }

    if (fHasDriverObjectReference)
    {
        ObDereferenceObject(pDriverObject);
    }

    return ntstatus;
}
#endif
