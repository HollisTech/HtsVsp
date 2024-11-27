/*++

Copyright (C) Microsoft Corporation, All Rights Reserved

Module Name:

    Internal.h

Abstract:

    This module contains the local type definitions for the VirtualSerial
    driver sample.

Environment:

    Windows Driver Framework

--*/

#pragma once

#ifdef _KERNEL_MODE
#include <ntddk.h>
#else
#include <windows.h>
#endif

#include <wdf.h>

#define _NTDEF_

//
// Include the type specific headers.
//
#include <winsock2.h>
#include <Ws2tcpip.h>
#include "htsvsp.h"
#include "serial.h"
#include "driver.h"
#include "device.h"
#include "ringbuffer.h"
#include "queue.h"
#include "network.h"

typedef struct _CTX_GLOBAL_DATA {
    //
    // Field to control nature of debug output
    //
    ULONG TraceLevel;
    ULONG WaitUnits;

} CTX_GLOBAL_DATA, * PCTX_GLOBAL_DATA;

extern CTX_GLOBAL_DATA Globals;
//
// Tracing and Assert
//
#define TRACE_LEVEL_ERROR   DPFLTR_ERROR_LEVEL
#define TRACE_LEVEL_ALWAYS  TRACE_LEVEL_ERROR
#define TRACE_LEVEL_INFO    DPFLTR_TRACE_LEVEL
#define TRACE_LEVEL_VERBOSE DPFLTR_INFO_LEVEL
#define TRACE_LEVEL_MAX     TRACE_LEVEL_VERBOSE

#define DBPrefix "htsvsp!"

#define Trace(Level, _fmt_, ...)                                \
    __pragma(warning(suppress: 4296))                           \
    if ((Level) <= Globals.TraceLevel) {                        \
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, TRACE_LEVEL_ERROR,      \
            DBPrefix __FUNCTION__ " " _fmt_ "\n", __VA_ARGS__); \
    }


#ifndef ASSERT
#define ASSERT(exp) {                               \
    if (!(exp)) {                                   \
        RtlAssert(#exp, __FILE__, __LINE__, NULL);  \
    }                                               \
}
#endif

