#pragma once

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

#include <string>
#include <functional>

class TrayIcon {
public:
    // 回调函数类型定义
    using ShowWindowCallback = std::function<void()>;
    using ExitCallback = std::function<void()>;

#ifdef _WIN32
    TrayIcon(HWND hwnd, HICON icon);
    void UpdateWebUrl(const std::wstring& url);

    void UpdateStatus(const std::wstring &status, const std::wstring &pid);
    // 静态窗口过程
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
#else
    TrayIcon(void* app_delegate, void* icon);
    void UpdateWebUrl(const std::string& url);
    void OnMacMenuAction(int action);
#endif

    ~TrayIcon();

    void Show();
    void Hide();

    // 设置回调函数
    void SetShowWindowCallback(const ShowWindowCallback &callback);
    void SetExitCallback(const ExitCallback &callback);

private:
    void CreateMenu();
    void DestroyMenu();

#ifdef _WIN32
    void ShowContextMenu() const;

    HWND m_hwnd;
    HICON m_icon;
    NOTIFYICONDATA m_nid{};
    std::wstring m_web_url;
    std::wstring m_status;
    std::wstring m_pid;
    HMENU m_menu;
#else
    void ShowMacTrayIcon();
    void HideMacTrayIcon();
    void CreateMacMenu();
    void DestroyMacMenu();

    void* m_app_delegate;
    void* m_icon;
    std::string m_web_url;
#endif

    bool m_visible;

    // 回调函数
    ShowWindowCallback m_show_window_callback;
    ExitCallback m_exit_callback;
};

#ifdef __cplusplus
extern "C" {
#endif

    // C 接口函数声明，供 Objective-C 调用
    void TrayIconMenuCallback(void* tray_instance, int action);

#ifdef __cplusplus
}
#endif
