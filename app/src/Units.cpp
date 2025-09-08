#include "Units.h"
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <windows.h>
std::wstring StringToWide(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    std::wstring wstr(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size);
    return wstr;
}

std::string WideToString(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string str(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], size, nullptr, nullptr);
    return str;
}

void SetAutoStart(bool enable) {
    HKEY hKey;
    LPCWSTR path = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

    if (RegOpenKeyEx(HKEY_CURRENT_USER, path, 0, KEY_WRITE, &hKey) != ERROR_SUCCESS)
        return;

    WCHAR exePath[MAX_PATH];
    GetModuleFileName(NULL, exePath, MAX_PATH);

    if (enable) {
        RegSetValueEx(hKey, L"CLIManager", 0, REG_SZ,
            (BYTE*)exePath, (wcslen(exePath) + 1) * sizeof(WCHAR));
    }
    else {
        RegDeleteValue(hKey, L"CLIManager");
    }
    RegCloseKey(hKey);
}

bool IsAutoStartEnabled() {
    HKEY hKey;
    LPCWSTR path = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

    if (RegOpenKeyEx(HKEY_CURRENT_USER, path, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return false;

    DWORD type, size = 0;
    bool exists = (RegQueryValueEx(hKey, L"CLIManager", NULL, &type, NULL, &size) == ERROR_SUCCESS);

    RegCloseKey(hKey);
    return exists;
}