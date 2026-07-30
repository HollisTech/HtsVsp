// FileTransfer.cpp : 
//  Open a listening socket on a specified port and wait for a connection. 
// 
//  Once connected, wait for a directory change notification.
//  On directory change notification, send each file in the directory to the connected client and then delete that file.  
//  Repeat until the connection is closed by the client.  Then wait for a new connection and repeat the process.
// 
//  log any data received from the client to a log file.  
//  The log file name is specified on the command line.  
//  If no log file name is specified, then log to the console.
// 
//

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <htsvsp.h>
#include <iostream>
#include <fstream>
#include <filesystem>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#include "cxxopts.hpp"

using std::cout;
using std::endl;

SOCKET createListeningSocket(USHORT port, std::ostream& outfile);
bool logSocketData(SOCKET clientSocket, std::ostream& outfile);
void transferLoop(SOCKET listenSocket, const std::string& directory, std::ostream& outfile);
#pragma comment(lib, "Ws2_32.lib") 

int main(int argc, CHAR* argv[])
{
    USHORT port = 0;
    bool verbose = false;
    bool haveLogStream = false;
    std::ofstream logFileStream;
    std::string directory;
    SOCKET listenSocket = INVALID_SOCKET;

    try {
        cxxopts::Options options(argv[0], "file transfer test for htsvsp");
        options.add_options()
            ("h,help", "print usage")   
            ("p,port", "service port, must be greater than zero.", cxxopts::value<USHORT>())
            ("l,logfile", "log file name.", cxxopts::value<std::string>())
            ("d,directory", "directory to watch for changes.", cxxopts::value<std::string>())
            ("v,verbose", "verbose output.");

        auto optResult = options.parse(argc, argv);
        if (optResult.count("help")) {
            std::cout << options.help() << std::endl;
            return 0;
        }
        if (optResult.count("logfile")) {
            std::string logFile = optResult["logfile"].as<std::string>();
            // the log file needs to be opened (append) as an ofstream and used to log data received from the client
            logFileStream.open(logFile, std::ios::app);
            if (!logFileStream.is_open()) {
                cout << "failed to open log file: " << logFile << endl;
                return 1;
            }
            haveLogStream = true;
            cout << "log file: " << logFile << endl;
        }
        // use log file if specified, otherwise log to console
        std::ostream& outfile = haveLogStream ? logFileStream : std::cout; // default to console output
        if (optResult.count("port")) {
            port = optResult["port"].as<USHORT>();
        }

        if (optResult.count("directory")) {
            directory = optResult["directory"].as<std::string>();
            // the directory needs to be watched for changes and when a change is detected, 
            // for each file in the directory send the file contents as a byte stream to the connected client and then delete the file
            outfile << "directory: " << directory << endl;
        }
        if (optResult.count("verbose")) {
            verbose = true;
        }

        // port is mandatory, if not specified then print usage and exit
        if (port == 0) {
            outfile << "port is mandatory" << endl;
            std::cout << options.help() << std::endl;
            return 1;
        }
        // directory is mandatory, if not specified then print usage and exit
        if (directory.empty()) {
            outfile << "directory is mandatory" << endl;
            std::cout << options.help() << std::endl;
            return 1;
        }
        // create the directory if it does not exist
        std::error_code ec;
        std::filesystem::path dirpath(directory);
        if (!std::filesystem::create_directories(dirpath, ec)) {
            if (ec) {
                outfile << "failed to create directory: " << directory << ", error: " << ec.message() << endl;
                return 1;
            }
        }

        // start the file transfer server
        outfile << "starting file transfer server on port: " << port << endl;
        listenSocket = createListeningSocket(port, outfile);
        if (listenSocket == INVALID_SOCKET) {
            return 1;
        }

        transferLoop(listenSocket, directory, outfile);

    }
    catch (const cxxopts::exceptions::exception& e) {
        cout << "error parsing options: " << e.what() << endl;
        return 1;
    }
    catch (const std::exception& e) {
        cout << "error: " << e.what() << endl;
    }
    if (listenSocket != INVALID_SOCKET) {
        closesocket(listenSocket);
    }
    WSACleanup();
}

SOCKET createListeningSocket(USHORT port, std::ostream& outfile)
{
    UINT16  versionRequested = WINSOCK_VERSION;
    WSADATA wsaData = { 0 };
    UINT32  status = WSAStartup(versionRequested,
        &wsaData);
    if (status != NO_ERROR) {
        outfile << "echo: WSAStartup error " << status << endl;
        return 1;
    }
    SOCKET listenSocket = INVALID_SOCKET;
    listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        outfile << "socket failed with error: " << WSAGetLastError() << endl;
        return INVALID_SOCKET;
    }
    sockaddr_in service;
    service.sin_family = AF_INET;
    service.sin_addr.s_addr = INADDR_ANY;
    service.sin_port = htons(port);
    if (bind(listenSocket, (SOCKADDR*)&service, sizeof(service)) == SOCKET_ERROR) {
        outfile << "bind failed with error: " << WSAGetLastError() << endl;
        closesocket(listenSocket);
        return INVALID_SOCKET;
    }
    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        outfile << "listen failed with error: " << WSAGetLastError() << endl;
        closesocket(listenSocket);
        return INVALID_SOCKET;
    }
    return listenSocket;
}

