#include "TrayIcon.h"
#include "Units.h"

TrayIcon::TrayIcon(HWND hwnd, HICON icon)
    : m_hwnd(hwnd), m_icon(icon), m_visible(false), m_menu(nullptr) {

    ZeroMemory(&m_nid, sizeof(m_nid));
    m_nid.cbSize = sizeof(m_nid);
    m_nid.hWnd = m_hwnd;
    m_nid.uID = 1;
    m_nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    m_nid.uCallbackMessage = WM_APP + 1;
    m_nid.hIcon = m_icon;
    wcscpy_s(m_nid.szTip, L"CLI程序管理工具");
    m_web_url = L"http://localhost:8080"; // 默认URL

    CreateMenu();
}

TrayIcon::~TrayIcon() {
    Hide();
    DestroyMenu();
}

void TrayIcon::Show() {
    if (!m_visible) {
        Shell_NotifyIcon(NIM_ADD, &m_nid);
        m_visible = true;
    }
}

void TrayIcon::Hide() {
    if (m_visible) {
        Shell_NotifyIcon(NIM_DELETE, &m_nid);
        m_visible = false;
    }
}

void TrayIcon::UpdateWebUrl(const std::wstring& url) {
    m_web_url = url;
    // 重新创建菜单以更新Web URL显示
    DestroyMenu();
    CreateMenu();
}

void TrayIcon::SetShowWindowCallback(const ShowWindowCallback &callback) {
    m_show_window_callback = callback;
}

void TrayIcon::SetExitCallback(const ExitCallback &callback) {
    m_exit_callback = callback;
}

void TrayIcon::CreateMenu() {
    if (m_menu) {
        DestroyMenu();
    }

    m_menu = CreatePopupMenu();
    AppendMenu(m_menu, MF_STRING, 1001, L"显示主窗口");
    AppendMenu(m_menu, MF_SEPARATOR, 0, NULL);

    // 添加Web地址菜单项（如果有设置）
    if (!m_web_url.empty() && m_web_url != L"") {
        std::wstring webText = L"打开Web页面: " + m_web_url;
        AppendMenu(m_menu, MF_STRING, 1002, webText.c_str());
        AppendMenu(m_menu, MF_SEPARATOR, 0, NULL);
    }

    AppendMenu(m_menu, MF_STRING, 1003, L"退出");
}

void TrayIcon::DestroyMenu() {
    if (m_menu) {
        ::DestroyMenu(m_menu);
        m_menu = nullptr;
    }
}

void TrayIcon::ShowContextMenu() const {
    if (!m_menu) return;

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(m_hwnd);
    TrackPopupMenu(m_menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_hwnd, NULL);
}

LRESULT CALLBACK TrayIcon::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* tray = reinterpret_cast<TrayIcon*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_APP + 1: // 托盘图标消息
        switch (LOWORD(lParam)) {
        case WM_LBUTTONDBLCLK:
            if (tray && tray->m_show_window_callback) {
                tray->m_show_window_callback();
            }
            break;
        case WM_RBUTTONUP:
            if (tray) {
                tray->ShowContextMenu();
            }
            break;
        }
        break;

    case WM_COMMAND:
        if (tray) {
            switch (LOWORD(wParam)) {
            case 1001: // 显示主窗口
                if (tray->m_show_window_callback) {
                    tray->m_show_window_callback();
                }
                break;
            case 1002: // 打开Web页面
                if (!tray->m_web_url.empty()) {
                    ShellExecute(NULL, L"open", tray->m_web_url.c_str(), NULL, NULL, SW_SHOWNORMAL);
                }
                break;
            case 1003: // 退出
                if (tray->m_exit_callback) {
                    tray->m_exit_callback();
                }
                break;
            }
        }
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}