#define _CRT_SECURE_NO_WARNINGS
#include "server.h"
#include <iostream>
#include <windows.h>
#include <thread>
#include <fstream>
#include <string>
#include <ctime>
#include <sstream>
#include <iomanip>
#pragma comment(lib, "ws2_32.lib")

ChatServer* g_server = nullptr;
bool g_serviceRunning = false;
SERVICE_STATUS g_serviceStatus = { 0 };
SERVICE_STATUS_HANDLE g_statusHandle = NULL;
std::wstring g_logFile = L"ChatServer.log";

void WriteLog(const std::wstring& message);
std::wstring GetCurrentTimestamp();
void InstallService();
void RemoveService();
void WINAPI RunService(DWORD argc, LPTSTR* argv);
void WINAPI ServiceCtrlHandler(DWORD ctrl);
void SetServiceStatus(DWORD currentState, DWORD win32ExitCode, DWORD waitHint);
void MainWorker();

void GlobalServerThreadFunction() {
    if (g_server) {
        WriteLog(L"Starting server thread");
        g_server->Start();
        WriteLog(L"Server thread stopped");
    }
}

std::wstring GetCurrentTimestamp() {
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    std::wstringstream ss;
    ss << std::put_time(&tm, L"%Y-%m-%d %H:%M:%S");
    return ss.str();
}

void WriteLog(const std::wstring& message) {
    std::wofstream logFile(g_logFile, std::ios::app);
    if (logFile.is_open()) {
        logFile << L"[" << GetCurrentTimestamp() << L"] " << message << std::endl;
        logFile.close();
    }
}

int wmain(int argc, wchar_t* argv[]) {
    if (argc > 1) {
        if (wcscmp(argv[1], L"install") == 0) {
            InstallService();
            system("sc start ChatServer");
            std::wcout << L"Service installed and started!" << std::endl;
            return 0;
        }
        if (wcscmp(argv[1], L"remove") == 0) {
            system("sc stop ChatServer");
            RemoveService();
            std::wcout << L"Service removed!" << std::endl;
            return 0;
        }
    }
#ifdef SERVICE__
    static wchar_t serviceName[] = L"ChatServer";
    SERVICE_TABLE_ENTRY table[] = {
        { serviceName, RunService },
        { NULL, NULL }
    };

    if (!StartServiceCtrlDispatcher(table)) {
        std::wcout << L"Unable to start service. Use 'install' parameter to install service." << std::endl;
    }
#else
    MainWorker();
#endif

    return 0;
}

void WINAPI ServiceCtrlHandler(DWORD ctrl) {
    if (ctrl == SERVICE_CONTROL_STOP) {
        SetServiceStatus(SERVICE_STOP_PENDING, NO_ERROR, 3000);
        g_serviceRunning = false;
        if (g_server) {
            g_server->Stop();
        }
        SetServiceStatus(SERVICE_STOPPED, NO_ERROR, 0);
    }
}

void SetServiceStatus(DWORD currentState, DWORD win32ExitCode, DWORD waitHint) {
    g_serviceStatus.dwCurrentState = currentState;
    g_serviceStatus.dwWin32ExitCode = win32ExitCode;
    g_serviceStatus.dwWaitHint = waitHint;
    g_serviceStatus.dwControlsAccepted = (currentState == SERVICE_START_PENDING) ? 0 : SERVICE_ACCEPT_STOP;
    ::SetServiceStatus(g_statusHandle, &g_serviceStatus);
}

void MainWorker()
{
    g_server = new ChatServer();
    if (!g_server->Initialize(12345)) {
        SetServiceStatus(SERVICE_STOPPED, ERROR_SERVICE_SPECIFIC_ERROR, 0);
        delete g_server;
        return;
    }

    g_serviceRunning = true;
    std::thread serverThread(GlobalServerThreadFunction);

    SetServiceStatus(SERVICE_RUNNING, NO_ERROR, 0);

    while (g_serviceRunning) {
        Sleep(1000);
    }

    g_server->Stop();
    if (serverThread.joinable()) {
        serverThread.join();
    }

    delete g_server;
}

void WINAPI RunService(DWORD argc, LPTSTR* argv) {
    g_statusHandle = RegisterServiceCtrlHandler(L"ChatServer", ServiceCtrlHandler);
    if (g_statusHandle == NULL) return;

    g_serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_serviceStatus.dwServiceSpecificExitCode = 0;

    SetServiceStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

    MainWorker();
    
    SetServiceStatus(SERVICE_STOPPED, NO_ERROR, 0);
}

void InstallService() {
    wchar_t path[MAX_PATH];
    GetModuleFileName(NULL, path, MAX_PATH);

    SC_HANDLE scManager = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (scManager == NULL) {
        std::wcout << L"OpenSCManager failed: " << GetLastError() << std::endl;
        return;
    }

    SC_HANDLE service = CreateService(
        scManager, L"ChatServer", L"Chat Server Service",
        SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START,
        SERVICE_ERROR_NORMAL, path, NULL, NULL, NULL, NULL, NULL
    );

    if (service == NULL) {
        DWORD error = GetLastError();
        if (error == ERROR_SERVICE_EXISTS) {
            std::wcout << L"Service already exists." << std::endl;
        }
        else {
            std::wcout << L"CreateService failed: " << error << std::endl;
        }
    }
    else {
        std::wcout << L"Service installed successfully." << std::endl;
        CloseServiceHandle(service);
    }
    CloseServiceHandle(scManager);
}

void RemoveService() {
    SC_HANDLE scManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (scManager == NULL) return;

    SC_HANDLE service = OpenService(scManager, L"ChatServer", DELETE);
    if (service == NULL) {
        CloseServiceHandle(scManager);
        return;
    }

    if (DeleteService(service)) {
        std::wcout << L"Service removed successfully." << std::endl;
    }
    else {
        std::wcout << L"DeleteService failed: " << GetLastError() << std::endl;
    }

    CloseServiceHandle(service);
    CloseServiceHandle(scManager);
}