void transferLoop(SOCKET listenSocket, const std::string& directory, std::ostream& outfile)
{
    while (true) {
        SOCKET clientSocket = accept(listenSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            outfile << "accept failed with error: " << WSAGetLastError() << endl;
            continue;
        }
        // Get client address info
        sockaddr_in clientInfo;
        int addrSize = sizeof(clientInfo);
        if (getpeername(clientSocket, (sockaddr*)&clientInfo, &addrSize) == 0) {
            char clientIP[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &clientInfo.sin_addr, clientIP, sizeof(clientIP));
            int clientPort = ntohs(clientInfo.sin_port);

            outfile << "Client connected from IP: " << clientIP
                << " Port: " << clientPort << "\n";
        }
        else {
            std::cerr << "getpeername failed: " << WSAGetLastError() << "\n";
        }
        // Watch the directory for changes
        HANDLE hDir = CreateFile(
            directory.c_str(),
            FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
            NULL);
        if (hDir == INVALID_HANDLE_VALUE) {
            outfile << "CreateFile failed with error: " << GetLastError() << endl;
            closesocket(clientSocket);
            return;
        }
        char buffer[1024];
        DWORD bytesReturned;
        OVERLAPPED overLapped;
        ZeroMemory(&overLapped, sizeof(overLapped));
        HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        overLapped.hEvent = hEvent;
        if (!ReadDirectoryChangesW(
            hDir,
            buffer,
            sizeof(buffer),
            FALSE,
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
            &bytesReturned,
            &overLapped,
            NULL))
        {
            outfile << "Initial ReadDirectoryChangesW error " << GetLastError() << endl;
            return;
        }
        while (true) {
            if (!logSocketData(clientSocket, outfile)) {
                break;                
            }
            DWORD bytes;
            if (GetOverlappedResultEx(hDir, &overLapped, &bytes, 500, FALSE)) {
                // Process the changes
                // For each file in the directory, send the file contents to the client and then delete the file
                for (const auto& entry : std::filesystem::directory_iterator(directory)) {
                    if (entry.is_regular_file()) {
                        std::ifstream file(entry.path(), std::ios::binary);
                        if (file.is_open()) {
                            char buffer[1024];
                            int sockerr = 0;
                            size_t totalSent = 0;
                            outfile << entry.path().string() << " transfer started" << endl;
                            while ((sockerr == 0) && (file.read(buffer, sizeof(buffer)) || file.gcount() > 0))
                            {
                                size_t bytesToSend = file.gcount();
                                size_t sentBytes = 0;
                                int zeroCount = 0;
                                while (totalSent < bytesToSend)
                                {
                                    int bytes = send(clientSocket, buffer + sentBytes, (int) (bytesToSend - sentBytes), 0); // ignore zero return?
                                    if (bytes == SOCKET_ERROR) {
                                        sockerr = WSAGetLastError();
                                        outfile << "send error: " << sockerr << " total sent: " << sentBytes << endl;
                                        break;
                                    }
                                    sentBytes += bytes;
                                    totalSent += bytes;
                                    if (bytes == 0) {
                                        zeroCount++;
                                        outfile << "send returned zero. zeroCount: " << zeroCount << " error: " << WSAGetLastError() << " bytesToSend : " << bytesToSend << endl;
                                        if (zeroCount > 4)
                                        {
                                            sockerr = WSAELOOP; // whatever
                                            break;
                                        }
                                        Sleep(500);
                                    }
                                    else {
                                        zeroCount = 0;
                                    }
                                }
                                outfile << "transmitted:  " << sentBytes << "bytes" << endl;
                            }
                            file.close();
                            std::filesystem::remove(entry.path());
                            outfile << "transferred " << totalSent << " and deleted: " << entry.path().string() << endl;
                        }
                        else {
                            outfile << "failed to open file: " << entry.path().string() << endl;
                        }
                    }
                    if (!logSocketData(clientSocket, outfile)) {
                        break;
                    }
                }
                ResetEvent(overLapped.hEvent);
                if (!ReadDirectoryChangesW(
                    hDir,
                    buffer,
                    sizeof(buffer),
                    FALSE,
                    FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
                    &bytesReturned,
                    &overLapped,
                    NULL))
                {
                    outfile << "ReadDirectoryChangesW error " << GetLastError() << endl;
                    break;
                }
            }
            if (!logSocketData(clientSocket, outfile)) {
                break;
            }
        }
        CloseHandle(hDir);
        closesocket(clientSocket);
    }
}

int is_data_available_ioctl(SOCKET sock) {
    u_long bytes_available = 0;
    if (ioctlsocket(sock, FIONREAD, &bytes_available) != 0) {
        return -1; // Error
    }
    return (bytes_available > 0) ? 1 : 0;
}


// read any socket data and log it to the specified output stream
bool logSocketData(SOCKET clientSocket, std::ostream& outfile)
{
    char buffer[1024];
    int result;
    int bytesAvailable = is_data_available_ioctl(clientSocket);
    while (bytesAvailable > 0) {
        result = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (result > 0) {
            outfile << "received data: " << std::string(buffer, result) << endl;
        }
        else if (result == 0) {
            outfile << "client disconnected" << endl;
            break;
        }
        else {
            cout << "recv failed with error: " << WSAGetLastError() << endl;
            break;
        }
        bytesAvailable = is_data_available_ioctl(clientSocket);
    }
    if (bytesAvailable < 0) {
        cout << "ioctlsocket failed with error: " << WSAGetLastError() << endl;
        return false;
    }
    return true;
}
