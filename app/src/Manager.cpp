#include "Manager.h"
#include <cstdio>
#include <algorithm>

#include "Units.h"


Manager::Manager() = default;

Manager::~Manager() {
    Shutdown();
}

bool Manager::Initialize() {
    if (m_initialized) return true;

#ifdef USE_WIN32_BACKEND
    if (!InitializeWin32()) return false;
    if (!InitializeDirectX11()) return false;
#else
    if (!InitializeGLFW()) return false;
#endif

    // 初始化ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigViewportsNoAutoMerge = true;
    io.IniFilename = "imgui.ini";

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // 设置样式
    style.WindowPadding = ImVec2(15, 15);
    style.FramePadding = ImVec2(5, 5);
    style.ItemSpacing = ImVec2(10, 8);
    style.ItemInnerSpacing = ImVec2(8, 6);
    style.IndentSpacing = 25.0f;
    style.ScrollbarSize = 15.0f;
    style.GrabMinSize = 10.0f;
#ifdef USE_WIN32_BACKEND
    ImGui_ImplWin32_Init(m_hwnd);
    ImGui_ImplDX11_Init(m_pd3dDevice, m_pd3dDeviceContext);
    // ImGui_ImplWin32_EnableDpiAwareness();
    // m_dpi_scale=ImGui_ImplWin32_GetDpiScaleForHwnd(m_hwnd);
    // style.ScaleAllSizes(m_dpi_scale);
#else
    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init(m_glsl_version);
#endif

    // 加载中文字体
    ImFont* font = io.Fonts->AddFontFromFileTTF(
        "C:/Windows/Fonts/msyh.ttc",
        18.0f,
        nullptr,
        io.Fonts->GetGlyphRangesChineseFull()
    );
    IM_ASSERT(font != nullptr);

    // 初始化托盘
    if (!InitializeTray()) return false;

    // 初始化应用状态
    m_app_state.LoadSettings();
    m_app_state.auto_start = IsAutoStartEnabled();
    m_app_state.ApplySettings();
    m_tray->UpdateWebUrl(StringToWide(m_app_state.web_url));

    // 如果开启了开机自启动且有启动命令，则自动启动子进程
    if (m_app_state.auto_start && strlen(m_app_state.command_input) > 0) {
        m_app_state.cli_process.Start(m_app_state.command_input);
    }

    m_initialized = true;
    return true;
}

void Manager::Run() {
    if (!m_initialized) return;

    while (!ShouldExit()) {
        HandleMessages();

        if (m_should_exit) break;

        if (m_app_state.settings_dirty) {
            m_app_state.SaveSettings();
        }

        if (m_app_state.show_main_window) {
#ifdef USE_WIN32_BACKEND
            // Win32 渲染循环
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
#else
            // GLFW 渲染循环
            if (glfwWindowShouldClose(m_window)) {
                HideMainWindow();
                continue;
            }

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
#endif

            ImGui::NewFrame();
            RenderUI();
            ImGui::Render();

#ifdef USE_WIN32_BACKEND
            float clearColor[4] = {0.1f, 0.1f, 0.1f, 1.0f};
            m_pd3dDeviceContext->OMSetRenderTargets(1, &m_mainRenderTargetView, nullptr);
            m_pd3dDeviceContext->ClearRenderTargetView(m_mainRenderTargetView, clearColor);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            m_pSwapChain->Present(1, 0);
#else
            int display_w, display_h;
            glfwGetFramebufferSize(m_window, &display_w, &display_h);
            glViewport(0, 0, display_w, display_h);
            glClearColor(0.1f, 0.1f, 0.1f, 1.00f);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(m_window);
#endif
        } else {
#ifdef USE_WIN32_BACKEND
            WaitMessage();
#else
            glfwWaitEvents();
#endif
        }
    }

    if (m_app_state.settings_dirty) {
        m_app_state.SaveSettings();
    }
}

