/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#include "mouhid_hook_manager.h"

#include <kbdmou.h>
#include <ntddmou.h>
#include <wdmguid.h>

#include "debug.h"
#include "io_util.h"
#include "log.h"
#include "nt.h"
#include "mouclass.h"
#include "mouhid.h"
#include "object_util.h"

#include "../Common/time.h"


//=============================================================================
// Constants
//=============================================================================
#define MODULE_TITLE    "MouHid Hook Manager"

#define UNHOOK_DELAY_INTERVAL_MS    250


//=============================================================================
// Private Types
//=============================================================================
typedef struct _MOUHID_DEVICE_OBJECT
{
    //
    // Referenced device object pointer for this MouHid device.
    //
    PDEVICE_OBJECT DeviceObject;

    //
    // Pointer to the connect data object in the device extension of
    //  'DeviceObject'.
    //
    POINTER_ALIGNMENT PCONNECT_DATA ConnectData;

    PMOUSE_SERVICE_CALLBACK_ROUTINE ServiceCallbackOriginal;

} MOUHID_DEVICE_OBJECT, *PMOUHID_DEVICE_OBJECT;

/*++

Type Name:

    MOUHID_HOOK_CONTEXT

Type Description:

    Contains the array of hooked MouHid device objects for a mouse device
    stack.

--*/
typedef struct _MOUHID_HOOK_CONTEXT
{
    PMOUSE_SERVICE_CALLBACK_ROUTINE ServiceCallbackHook;

    ULONG NumberOfDeviceObjects;
    MOUHID_DEVICE_OBJECT DeviceObjectArray[ANYSIZE_ARRAY];

} MOUHID_HOOK_CONTEXT, *PMOUHID_HOOK_CONTEXT;

typedef struct _MHK_REGISTRATION_ENTRY
{
    PMHK_HOOK_CALLBACK_ROUTINE HookCallback;
    PMHK_NOTIFICATION_CALLBACK_ROUTINE NotificationCallback;
    PVOID Context;

} MHK_REGISTRATION_ENTRY, *PMHK_REGISTRATION_ENTRY;

typedef struct _MOUHID_HOOK_MANAGER
{
    HANDLE MousePnpNotificationHandle;

    POINTER_ALIGNMENT ERESOURCE Resource;

    _Guarded_by_(Resource) BOOLEAN HookActive;
    _Guarded_by_(Resource) PMOUHID_HOOK_CONTEXT HookContext;

    _Guarded_by_(Resource) PMHK_REGISTRATION_ENTRY RegistrationEntry;

} MOUHID_HOOK_MANAGER, *PMOUHID_HOOK_MANAGER;


//=============================================================================
// Module Globals
//=============================================================================
EXTERN_C static MOUHID_HOOK_MANAGER g_MhkManager = {};


//=============================================================================
// Private Prototypes
//=============================================================================
_Requires_lock_not_held_(g_MhkManager.Resource)
EXTERN_C
static
MOUSE_PNP_NOTIFICATION_CALLBACK_ROUTINE
MhkpMousePnpNotificationCallbackRoutine;

_Requires_exclusive_lock_held_(g_MhkManager.Resource)
_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Check_return_
EXTERN_C
static
NTSTATUS
MhkpUnregisterCallbacks(
    _Inout_ HANDLE RegistrationHandle
);

_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Check_return_
EXTERN_C
static
NTSTATUS
MhkpCreateHookContext(
    _In_ PMOUSE_SERVICE_CALLBACK_ROUTINE pServiceCallbackHook,
    _Outptr_result_nullonfailure_ PMOUHID_HOOK_CONTEXT* ppHookContext
);

_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Check_return_
EXTERN_C
static
VOID
MhkpFreeHookContext(
    _Pre_notnull_ __drv_freesMem(Mem) PMOUHID_HOOK_CONTEXT pHookContext
);

