#include "internal.h"
#include <iostream>
#include <string>

_Success_(return == NO_ERROR)
UINT32 WinSockInitialize()
{
    UINT16  versionRequested = WINSOCK_VERSION;
    WSADATA wsaData = { 0 };
    UINT32  status =  WSAStartup(versionRequested,
        &wsaData);
    if (status != NO_ERROR) {
        Trace(TRACE_LEVEL_ERROR, "status: %#x",
            status);
    }

    return status;
}


_Success_(return == NO_ERROR)
UINT32 WinSockCleanup()
{
    UINT32 status = WSACleanup();
    if (status != NO_ERROR)
    {
        status = WSAGetLastError();

        Trace(TRACE_LEVEL_ERROR, "status: %#x",
            status);
    }

    return status;
}

_Success_(return == NO_ERROR)
UINT32 WinSockCreate(SOCKET * pSocket)
{
    UINT32 status = NO_ERROR;

    if (*pSocket != INVALID_SOCKET) {
        closesocket(*pSocket);
        *pSocket = INVALID_SOCKET;
    }
        
    *pSocket = socket(
        AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (*pSocket == INVALID_SOCKET)
    {
        status = WSAGetLastError();

        Trace(TRACE_LEVEL_ERROR, "socket() status: %#x",
            status);
    }

    return status;
}

void CleanupNetwork(PDEVICE_CONTEXT deviceContext)
{
    WdfTimerStop(deviceContext->IntervalTimer, FALSE);
    WdfTimerStop(deviceContext->TotalTimer, FALSE);

    if (deviceContext->ThreadHandle != NULL) {
        DWORD result = WAIT_FAILED;
        deviceContext->TerminateThread = true;
        if (deviceContext->ThreadEvent != NULL) {
            SetEvent(deviceContext->ThreadEvent);
            result = WaitForSingleObject(deviceContext->ThreadHandle, 5000);
        }
        if (result != WAIT_OBJECT_0) { // wtf?
            TerminateThread(deviceContext->ThreadEvent, 1);
        }
        CloseHandle(deviceContext->ThreadHandle);
        deviceContext->ThreadHandle = NULL;
    }

    if (deviceContext->ServiceSocket != INVALID_SOCKET) {
        closesocket(deviceContext->ServiceSocket);
        deviceContext->ServiceSocket = INVALID_SOCKET;
    }
    if (deviceContext->ClientSocket != INVALID_SOCKET) {
        closesocket(deviceContext->ClientSocket);
        deviceContext->ClientSocket = INVALID_SOCKET;
    }

    if (deviceContext->ClientThreadHandle != NULL) {
        TerminateThread(deviceContext->ClientThreadHandle, 1);
        CloseHandle(deviceContext->ClientThreadHandle);
        deviceContext->ClientThreadHandle = NULL;
    }
}

void CloseNetwork(
    PDEVICE_CONTEXT deviceContext)
{
	CleanupNetwork(deviceContext);

    if (deviceContext->ThreadEvent != NULL) {
        CloseHandle(deviceContext->ThreadEvent);
        deviceContext->ThreadEvent = NULL;
    }
    if (deviceContext->ServiceSocket != INVALID_SOCKET) {
        closesocket(deviceContext->ServiceSocket);
        deviceContext->ServiceSocket = INVALID_SOCKET;
    }
    if (deviceContext->ServiceSocketEvent != WSA_INVALID_EVENT) {
        WSACloseEvent(deviceContext->ServiceSocketEvent);
        deviceContext->ServiceSocketEvent = WSA_INVALID_EVENT;
    }
    if (deviceContext->ClientSocket != INVALID_SOCKET) {
        closesocket(deviceContext->ClientSocket);
        deviceContext->ClientSocket = INVALID_SOCKET;
    }
    if (deviceContext->ClientSocketEvent != WSA_INVALID_EVENT) {
        WSACloseEvent(deviceContext->ClientSocketEvent);
        deviceContext->ClientSocketEvent = WSA_INVALID_EVENT;
    }
}

//
// try to send. Ignore failures.
//
UINT32 WinSockSend(PDEVICE_CONTEXT deviceContext,
    char* buffer,
    int length)
{
    if (deviceContext->ClientSocket != INVALID_SOCKET) {
        int sent = 0;
        while (sent < length) {
            int result = send(deviceContext->ClientSocket, buffer + sent, length - sent, 0);
            if (result == SOCKET_ERROR) {
                Trace(TRACE_LEVEL_ERROR, "send error %#x",
                    WSAGetLastError());
                break;
            }
            if (result == 0) {
                Trace(TRACE_LEVEL_ERROR, "send  returned zero!");
                break;
            }
            sent += result;
            deviceContext->Stats.bytesWritten += result;
        }
    }
    return NO_ERROR;
}

void calculateReadTimers(PDEVICE_CONTEXT deviceContext)
{
    PREQUEST_CONTEXT requestContext = GetRequestContext(deviceContext->CurrentRequest);
    deviceContext->NumberNeededForRead = requestContext->Length;

    WdfTimerStop(deviceContext->IntervalTimer, FALSE);
    WdfTimerStop(deviceContext->TotalTimer, FALSE);

    deviceContext->returnWithWhatsPresent = FALSE;
    deviceContext->os2ssreturn = FALSE;
    deviceContext->crunchDownToOne = FALSE;
    deviceContext->UseTotalTimer = FALSE;
    deviceContext->UseIntervalTimer = FALSE;

    ULONG multiplierVal = 0;
    ULONG constantVal = 0;
    LARGE_INTEGER totalTime = { 0 };
    SERIAL_TIMEOUTS timeoutsForIrp = deviceContext->Timeouts;

    if (timeoutsForIrp.ReadIntervalTimeout &&
        (timeoutsForIrp.ReadIntervalTimeout !=
            MAXULONG)) 
    {
        deviceContext->UseIntervalTimer = TRUE;
        deviceContext->IntervalTimeRelative.QuadPart = WDF_REL_TIMEOUT_IN_MS(timeoutsForIrp.ReadIntervalTimeout);
        deviceContext->BytesFromLastRead =  0;
        Trace(TRACE_LEVEL_VERBOSE, "UseIntervalTimer: %I64x", deviceContext->IntervalTimeRelative.QuadPart);
    }
    if (timeoutsForIrp.ReadIntervalTimeout == MAXULONG) {
        //
        // We need to do special return quickly stuff here.
        //
        // 1) If both constant and multiplier are
        //    0 then we return immediately with whatever
        //    we've got, even if it was zero.
        //
        // 2) If constant and multiplier are not MAXULONG
        //    then return immediately if any characters
        //    are present, but if nothing is there, then
        //    use the timeouts as specified.
        //
        // 3) If multiplier is MAXULONG then do as in
        //    "2" but return when the first character
        //    arrives.
        //
        if (!timeoutsForIrp.ReadTotalTimeoutConstant &&
            !timeoutsForIrp.ReadTotalTimeoutMultiplier) {

            deviceContext->returnWithWhatsPresent = TRUE;

        }
        else if ((timeoutsForIrp.ReadTotalTimeoutConstant != MAXULONG)
            &&
            (timeoutsForIrp.ReadTotalTimeoutMultiplier
                != MAXULONG)) {

            deviceContext->UseTotalTimer = TRUE;
            deviceContext->os2ssreturn = TRUE;
            multiplierVal = timeoutsForIrp.ReadTotalTimeoutMultiplier;
            constantVal = timeoutsForIrp.ReadTotalTimeoutConstant;

            Trace(TRACE_LEVEL_INFO, "UseTotalTimer: os2ssreturn");

        }
        else if ((timeoutsForIrp.ReadTotalTimeoutConstant != MAXULONG)
            &&
            (timeoutsForIrp.ReadTotalTimeoutMultiplier
                == MAXULONG)) {

            deviceContext->UseTotalTimer = TRUE;
            deviceContext->os2ssreturn = TRUE;
            deviceContext->crunchDownToOne = TRUE;
            multiplierVal = 0;
            constantVal = timeoutsForIrp.ReadTotalTimeoutConstant;
            Trace(TRACE_LEVEL_INFO, "UseTotalTimer: os2ssreturn and crunchDownToOne");

        }
    }
    else {
        //
        // If both the multiplier and the constant are
        // zero then don't do any total timeout processing.
        //
        if (timeoutsForIrp.ReadTotalTimeoutMultiplier ||
            timeoutsForIrp.ReadTotalTimeoutConstant) {
            //
            // We have some timer values to calculate.
            //
            deviceContext->UseTotalTimer = TRUE;
            multiplierVal = timeoutsForIrp.ReadTotalTimeoutMultiplier;
            constantVal = timeoutsForIrp.ReadTotalTimeoutConstant;

        }
    }

    if (deviceContext->UseTotalTimer) {

        deviceContext->TotalTimeRelative.QuadPart = WDF_REL_TIMEOUT_IN_MS(
            ((UInt32x32To64(deviceContext->NumberNeededForRead,
            multiplierVal) 
                + constantVal))); 
        Trace(TRACE_LEVEL_VERBOSE, "UseTotalTimer: %I64x", 
            deviceContext->TotalTimeRelative.QuadPart);

    }

}

// returns true if the request was completed else false.
void processRequest(PDEVICE_CONTEXT deviceContext,
    WSANETWORKEVENTS * networkEvents)
{
    WDFREQUEST readRequest = deviceContext->CurrentRequest;
    PREQUEST_CONTEXT requestContext = GetRequestContext(readRequest);

    int result = recv(deviceContext->ClientSocket, 
        (char*)requestContext->Buffer + requestContext->Information,
        (int)requestContext->Length - requestContext->Information, 0);
    if (result > 0) { 
        // if result is less than length, this request
        // should respect the read timers and wait for more
        // data if required. 
        // If deviceContext->returnWithWhatsPresent is true,
        // just complete the request.
        Trace(TRACE_LEVEL_VERBOSE, "recv %d bytes. Len: %d req: %p Info: %d Needed: %d",
            result,
            requestContext->Length,
            readRequest,
            requestContext->Information,
            deviceContext->NumberNeededForRead);

        deviceContext->Stats.sockRecvData++;
        deviceContext->Stats.bytesRead += result;
        deviceContext->NumberNeededForRead -= result;
        requestContext->Information += result;
        deviceContext->BytesFromLastRead = result;

        if (deviceContext->returnWithWhatsPresent ||
            (0 == deviceContext->NumberNeededForRead) ||
            (deviceContext->os2ssreturn &&
                requestContext->Information))
        {
            deviceContext->CurrentRequest = NULL;
            Trace(TRACE_LEVEL_VERBOSE, "complete req %p info %d",
                readRequest,
                requestContext->Information);
            WdfRequestCompleteWithInformation(readRequest, STATUS_SUCCESS, requestContext->Information);
        }
        return;
    }
    if (result == 0) { // TBD socket is closed?
        Trace(TRACE_LEVEL_ERROR, "recv 0:");
        deviceContext->CurrentRequest = NULL;
        WdfRequestComplete(readRequest, STATUS_UNSUCCESSFUL); 
        return;
    }
    // error returned from recv.
    // EWOULDBLOCK is expected
    // any other error is a damaged connection
    int wsaError = WSAGetLastError();
    if (wsaError == WSAEWOULDBLOCK) {
        if (FD_READ & networkEvents->lNetworkEvents) {

            networkEvents->lNetworkEvents = 0;
        }
        deviceContext->BytesFromLastRead = 0;
        Trace(TRACE_LEVEL_VERBOSE, "recv error: WSAEWOULDBLOCK");
        return;
    }
    // any other recv error is a broken connection.
    Trace(TRACE_LEVEL_ERROR, "recv error: %#x unexpected. Socket closed.", wsaError);
    deviceContext->CurrentRequest = NULL;
    WdfRequestComplete(readRequest, STATUS_UNSUCCESSFUL);
    closesocket(deviceContext->ClientSocket);
    deviceContext->ClientSocket = INVALID_SOCKET;
    return;
}

DWORD ClientThread(PVOID context)
{
    PQUEUE_CONTEXT queueContext = (PQUEUE_CONTEXT)context;
    PDEVICE_CONTEXT deviceContext = queueContext->DeviceContext;

    HANDLE eventArray[] = {
        deviceContext->ClientSocketEvent,
        deviceContext->ThreadEvent,
        deviceContext->ReadQueueEvent,
        deviceContext->CancelEvent,
        deviceContext->IntervalTimerEvent,
        deviceContext->TotalTimerEvent
    };
    DWORD nEvents = sizeof(eventArray) / sizeof(HANDLE);

    deviceContext->CurrentRequest = NULL;
    //
    // loop until terminated.
    //
    WSANETWORKEVENTS networkEvents = { 0 };
    BOOL waitTimeout = false;

    while (!deviceContext->TerminateThread)
    {
        NTSTATUS status;
        WDFREQUEST readRequest;

        if (deviceContext->ClientSocket == INVALID_SOCKET)
        {
            Trace(TRACE_LEVEL_ERROR, "client socket closed");
            break;
        }

        if (!deviceContext->CurrentRequest) {
            status = WdfIoQueueRetrieveNextRequest(queueContext->ReadQueue, &readRequest);
            if (NT_SUCCESS(status)) {
                deviceContext->CurrentRequest = readRequest;
                deviceContext->Stats.readDequeue++;
                calculateReadTimers(deviceContext);
            }
        }

        if ((deviceContext->CurrentRequest) &&
            ((FD_READ & networkEvents.lNetworkEvents) ||
                (waitTimeout)))
        {
            if (waitTimeout) {
                waitTimeout = false;
            }
            processRequest(deviceContext, &networkEvents);
		}
		// either consumes CurrentRequest by completing it
		// or the CurrentRequest needs to wait for more data.
	    // if the request was not completed wait for events
        if (deviceContext->CurrentRequest) {
            //
            // mark request cancelable.
            //
            status = WdfRequestMarkCancelableEx(deviceContext->CurrentRequest, EvtReadRequestCancel);
            if (!NT_SUCCESS(status)) {
                WdfRequestComplete(deviceContext->CurrentRequest, status);
                deviceContext->CurrentRequest = NULL;
                continue;
            }
            // set up timers
            if (deviceContext->UseIntervalTimer) {
                Trace(TRACE_LEVEL_VERBOSE, "set interval timer to %I64x",
                    deviceContext->IntervalTimeRelative.QuadPart);
                WdfTimerStart(deviceContext->IntervalTimer, deviceContext->IntervalTimeRelative.QuadPart);
            }
            else {
                WdfTimerStop(deviceContext->IntervalTimer, FALSE);
            }
            if (deviceContext->UseTotalTimer) {
                Trace(TRACE_LEVEL_VERBOSE, "set total timer to %I64x",
                    deviceContext->TotalTimeRelative.QuadPart);
                WdfTimerStart(deviceContext->TotalTimer, deviceContext->TotalTimeRelative.QuadPart);
            }
            else {
                WdfTimerStop(deviceContext->TotalTimer, FALSE);
            }
        }
        DWORD timeout = INFINITE;
        if (deviceContext->CurrentRequest &&
            !deviceContext->UseIntervalTimer &&
            !deviceContext->UseTotalTimer) {
            timeout = 500;
        }
        DWORD eventIndex = WSAWaitForMultipleEvents(nEvents, eventArray, false, 
            timeout,
            true);
        if (deviceContext->CurrentRequest) {
            WdfRequestUnmarkCancelable(deviceContext->CurrentRequest);
        }

        switch (eventIndex) {
        case WAIT_TIMEOUT:
        {
            waitTimeout = true;

            if (deviceContext->CurrentRequest) {
                PREQUEST_CONTEXT requestContext = GetRequestContext(deviceContext->CurrentRequest);
                requestContext->WaitTimeouts++;

                if (requestContext->WaitTimeouts >= Globals.WaitUnits) {
                    deviceContext->Stats.waitTimeouts++;
                    ULONG level = requestContext->Information ? TRACE_LEVEL_INFO : TRACE_LEVEL_VERBOSE;
                    Trace(level, "complete request STATUS_TIMEOUT n= %d Info: %d",
                        requestContext->WaitTimeouts,
                        requestContext->Information);
                    // ? if information > 0 STATUS_SUCCESS?
                    WdfRequestCompleteWithInformation(deviceContext->CurrentRequest,
                        STATUS_TIMEOUT, 0);
                    deviceContext->CurrentRequest = NULL;
                }
            }
            break;
        }

        case WAIT_OBJECT_0: // socket event
		{
			deviceContext->Stats.totalSocketEvents++;
			networkEvents.lNetworkEvents = 0;
            if (SOCKET_ERROR == WSAEnumNetworkEvents(deviceContext->ClientSocket,
                deviceContext->ClientSocketEvent, &networkEvents)) {
                Trace(TRACE_LEVEL_ERROR, "WSAEnumNetworkEvents error %d",
                    WSAGetLastError());
            }
			if (FD_READ & networkEvents.lNetworkEvents)
			{
				// there is recv data
				Trace(TRACE_LEVEL_VERBOSE, "socket event request %p",
					deviceContext->CurrentRequest);
				deviceContext->Stats.sockReadEvents++;
                /// TBD: read all available data into the read ringbuffer.
			}
            else if (0 == networkEvents.lNetworkEvents) {
                // this is normal.
                Trace(TRACE_LEVEL_VERBOSE, "socket event zero!");
            }
            else {
                Trace(TRACE_LEVEL_INFO, "unexpected socket event %x",
                    networkEvents.lNetworkEvents);
            }
            break;

		}

        case WAIT_OBJECT_0 + 1: // thread event: terminate.
            Trace(TRACE_LEVEL_ERROR, "thread terminate event.");
            if (deviceContext->CurrentRequest) {
                WdfRequestCompleteWithInformation(deviceContext->CurrentRequest,
                    STATUS_CANCELLED, 0);
                deviceContext->CurrentRequest = NULL;
            }
            return 0;

        case WAIT_OBJECT_0 + 2: // read queue event.
            deviceContext->Stats.readQueueEvents++;
            break;

        case WAIT_OBJECT_0 +3: // cancel event.
            if (deviceContext->CurrentRequest) {
                Trace(TRACE_LEVEL_INFO, "cancel event.");
                WdfRequestCompleteWithInformation(deviceContext->CurrentRequest,
                    STATUS_CANCELLED, 0);
                deviceContext->CurrentRequest = NULL;
            }
            break;

        case WAIT_OBJECT_0 + 4: // interval timer event.
            if (deviceContext->CurrentRequest) {
                PREQUEST_CONTEXT requestContext = GetRequestContext(deviceContext->CurrentRequest);
                Trace(TRACE_LEVEL_INFO, "interval timer event. bytes read: %d",
                    requestContext->Information);

                deviceContext->Stats.intervalTimerEvents++;

                if (requestContext->Information && 
                    (deviceContext->BytesFromLastRead == 0)) {
                    // timer doesn't start until at least one byte is read.

                    WdfRequestCompleteWithInformation(deviceContext->CurrentRequest,
                        STATUS_CANCELLED, requestContext->Information);
                    deviceContext->CurrentRequest = NULL;
                }
            }
            break;

        case WAIT_OBJECT_0 + 5: // total timer event.
            if (deviceContext->CurrentRequest) {
                PREQUEST_CONTEXT requestContext = GetRequestContext(deviceContext->CurrentRequest);
                Trace(TRACE_LEVEL_INFO, "total timer event.");

                deviceContext->Stats.totalTimerEvents++;

                WdfRequestCompleteWithInformation(deviceContext->CurrentRequest,
                    STATUS_CANCELLED, requestContext->Information);
                deviceContext->CurrentRequest = NULL;
            }
            break;

        case WSA_WAIT_IO_COMPLETION: // io completion interrupted the wait. retry.
            Trace(TRACE_LEVEL_ERROR, "wait io completion event.");
            break;

        default: // wtf event.
            break;
        }
    }
    return 0;
}DWORD ServiceThread(PVOID context)
{
    PQUEUE_CONTEXT queueContext = (PQUEUE_CONTEXT)context;
    PDEVICE_CONTEXT deviceContext = queueContext->DeviceContext;
    int result;

    HANDLE eventArray[3] = {
        deviceContext->ServiceSocketEvent, // service socket accept event 
        deviceContext->ThreadEvent,        //  terminate
        NULL
    };
    //
    // loop until terminated.
    //
    while (!deviceContext->TerminateThread)
    {
        WSANETWORKEVENTS networkEvents;
        DWORD nEvents = eventArray[2] ? 3 : 2;

        DWORD eventIndex = WSAWaitForMultipleEvents(nEvents, eventArray,
            false, INFINITE, true);

        switch (eventIndex) {
        case WAIT_OBJECT_0:     // socket event
        {
            // only accept one connection at a time.
            if (deviceContext->ClientSocket == INVALID_SOCKET) {
                // reset the event only if we are going to do an accept.
                WSAEnumNetworkEvents(deviceContext->ServiceSocket,
                    deviceContext->ServiceSocketEvent, &networkEvents);

                deviceContext->ClientSocket = accept(deviceContext->ServiceSocket, NULL, 0);
                if (deviceContext->ClientSocket == INVALID_SOCKET) {
                    Trace(TRACE_LEVEL_ERROR, "accept failure %#x",
                        WSAGetLastError());
                    break;
                }

                result = WSAEventSelect(deviceContext->ClientSocket,
                    deviceContext->ClientSocketEvent,
                    FD_READ);
                if (result == SOCKET_ERROR) {
                    Trace(TRACE_LEVEL_ERROR, "WSAEventSelect error % #x",
                        WSAGetLastError());
                    closesocket(deviceContext->ClientSocket);
                    deviceContext->ClientSocket = INVALID_SOCKET;
                    break;
                }
                deviceContext->ClientThreadHandle = CreateThread(NULL, 0, ClientThread, queueContext, 0, NULL);
                if (deviceContext->ClientThreadHandle == NULL) {
                    result = GetLastError();
                    Trace(TRACE_LEVEL_ERROR, "CreateThread error: %#x",
                        result);
                    WSAEventSelect(deviceContext->ClientSocket,
                        deviceContext->ClientSocketEvent,
                        0);
                    closesocket(deviceContext->ClientSocket);
                    deviceContext->ClientSocket = INVALID_SOCKET;
                    break;
                }
                eventArray[2] = deviceContext->ClientThreadHandle;
            }
            break;
        }
        case WAIT_OBJECT_0 + 1:  //  terminate
            return 0;

        case WAIT_OBJECT_0 + 2: // client recv data available
            break;
        case WSA_WAIT_IO_COMPLETION: // io completion interrupted the wait. retry.
            break;
        default:
            break;
        }
    }
    return 0;
}

void stopThread(PDEVICE_CONTEXT deviceContext)
{
    TerminateThread(deviceContext->ThreadHandle, 0);
    CloseHandle(deviceContext->ThreadHandle);
    deviceContext->ThreadHandle = 0;
}

void cleanupSocket(PDEVICE_CONTEXT deviceContext)
{
    WSAEventSelect(deviceContext->ClientSocket,
        deviceContext->ClientSocketEvent,
        0);
    closesocket(deviceContext->ClientSocket);
    deviceContext->ClientSocket = INVALID_SOCKET;
}


UINT32
ConfigureClient(PHTS_VSP_CONFIG vspConfig, PQUEUE_CONTEXT queueContext)
{
    PDEVICE_CONTEXT deviceContext = queueContext->DeviceContext;

    CleanupNetwork(deviceContext);

    struct addrinfo hints = { };
    hints.ai_flags = 0;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    std::string port = std::to_string(vspConfig->port);
    struct addrinfo* srvAddr = NULL;
    struct addrinfo* addr = NULL;

    UINT32 result = WinSockCreate(&deviceContext->ClientSocket);
    if (result != NO_ERROR) {
        // no point in continuing.
        Trace(TRACE_LEVEL_ERROR, "WinSockCreate error: %#x",
            result);
        goto cleanup;
    }
    result = getaddrinfo(vspConfig->address, port.c_str(), &hints, &srvAddr);
    if (result != NO_ERROR) {
        Trace(TRACE_LEVEL_ERROR, "getaddrinfo error %#x",
            result);
        goto cleanup;
    }

    result = ERROR_NO_MORE_ITEMS;
    for (addr = srvAddr; addr != NULL; addr = addr->ai_next) {
        // we can retry connect on each available addresses.
        result = connect(deviceContext->ClientSocket, addr->ai_addr, (int) addr->ai_addrlen);
        if (result == NO_ERROR) {
            Trace(TRACE_LEVEL_INFO,
                "connected to %s:%d",
                vspConfig->address, vspConfig->port);
            break;
        }
    }

    if (result == NO_ERROR) {
        
        // note that this sets the TCP_NODELAY property on the socket.
        result = WSAEventSelect(deviceContext->ClientSocket,
            deviceContext->ClientSocketEvent,
            FD_READ);
        if (result == SOCKET_ERROR) {
            result = WSAGetLastError();
            Trace(TRACE_LEVEL_ERROR, "WSAEventSelect error: %#x",
                result);
            goto cleanup;

        }
        int yes = 1;
        result = setsockopt(deviceContext->ClientSocket, IPPROTO_TCP, TCP_NODELAY, (char*)&yes, sizeof(yes));
        if (result == SOCKET_ERROR) {
            result = WSAGetLastError();
            Trace(TRACE_LEVEL_ERROR, "setsockopt NO_DELAY error: %#x",
                result);
            goto cleanup;

        }
        deviceContext->ThreadHandle = CreateThread(NULL, 0, ClientThread, queueContext, 0, NULL);
        if (deviceContext->ThreadHandle == NULL) {
            result = GetLastError();
            Trace(TRACE_LEVEL_ERROR, "CreateThread error: %#x",
                result);
            goto cleanup;
        }
        Trace(TRACE_LEVEL_INFO, "client thread is ready.");
    }

cleanup:
    if (srvAddr) {
        freeaddrinfo(srvAddr);
    }
    if (result != NO_ERROR) {

        if (deviceContext->ClientSocket != INVALID_SOCKET) {
            cleanupSocket(deviceContext); 
        }
        if (deviceContext->ThreadHandle) {
            stopThread(deviceContext);
        }
    }
    return result == NO_ERROR ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}



UINT32
ConfigureService(PHTS_VSP_CONFIG vspConfig, PQUEUE_CONTEXT queueContext)
{
    PDEVICE_CONTEXT deviceContext = queueContext->DeviceContext;

    CleanupNetwork(deviceContext);

    sockaddr_in service;
    service.sin_family = AF_INET;
    service.sin_addr.s_addr = htonl(INADDR_ANY);
    service.sin_port = htons(vspConfig->port);

    int result = bind(deviceContext->ServiceSocket, (sockaddr*)&service, sizeof(service));
    if (result == SOCKET_ERROR) {
        Trace(TRACE_LEVEL_ERROR, "WSACreateEvent error: %#x",
            WSAGetLastError());
        goto cleanup;

    }
    result = listen(deviceContext->ServiceSocket, 2);
    if (result == SOCKET_ERROR) {
        Trace(TRACE_LEVEL_ERROR, "WSACreateEvent error: %#x",
            WSAGetLastError());
        goto cleanup;

    }
    result = WSAEventSelect(deviceContext->ServiceSocket,
        deviceContext->ServiceSocketEvent,
        FD_ACCEPT);
    if (result == SOCKET_ERROR) {
        Trace(TRACE_LEVEL_ERROR, "WSAEventSelect error: %#x",
            WSAGetLastError());
        goto cleanup;

    }
    deviceContext->ThreadHandle = CreateThread(NULL, 0, ServiceThread, queueContext, 0, NULL);
    if (deviceContext->ThreadHandle == NULL) {
        result = GetLastError();
        Trace(TRACE_LEVEL_ERROR, "CreateThread error: %#x",
            result);
        goto cleanup;
    }

cleanup:
    if (result != NO_ERROR) {

        if (deviceContext->ServiceSocket != INVALID_SOCKET) {
            WSAEventSelect(deviceContext->ServiceSocket,
                deviceContext->ServiceSocketEvent,
                0);
            closesocket(deviceContext->ServiceSocket);
            deviceContext->ServiceSocket = INVALID_SOCKET;
        }

        if (deviceContext->ClientSocket != INVALID_SOCKET) {
            WSAEventSelect(deviceContext->ClientSocket,
                deviceContext->ClientSocketEvent,
                0);
            closesocket(deviceContext->ClientSocket);
            deviceContext->ClientSocket = INVALID_SOCKET;
        }

        if (deviceContext->ThreadHandle) {
            TerminateThread(deviceContext->ThreadHandle, 0);
            CloseHandle(deviceContext->ThreadHandle);
            deviceContext->ThreadHandle = 0;
        }
    }

    Trace(TRACE_LEVEL_INFO, "service thread is ready.");

    return result == NO_ERROR ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}