void Manager::RenderUI() {
#ifdef USE_WIN32_BACKEND
    RECT rect;
    GetClientRect(m_hwnd, &rect);
    int display_w = rect.right - rect.left;
    int display_h = rect.bottom - rect.top;
#else
    int display_w, display_h;
    glfwGetFramebufferSize(m_window, &display_w, &display_h);
#endif

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(display_w, display_h));

    ImGui::Begin("CLI程序管理工具", &m_app_state.show_main_window,
                 ImGuiWindowFlags_NoTitleBar |
                 ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoCollapse |
                 ImGuiWindowFlags_MenuBar);

    RenderMenuBar();
    RenderMainContent();

    ImGui::End();
}

void Manager::RenderMenuBar() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("设置")) {
            RenderSettingsMenu();
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
}

void Manager::RenderSettingsMenu() {
    if (ImGui::MenuItem("开机自启动", nullptr, m_app_state.auto_start)) {
        m_app_state.auto_start = !m_app_state.auto_start;
        SetAutoStart(m_app_state.auto_start);
        m_app_state.settings_dirty = true;
    }

    ImGui::Separator();
    ImGui::Text("日志设置");
    if (ImGui::InputInt("最大日志行数", &m_app_state.max_log_lines, 100, 500)) {
        m_app_state.max_log_lines = std::max(100, std::min(m_app_state.max_log_lines, 10000));
        m_app_state.cli_process.SetMaxLogLines(m_app_state.max_log_lines);
        m_app_state.settings_dirty = true;
    }

    ImGui::Separator();
    ImGui::Text("Web设置");
    if (ImGui::InputText("Web地址", m_app_state.web_url, IM_ARRAYSIZE(m_app_state.web_url))) {
        m_tray->UpdateWebUrl(StringToWide(m_app_state.web_url));
        m_app_state.settings_dirty = true;
    }

    RenderStopCommandSettings();
    RenderEnvironmentVariablesSettings();
    RenderOutputEncodingSettings(); // 新增：渲染编码设置
}


void Manager::RenderStopCommandSettings() {
    ImGui::Separator();
    ImGui::Text("停止命令设置");

    if (ImGui::Checkbox("启用优雅停止命令", &m_app_state.use_stop_command)) {
        m_app_state.settings_dirty = true;
        m_app_state.ApplySettings();
    }

    if (m_app_state.use_stop_command) {
        if (ImGui::InputText("停止命令", m_app_state.stop_command, IM_ARRAYSIZE(m_app_state.stop_command))) {
            m_app_state.settings_dirty = true;
            m_app_state.ApplySettings();
        }

        if (ImGui::InputInt("超时时间(毫秒)", &m_app_state.stop_timeout_ms, 1000, 5000)) {
            m_app_state.stop_timeout_ms = std::max(1000, std::min(m_app_state.stop_timeout_ms, 60000));
            m_app_state.settings_dirty = true;
            m_app_state.ApplySettings();
        }

        ImGui::TextWrapped("说明：启用后，停止程序时会先发送指定命令，等待程序优雅退出。超时后将强制终止。");
    } else {
        ImGui::BeginDisabled(true);
        ImGui::InputText("停止命令", m_app_state.stop_command, IM_ARRAYSIZE(m_app_state.stop_command));
        ImGui::InputInt("超时时间(毫秒)", &m_app_state.stop_timeout_ms);
        ImGui::EndDisabled();
        ImGui::TextWrapped("说明：禁用时将直接强制终止程序。");
    }
}