#if defined(DBG)
_IRQL_requires_(HIGH_LEVEL)
_IRQL_requires_same_
EXTERN_C
static
VOID
MhkpPrintHookContext(
    _In_ PMOUHID_HOOK_CONTEXT pHookContext
);
#endif

_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
EXTERN_C
static
VOID
MhkpInstallConnectDataHooks(
    _Inout_ PMOUHID_HOOK_CONTEXT pHookContext
);

_Requires_exclusive_lock_held_(g_MhkManager.Resource)
_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
_Check_return_
EXTERN_C
static
NTSTATUS
MhkpHookMouHidDeviceObjects(
    _In_ PMOUSE_SERVICE_CALLBACK_ROUTINE pServiceCallbackHook
);

_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
EXTERN_C
static
VOID
MhkpUninstallConnectDataHooks(
    _Inout_ PMOUHID_HOOK_CONTEXT pHookContext
);

_Requires_exclusive_lock_held_(g_MhkManager.Resource)
_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
EXTERN_C
static
VOID
MhkpUnhookMouHidDeviceObjects();

EXTERN_C
static
MOUSE_SERVICE_CALLBACK_ROUTINE
MhkpServiceCallbackHook;


//=============================================================================
// Meta Interface
//=============================================================================
_Use_decl_annotations_
EXTERN_C
NTSTATUS
MhkDriverEntry()
/*++

Routine Description:

    Initializes the MouHid Hook Manager module.

Required Modules:

    MouClass Manager

Remarks:

    If successful, the caller must call MhkDriverUnload when the driver is
    unloaded.

--*/
{
    BOOLEAN fResourceInitialized = FALSE;
    HANDLE MousePnpNotificationHandle = NULL;
    BOOLEAN fCallbackRegistered = FALSE;
    NTSTATUS ntstatus = STATUS_SUCCESS;

    DBG_PRINT("Loading %s.\n", MODULE_TITLE);

    //
    // NOTE We must initialize the resource before registering the mouse
    //  notification callback because the notification callback uses the
    //  resource.
    //
    ntstatus = ExInitializeResourceLite(&g_MhkManager.Resource);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("ExInitializeResourceLite failed: 0x%X\n", ntstatus);
        goto exit;
    }
    //
    fResourceInitialized = TRUE;

    ntstatus = MclRegisterMousePnpNotificationCallback(
        MhkpMousePnpNotificationCallbackRoutine,
        NULL,
        &MousePnpNotificationHandle);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("MclRegisterMousePnpNotificationCallback failed: 0x%X\n",
            ntstatus);
        goto exit;
    }
    //
    fCallbackRegistered = TRUE;

    //
    // Initialize the global context.
    //
    g_MhkManager.MousePnpNotificationHandle = MousePnpNotificationHandle;

    DBG_PRINT("%s loaded.\n", MODULE_TITLE);

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
            VERIFY(ExDeleteResourceLite(&g_MhkManager.Resource));
        }
    }

    return ntstatus;
}


_Use_decl_annotations_
EXTERN_C
VOID
MhkDriverUnload()
{
    DBG_PRINT("Unloading %s.\n", MODULE_TITLE);

    NT_ASSERT(!g_MhkManager.HookActive);
    NT_ASSERT(!g_MhkManager.HookContext);
    NT_ASSERT(!g_MhkManager.RegistrationEntry);

    MclUnregisterMousePnpNotificationCallback(
        g_MhkManager.MousePnpNotificationHandle);

    VERIFY(ExDeleteResourceLite(&g_MhkManager.Resource));

    DBG_PRINT("%s unloaded.\n", MODULE_TITLE);
}


