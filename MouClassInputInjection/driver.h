/*++

Copyright (c) 2019 changeofpace. All rights reserved.

Use of this source code is governed by the MIT license. See the 'LICENSE' file
for more information.

--*/

#pragma once

#include <fltKernel.h>

//=============================================================================
// Meta Interface
//=============================================================================
EXTERN_C
DRIVER_INITIALIZE
DriverEntry;

EXTERN_C
DRIVER_UNLOAD
MouClassInputInjectionDriverUnload;

//=============================================================================
// Public Interface
//=============================================================================
_Dispatch_type_(IRP_MJ_CREATE)
EXTERN_C
DRIVER_DISPATCH
MouClassInputInjectionDispatchCreate;

_Dispatch_type_(IRP_MJ_CLOSE)
EXTERN_C
DRIVER_DISPATCH
MouClassInputInjectionDispatchClose;

_Dispatch_type_(IRP_MJ_DEVICE_CONTROL)
EXTERN_C
DRIVER_DISPATCH
MouClassInputInjectionDispatchDeviceControl;
