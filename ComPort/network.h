#pragma once

#pragma comment(lib, "Ws2_32.lib")

_Success_(return == NO_ERROR)
UINT32 WinSockInitialize();

_Success_(return == NO_ERROR)
UINT32 WinSockCleanup();


_Success_(return == NO_ERROR)
UINT32 WinSockCreate(SOCKET * pSocket);

void CloseNetwork(
    PDEVICE_CONTEXT deviceContext);

UINT32 WinSockSend(PDEVICE_CONTEXT deviceContext,
    char* buffer,
    int length);

UINT32
ConfigureService(PHTS_VSP_CONFIG vspConfig, PQUEUE_CONTEXT queueContext);

UINT32
ConfigureClient(PHTS_VSP_CONFIG vspConfig, PQUEUE_CONTEXT queueContext);