//=============================================================================
// Public Interface
//=============================================================================
_Use_decl_annotations_
EXTERN_C
NTSTATUS
MhkRegisterCallbacks(
    PMHK_HOOK_CALLBACK_ROUTINE pHookCallback,
    PMHK_NOTIFICATION_CALLBACK_ROUTINE pNotificationCallback,
    PVOID pContext,
    PHANDLE pRegistrationHandle
)
/*++

Routine Description:

    Registers an MHK hook callback and an optional MHK notification callback.
    If registration succeeds then each MouHid device object is hooked.

Parameters:

    pHookCallback - Pointer to the MHK hook callback to be registered.

    pNotificationCallback - Pointer to the MHK notification callback to be
        registered.

    pContext - Pointer to caller-defined data to be passed as the context
        parameter each time the callbacks are invoked.

    pRegistrationHandle - Returns an opaque registration handle to be used to
        unregister the callbacks.

Remarks:

    If successful, the caller must unregister the callbacks by calling
    MhkUnregisterCallbacks.

    NOTE This routine modifies the device extension of every MouHid device
    object.

--*/
{
    PMHK_REGISTRATION_ENTRY pEntry = NULL;
    NTSTATUS ntstatus = STATUS_SUCCESS;

    //
    // Zero out parameters.
    //
    *pRegistrationHandle = NULL;

    DBG_PRINT(
        "Registering MHK callbacks."
        " (HookCallback = %p, NotificationCallback = %p, Context = %p)\n",
        pHookCallback,
        pNotificationCallback,
        pContext);

    ExEnterCriticalRegionAndAcquireResourceExclusive(&g_MhkManager.Resource);

    //
    // Fail if a registration entry already exists.
    //
    if (g_MhkManager.RegistrationEntry)
    {
        ERR_PRINT("Registration limit reached.\n");
        ntstatus = STATUS_IMPLEMENTATION_LIMIT;
        goto exit;
    }

    //
    // Allocate and initialize the new registration entry.
    //
    pEntry = (PMHK_REGISTRATION_ENTRY)ExAllocatePool(
        NonPagedPool,
        sizeof(*pEntry));
    if (!pEntry)
    {
        ntstatus = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }
    //
    RtlSecureZeroMemory(pEntry, sizeof(*pEntry));

    pEntry->HookCallback = pHookCallback;
    pEntry->NotificationCallback = pNotificationCallback;
    pEntry->Context = pContext;

    //
    // Update the global context.
    //
    g_MhkManager.RegistrationEntry = pEntry;

    if (!g_MhkManager.HookActive)
    {
        ntstatus = MhkpHookMouHidDeviceObjects(MhkpServiceCallbackHook);
        if (!NT_SUCCESS(ntstatus))
        {
            ERR_PRINT("MhkpHookMouHidDeviceObjects failed: 0x%X\n", ntstatus);
            goto exit;
        }
    }

    DBG_PRINT("MHK callbacks registered. (RegistrationHandle = %p)\n", pEntry);

    //
    // Set out parameters.
    //
    *pRegistrationHandle = (HANDLE)pEntry;

exit:
    if (!NT_SUCCESS(ntstatus))
    {
        if (g_MhkManager.RegistrationEntry)
        {
            g_MhkManager.RegistrationEntry = NULL;
        }

        if (pEntry)
        {
            ExFreePool(pEntry);
        }
    }

    ExReleaseResourceAndLeaveCriticalRegion(&g_MhkManager.Resource);

    return ntstatus;
}


_Use_decl_annotations_
EXTERN_C
NTSTATUS
MhkUnregisterCallbacks(
    HANDLE RegistrationHandle
)
{
    NTSTATUS ntstatus = STATUS_SUCCESS;

    ExEnterCriticalRegionAndAcquireResourceExclusive(&g_MhkManager.Resource);

    ntstatus = MhkpUnregisterCallbacks(RegistrationHandle);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("MhkpUnregisterCallbacks failed: 0x%X\n", ntstatus);
        goto exit;
    }

exit:
    ExReleaseResourceAndLeaveCriticalRegion(&g_MhkManager.Resource);

    return ntstatus;
}


