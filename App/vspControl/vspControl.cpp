// test.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <htsvsp.h>
#include <iostream>
#include <sstream>
#include <string>
#include <msports.h>
#include "PortDeviceManager.h"
#include "logger.h"

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#include "cxxopts.hpp"


using std::cout;
using std::endl;

#define _CRT_SECURE_CPP_OVERLOAD_SECURE_NAMES 1

ULONG findHtsVsp(ULONG& portNumber);
HANDLE configVSp(ULONG portNumber, HTS_VSP_CONFIG& config);
void listComportDatabase();
void deleteComPort(ULONG comport);
void setTraceLevel(ULONG level);
void reportStatistics();
int echoService(HTS_VSP_CONFIG& config);
void setWaitUnits(ULONG units);


// Initialize the static members
Logger::Level Logger::currentLevel = Logger::INFO_LVL;
std::mutex Logger::logMutex;
Logger logger;
ULONG htsvspPortNumber = 0;

int main(int argc, CHAR* argv[])
{
    try {
        HTS_VSP_CONFIG config = { 0 };
        bool echoServiceMode = false;

        cxxopts::Options options(argv[0], "control utility for htsvsp");
        options.add_options()
            ("h,help", "print usage")
            ("c,client", "client mode. requires port and ipddress")
            ("s,server", "server mode. requires port and ipaddress")
            ("p,port", "service port, must be greater than zero.", cxxopts::value<USHORT>())
            ("i,ipaddress", "ip address or dns name.", cxxopts::value<std::string>())
            ("l,listports", "list all active comport database ports.")
            ("d,deleteport", "delete comport number.", cxxopts::value<ULONG>())
            ("t,trace", "trace log level (0-3).", cxxopts::value<ULONG>())
            ("r,report", "report statistics.")
            ("v,verbose", "verbose output.")
            ("w,waitUnits", "set the 500ms wait units to n.", cxxopts::value<ULONG>())
            ("addDevice", "add a new htsvsp device")
            ("removeDevice", "remove htsvsp device specified by com port", cxxopts::value<std::string>())
            ("enableDevice", "enable htsvsp device specified by com port", cxxopts::value<std::string>())
            ("disableDevice", "disable htsvsp device specified by com port", cxxopts::value<std::string>())
            ("listDevices", "list all htsvsp device com ports")
            ("stop", "stop network operations.")
            ("selectPort", "select which htsvsp port to use. Default is first found.", cxxopts::value<ULONG>())
            ("echoservice", "run as an echo service on the specified port.", cxxopts::value<USHORT>())
            ("install", "install driver, requires path to the inf file.", cxxopts::value<std::string>())
            ("uninstall", "uninstall driver, requires path to the inf file.", cxxopts::value<std::string>());

        SystemApi api;
        PortDeviceManager portDevice("UMDF\\HtsVsp", &api);

        auto optResult = options.parse(argc, argv);
        if (optResult.count("verbose")) {
            logger.setLogLevel(Logger::VERBOSE_LVL);
        }
        if (optResult.count("help")) {
            std::cout << options.help() << std::endl;
            return 0;
        }
        if (optResult.count("stop")) {
            config.closeConnections = true;
        }
        if (optResult.count("install")) {
            std::string infFile = optResult["install"].as<std::string>();
            return portDevice.installDriver(infFile, true);
        }
        if (optResult.count("uninstall")) {
            std::string infFile = optResult["uninstall"].as<std::string>();
            return portDevice.uninstallDriver(infFile);
        }
        if (optResult.count("addDevice")) {
            return portDevice.addDevice();
        }
        if (optResult.count("listDevices")) {
            portDevice.listDevices();
            return 0;
        }
        if (optResult.count("removeDevice")) {
            std::string comport = str_toupper(optResult["removeDevice"].as<std::string>());
            if (portDevice.removeDevice(comport) == 0) {
                logger << "device " << comport << " removed\n";
                logger.flush(Logger::INFO_LVL);
                return 0;
            }
            return 1;
        }
        if (optResult.count("enableDevice")) {
            std::string comport = str_toupper(optResult["enableDevice"].as<std::string>());
            if (portDevice.enableDevice(comport) == 0) {
                logger << "device " << comport << " enabled\n";
                logger.flush(Logger::INFO_LVL);
                return 0;
            }
            return 1;
        }
        if (optResult.count("disableDevice")) {
            std::string comport = str_toupper(optResult["disableDevice"].as<std::string>());
            if (portDevice.disableDevice(comport) == 0) {
                logger << "device " << comport << " disabled\n";
                logger.flush(Logger::INFO_LVL);
                return 0;
            }
            return 1;
        }
        if (optResult.count("ipaddress")) {
            strcpy_s(config.address, (optResult["ipaddress"].as<std::string>()).c_str());
        }
        if (optResult.count("port")) {
            config.port = optResult["port"].as<USHORT>();
        }
        if (optResult.count("client")) {
            config.clientMode = true;
        }
        if (optResult.count("listports")) {
            listComportDatabase();
            return 0;
        }
        if (optResult.count("deleteport")) {
            deleteComPort(optResult["deleteport"].as<ULONG>());
            return 0;
        }

        if (optResult.count("selectPort")) {
            htsvspPortNumber = optResult["selectPort"].as<ULONG>();
        }

        ULONG result = findHtsVsp(htsvspPortNumber);
        if (result == ERROR_SUCCESS) {
            logger << "found htsvsp at \\\\.\\COM" << htsvspPortNumber << "\n";
            logger.flush(Logger::INFO_LVL);
        }
        else {
            logger << "no htsvsp ports found\n";
            logger.flush(Logger::ERROR_LVL);
            return result;
        }

        // these three functions depend on selectPort to work correctly.
        if (optResult.count("trace")) {
            setTraceLevel(optResult["trace"].as<ULONG>());
            return 0;
        }
        if (optResult.count("report")) {
            reportStatistics();
            return 0;
        }
        if (optResult.count("waitUnits")) {
            setWaitUnits(optResult["waitUnits"].as<ULONG>());
            return 0;
        }

        if (optResult.count("echoservice")) {
            config.port = optResult["echoservice"].as<USHORT>();
            echoServiceMode = true;
        }
        if (config.closeConnections) {
            HANDLE handle = configVSp(htsvspPortNumber, config);
            if (handle == INVALID_HANDLE_VALUE) {
                logger << "failed to close connections\n";
                logger.flush(Logger::ERROR_LVL);
                return 1;
            }
            logger << "connections closed\n";
            logger.flush(Logger::INFO_LVL);
            CloseHandle(handle);
            return 0;
        }
        if (config.port == 0) {
            logger.log(Logger::ERROR_LVL, "port number must be greater than zero\n");
            return 0;
        }
        if (config.clientMode && config.address[0] == 0) {
            logger.log(Logger::ERROR_LVL, "client mode requires an ip address or dns name\n");
            return 0;
        }
        if (echoServiceMode) {
            return echoService(config);
        }

        result = ERROR_SUCCESS;
        HANDLE handle = configVSp(htsvspPortNumber, config);
        if (handle == INVALID_HANDLE_VALUE) {
            result = ERROR_FILE_NOT_FOUND;
        }
        else {
            logger << "htsvsp at \\\\.\\COM" << htsvspPortNumber << " configured\n";
            CloseHandle(handle);
        }
        logger.flush(Logger::INFO_LVL);
        return (int)result;
    }
    catch (const cxxopts::exceptions::exception& e) {
        logger << "error parsing options: " << e.what() << endl;
        logger.flush(Logger::ERROR_LVL);
        return 1;
    }
    catch (const std::exception& e) {
        logger << "error: " << e.what() << endl;
        logger.flush(Logger::ERROR_LVL);
        return 1;
    }
}

