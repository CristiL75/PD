#define UNICODE
#define _UNICODE

#include <windows.h>

#include <string>

namespace {

constexpr wchar_t kServiceName[] = L"Tema3HelloWorldService";

SERVICE_STATUS g_serviceStatus = {};
SERVICE_STATUS_HANDLE g_statusHandle = nullptr;
HANDLE g_stopEvent = nullptr;

void UpdateServiceStatus(DWORD currentState, DWORD win32ExitCode, DWORD waitHint) {
    static DWORD checkPoint = 1;

    g_serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_serviceStatus.dwCurrentState = currentState;
    g_serviceStatus.dwWin32ExitCode = win32ExitCode;
    g_serviceStatus.dwWaitHint = waitHint;

    if (currentState == SERVICE_START_PENDING) {
        g_serviceStatus.dwControlsAccepted = 0;
    } else {
        g_serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    }

    if (currentState == SERVICE_RUNNING || currentState == SERVICE_STOPPED) {
        g_serviceStatus.dwCheckPoint = 0;
    } else {
        g_serviceStatus.dwCheckPoint = checkPoint++;
    }

    SetServiceStatus(g_statusHandle, &g_serviceStatus);
}

void LogHelloWorldToFile() {
    // Scrie in C:\ProgramData unde LocalSystem (identitatea serviciului) are drepturi
    std::wstring logFile = L"C:\\ProgramData\\Tema3_HelloWorld.log";

    // Deschide fisierul in append mode
    HANDLE hFile = CreateFileW(
        logFile.c_str(),
        FILE_APPEND_DATA,
        FILE_SHARE_READ,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        return;
    }

    // Scrie timestamp si mesajul
    SYSTEMTIME st;
    GetLocalTime(&st);

    wchar_t buffer[256];
    swprintf_s(buffer, sizeof(buffer) / sizeof(buffer[0]),
        L"[%04d-%02d-%02d %02d:%02d:%02d] Hello World! Service initialized successfully.\r\n",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    DWORD bytesWritten = 0;
    WriteFile(hFile, buffer, static_cast<DWORD>(wcslen(buffer) * sizeof(wchar_t)), &bytesWritten, nullptr);

    CloseHandle(hFile);
}

void WINAPI ServiceCtrlHandler(DWORD controlCode) {
    switch (controlCode) {
    case SERVICE_CONTROL_STOP:
        if (g_serviceStatus.dwCurrentState != SERVICE_RUNNING) {
            break;
        }

        UpdateServiceStatus(SERVICE_STOP_PENDING, NO_ERROR, 3000);
        SetEvent(g_stopEvent);
        break;

    default:
        break;
    }
}

void WINAPI ServiceMain(DWORD /*argc*/, LPWSTR* /*argv*/) {
    g_statusHandle = RegisterServiceCtrlHandlerW(kServiceName, ServiceCtrlHandler);
    if (g_statusHandle == nullptr) {
        return;
    }

    UpdateServiceStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

    g_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (g_stopEvent == nullptr) {
        UpdateServiceStatus(SERVICE_STOPPED, GetLastError(), 0);
        return;
    }

    LogHelloWorldToFile();
    UpdateServiceStatus(SERVICE_RUNNING, NO_ERROR, 0);

    WaitForSingleObject(g_stopEvent, INFINITE);

    if (g_stopEvent != nullptr) {
        CloseHandle(g_stopEvent);
        g_stopEvent = nullptr;
    }

    UpdateServiceStatus(SERVICE_STOPPED, NO_ERROR, 0);
}

}  // namespace

int wmain() {
    SERVICE_TABLE_ENTRYW serviceTable[] = {
        {const_cast<LPWSTR>(kServiceName), ServiceMain},
        {nullptr, nullptr}
    };

    if (!StartServiceCtrlDispatcherW(serviceTable)) {
        return static_cast<int>(GetLastError());
    }

    return 0;
}
