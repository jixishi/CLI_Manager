#include "TrayIcon.h"
#include "Units.h"

#ifdef _WIN32
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
#else
TrayIcon::TrayIcon(void* app_delegate, void* icon)
    : m_app_delegate(app_delegate), m_icon(icon), m_visible(false) {
    m_web_url = "http://localhost:8080"; // 默认URL
    CreateMenu();
}
#endif

TrayIcon::~TrayIcon() {
    Hide();
    DestroyMenu();
}

void TrayIcon::Show() {
    if (!m_visible) {
#ifdef _WIN32
        Shell_NotifyIcon(NIM_ADD, &m_nid);
#else
        ShowMacTrayIcon();
#endif
        m_visible = true;
    }
}

void TrayIcon::Hide() {
    if (m_visible) {
#ifdef _WIN32
        Shell_NotifyIcon(NIM_DELETE, &m_nid);
#else
        HideMacTrayIcon();
#endif
        m_visible = false;
    }
}

#ifdef _WIN32
void TrayIcon::UpdateWebUrl(const std::wstring &url) {
    m_web_url = url;
    // 重新创建菜单以更新Web URL显示
    DestroyMenu();
    CreateMenu();
}
#else
void TrayIcon::UpdateWebUrl(const std::string &url)
{
    m_web_url = url;
    // 重新创建菜单以更新Web URL显示
    DestroyMenu();
    CreateMenu();
}
#endif

void TrayIcon::SetShowWindowCallback(const ShowWindowCallback &callback) {
    m_show_window_callback = callback;
}

void TrayIcon::SetExitCallback(const ExitCallback &callback) {
    m_exit_callback = callback;
}

#ifdef _WIN32
void TrayIcon::ShowNotification(const std::wstring &title, const std::wstring &message, NotifyAction notify) const {
    NOTIFYICONDATA nid = m_nid;
    nid.uFlags |= NIF_INFO;
    wcsncpy_s(nid.szInfoTitle, title.c_str(), _TRUNCATE);
    wcsncpy_s(nid.szInfo, message.c_str(), _TRUNCATE);
    nid.dwInfoFlags = static_cast<DWORD>(notify); // 信息图标，可选 NIIF_WARNING, NIIF_ERROR
    Shell_NotifyIcon(NIM_MODIFY, &nid);
}
#elif __APPLE__
void TrayIcon::ShowNotification(const std::string &title, const std::string &message)
{
    // 通过 AppleScript 或 Objective-C 桥接
    std::string script = "display notification \"" + message + "\" with title \"" + title + "\"";
    std::string cmd = "osascript -e '" + script + "'";
    system(cmd.c_str());
}
#else
void TrayIcon::ShowNotification(const std::string &title, const std::string &message)
{
    // 使用 notify-send 命令
    std::string cmd = "notify-send \"" + title + "\" \"" + message + "\"";
    system(cmd.c_str());
}
#endif

void TrayIcon::CreateMenu() {
#ifdef _WIN32
    if (m_menu) {
        DestroyMenu();
    }

    m_menu = CreatePopupMenu();
    AppendMenu(m_menu, MF_STRING, 1001, L"显示主窗口");
    AppendMenu(m_menu, MF_SEPARATOR, 0, nullptr);
    std::wstring statusText = L"状态:" + m_status;
    AppendMenu(m_menu, MF_INSERT, 0, statusText.c_str());
    std::wstring pidText = L"PID:" + m_pid;
    AppendMenu(m_menu, MF_INSERT, 0, pidText.c_str());
    AppendMenu(m_menu, MF_SEPARATOR, 0, nullptr);
    // 添加Web地址菜单项（如果有设置）
    if (!m_web_url.empty() && m_web_url != L"") {
        std::wstring webText = L"打开Web页面: " + m_web_url;
        AppendMenu(m_menu, MF_STRING, 1002, webText.c_str());
        AppendMenu(m_menu, MF_SEPARATOR, 0, NULL);
    }

    AppendMenu(m_menu, MF_STRING, 1003, L"退出");
#else
    CreateMacMenu();
#endif
}

void TrayIcon::DestroyMenu() {
#ifdef _WIN32
    if (m_menu) {
        ::DestroyMenu(m_menu);
        m_menu = nullptr;
    }
#else
    DestroyMacMenu();
#endif
}

#ifdef _WIN32
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

void TrayIcon::UpdateStatus(const std::wstring &status, const std::wstring &pid) {
    m_status = status;
    m_pid = pid;
    // 重新创建菜单以更新Status显示
    DestroyMenu();
    CreateMenu();
}

#else
// macOS 特定实现
void TrayIcon::ShowMacTrayIcon()
{
    // 通过 Objective-C 接口显示托盘图标
    ShowMacTrayIconImpl(m_app_delegate, m_icon);
}

void TrayIcon::HideMacTrayIcon()
{
    // 通过 Objective-C 接口隐藏托盘图标
    HideMacTrayIconImpl(m_app_delegate);
}

void TrayIcon::CreateMacMenu()
{
    // 通过 Objective-C 接口创建菜单
    CreateMacMenuImpl(m_app_delegate, m_web_url.c_str());
}

void TrayIcon::DestroyMacMenu()
{
    // 通过 Objective-C 接口销毁菜单
    DestroyMacMenuImpl(m_app_delegate);
}

void TrayIcon::OnMacMenuAction(int action)
{
    switch (action)
    {
    case 1001: // 显示主窗口
        if (m_show_window_callback)
        {
            m_show_window_callback();
        }
        break;
    case 1002: // 打开Web页面
        if (!m_web_url.empty())
        {
            OpenWebPageMac(m_web_url.c_str());
        }
        break;
    case 1003: // 退出
        if (m_exit_callback)
        {
            m_exit_callback();
        }
        break;
    }
}

// C 接口函数，供 Objective-C 调用
extern "C" void TrayIconMenuCallback(void *tray_instance, int action)
{
    if (tray_instance)
    {
        static_cast<TrayIcon *>(tray_instance)->OnMacMenuAction(action);
    }
}

// 外部声明的 Objective-C 接口函数
extern "C"
{
    void ShowMacTrayIconImpl(void *app_delegate, void *icon);
    void HideMacTrayIconImpl(void *app_delegate);
    void CreateMacMenuImpl(void *app_delegate, const char *web_url);
    void DestroyMacMenuImpl(void *app_delegate);
    void OpenWebPageMac(const char *url);
}
#endif