void reportStatistics()
{
    ULONG portNumber;
    ULONG result = findHtsVsp(portNumber);
    if (result == ERROR_SUCCESS) {
        logger << "found htsvsp at \\\\.\\COM" << portNumber << "\n";
        logger.flush(Logger::INFO_LVL);
    }
    else {
        logger << "no htsvsp ports found\n";
        logger.flush(Logger::INFO_LVL);
        return;
    }

    HANDLE h = OpenCommPort(portNumber, GENERIC_READ | GENERIC_WRITE, FILE_FLAG_OVERLAPPED);
    if (h != INVALID_HANDLE_VALUE) {
        ULONG bytesReturned;
        HTS_VSP_REPORT report = { 0 };
        
        bool bResult = DeviceIoControl(h, IOCTL_HTSVSP_REPORT,
            NULL, 0, &report, sizeof(report), &bytesReturned, NULL);
        if (!bResult) {
            logger << "DeviceIoControl IOCTL_HTSVSP_REPORT failed error " << GetLastError() << "\n";
            logger.flush(Logger::ERROR_LVL);
            CloseHandle(h);
            return;
        }
        logger <<
            "bytes written:     " << report.bytesWritten << endl <<
            "bytes read:        " << report.bytesRead << endl <<
            "interval events:   " << report.intervalTimerEvents << endl <<
            "total tm events:   " << report.totalTimerEvents << endl <<
            "total sock events: " << report.totalSocketEvents << endl <<
            "read sock events:  " << report.sockReadEvents << endl <<
            "recv data success: " << report.sockRecvData << endl <<
            "read queue events: " << report.readQueueEvents << endl <<
            "read de-queues:    " << report.readDequeue << endl <<
            "wait timeouts:     " << report.waitTimeouts << endl <<
            "wait units:        " << report.waitUnits << endl <<
            "trace level:       " << report.traceLevel << endl;
        logger.flush(Logger::INFO_LVL);
        CloseHandle(h);
    }

}