//=============================================================================
// Private Interface
//=============================================================================
_Use_decl_annotations_
EXTERN_C
static
VOID
MhkpMousePnpNotificationCallbackRoutine(
    MOUSE_PNP_NOTIFICATION_EVENT Event,
    PVOID pContext
)
{
    PMHK_REGISTRATION_ENTRY pEntry = NULL;
    HANDLE RegistrationHandle = NULL;
    PMHK_NOTIFICATION_CALLBACK_ROUTINE pRegisteredNotificationCallback = NULL;
    PVOID pRegisteredCallbackContext = NULL;

    UNREFERENCED_PARAMETER(pContext);

#if defined(DBG)
    if (MousePnpNotificationEventArrival == Event)
    {
        DBG_PRINT("Received mouse PnP notification. (Arrival)\n");
    }
    else if (MousePnpNotificationEventRemoval == Event)
    {
        DBG_PRINT("Received mouse PnP notification. (Removal)\n");
    }
    else
    {
        ERR_PRINT("Received mouse PnP notification. (Unknown)\n");
        DEBUG_BREAK;
    }
#else
    UNREFERENCED_PARAMETER(Event);
#endif

    ExEnterCriticalRegionAndAcquireResourceExclusive(&g_MhkManager.Resource);

    pEntry = g_MhkManager.RegistrationEntry;
    if (pEntry)
    {
        //
        // Store the registration entry information because it will be reset
        //  during the unregister call below.
        //
        RegistrationHandle = (HANDLE)pEntry;
        pRegisteredNotificationCallback = pEntry->NotificationCallback;
        pRegisteredCallbackContext = pEntry->Context;

        VERIFY(MhkpUnregisterCallbacks((HANDLE)pEntry));
    }

    ExReleaseResourceAndLeaveCriticalRegion(&g_MhkManager.Resource);

    //
    // Invoke the notification callback after releasing the resource so that
    //  the callback cannot deadlock the system by invoking a public MHK
    //  function.
    //
    // WARNING This strategy currently enables a race condition where the
    //  registrant attempts to unregister their MHK callbacks before their MHK
    //  notification callback is invoked below. This unregister attempt will
    //  fail because we previously unregistered the MHK callbacks.
    //
    if (pRegisteredNotificationCallback)
    {
        DBG_PRINT(
            "Invoking MHK notification callback. (RegistrationHandle = %p)\n",
            RegistrationHandle);

        //
        // NOTE 'RegistrationHandle' now points to freed memory.
        //
        pRegisteredNotificationCallback(
            RegistrationHandle,
            Event,
            pRegisteredCallbackContext);
    }
}


_Use_decl_annotations_
EXTERN_C
static
NTSTATUS
MhkpUnregisterCallbacks(
    HANDLE RegistrationHandle
)
{
    PMHK_REGISTRATION_ENTRY pEntry = NULL;
    NTSTATUS ntstatus = STATUS_SUCCESS;

    DBG_PRINT("Unregistering MHK callbacks. (RegistrationHandle = %p)\n",
        RegistrationHandle);

    pEntry = (PMHK_REGISTRATION_ENTRY)RegistrationHandle;

    //
    // Fail if the specified handle does not match the active registration
    //  entry.
    //
    if (pEntry != g_MhkManager.RegistrationEntry)
    {
        ERR_PRINT("Invalid registration handle: %p\n", RegistrationHandle);
        ntstatus = STATUS_INVALID_PARAMETER;
        goto exit;
    }

    if (g_MhkManager.HookActive)
    {
        MhkpUnhookMouHidDeviceObjects();
    }

    ExFreePool(g_MhkManager.RegistrationEntry);

    //
    // Update the global context.
    //
    g_MhkManager.RegistrationEntry = NULL;

    DBG_PRINT("MHK callbacks unregistered.\n");

exit:
    return ntstatus;
}


