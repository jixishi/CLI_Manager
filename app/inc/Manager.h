#pragma once

// 系统头文件
#include <memory>

// 第三方库头文件
#include "imgui.h"

// 平台相关头文件
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

// 项目头文件
#include "AppState.h"
#include "TrayIcon.h"

class Manager {
public:
    // 构造函数和析构函数
    Manager();

    ~Manager();

    // 核心生命周期管理
    bool Initialize();      // 初始化应用程序
    void Run();            // 运行主循环
    void Shutdown();       // 关闭应用程序

    // 托盘事件回调
    void OnTrayShowWindow(); // 托盘显示窗口事件
    void OnTrayExit();       // 托盘退出事件

    // 公共成员变量
    AppState m_app_state;    // 应用程序状态

private:
    // 枚举类型定义
    enum class LayoutPreset {
        Classic,        // 经典布局
        Development,    // 开发布局
        Monitoring      // 监控布局
    };


    // UI渲染相关方法
    void RenderUI();                              // 渲染主UI
    void RenderMenuBar();                         // 渲染菜单栏
    void RenderMainContent();                     // 渲染主内容区域
    void RenderSettingsMenu();                    // 渲染设置菜单
    void RenderStopCommandSettings();             // 渲染停止命令设置
    void RenderEnvironmentVariablesSettings();    // 渲染环境变量设置
    void RenderOutputEncodingSettings();          // 渲染输出编码设置
    void RenderControlPanel(float buttonWidth, float buttonHeight, float inputWidth);  // 渲染控制面板
    void RenderCommandPanel(float buttonWidth, float inputWidth);                      // 渲染命令面板
    void RenderLogPanel();                        // 渲染日志面板
    void RenderCommandHistory();                  // 渲染命令历史
    void RenderStatusMessages();                  // 渲染状态消息

    // 布局管理相关方法
    void SetupDefaultDockingLayout(ImGuiID dockspace_id);  // 设置默认停靠布局
    void RenderLayoutMenu();                               // 渲染布局菜单
    static void ApplyPresetLayout(LayoutPreset preset);    // 应用预设布局
    void SaveCurrentLayout();                              // 保存当前布局
    void LoadSavedLayout();                                // 加载已保存布局

    // 事件处理相关方法
    void HandleMessages();                                  // 处理消息
    bool ShouldExit() const;                               // 检查是否应该退出
    static void ContentScaleCallback(GLFWwindow *window, float xscale, float yscale);  // 内容缩放回调

    // 窗口管理相关方法
    void ShowMainWindow();                                  // 显示主窗口
    void HideMainWindow();                                  // 隐藏主窗口

    // DPI相关方法
    void UpdateDPIScale();                                  // 更新DPI缩放
    void ReloadFonts() const;                              // 重新加载字体

    // 平台相关初始化方法
#ifdef USE_WIN32_BACKEND
    bool InitializeWin32();                                 // 初始化Win32
    bool InitializeDirectX11();                            // 初始化DirectX11
    void CleanupWin32();                                   // 清理Win32
    void CleanupDirectX11();                               // 清理DirectX11
    static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);  // 窗口过程
#else

    bool InitializeGLFW();                                 // 初始化GLFW
    void CleanupGLFW();                                    // 清理GLFW
    static void GlfwErrorCallback(int error, const char *description);  // GLFW错误回调
#endif

    // 托盘相关方法
    bool InitializeTray();                                 // 初始化托盘
    void CleanupTray();                                    // 清理托盘
#ifdef _WIN32

    static HWND CreateHiddenWindow();                      // 创建隐藏窗口
#endif

    // 平台相关成员变量
#ifdef USE_WIN32_BACKEND
    HWND m_hwnd = nullptr;                                 // 窗口句柄
    WNDCLASSEX m_wc = {};                                  // 窗口类
    ID3D11Device* m_pd3dDevice = nullptr;                 // D3D11设备
    ID3D11DeviceContext* m_pd3dDeviceContext = nullptr;   // D3D11设备上下文
    IDXGISwapChain* m_pSwapChain = nullptr;               // 交换链
    ID3D11RenderTargetView* m_mainRenderTargetView = nullptr;  // 主渲染目标视图
#else
    GLFWwindow *m_window = nullptr;                        // GLFW窗口
    const char *m_glsl_version = nullptr;                 // GLSL版本
#endif

    // 托盘相关成员变量
    std::unique_ptr<TrayIcon> m_tray;                      // 托盘图标
#ifdef _WIN32
    HWND m_tray_hwnd = nullptr;                           // 托盘窗口句柄
#endif

    // 控制标志
    bool m_should_exit = false;                           // 是否应该退出
    bool m_initialized = false;                           // 是否已初始化
    bool m_fullscreen = false;
    bool m_padding = false;
    // DPI缩放相关
    float m_dpi_scale = 1.0f;                            // 当前DPI缩放
    float m_last_dpi_scale = 1.0f;                       // 上次DPI缩放

    // 布局相关成员变量
    ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                    ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar;
    bool m_apply_preset_layout = false;                   // 是否需要应用预设布局
    LayoutPreset m_pending_preset = LayoutPreset::Classic;  // 待应用的预设布局
    bool m_reset_layout = false;                          // 是否重置布局
    bool m_show_save_success = false;                     // 是否显示保存成功消息
    bool m_show_load_success = false;                     // 是否显示加载成功消息
    float m_save_success_timer = 0.0f;                    // 保存成功消息计时器
    float m_load_success_timer = 0.0f;                    // 加载成功消息计时器

    // UI状态相关成员变量
    char env_key_input_[256] = {};                        // 环境变量键输入缓冲区
    char env_value_input_[512] = {};                      // 环境变量值输入缓冲区
    bool show_env_settings_ = false;                      // 是否显示环境变量设置
    bool show_encoding_settings_ = false;                 // 是否显示编码设置
    bool show_command_history_ = false;                   // 是否显示命令历史
};