void setTraceLevel(ULONG level) 
{
    ULONG portNumber;
    ULONG result = findHtsVsp(portNumber);
    if (result == ERROR_SUCCESS) {
        cout << "found htsvsp at \\\\.\\COM" << portNumber << "\n";
    }
    else {
        cout << "no htsvsp ports found\n";
        return;
    }

    HANDLE h = OpenCommPort(portNumber, GENERIC_READ | GENERIC_WRITE, FILE_FLAG_OVERLAPPED);
    if (h != INVALID_HANDLE_VALUE) {
        ULONG bytesReturned;
        DWORD currentLevel;
        bool bResult = DeviceIoControl(h, IOCTL_HTSVSP_GET_LOGLEVEL,
            NULL, 0, &currentLevel, sizeof(currentLevel), &bytesReturned, NULL);
        if (!bResult) {
            cout << "DeviceIoControl IOCTL_HTSVSP_GET_LOGLEVEL failed error " << GetLastError() << "\n";
            CloseHandle(h);
            return;
        }
        if (level == currentLevel) {
            cout << "current log level is already set to " << currentLevel << "\n";
            CloseHandle(h);
            return;
        }
        bResult = DeviceIoControl(h, IOCTL_HTSVSP_SET_LOGLEVEL,
            &level, sizeof(level), NULL, 0, &bytesReturned, NULL);
        if (!bResult) {
            cout << "DeviceIoControl IOCTL_HTSVSP_SET_LOGLEVEL failed error " << GetLastError() << "\n";
        }
        else
        {
            cout << "trace log level set to " << level << "\n";
        }
        CloseHandle(h);
    }

}
void setWaitUnits(ULONG units)
{
    ULONG portNumber;
    ULONG result = findHtsVsp(portNumber);
    if (result == ERROR_SUCCESS) {
        cout << "found htsvsp at \\\\.\\COM" << portNumber << "\n";
    }
    else {
        cout << "no htsvsp ports found\n";
        return;
    }

    HANDLE h = OpenCommPort(portNumber, GENERIC_READ | GENERIC_WRITE, FILE_FLAG_OVERLAPPED);
    if (h != INVALID_HANDLE_VALUE) {
        ULONG bytesReturned;
        DWORD currentUnits;
        bool bResult = DeviceIoControl(h, IOCTL_HTSVSP_GET_WAIT_UNITS,
            NULL, 0, &currentUnits, sizeof(currentUnits), &bytesReturned, NULL);
        if (!bResult) {
            cout << "DeviceIoControl IOCTL_HTSVSP_GET_WAIT_UNITS failed error " << GetLastError() << "\n";
            CloseHandle(h);
            return;
        }
        if (units == currentUnits) {
            cout << "current log level is already set to " << currentUnits << "\n";
            CloseHandle(h);
            return;
        }
        bResult = DeviceIoControl(h, IOCTL_HTSVSP_SET_WAIT_UNITS,
            &units, sizeof(units), NULL, 0, &bytesReturned, NULL);
        if (!bResult) {
            cout << "DeviceIoControl IOCTL_HTSVSP_SET_WAIT_UNITS failed error " << GetLastError() << "\n";
        }
        else
        {
            cout << "wait units set to " << units << "\n";
        }
        CloseHandle(h);
    }

}