void Manager::RenderEnvironmentVariablesSettings() {
    ImGui::Separator();
    ImGui::Text("环境变量设置");

    if (ImGui::Checkbox("使用自定义环境变量", &m_app_state.use_custom_environment)) {
        m_app_state.settings_dirty = true;
        m_app_state.ApplySettings();
    }

    if (m_app_state.use_custom_environment) {
        ImGui::Indent();

        // 添加新环境变量
        ImGui::Text("添加环境变量:");
        ImGui::SetNextItemWidth(200.0f);
        ImGui::InputText("变量名", env_key_input_, IM_ARRAYSIZE(env_key_input_));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(300.0f);
        ImGui::InputText("变量值", env_value_input_, IM_ARRAYSIZE(env_value_input_));
        ImGui::SameLine();

        if (ImGui::Button("添加") && strlen(env_key_input_) > 0) {
            m_app_state.environment_variables[env_key_input_] = env_value_input_;
            m_app_state.cli_process.AddEnvironmentVariable(env_key_input_, env_value_input_);
            memset(env_key_input_, 0, sizeof(env_key_input_));
            memset(env_value_input_, 0, sizeof(env_value_input_));
            m_app_state.settings_dirty = true;
        }

        ImGui::Spacing();

        // 显示当前环境变量列表
        if (!m_app_state.environment_variables.empty()) {
            ImGui::Text("当前环境变量 (%d个):", static_cast<int>(m_app_state.environment_variables.size()));

            if (ImGui::BeginChild("EnvVarsList", ImVec2(0, 150), true)) {
                std::vector<std::string> keysToRemove;

                for (const auto& pair : m_app_state.environment_variables) {
                    ImGui::PushID(pair.first.c_str());

                    // 显示环境变量
                    ImGui::Text("%s = %s", pair.first.c_str(), pair.second.c_str());
                    ImGui::SameLine();

                    // 删除按钮
                    if (ImGui::SmallButton("删除")) {
                        keysToRemove.push_back(pair.first);
                    }

                    ImGui::PopID();
                }

                // 删除标记的环境变量
                for (const auto& key : keysToRemove) {
                    m_app_state.environment_variables.erase(key);
                    m_app_state.cli_process.RemoveEnvironmentVariable(key);
                    m_app_state.settings_dirty = true;
                }
            }
            ImGui::EndChild();

            // 清空所有环境变量按钮
            if (ImGui::Button("清空所有环境变量")) {
                m_app_state.environment_variables.clear();
                m_app_state.cli_process.ClearEnvironmentVariables();
                m_app_state.settings_dirty = true;
            }
        } else {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "暂无自定义环境变量");
        }

        ImGui::Spacing();
        ImGui::TextWrapped("说明：启用后，CLI程序将使用这些自定义环境变量。这些变量会与系统环境变量合并，同名变量会被覆盖。");

        ImGui::Unindent();
    } else {
        ImGui::BeginDisabled(true);
        ImGui::TextWrapped("说明：禁用时将使用系统默认环境变量启动程序。");
        ImGui::EndDisabled();
    }
}

void Manager::RenderOutputEncodingSettings() {
    ImGui::Separator();
    ImGui::Text("输出编码设置");

    // 获取支持的编码列表
    auto supportedEncodings = CLIProcess::GetSupportedEncodings();

    // 当前选择的编码索引
    int currentEncodingIndex = static_cast<int>(m_app_state.output_encoding);

    // 创建编码名称数组用于Combo
    std::vector<const char*> encodingNames;
    for (const auto& encoding : supportedEncodings) {
        encodingNames.push_back(encoding.second.c_str());
    }

    if (ImGui::Combo("输出编码", &currentEncodingIndex, encodingNames.data(), static_cast<int>(encodingNames.size()))) {
        if (currentEncodingIndex >= 0 && currentEncodingIndex < static_cast<int>(supportedEncodings.size())) {
            m_app_state.output_encoding = supportedEncodings[currentEncodingIndex].first;
            m_app_state.cli_process.SetOutputEncoding(m_app_state.output_encoding);
            m_app_state.settings_dirty = true;
        }
    }

    // 显示当前编码信息
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "当前: %s",
                      CLIProcess::GetEncodingName(m_app_state.output_encoding).c_str());

    // 编码说明
    ImGui::Spacing();
    ImGui::TextWrapped("说明：");
    ImGui::BulletText("自动检测：程序会尝试自动识别输出编码并转换为UTF-8显示");
    ImGui::BulletText("UTF-8：适用于现代程序和国际化应用");
    ImGui::BulletText("GBK/GB2312：适用于中文Windows系统的程序");
    ImGui::BulletText("Big5：适用于繁体中文程序");
    ImGui::BulletText("Shift-JIS：适用于日文程序");

    // // 测试按钮
    // if (ImGui::Button("测试编码转换")) {
    //     std::string testText = "测试中编码转换显示：中文，English, 日本語, 한국어";
    //     m_app_state.cli_process.TestOutputEncoding(testText);
    // }
}

