#pragma once
#include <windows.h>

#define VSP_CODE_BASE 200


// no data in either direction. returns success if this port is a htsvsp port.
#define IOCTL_HTSVSP_IDENTIFY  CTL_CODE(FILE_DEVICE_SERIAL_PORT,VSP_CODE_BASE,METHOD_BUFFERED,FILE_ANY_ACCESS)

// input is a HST_VSP_CONFIG structure defining the role and network address of the port
// no output
// returns success if configuration succeeded, else an error.
// 
#define IOCTL_HTSVSP_CONFIGURE  CTL_CODE(FILE_DEVICE_SERIAL_PORT,VSP_CODE_BASE + 1,METHOD_BUFFERED,FILE_ANY_ACCESS)

// input is a DWORD that sets the trace log level for the driver. 0 is log everything.
#define IOCTL_HTSVSP_SET_LOGLEVEL  CTL_CODE(FILE_DEVICE_SERIAL_PORT,VSP_CODE_BASE + 2,METHOD_BUFFERED,FILE_ANY_ACCESS)

// output is a DWORD is the current trace log level. 0 is log everything.
#define IOCTL_HTSVSP_GET_LOGLEVEL  CTL_CODE(FILE_DEVICE_SERIAL_PORT,VSP_CODE_BASE + 3,METHOD_BUFFERED,FILE_ANY_ACCESS)

// output is a HTS_VSP_STATS object
#define IOCTL_HTSVSP_REPORT  CTL_CODE(FILE_DEVICE_SERIAL_PORT,VSP_CODE_BASE + 4,METHOD_BUFFERED,FILE_ANY_ACCESS)

// output is a DWORD
#define IOCTL_HTSVSP_GET_WAIT_UNITS  CTL_CODE(FILE_DEVICE_SERIAL_PORT,VSP_CODE_BASE + 5,METHOD_BUFFERED,FILE_ANY_ACCESS)

// input is a DWORD
#define IOCTL_HTSVSP_SET_WAIT_UNITS  CTL_CODE(FILE_DEVICE_SERIAL_PORT,VSP_CODE_BASE +6,METHOD_BUFFERED,FILE_ANY_ACCESS)

struct HTS_VSP_CONFIG
{
	bool clientMode;     // if true sets to client mode else service mode.
	USHORT port;         // service port number
	CHAR   address[256]; // client: server address (domain name or ip address.)
	                     // service: 0 (INADDR_ANY) for all addresses or a specific network.
};
typedef HTS_VSP_CONFIG* PHTS_VSP_CONFIG;

struct HTS_VSP_REPORT
{
	INT64   bytesWritten;     // total bytes sent
	INT64   bytesRead;        // total bytes recv.
	INT64   intervalTimerEvents;
	INT64   totalTimerEvents;
	INT64   totalSocketEvents;
	INT64   sockReadEvents;   // socket read event signaled.
	INT64   sockRecvData;     // socket recv has data.
	INT64   readQueueEvents;  // queue ready event signalled
	INT64   readDequeue;      // request dequeued
	INT64   waitTimeouts;

	DWORD   traceLevel;
	DWORD   waitUnits;
};
typedef HTS_VSP_REPORT* PHTS_VSP_REPORT;