bool testHtsVspPort(ULONG portNumber)
{
#pragma warning(push)
#pragma warning(disable:6385)
    HANDLE h = OpenCommPort(portNumber, GENERIC_READ | GENERIC_WRITE, FILE_FLAG_OVERLAPPED);
    if (h == INVALID_HANDLE_VALUE) {
        cout << "OpenCommPort failed error " << GetLastError() << "\n";
        return false;
    }
    ULONG bytesReturned;
    bool bResult = DeviceIoControl(h, IOCTL_HTSVSP_IDENTIFY,
        NULL, 0, NULL, 0, &bytesReturned, NULL);
    CloseHandle(h);
    return bResult;
#pragma warning(pop)
}

ULONG findHtsVsp(ULONG& portNumber)
{
    // htsvspPortNumber overrides the search if it is not zero
    if (htsvspPortNumber != 0) {
        if (testHtsVspPort(htsvspPortNumber)) {
            portNumber = htsvspPortNumber;
            return ERROR_SUCCESS;
        }
        return ERROR_FILE_NOT_FOUND;
    }
    ULONG* portNumbers = NULL;
    ULONG portNumbersCount = 0;
    ULONG portNumbersFound = 0;

    ULONG result = GetCommPorts(portNumbers, portNumbersCount, &portNumbersFound);
    while (result == ERROR_MORE_DATA) {
        if (portNumbers) {
            delete[] portNumbers;
        }
        portNumbers = new ULONG[portNumbersFound];
        if (portNumbers == NULL) {
            cout << "allocation of portNumbers failed\n";
            return 1;
        }
        portNumbersCount = portNumbersFound;
        result = GetCommPorts(portNumbers, portNumbersCount, &portNumbersFound);
    }
    if (portNumbers == NULL) {
        cout << "no ports found\n";
        return 1;
    }
    if (result != ERROR_SUCCESS) {
        cout << "GetCommPorts failed error: " << result << "\n";
        return result;
    }
    result = ERROR_FILE_NOT_FOUND;
    for (ULONG i = 0; i < portNumbersFound; i++) {
        if (testHtsVspPort(portNumbers[i])) {
            portNumber = portNumbers[i];
            result = ERROR_SUCCESS;
            break;
        }
    }
    delete[] portNumbers;
    return result;
}

HANDLE configVSp(ULONG portNumber, HTS_VSP_CONFIG& config)
{
    HANDLE h = OpenCommPort(portNumber, GENERIC_READ | GENERIC_WRITE, FILE_FLAG_OVERLAPPED);
    if (h != INVALID_HANDLE_VALUE) {
        ULONG bytesReturned;
        bool bResult = DeviceIoControl(h, IOCTL_HTSVSP_CONFIGURE,
            &config, sizeof(config), NULL, 0, &bytesReturned, NULL);
        if (!bResult) {
            cout << "DeviceIoControl IOCTL_HTSVSP_CONFIGURE failed error " << GetLastError() << "\n";
            CloseHandle(h);
            h = INVALID_HANDLE_VALUE;
        }
    }
    return h;
}