void Manager::RenderMainContent() {
    float buttonWidth = 80.0f * m_dpi_scale;
    float buttonHeight = 40.0f * m_dpi_scale;
    float inputWidth = ImGui::GetContentRegionAvail().x * 0.6f;
    
    // 启动命令输入
    ImGui::SetNextItemWidth(inputWidth);
    if (ImGui::InputText("启动命令", m_app_state.command_input, IM_ARRAYSIZE(m_app_state.command_input))) {
        m_app_state.settings_dirty = true;
    }

    // 控制按钮
    ImGui::BeginGroup();
    if (ImGui::Button("启动", ImVec2(buttonWidth, buttonHeight))) {
        m_app_state.cli_process.Start(m_app_state.command_input);
    }
    ImGui::SameLine();
    if (ImGui::Button("停止", ImVec2(buttonWidth, buttonHeight))) {
        m_app_state.cli_process.Stop();
    }
    ImGui::SameLine();
    if (ImGui::Button("重启", ImVec2(buttonWidth, buttonHeight))) {
        m_app_state.cli_process.Restart(m_app_state.command_input);
    }
    ImGui::SameLine();
    if (ImGui::Button("清理日志", ImVec2(100.0f * m_dpi_scale, buttonHeight))) {
        m_app_state.cli_process.ClearLogs();
    }
    ImGui::EndGroup();

    ImGui::Text("状态: %s", m_app_state.cli_process.IsRunning() ? "运行中" : "已停止");

    ImGui::Separator();
    ImGui::Text("发送命令到CLI程序");

    // 命令发送
    ImGui::BeginGroup();
    ImGui::SetNextItemWidth(inputWidth);
    bool sendCommandPressed = ImGui::InputText("##命令输入", m_app_state.send_command, IM_ARRAYSIZE(m_app_state.send_command),
                                              ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    if (ImGui::Button("发送", ImVec2(buttonWidth, 0)) || sendCommandPressed) {
        if (m_app_state.cli_process.IsRunning() && strlen(m_app_state.send_command) > 0) {
            m_app_state.cli_process.SendCommand(m_app_state.send_command);
            memset(m_app_state.send_command, 0, sizeof(m_app_state.send_command));
        }
    }
    ImGui::EndGroup();

    ImGui::Separator();

    // 日志控制
    ImGui::BeginGroup();
    ImGui::Text("程序日志");

    float logControlButtonWidth = 100.0f * m_dpi_scale;
    ImGui::SameLine();
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - (350.0f * m_dpi_scale));
    if (ImGui::Button("复制日志", ImVec2(logControlButtonWidth, 0))) {
        m_app_state.cli_process.CopyLogsToClipboard();
    }

    ImGui::SameLine();
    ImGui::Checkbox("自动滚动", &m_app_state.auto_scroll_logs);

    ImGui::SameLine();
    ImGui::Text("行数: %d/%d",
                static_cast<int>(m_app_state.cli_process.GetLogs().size()),
                m_app_state.max_log_lines);
    ImGui::EndGroup();

    float logHeight = ImGui::GetContentRegionAvail().y - ImGui::GetStyle().ItemSpacing.y;
    ImGui::BeginChild("Logs", ImVec2(0, logHeight), true, ImGuiWindowFlags_HorizontalScrollbar);

    const auto& logs = m_app_state.cli_process.GetLogs();
    for (const auto& log : logs) {
        ImGui::TextUnformatted(log.c_str());
    }

    if (m_app_state.auto_scroll_logs && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
}

void Manager::OnTrayShowWindow() {
    m_app_state.show_main_window = true;
    ShowMainWindow();
#ifdef USE_WIN32_BACKEND
    SetForegroundWindow(m_hwnd);
#else
    glfwRestoreWindow(m_window);
    glfwFocusWindow(m_window);
#endif
}

void Manager::OnTrayExit() {
    m_should_exit = true;
    PostQuitMessage(0);
}

void Manager::ShowMainWindow() {
#ifdef USE_WIN32_BACKEND
    ShowWindow(m_hwnd, SW_RESTORE);
    SetForegroundWindow(m_hwnd);
#else
    glfwShowWindow(m_window);
    glfwRestoreWindow(m_window);
    glfwFocusWindow(m_window);
#endif
    m_app_state.show_main_window = true;
}

void Manager::HideMainWindow() {
#ifdef USE_WIN32_BACKEND
    ShowWindow(m_hwnd, SW_HIDE);
#else
    glfwHideWindow(m_window);
#endif
    m_app_state.show_main_window = false;
}

#ifdef USE_WIN32_BACKEND
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI Manager::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Manager* manager = nullptr;
    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        manager = reinterpret_cast<Manager*>(cs->lpCreateParams);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(manager));
    } else {
        manager = reinterpret_cast<Manager*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    if (manager && ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_CLOSE:
        // 主窗口关闭时隐藏到托盘
        if (manager) {
            manager->HideMainWindow();
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED && manager) {
            // 最小化时隐藏到托盘
            manager->HideMainWindow();
        }
        break;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}
#endif

bool Manager::InitializeTray() {
    m_tray_hwnd = CreateHiddenWindow();
    if (!m_tray_hwnd) {
        return false;
    }

    HICON trayIcon = LoadIcon(NULL, IDI_APPLICATION);
    m_tray = std::make_unique<TrayIcon>(m_tray_hwnd, trayIcon);

    // 设置回调函数
    m_tray->SetShowWindowCallback([this]() {
        OnTrayShowWindow();
    });

    m_tray->SetExitCallback([this]() {
        OnTrayExit();
    });

    m_tray->Show();

    // 设置托盘窗口的用户数据，指向TrayIcon实例
    SetWindowLongPtr(m_tray_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(m_tray.get()));

    return true;
}

HWND Manager::CreateHiddenWindow() {
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = TrayIcon::WindowProc;  // 使用TrayIcon的窗口过程
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"CLIManagerTrayWindow";

    if (!RegisterClassEx(&wc)) {
        return NULL;
    }

    return CreateWindowEx(
        0,
        wc.lpszClassName,
        L"CLI Manager Tray Window",
        0,
        0, 0, 0, 0,
        NULL, NULL,
        wc.hInstance,
        NULL
    );
}

void Manager::HandleMessages() {
#ifdef USE_WIN32_BACKEND
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            m_should_exit = true;
        }
        else if (msg.message == WM_CLOSE) {
            // 主窗口关闭时隐藏到托盘，而不是退出
            if (msg.hwnd == m_hwnd) {
                HideMainWindow();
                continue;
            } else {
                m_should_exit = true;
            }
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
#else
    // GLFW后端的消息处理
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            m_should_exit = true;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    glfwPollEvents();
    if (glfwWindowShouldClose(m_window)) {
        HideMainWindow();
        glfwSetWindowShouldClose(m_window, GLFW_FALSE);
    }
#endif
}

bool Manager::ShouldExit() const {
    return m_should_exit;
}

#ifdef USE_WIN32_BACKEND
bool Manager::InitializeWin32() {
    m_wc = {};
    m_wc.cbSize = sizeof(WNDCLASSEX);
    m_wc.style = CS_HREDRAW | CS_VREDRAW;
    m_wc.lpfnWndProc = WndProc;
    m_wc.hInstance = GetModuleHandle(NULL);
    m_wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    m_wc.lpszClassName = L"CLIManagerWin32Class";

    if (!RegisterClassEx(&m_wc)) {
        return false;
    }

    m_hwnd = CreateWindowEx(
        0,
        m_wc.lpszClassName,
        L"CLI程序管理工具",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1280,
        800,
        NULL,
        NULL,
        m_wc.hInstance,
        this // 将 this 指针传递给窗口创建数据
    );

    if (!m_hwnd) {
        return false;
    }

    ShowWindow(m_hwnd, SW_SHOWDEFAULT);
    UpdateWindow(m_hwnd);

    return true;
}

bool Manager::InitializeDirectX11() {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0; // 自动适配窗口大小
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = m_hwnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0,
    };

    if (D3D11CreateDeviceAndSwapChain(
            NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags,
            featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &m_pSwapChain,
            &m_pd3dDevice, &featureLevel, &m_pd3dDeviceContext) != S_OK) {
        return false;
    }

    ID3D11Texture2D* pBackBuffer;
    if (m_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer)) != S_OK) {
        return false;
    }

    if (m_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &m_mainRenderTargetView) != S_OK) {
        pBackBuffer->Release();
        return false;
    }

    pBackBuffer->Release();

    return true;
}