_Use_decl_annotations_
EXTERN_C
static
NTSTATUS
MhkpCreateHookContext(
    PMOUSE_SERVICE_CALLBACK_ROUTINE pServiceCallbackHook,
    PMOUHID_HOOK_CONTEXT* ppHookContext
)
{
    SIZE_T cbConnectDataFieldOffset = 0;
    UNICODE_STRING usDriverObject = {};
    PDRIVER_OBJECT pDriverObject = NULL;
    BOOLEAN fHasDriverObjectReference = FALSE;
    PDEVICE_OBJECT* ppDeviceObjectList = NULL;
    ULONG nDeviceObjectList = 0;
    SIZE_T cbHookContext = 0;
    PMOUHID_HOOK_CONTEXT pHookContext = NULL;
    ULONG i = 0;
    PDEVICE_OBJECT pDeviceObject = NULL;
    PMOUHID_DEVICE_OBJECT pElement = NULL;
    NTSTATUS ntstatus = STATUS_SUCCESS;

    //
    // Zero out parameters.
    //
    *ppHookContext = NULL;

    DBG_PRINT("Creating MouHid hook context.\n");

    cbConnectDataFieldOffset = MhdGetConnectDataFieldOffset();
    if (!cbConnectDataFieldOffset)
    {
        ERR_PRINT("MhdGetConnectDataFieldOffset failed.\n");
        ntstatus = STATUS_INTERNAL_ERROR;
        goto exit;
    }

    //
    // Open the MouHid driver object.
    //
    usDriverObject = RTL_CONSTANT_STRING(MOUHID_DRIVER_OBJECT_PATH_U);

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
        ERR_PRINT("ObReferenceObjectByName failed: 0x%X\n", ntstatus);
        goto exit;
    }
    //
    fHasDriverObjectReference = TRUE;

    //
    // Get a snapshot of all the MouHid device objects.
    //
    ntstatus = IouEnumerateDeviceObjectList(
        pDriverObject,
        &ppDeviceObjectList,
        &nDeviceObjectList);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("IouEnumerateDeviceObjectList failed: 0x%X\n", ntstatus);
        goto exit;
    }

#if defined(DBG)
    IouPrintDeviceObjectList(
        MOUHID_DRIVER_OBJECT_PATH_U,
        ppDeviceObjectList,
        nDeviceObjectList);
#endif

    //
    // Allocate and initialize the new hook context.
    //
    cbHookContext = UFIELD_OFFSET(
        MOUHID_HOOK_CONTEXT,
        DeviceObjectArray[nDeviceObjectList]);

    pHookContext = (PMOUHID_HOOK_CONTEXT)ExAllocatePool(
        NonPagedPool,
        cbHookContext);
    if (!pHookContext)
    {
        ntstatus = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }
    //
    RtlSecureZeroMemory(pHookContext, cbHookContext);

    pHookContext->ServiceCallbackHook = pServiceCallbackHook;
    pHookContext->NumberOfDeviceObjects = nDeviceObjectList;

    for (i = 0; i < nDeviceObjectList; ++i)
    {
        pDeviceObject = ppDeviceObjectList[i];

        //
        // We reference each MouHid device object to ensure that the memory
        //  backing their device extensions remains valid even if the MouHid
        //  driver is unloaded. This allows us to safely uninstall connect data
        //  hooks from 'stale' MouHid device objects.
        //
        ObReferenceObject(pDeviceObject);

        pElement = &pHookContext->DeviceObjectArray[i];

        pElement->DeviceObject = pDeviceObject;

        pElement->ConnectData = OFFSET_POINTER(
            pDeviceObject->DeviceExtension,
            cbConnectDataFieldOffset,
            CONNECT_DATA);
    }

    //
    // Set out parameters.
    //
    *ppHookContext = pHookContext;

exit:
    if (!NT_SUCCESS(ntstatus))
    {
        if (pHookContext)
        {
            ExFreePool(pHookContext);
        }
    }

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