void listComportDatabase()
{
    HCOMDB handle;
    LONG result = ComDBOpen(&handle);
    if (result != ERROR_SUCCESS) {
        cout << "ComDBOpen error " << result << endl;
        return;
    }
    DWORD nPorts = 0;
    result = ComDBGetCurrentPortUsage(
        handle,
        NULL, 0,
        CDB_REPORT_BYTES,
        &nPorts);
    if (result != ERROR_SUCCESS) {
        ComDBClose(handle);
        cout << "ComDBGetCurrentPortUsage error " << result << endl;
        return;
    }
    PUCHAR portBuffer = new UCHAR[nPorts];
    if (portBuffer == NULL) {
        ComDBClose(handle);
        return;
    }
    result = ComDBGetCurrentPortUsage(
        handle,
        portBuffer, sizeof(UCHAR) * nPorts,
        CDB_REPORT_BYTES,
        &nPorts);
    if (result != ERROR_SUCCESS) {
        cout << "ComDBGetCurrentPortUsage error " << result << endl;
        ComDBClose(handle);
        delete[] portBuffer;
        return;
    }
    ULONG active = 0;
    for (ULONG index = 0; index < nPorts; index++) {
#pragma warning(push)
#pragma warning(disable:6385)
        if (portBuffer[index]) {
            active++;
        }
#pragma warning(pop)
    }
    cout << "ComDB Ports (total: " << nPorts << " Active ports: " << active << "):\n";
    for (ULONG index = 0; index < nPorts; index++) {
        if (portBuffer[index]) {
            cout << "\t" << index + 1 << endl;
        }
    }
    ComDBClose(handle);
    delete[] portBuffer;

}
void deleteComPort(ULONG comport)
{
    cout << "deleting COM" << comport << endl;

    HCOMDB handle;
    LONG result = ComDBOpen(&handle);
    if (result != ERROR_SUCCESS) {
        cout << "ComDBOpen error " << result << endl;
        return;
    }
    result = ComDBReleasePort(handle, comport);
    if (result != ERROR_SUCCESS) {
        ComDBClose(handle);
        cout << "ComDBReleasePort error " << result << endl;
        return;
    }
    ComDBClose(handle);
}

int echoService(HTS_VSP_CONFIG& config)
{
    SOCKET srvSocket = INVALID_SOCKET;
    SOCKET client = INVALID_SOCKET;

    UINT16  versionRequested = WINSOCK_VERSION;
    WSADATA wsaData = { 0 };
    UINT32  status = WSAStartup(versionRequested,
        &wsaData);
    if (status != NO_ERROR) {
        cout << "echo: WSAStartup error " << status << endl;
        return 1;
    }
    srvSocket = socket(
        AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (srvSocket == INVALID_SOCKET)
    {
        status = WSAGetLastError();
        cout << "echo: socket() status: " << status << endl;
        return 1;
    };
    sockaddr_in service;
    service.sin_family = AF_INET;
    service.sin_addr.s_addr = htonl(INADDR_ANY);
    service.sin_port = htons(config.port);
    cout << "echo: bind to: " << inet_ntoa(service.sin_addr) << ":" << ntohs(service.sin_port) << endl;
    int result = bind(srvSocket, (sockaddr*)&service, sizeof(service));
    if (result == SOCKET_ERROR) {
        cout << "echo: bind error " << WSAGetLastError() << endl;
        closesocket(srvSocket);
        return 1;

    }

    struct sockaddr addr = { 0 };
    int addrlen = sizeof(addr);
    if (getsockname(srvSocket, &addr, &addrlen) == SOCKET_ERROR)
    {
        cout << "echo: getsockname error " << WSAGetLastError() << endl;
        return 1;
    }
    else
    {
        char buffer[1024];
        DWORD cbBuffer = 1024;
        if ((WSAAddressToString(&addr,
            addrlen,
            NULL,
            buffer,
            &cbBuffer)) == SOCKET_ERROR)
        {
            cout << "echo: WSAAddressToString error " << WSAGetLastError() << endl;
            return 1;
        }
        cout << "echo: listen on: " << buffer << endl;
    }

    result = listen(srvSocket, 2);
    if (result == SOCKET_ERROR) {
        cout << "echo: listen error: " << WSAGetLastError() << endl;
        closesocket(srvSocket);
        return 1;
    }

    while (srvSocket != INVALID_SOCKET) {
        if ((client = accept(srvSocket, NULL, 0)) != INVALID_SOCKET) {
            while (client != INVALID_SOCKET) {
                char buffer[4096];
                result = recv(client, buffer, 4096, 0);
                if (result > 0) {
                    result = send(client, buffer, result, 0);
                    if (result == SOCKET_ERROR) {
                        cout << "echo: send error: " << WSAGetLastError() << endl;
                        closesocket(client);
                        client = INVALID_SOCKET;
                    }
                }
                else if (result == 0) {
                    cout << "echo: client closed connection." << endl;
                    closesocket(client);
                    client = INVALID_SOCKET;

                }
                else {
                    cout << "echo: recv error: " << WSAGetLastError() << endl;
                    closesocket(client);
                    client = INVALID_SOCKET;

                }
            }
        }
        else {
            cout << "echo: accept error: " << WSAGetLastError() << endl;
            closesocket(srvSocket);
            srvSocket = INVALID_SOCKET;

        }
    }
    return 0;
}
