#pragma once
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <functional>

class TrayIcon {
public:
    // 回调函数类型定义
    using ShowWindowCallback = std::function<void()>;
    using ExitCallback = std::function<void()>;

    TrayIcon(HWND hwnd, HICON icon);
    ~TrayIcon();

    void Show();
    void Hide();
    void UpdateWebUrl(const std::wstring& url);

    // 设置回调函数
    void SetShowWindowCallback(const ShowWindowCallback &callback);
    void SetExitCallback(const ExitCallback &callback);

    // 静态窗口过程
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    void CreateMenu();
    void DestroyMenu();
    void ShowContextMenu() const;

    HWND m_hwnd;
    HICON m_icon;
    NOTIFYICONDATA m_nid{};
    std::wstring m_web_url;
    bool m_visible;
    HMENU m_menu;

    // 回调函数
    ShowWindowCallback m_show_window_callback;
    ExitCallback m_exit_callback;
};