_Use_decl_annotations_
EXTERN_C
static
VOID
MhkpFreeHookContext(
    PMOUHID_HOOK_CONTEXT pHookContext
)
{
    ULONG i = 0;

    for (i = 0; i < pHookContext->NumberOfDeviceObjects; ++i)
    {
        ObDereferenceObject(pHookContext->DeviceObjectArray[i].DeviceObject);
    }

    ExFreePool(pHookContext);
}


#if defined(DBG)
_Use_decl_annotations_
EXTERN_C
static
VOID
MhkpPrintHookContext(
    PMOUHID_HOOK_CONTEXT pHookContext
)
{
    ULONG i = 0;
    PMOUHID_DEVICE_OBJECT pElement = NULL;

    DBG_PRINT("MouHid Hook Context:\n");

    for (i = 0; i < pHookContext->NumberOfDeviceObjects; ++i)
    {
        pElement = &pHookContext->DeviceObjectArray[i];

        DBG_PRINT("%u.\n", i);
        DBG_PRINT("    DeviceObject:        %p\n", pElement->DeviceObject);
        DBG_PRINT("    DeviceExtension:     %p\n",
            pElement->DeviceObject->DeviceExtension);
        DBG_PRINT("    ConnectData:         %p\n", pElement->ConnectData);
        DBG_PRINT("    ClassDeviceObject:   %p\n",
            pElement->ConnectData->ClassDeviceObject);
        DBG_PRINT("    ClassService:        %p\n",
            pElement->ConnectData->ClassService);
    }
}
#endif


_Use_decl_annotations_
EXTERN_C
static
VOID
MhkpInstallConnectDataHooks(
    PMOUHID_HOOK_CONTEXT pHookContext
)
{
    ULONG i = 0;
    PMOUHID_DEVICE_OBJECT pElement = NULL;

    DBG_PRINT("Installing connect data hooks:\n");

    for (i = 0; i < pHookContext->NumberOfDeviceObjects; ++i)
    {
        pElement = &pHookContext->DeviceObjectArray[i];

        pElement->ServiceCallbackOriginal =
            (PMOUSE_SERVICE_CALLBACK_ROUTINE)InterlockedExchangePointer(
                &pElement->ConnectData->ClassService,
                pHookContext->ServiceCallbackHook);

        DBG_PRINT(
            "    %u. Hooked: %p -> %p (DeviceObject = %p)\n",
            i,
            pElement->ServiceCallbackOriginal,
            pElement->ConnectData->ClassService,
            pElement->DeviceObject);
    }
}


_Use_decl_annotations_
EXTERN_C
static
NTSTATUS
MhkpHookMouHidDeviceObjects(
    PMOUSE_SERVICE_CALLBACK_ROUTINE pServiceCallbackHook
)
{
    PMOUHID_HOOK_CONTEXT pHookContext = NULL;
    NTSTATUS ntstatus = STATUS_SUCCESS;

    DBG_PRINT("Hooking MouHid device objects.\n");

    ntstatus = MhkpCreateHookContext(pServiceCallbackHook, &pHookContext);
    if (!NT_SUCCESS(ntstatus))
    {
        ERR_PRINT("MhkpCreateHookContext failed: 0x%X\n", ntstatus);
        goto exit;
    }

    //
    // Set the hook context pointer in the global context before we install the
    //  connect data hooks so that our service callback hook can access it.
    //
    g_MhkManager.HookContext = pHookContext;

    MhkpInstallConnectDataHooks(pHookContext);

    //
    // Update the global context.
    //
    g_MhkManager.HookActive = TRUE;

exit:
    if (!NT_SUCCESS(ntstatus))
    {
        if (pHookContext)
        {
            MhkpFreeHookContext(pHookContext);
        }
    }

    return ntstatus;
}