void Manager::CleanupWin32() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    UnregisterClass(m_wc.lpszClassName, m_wc.hInstance);
}

void Manager::CleanupDirectX11() {
    if (m_mainRenderTargetView) {
        m_mainRenderTargetView->Release();
        m_mainRenderTargetView = nullptr;
    }
    if (m_pSwapChain) {
        m_pSwapChain->Release();
        m_pSwapChain = nullptr;
    }
    if (m_pd3dDeviceContext) {
        m_pd3dDeviceContext->Release();
        m_pd3dDeviceContext = nullptr;
    }
    if (m_pd3dDevice) {
        m_pd3dDevice->Release();
        m_pd3dDevice = nullptr;
    }
}
#else
bool Manager::InitializeGLFW() {
    glfwSetErrorCallback(GlfwErrorCallback);
    if (!glfwInit())
        return false;

#if defined(IMGUI_IMPL_OPENGL_ES2)
    m_glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(IMGUI_IMPL_OPENGL_ES3)
    m_glsl_version = "#version 300 es";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    m_glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    m_glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    int screenWidth, screenHeight;
    GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);
    screenWidth = mode->width;
    screenHeight = mode->height;

    int windowWidth = static_cast<int>(screenWidth * 0.8);
    int windowHeight = static_cast<int>(screenHeight * 0.8);

    m_window = glfwCreateWindow(windowWidth, windowHeight, "CLI程序管理工具", nullptr, nullptr);
    if (!m_window)
        return false;

    glfwSetWindowPos(m_window,
                     (screenWidth - windowWidth) / 2,
                     (screenHeight - windowHeight) / 2);

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);

    return true;
}

void Manager::CleanupGLFW() {
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    glfwTerminate();
}

void Manager::GlfwErrorCallback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}
#endif

void Manager::CleanupTray() {
    m_tray.reset();
    if (m_tray_hwnd) {
        DestroyWindow(m_tray_hwnd);
        m_tray_hwnd = nullptr;
    }
    UnregisterClass(L"CLIManagerTrayWindow", GetModuleHandle(nullptr));
}

void Manager::Shutdown() {
    if (!m_initialized) return;

    if (m_app_state.settings_dirty) {
        m_app_state.SaveSettings();
    }

    CleanupTray();

#ifdef USE_WIN32_BACKEND
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDirectX11();
    CleanupWin32();
#else
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    CleanupGLFW();
#endif

    m_initialized = false;
}