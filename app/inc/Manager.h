#pragma once

#include "imgui.h"
#include "AppState.h"
#include "TrayIcon.h"

#ifdef USE_WIN32_BACKEND
#include <d3d11.h>
#include <windows.h>
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#else
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <windows.h>
#elif __APPLE__
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#else
#include <GLFW/glfw3.h>
#endif
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#endif

#include <memory>

class Manager {
public:
    Manager();
    ~Manager();

    bool Initialize();
    void Run();
    void Shutdown();

    void OnTrayShowWindow();
    void OnTrayExit();

    AppState m_app_state;


private:
    // UI渲染
    void RenderUI();
    void RenderMenuBar();
    void RenderMainContent();
    void RenderSettingsMenu();
    void RenderStopCommandSettings();
    void RenderEnvironmentVariablesSettings();
    void RenderOutputEncodingSettings();

    // 事件处理
    void HandleMessages();
    bool ShouldExit() const;
    void ShowMainWindow();
    void HideMainWindow();

    // 平台相关初始化
#ifdef USE_WIN32_BACKEND
    bool InitializeWin32();
    bool InitializeDirectX11();
    void CleanupWin32();
    void CleanupDirectX11();
    static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HWND m_hwnd = nullptr;
    WNDCLASSEX m_wc = {};
    ID3D11Device* m_pd3dDevice = nullptr;
    ID3D11DeviceContext* m_pd3dDeviceContext = nullptr;
    IDXGISwapChain* m_pSwapChain = nullptr;
    ID3D11RenderTargetView* m_mainRenderTargetView = nullptr;
#else
    bool InitializeGLFW();
    void CleanupGLFW();
    static void GlfwErrorCallback(int error, const char* description);

    GLFWwindow* m_window = nullptr;
    const char* m_glsl_version = nullptr;
#endif

    // 托盘相关
    bool InitializeTray();
    void CleanupTray();
#ifdef _WIN32
    static HWND CreateHiddenWindow();
    HWND m_tray_hwnd = nullptr;
#endif

    std::unique_ptr<TrayIcon> m_tray;

    // 控制标志
    bool m_should_exit = false;
    bool m_initialized = false;

    // DPI缩放因子
    float m_dpi_scale = 1.0f;

    // 环境变量UI状态
    char env_key_input_[256] = {};
    char env_value_input_[512] = {};
    bool show_env_settings_ = false;

    // 编码设置UI状态
    bool show_encoding_settings_ = false;
    // 历史命令UI状态
    bool show_command_history_;
};