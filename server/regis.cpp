//#define _CRT_SECURE_NO_WARNINGS
//#include "server.h"
//#include <iostream>
//#include <windows.h>
//#include <thread>
//#pragma comment(lib, "ws2_32.lib")
//
//ChatServer* g_server = nullptr;
//
//void AddAutorun();
//void RemoveAutorun();
//
//int wmain(int argc, wchar_t* argv[]) {
//    if (argc > 1) {
//        if (wcscmp(argv[1], L"install") == 0) {
//            AddAutorun();
//            std::wcout << L"Autorun installed!" << std::endl;
//        }
//        else if (wcscmp(argv[1], L"remove") == 0) {
//            RemoveAutorun();
//            std::wcout << L"Autorun removed!" << std::endl;
//            return 0;
//        }
//    }
//
//    ShowWindow(GetConsoleWindow(), SW_HIDE);
//
//    ChatServer server;
//    g_server = &server;
//
//    if (server.Initialize(12345)) {
//        std::thread serverThread([&server]() {
//            server.Start();
//            });
//
//        while (true) {
//            Sleep(1000);
//        }
//
//        if (serverThread.joinable()) {
//            serverThread.join();
//        }
//    }
//
//    return 0;
//}
//
//void AddAutorun() {
//    wchar_t path[MAX_PATH];
//    GetModuleFileName(NULL, path, MAX_PATH);
//
//    HKEY hKey;
//    if (RegOpenKeyEx(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
//        0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
//        RegSetValueEx(hKey, L"ChatServer", 0, REG_SZ, (BYTE*)path, (wcslen(path) + 1) * sizeof(wchar_t));
//        RegCloseKey(hKey);
//    }
//}
//
//void RemoveAutorun() {
//    HKEY hKey;
//    if (RegOpenKeyEx(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
//        0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
//        RegDeleteValue(hKey, L"ChatServer");
//        RegCloseKey(hKey);
//    }
//}