/*++

Copyright (C) Microsoft Corporation, All Rights Reserved.

Module Name:

    Driver.c

Abstract:

    This module contains the implementation of the VirtualSerial Sample's
    core driver callback object.

Environment:

    Windows Driver Framework

--*/

#include <initguid.h>
#include "internal.h"
#include "network.h"

CTX_GLOBAL_DATA Globals;

EXTERN_C
NTSTATUS
DriverEntry(
    _In_  PDRIVER_OBJECT    DriverObject,
    _In_  PUNICODE_STRING   RegistryPath
    )
{
    NTSTATUS                status;
    WDF_DRIVER_CONFIG       driverConfig;
    WDF_OBJECT_ATTRIBUTES attributes;
#if DBG
    Globals.TraceLevel = TRACE_LEVEL_INFO;
#else
    Globals.TraceLevel = TRACE_LEVEL_ERROR;
#endif
    Globals.WaitUnits = 3;

    UINT32 error = WinSockInitialize();
   if (error != NO_ERROR) {
       Trace(TRACE_LEVEL_ERROR,
           "WSAStartup error %d", error)
           status = STATUS_UNSUCCESSFUL;
   }
   else {
       WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
       attributes.EvtCleanupCallback = EvtDriverContextCleanup;


       WDF_DRIVER_CONFIG_INIT(&driverConfig,
           EvtDeviceAdd);

       status = WdfDriverCreate(DriverObject,
           RegistryPath,
           WDF_NO_OBJECT_ATTRIBUTES,
           &driverConfig,
           WDF_NO_HANDLE);
   }
    if (!NT_SUCCESS(status)) {
        Trace(TRACE_LEVEL_ERROR,
            "Error: WdfDriverCreate failed 0x%x", status);
        WinSockCleanup();
        return status;
    }

    return status;
}

NTSTATUS
EvtDeviceAdd(
    _In_  WDFDRIVER         Driver,
    _Inout_ PWDFDEVICE_INIT DeviceInit
    )
{
    NTSTATUS                status;
    PDEVICE_CONTEXT         deviceContext;

    status = DeviceCreate(Driver,
                            DeviceInit,
                            &deviceContext);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = DeviceConfigure(deviceContext);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    return status;
}

VOID
EvtDriverContextCleanup(
    _In_ WDFOBJECT DriverObject
)
{
    UNREFERENCED_PARAMETER(DriverObject);

    PAGED_CODE();

    Trace(TRACE_LEVEL_INFO, "");
    WinSockCleanup();

    //
    // Stop WPP Tracing
    //
    // WPP_CLEANUP(WdfDriverWdmGetDriverObject((WDFDRIVER)DriverObject));
}


