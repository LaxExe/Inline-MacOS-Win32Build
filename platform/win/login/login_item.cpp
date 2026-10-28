#include "platform/win/login/login_item.h"

#ifdef _WIN32
#include <windows.h>
#endif

bool setLaunchAtLogin(bool enable) {
#ifdef _WIN32
    HKEY hKey;
    const wchar_t* czRun = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    if (RegOpenKeyExW(HKEY_CURRENT_USER, czRun, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        if (enable) {
            wchar_t szPath[MAX_PATH];
            GetModuleFileNameW(NULL, szPath, MAX_PATH);
            RegSetValueExW(hKey, L"TypingAssistant", 0, REG_SZ, (BYTE*)szPath, (wcslen(szPath) + 1) * sizeof(wchar_t));
        } else {
            RegDeleteValueW(hKey, L"TypingAssistant");
        }
        RegCloseKey(hKey);
        return true;
    }
    return false;
#else
    (void)enable;
    return true;
#endif
}

bool isLaunchAtLoginEnabled() {
#ifdef _WIN32
    HKEY hKey;
    const wchar_t* czRun = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    if (RegOpenKeyExW(HKEY_CURRENT_USER, czRun, 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
        DWORD dwType;
        LONG result = RegQueryValueExW(hKey, L"TypingAssistant", NULL, &dwType, NULL, NULL);
        RegCloseKey(hKey);
        return result == ERROR_SUCCESS;
    }
    return false;
#else
    return false;
#endif
}