_Use_decl_annotations_
EXTERN_C
static
VOID
MhkpUninstallConnectDataHooks(
    PMOUHID_HOOK_CONTEXT pHookContext
)
{
    ULONG i = 0;
    PMOUHID_DEVICE_OBJECT pElement = NULL;
    PVOID pExchangeResult = NULL;

    DBG_PRINT("Uninstalling connect data hooks:\n");

    for (i = 0; i < pHookContext->NumberOfDeviceObjects; ++i)
    {
        pElement = &pHookContext->DeviceObjectArray[i];

        pExchangeResult = InterlockedExchangePointer(
            &pElement->ConnectData->ClassService,
            pElement->ServiceCallbackOriginal);
        if (pExchangeResult != pHookContext->ServiceCallbackHook)
        {
            ERR_PRINT("Unexpected ClassService: %p (DeviceObject = %p)\n",
                pExchangeResult,
                pElement->DeviceObject);
            DEBUG_BREAK;
        }

        DBG_PRINT(
            "    %u. Unhooked: %p -> %p (DeviceObject = %p)\n",
            i,
            pExchangeResult,
            pElement->ConnectData->ClassService,
            pElement->DeviceObject);
    }
}


_Use_decl_annotations_
EXTERN_C
static
VOID
MhkpUnhookMouHidDeviceObjects()
{
    PMOUHID_HOOK_CONTEXT pHookContext = NULL;
    LARGE_INTEGER DelayInterval = {};

    DBG_PRINT("Unhooking MouHid device objects.\n");

    pHookContext = g_MhkManager.HookContext;

    MhkpUninstallConnectDataHooks(pHookContext);

    //
    // Delay execution to mitigate the race condition where we free the hook
    //  context before all threads have exited the service callback hook.
    //
    MakeRelativeIntervalMilliseconds(&DelayInterval, UNHOOK_DELAY_INTERVAL_MS);

    VERIFY(KeDelayExecutionThread(KernelMode, FALSE, &DelayInterval));

    //
    // Update the global context.
    //
    g_MhkManager.HookActive = FALSE;
    g_MhkManager.HookContext = NULL;

    MhkpFreeHookContext(pHookContext);
}


_Use_decl_annotations_
EXTERN_C
static
VOID
NTAPI
MhkpServiceCallbackHook(
    PDEVICE_OBJECT pDeviceObject,
    PMOUSE_INPUT_DATA pInputDataStart,
    PMOUSE_INPUT_DATA pInputDataEnd,
    PULONG pnInputDataConsumed
)
/*++

Routine Description:

    The mouse class service callback hook.

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

    This routine is installed in the 'ClassService' field of the CONNECT_DATA
    object inside the device extension of a hooked MouHid device object.

--*/
{
    PMOUHID_HOOK_CONTEXT pHookContext = NULL;
    ULONG i = 0;
    PMOUSE_SERVICE_CALLBACK_ROUTINE pServiceCallbackOriginal = NULL;
    PMOUHID_DEVICE_OBJECT pElement = NULL;

    pHookContext = g_MhkManager.HookContext;

    //
    // Map the target class device object to its original service callback.
    //
    for (i = 0; i < pHookContext->NumberOfDeviceObjects; ++i)
    {
        pElement = &pHookContext->DeviceObjectArray[i];

        if (pElement->ConnectData->ClassDeviceObject == pDeviceObject)
        {
            pServiceCallbackOriginal = pElement->ServiceCallbackOriginal;
            break;
        }
    }
    //
    if (!pServiceCallbackOriginal)
    {
        ERR_PRINT("Unhandled class device object: %p\n", pDeviceObject);
        DEBUG_BREAK;
        goto exit;
    }

    g_MhkManager.RegistrationEntry->HookCallback(
        pServiceCallbackOriginal,
        pDeviceObject,
        pInputDataStart,
        pInputDataEnd,
        pnInputDataConsumed,
        g_MhkManager.RegistrationEntry->Context);

exit:
    return;
}
