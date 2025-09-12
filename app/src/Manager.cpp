#include "Manager.h"
#include <cstdio>
#include <algorithm>
#include <fstream>
#include <sstream>


#include "imgui_internal.h"
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
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
//    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
    io.ConfigViewportsNoAutoMerge = true;
    io.IniFilename = "imgui.ini";

    ImGui::StyleColorsDark();
    ImGuiStyle &style = ImGui::GetStyle();
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
    m_dpi_scale = ImGui_ImplWin32_GetDpiScaleForHwnd(m_hwnd);
    style.ScaleAllSizes(m_dpi_scale);
#else
    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init(m_glsl_version);
    float xscale, yscale;
    glfwGetWindowContentScale(m_window, &xscale, &yscale);
    m_dpi_scale = xscale;
#endif

    // 加载字体
#ifdef _WIN32
    ImFont *font = io.Fonts->AddFontFromFileTTF(
            "C:/Windows/Fonts/msyh.ttc",
            16.0f * m_dpi_scale,
            nullptr,
            io.Fonts->GetGlyphRangesChineseFull()
    );
#elif __APPLE__
    // macOS 中文字体路径
    ImFont* font = io.Fonts->AddFontFromFileTTF(
        "/System/Library/Fonts/PingFang.ttc",
        16.0f*m_dpi_scale,
        nullptr,
        io.Fonts->GetGlyphRangesChineseFull()
    );
    // 备用字体
    if (!font) {
        font = io.Fonts->AddFontFromFileTTF(
            "/System/Library/Fonts/STHeiti Light.ttc",
            16.0f*m_dpi_scale,
            nullptr,
            io.Fonts->GetGlyphRangesChineseFull()
        );
    }
#else
    // Linux 字体路径
    ImFont* font = io.Fonts->AddFontFromFileTTF(
        "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
        14.0f,
        nullptr,
        io.Fonts->GetGlyphRangesChineseFull()
    );
#endif

    if (!font) {
        // 如果没有找到中文字体，使用默认字体
        font = io.Fonts->AddFontDefault();
    }

    // 初始化托盘
    if (!InitializeTray()) return false;

    // 初始化应用状态
    m_app_state.LoadSettings();
    m_app_state.auto_start = IsAutoStartEnabled();
    m_app_state.ApplySettings();
    m_app_state.SaveSettings();

#ifdef _WIN32
    m_tray->UpdateWebUrl(StringToWide(m_app_state.web_url));

#else
    m_tray->UpdateWebUrl(m_app_state.web_url);
#endif

    // 如果开启了开机自启动且有启动命令，则自动启动子进程
    if (m_app_state.auto_start && strlen(m_app_state.command_input) > 0) {
        m_app_state.cli_process.Start(m_app_state.command_input);
        m_app_state.show_main_window = false;
        HideMainWindow();
    }
    m_tray->UpdateStatus(m_app_state.cli_process.IsRunning() ? L"运行中" : L"已停止", m_app_state.cli_process.GetPid());
    m_initialized = true;
    return true;
}

void Manager::Run() {
    if (!m_initialized) return;

    while (!ShouldExit()) {
        HandleMessages();

        if (m_should_exit) break;
        UpdateDPIScale();
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

    if (m_fullscreen) {
        const ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                        ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    } else {
        dockspace_flags &= ~ImGuiDockNodeFlags_PassthruCentralNode;
    }
    if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
        window_flags |= ImGuiWindowFlags_NoBackground;
    if (!m_padding)
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    if (!m_padding)
        ImGui::PopStyleVar();
    if (m_fullscreen)
        ImGui::PopStyleVar(2);    // Submit the DockSpace
    ImGui::Begin("CLI程序管理工具", &m_app_state.show_main_window,
                 window_flags);

    RenderMenuBar();

    // 创建主要的Docking空间
    ImGuiID m_dockspace_id = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(m_dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags | ImGuiDockNodeFlags_NoTabBar);

    // 设置默认布局(仅在第一次运行时)
    SetupDefaultDockingLayout(m_dockspace_id);

    ImGuiWindowClass window_class;
    window_class.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton;
    ImGui::SetNextWindowClass(&window_class);

    RenderMainContent();

    ImGui::End();
}

void Manager::SetupDefaultDockingLayout(ImGuiID dockspace_id) {
    static bool first_time = true;

    // 检查是否需要重置布局或应用预设布局
    if (first_time || m_reset_layout || m_apply_preset_layout) {
        if (m_apply_preset_layout) {
            // 应用预设布局
            ApplyPresetLayout(m_pending_preset);
            m_apply_preset_layout = false;
            return; // 预设布局已经处理完毕，直接返回
        }

        first_time = false;
        m_reset_layout = false;

        // 清除现有布局
        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

        // 创建分割布局
        ImGuiID dock_left, dock_right, dock_bottom_right;

        // 左右分割 (左侧30%，右侧70%)
        ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.3f, &dock_left, &dock_right);

        // 右侧上下分割 (上侧30%，下侧70%)
        ImGui::DockBuilderSplitNode(dock_right, ImGuiDir_Up, 0.3f, &dock_right, &dock_bottom_right);

        // 将窗口停靠到指定位置
        ImGui::DockBuilderDockWindow("控制面板", dock_left);
        ImGui::DockBuilderDockWindow("命令发送", dock_right);
        ImGui::DockBuilderDockWindow("程序日志", dock_bottom_right);

        // 完成布局构建
        ImGui::DockBuilderFinish(dockspace_id);
    }
}

void Manager::RenderMenuBar() {
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("设置")) {
            RenderSettingsMenu();
            ImGui::EndMenu();
        }

        // 添加布局菜单
        if (ImGui::BeginMenu("布局")) {
            RenderLayoutMenu();
            ImGui::EndMenu();
        }

        // 添加主题菜单
        if (ImGui::BeginMenu("主题")) {
            if (ImGui::MenuItem("暗黑(Dark)")) { ImGui::StyleColorsDark(); }
            if (ImGui::MenuItem("明亮(Light)")) { ImGui::StyleColorsLight(); }
            if (ImGui::MenuItem("经典(Classic)")) { ImGui::StyleColorsClassic(); }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("选项(Options)")) {
            ImGui::MenuItem("全屏(Fullscreen)", nullptr, &m_fullscreen);
            ImGui::MenuItem("填充(Padding)", nullptr, &m_padding);
            ImGui::Separator();
            if (ImGui::MenuItem("标志：不分割(Flag: NoSplit)", "", (dockspace_flags & ImGuiDockNodeFlags_NoSplit) !=
                                                                  0)) { dockspace_flags ^= ImGuiDockNodeFlags_NoSplit; }
            if (ImGui::MenuItem("标志：不调整大小(Flag: NoResize)", "",
                                (dockspace_flags & ImGuiDockNodeFlags_NoResize) !=
                                0)) { dockspace_flags ^= ImGuiDockNodeFlags_NoResize; }
            if (ImGui::MenuItem("标志：不停靠在中心节点(Flag: NoDockingInCentralNode)", "",
                                (dockspace_flags & ImGuiDockNodeFlags_NoDockingInCentralNode) !=
                                0)) { dockspace_flags ^= ImGuiDockNodeFlags_NoDockingInCentralNode; }
            if (ImGui::MenuItem("标志：自动隐藏选项卡栏(Flag: AutoHideTabBar)", "",
                                (dockspace_flags & ImGuiDockNodeFlags_AutoHideTabBar) !=
                                0)) { dockspace_flags = ImGuiDockNodeFlags_AutoHideTabBar; }
            if (ImGui::MenuItem("标志：中心节点筛选器(Flag: PassthruCentralNode)", "",
                                (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode) != 0,
                                m_fullscreen)) { dockspace_flags ^= ImGuiDockNodeFlags_PassthruCentralNode; }
            ImGui::Separator();
            //不关闭菜单
            if (ImGui::MenuItem("关闭(Close)", nullptr, !m_app_state.show_main_window)) {
                HideMainWindow();
            }

            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
}

void Manager::RenderLayoutMenu() {
    if (ImGui::MenuItem("重置为默认布局")) {
        m_reset_layout = true;
    }

    ImGui::Separator();

    if (ImGui::MenuItem("保存当前布局")) {
        SaveCurrentLayout();
    }

    if (ImGui::MenuItem("加载保存的布局")) {
        LoadSavedLayout();
    }

    ImGui::Separator();

    // 预设布局选项
    if (ImGui::BeginMenu("预设布局")) {
        if (ImGui::MenuItem("经典布局 (左控制右日志)")) {
            m_apply_preset_layout = true;
            m_pending_preset = LayoutPreset::Classic;
        }

        if (ImGui::MenuItem("开发布局 (上控制下日志)")) {
            m_apply_preset_layout = true;
            m_pending_preset = LayoutPreset::Development;
        }

        if (ImGui::MenuItem("监控布局 (日志为主)")) {
            m_apply_preset_layout = true;
            m_pending_preset = LayoutPreset::Monitoring;
        }

        ImGui::EndMenu();
    }
}

void Manager::ApplyPresetLayout(LayoutPreset preset) {
    ImGuiID m_dockspace_id = ImGui::GetID("MainDockSpace");
    // 清除现有布局
    ImGui::DockBuilderRemoveNode(m_dockspace_id);
    ImGui::DockBuilderAddNode(m_dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(m_dockspace_id, ImGui::GetMainViewport()->Size);

    ImGuiID dock_1, dock_2, dock_3;

    switch (preset) {
        case LayoutPreset::Classic: {
            // 左右分割 (左30%，右70%)
            ImGui::DockBuilderSplitNode(m_dockspace_id, ImGuiDir_Left, 0.3f, &dock_1, &dock_2);
            // 右侧上下分割 (上30%，下70%)
            ImGui::DockBuilderSplitNode(dock_2, ImGuiDir_Up, 0.3f, &dock_2, &dock_3);

            ImGui::DockBuilderDockWindow("控制面板", dock_1);
            ImGui::DockBuilderDockWindow("命令发送", dock_2);
            ImGui::DockBuilderDockWindow("程序日志", dock_3);
            break;
        }

        case LayoutPreset::Development: {
            // 上下分割 (上40%，下60%)
            ImGui::DockBuilderSplitNode(m_dockspace_id, ImGuiDir_Up, 0.4f, &dock_1, &dock_2);
            // 上侧左右分割 (左60%，右40%)
            ImGui::DockBuilderSplitNode(dock_1, ImGuiDir_Left, 0.6f, &dock_1, &dock_3);

            ImGui::DockBuilderDockWindow("控制面板", dock_1);
            ImGui::DockBuilderDockWindow("命令发送", dock_3);
            ImGui::DockBuilderDockWindow("程序日志", dock_2);
            break;
        }

        case LayoutPreset::Monitoring: {
            // 上下分割 (上20%，下80%)
            ImGui::DockBuilderSplitNode(m_dockspace_id, ImGuiDir_Up, 0.2f, &dock_1, &dock_2);
            // 上侧左右分割 (左70%，右30%)
            ImGui::DockBuilderSplitNode(dock_1, ImGuiDir_Left, 0.7f, &dock_1, &dock_3);

            ImGui::DockBuilderDockWindow("控制面板", dock_1);
            ImGui::DockBuilderDockWindow("命令发送", dock_3);
            ImGui::DockBuilderDockWindow("程序日志", dock_2);
            break;
        }
    }

    // 完成布局构建并强制应用
    ImGui::DockBuilderFinish(m_dockspace_id);
}

void Manager::SaveCurrentLayout() {
    // 保存当前布局到配置文件
    std::string layout_file = "imgui.ini";

    // 获取当前ImGui布局数据
    size_t data_size = 0;
    const char *data = ImGui::SaveIniSettingsToMemory(&data_size);

    if (data && data_size > 0) {
        std::ofstream file(layout_file, std::ios::binary);
        if (file.is_open()) {
            file.write(data, data_size);
            file.close();

            // 显示保存成功提示
            m_show_save_success = true;
            m_save_success_timer = 3.0f; // 3秒后消失
        }
    }
}

void Manager::LoadSavedLayout() {
    std::string layout_file = "imgui.ini";

    std::ifstream file(layout_file, std::ios::binary | std::ios::ate);
    if (file.is_open()) {
        size_t file_size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<char> buffer(file_size + 1);
        file.read(buffer.data(), file_size);
        buffer[file_size] = '\0';
        file.close();

        // 加载布局数据
        ImGui::LoadIniSettingsFromMemory(buffer.data(), file_size);

        // 显示加载成功提示
        m_show_load_success = true;
        m_load_success_timer = 3.0f; // 3秒后消失
    }
}

void Manager::RenderStatusMessages() {
    // 渲染保存成功提示
    if (m_show_save_success) {
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetMainViewport()->Size.x * 0.5f, 50), ImGuiCond_Always,
                                ImVec2(0.5f, 0.0f));
        ImGui::SetNextWindowBgAlpha(0.8f);

        if (ImGui::Begin("SaveSuccess", nullptr,
                         ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                         ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "布局已保存");
        }
        ImGui::End();

        m_save_success_timer -= ImGui::GetIO().DeltaTime;
        if (m_save_success_timer <= 0.0f) {
            m_show_save_success = false;
        }
    }

    // 渲染加载成功提示
    if (m_show_load_success) {
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetMainViewport()->Size.x * 0.5f, 50), ImGuiCond_Always,
                                ImVec2(0.5f, 0.0f));
        ImGui::SetNextWindowBgAlpha(0.8f);

        if (ImGui::Begin("LoadSuccess", nullptr,
                         ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                         ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "布局已加载");
        }
        ImGui::End();

        m_load_success_timer -= ImGui::GetIO().DeltaTime;
        if (m_load_success_timer <= 0.0f) {
            m_show_load_success = false;
        }
    }
}

void Manager::RenderMainContent() {
    // 渲染状态消息
    RenderStatusMessages();

    // 控制面板窗口
    if (ImGui::Begin("控制面板", nullptr, ImGuiWindowFlags_NoCollapse)) {
        float buttonWidth = 40.0f * m_dpi_scale;
        float buttonHeight = 25.0f * m_dpi_scale;
        float inputWidth = ImGui::GetContentRegionAvail().x * 0.8f;
        RenderControlPanel(buttonWidth, buttonHeight, inputWidth);
    }
    ImGui::End();

    // 日志窗口
    if (ImGui::Begin("程序日志", nullptr, ImGuiWindowFlags_NoCollapse)) {
        RenderLogPanel();
    }
    ImGui::End();

    // 命令发送窗口
    if (ImGui::Begin("命令发送", nullptr, ImGuiWindowFlags_NoCollapse)) {
        float buttonWidth = 40.0f * m_dpi_scale;
        float inputWidth = ImGui::GetContentRegionAvail().x * 0.8f;
        RenderCommandPanel(buttonWidth, inputWidth);
    }
    ImGui::End();
}

void Manager::RenderSettingsMenu() {
    if (ImGui::MenuItem("开机自启动", nullptr, m_app_state.auto_start)) {
        m_app_state.auto_start = !m_app_state.auto_start;
        SetAutoStart(m_app_state.auto_start);
        m_app_state.settings_dirty = true;
    }
    if (ImGui::MenuItem("自动工作路径", nullptr, m_app_state.auto_working_dir)) {
        m_app_state.auto_working_dir = !m_app_state.auto_working_dir;
        m_app_state.cli_process.SetAutoWorkingDir(m_app_state.auto_working_dir);
        m_app_state.settings_dirty = true;
    }
    ImGui::Separator();
    ImGui::Text("日志设置");
    if (ImGui::InputInt("最大日志行数", &m_app_state.max_log_lines, 100, 500)) {
        m_app_state.max_log_lines = std::max(100, std::min(m_app_state.max_log_lines, 10000));
        m_app_state.cli_process.SetMaxLogLines(m_app_state.max_log_lines);
        m_app_state.settings_dirty = true;
    }

    // 新增：命令历史记录设置
    ImGui::Separator();
    ImGui::Text("命令历史记录设置");
    if (ImGui::InputInt("最大历史记录数", &m_app_state.max_command_history, 5, 10)) {
        m_app_state.max_command_history = std::max(5, std::min(m_app_state.max_command_history, 100));
        m_app_state.settings_dirty = true;
    }

    if (ImGui::Button("清空命令历史记录")) {
        m_app_state.ClearCommandHistory();
    }

    ImGui::Separator();
    ImGui::Text("Web设置");
    if (ImGui::InputText("Web地址", m_app_state.web_url, IM_ARRAYSIZE(m_app_state.web_url))) {
#ifdef _WIN32
        m_tray->UpdateWebUrl(StringToWide(m_app_state.web_url));
#else
        m_tray->UpdateWebUrl(m_app_state.web_url);
#endif
        m_app_state.settings_dirty = true;
    }

    RenderStopCommandSettings();
    RenderEnvironmentVariablesSettings();
    RenderOutputEncodingSettings();
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
        ImGui::TextWrapped("说明：禁用时将直接强制终止程序。");
        ImGui::EndDisabled();
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

                for (const auto &pair: m_app_state.environment_variables) {
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
                for (const auto &key: keysToRemove) {
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
        ImGui::TextWrapped(
                "说明：启用后，CLI程序将使用这些自定义环境变量。这些变量会与系统环境变量合并，同名变量会被覆盖。");

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
    std::vector<const char *> encodingNames;
    for (const auto &encoding: supportedEncodings) {
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
}

void Manager::RenderControlPanel(float buttonWidth, float buttonHeight, float inputWidth) {
    // 启动命令输入区域
    ImGui::SeparatorText("CLI程序");

    // 命令输入框和历史记录按钮
    ImGui::SetNextItemWidth(inputWidth);
    if (ImGui::InputText("##启动命令", m_app_state.command_input, IM_ARRAYSIZE(m_app_state.command_input))) {
        m_app_state.settings_dirty = true;
    }

    ImGui::SameLine();
    if (ImGui::Button("历史记录", ImVec2(80.0f * m_dpi_scale, 0))) {
        show_command_history_ = !show_command_history_;
    }

    // 显示命令历史记录
    if (show_command_history_) {
        RenderCommandHistory();
    }
    ImGui::SeparatorText("工作路径(留空且开启自动路径为文件父路径、不然为管理器路径)");
    if (ImGui::InputText("##工作路径", m_app_state.working_directory, IM_ARRAYSIZE(m_app_state.working_directory))) {
        m_app_state.cli_process.SetWorkingDirectory(m_app_state.working_directory);
        m_app_state.settings_dirty = true;
    }
    ImGui::Spacing();

    // 控制按钮组
    ImGui::SeparatorText("程序控制");
    if (ImGui::Button("启动", ImVec2(buttonWidth, buttonHeight))) {
        if (strlen(m_app_state.command_input) > 0) {
            m_app_state.cli_process.Start(m_app_state.command_input);
            m_app_state.AddCommandToHistory(m_app_state.command_input);
            if (strlen(m_app_state.working_directory) > 0) {
                m_app_state.cli_process.SetWorkingDirectory(m_app_state.working_directory);
            }else {
                strncpy_s(m_app_state.working_directory,m_app_state.cli_process.GetWorkingDirectory().c_str(),sizeof(m_app_state.working_directory)-1);
            }
            m_tray->UpdateStatus(m_app_state.cli_process.IsRunning() ? L"运行中" : L"已停止",
                                 m_app_state.cli_process.GetPid());
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("停止", ImVec2(buttonWidth, buttonHeight))) {
        m_app_state.cli_process.Stop();
        m_tray->UpdateStatus(m_app_state.cli_process.IsRunning() ? L"运行中" : L"已停止",
                             m_app_state.cli_process.GetPid());
    }
    ImGui::SameLine();
    if (ImGui::Button("重启", ImVec2(buttonWidth, buttonHeight))) {
        if (strlen(m_app_state.command_input) > 0) {
            m_app_state.cli_process.Restart(m_app_state.command_input);
            m_app_state.AddCommandToHistory(m_app_state.command_input);
            m_tray->UpdateStatus(m_app_state.cli_process.IsRunning() ? L"运行中" : L"已停止",
                                 m_app_state.cli_process.GetPid());
        }
    }

    ImGui::Spacing();

    // 状态显示
    ImGui::SeparatorText("运行状态");
    ImVec4 statusColor = m_app_state.cli_process.IsRunning() ?
                         ImVec4(0.0f, 1.0f, 0.0f, 1.0f) :
                         ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    ImGui::TextColored(statusColor, "状态: %s",
                       m_app_state.cli_process.IsRunning() ? "运行中" : "已停止");
}

void Manager::RenderCommandPanel(float buttonWidth, float inputWidth) {
    ImGui::SeparatorText("发送命令到CLI程序");

    // 命令发送
    ImGui::SetNextItemWidth(inputWidth);
    bool sendCommandPressed = ImGui::InputText("##命令输入", m_app_state.send_command,
                                               IM_ARRAYSIZE(m_app_state.send_command),
                                               ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    if (ImGui::Button("发送", ImVec2(buttonWidth, 0)) || sendCommandPressed) {
        if (m_app_state.cli_process.IsRunning() && strlen(m_app_state.send_command) > 0) {
            m_app_state.cli_process.SendCommand(m_app_state.send_command);
            memset(m_app_state.send_command, 0, sizeof(m_app_state.send_command));
        }
    }

    // 显示发送状态
    if (!m_app_state.cli_process.IsRunning()) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "提示: 程序未运行，无法发送命令");
    }
}

void Manager::RenderLogPanel() {
    // 日志控制工具栏
    if (ImGui::BeginTable("LogControls", 3, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 85.0f * m_dpi_scale);
        ImGui::TableSetupColumn("Settings", ImGuiTableColumnFlags_WidthFixed, 100.0f * m_dpi_scale);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();

        // 操作按钮列
        ImGui::TableNextColumn();
        if (ImGui::Button("复制日志", ImVec2(-1, 0))) {
            m_app_state.cli_process.CopyLogsToClipboard();
        }
        if (ImGui::Button("清理日志", ImVec2(-1, 0))) {
            m_app_state.cli_process.ClearLogs();
        }

        // 设置列
        ImGui::TableNextColumn();
        ImGui::Checkbox("自动滚动", &m_app_state.auto_scroll_logs);
        ImGui::Checkbox("彩色显示", &m_app_state.enable_colored_logs);

        // 状态列
        ImGui::TableNextColumn();
        ImGui::Text("行数: %d/%d",
                    static_cast<int>(m_app_state.cli_process.GetLogs().size()),
                    m_app_state.max_log_lines);
    }
    ImGui::EndTable();

    ImGui::Separator();

    // 日志内容区域
    if (ImGui::BeginChild("LogContent", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar)) {
        const auto &logs = m_app_state.cli_process.GetLogs();

        // 使用ImGuiListClipper优化大量日志的渲染性能
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(logs.size()));

        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                const std::string &log = logs[i];

                if (m_app_state.enable_colored_logs) {
                    RenderColoredLogLine(log);
                } else {
                    // 简单的日志级别颜色区分
                    ImVec4 textColor = GetLogLevelColor(log);
                    ImGui::TextColored(textColor, "%s", log.c_str());
                }
            }
        }

        // 自动滚动到底部
        if (m_app_state.auto_scroll_logs && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }
    }
    ImGui::EndChild();
}

void Manager::RenderCommandHistory() {
    const auto &history = m_app_state.GetCommandHistory();

    if (!history.empty()) {
        ImGui::Indent();
        ImGui::Text("选择历史命令 (%d个):", static_cast<int>(history.size()));

        if (ImGui::BeginChild("CommandHistory", ImVec2(0, 240), true)) {
            if (ImGui::BeginTable("HistoryTable", 3, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("操作", ImGuiTableColumnFlags_WidthFixed, 60.0f * m_dpi_scale);
                ImGui::TableSetupColumn("命令", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("删除", ImGuiTableColumnFlags_WidthFixed, 50.0f * m_dpi_scale);
                ImGui::TableHeadersRow();

                for (int i = 0; i < static_cast<int>(history.size()); ++i) {
                    ImGui::TableNextRow();
                    ImGui::PushID(i);

                    // 选择按钮列
                    ImGui::TableNextColumn();
                    if (ImGui::Button("选择", ImVec2(-1, 0))) {
                        strncpy_s(m_app_state.command_input, history[i].c_str(), sizeof(m_app_state.command_input) - 1);
                        show_command_history_ = false;
                        ImGui::PopID();
                        break;
                    }

                    // 命令内容列
                    ImGui::TableNextColumn();
                    std::string displayCommand = history[i];
                    if (displayCommand.length() > 60) {
                        displayCommand = displayCommand.substr(0, 57) + "...";
                    }
                    ImGui::TextUnformatted(displayCommand.c_str());

                    // 鼠标悬停时显示完整命令
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("%s", history[i].c_str());
                    }

                    // 删除按钮列
                    ImGui::TableNextColumn();
                    if (ImGui::SmallButton("删除")) {
                        m_app_state.RemoveCommandFromHistory(i);
                        ImGui::PopID();
                        continue;
                    }

                    ImGui::PopID();
                }
            }
            ImGui::EndTable();
        }
        ImGui::EndChild();

        // 操作按钮
        if (ImGui::Button("清空所有历史记录")) {
            m_app_state.ClearCommandHistory();
        }

        ImGui::Unindent();
    } else {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "暂无启动命令历史");
    }
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
#ifdef _WIN32
    SetProcessWorkingSetSize(GetCurrentProcess(), -1, -1);
#endif
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
    case WM_DPICHANGED:
        if (manager) {
            // DPI变化时强制更新
            manager->m_last_dpi_scale = 0.0f; // 强制触发更新
        }
    break;
    return DefWindowProc(hWnd, msg, wParam, lParam);
}
#endif

bool Manager::InitializeTray() {
#ifdef _WIN32
    m_tray_hwnd = CreateHiddenWindow();
    if (!m_tray_hwnd) {
        return false;
    }

    HICON trayIcon = LoadIcon(NULL, IDI_APPLICATION);
    m_tray = std::make_unique<TrayIcon>(m_tray_hwnd, trayIcon);

    // 设置托盘窗口的用户数据，指向TrayIcon实例
    SetWindowLongPtr(m_tray_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(m_tray.get()));
#else
    // macOS 托盘初始化
    void* app_delegate = GetMacAppDelegate(); // 需要实现这个函数
    void* tray_icon = GetMacTrayIcon(); // 需要实现这个函数

    m_tray = std::make_unique<TrayIcon>(app_delegate, tray_icon);
#endif

    // 设置回调函数
    m_tray->SetShowWindowCallback([this]() {
        OnTrayShowWindow();
    });

    m_tray->SetExitCallback([this]() {
        OnTrayExit();
    });

    m_tray->Show();
    return true;
}

#ifdef _WIN32

HWND Manager::CreateHiddenWindow() {
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = TrayIcon::WindowProc;
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

#endif

void Manager::HandleMessages() {
#ifdef USE_WIN32_BACKEND
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            m_should_exit = true;
        }
        else if (msg.message == WM_CLOSE) {
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
#ifdef _WIN32
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            m_should_exit = true;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
#endif

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

void Manager::ContentScaleCallback(GLFWwindow *window, float xscale, float yscale) {
    if (auto *manager = static_cast<Manager *>(glfwGetWindowUserPointer(window))) {
        // 强制触发DPI更新
        manager->m_last_dpi_scale = 0.0f;
    }
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
    GLFWmonitor *primaryMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode *mode = glfwGetVideoMode(primaryMonitor);
    screenWidth = mode->width;
    screenHeight = mode->height;

    int windowWidth = static_cast<int>(screenWidth * 0.8);
    int windowHeight = static_cast<int>(screenHeight * 0.8);

//    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);//窗口隐藏

    m_window = glfwCreateWindow(windowWidth, windowHeight, "CLI程序管理工具", nullptr, nullptr);
    if (!m_window)
        return false;

    glfwSetWindowPos(m_window,
                     (screenWidth - windowWidth) / 2,
                     (screenHeight - windowHeight) / 2);

    glfwMakeContextCurrent(m_window);
    glfwSetWindowContentScaleCallback(m_window, ContentScaleCallback);
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

void Manager::GlfwErrorCallback(int error, const char *description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

#endif

void Manager::CleanupTray() {
    m_tray.reset();
#ifdef _WIN32
    if (m_tray_hwnd) {
        DestroyWindow(m_tray_hwnd);
        m_tray_hwnd = nullptr;
    }
    UnregisterClass(L"CLIManagerTrayWindow", GetModuleHandle(nullptr));
#endif
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

void Manager::UpdateDPIScale() {
    float new_dpi_scale = 1.0f;

#ifdef USE_WIN32_BACKEND
    if (m_hwnd) {
        UINT dpi = GetDpiForWindow(m_hwnd);
        new_dpi_scale = dpi / 96.0f;
    }
#else
    if (m_window) {
        float xscale, yscale;
        glfwGetWindowContentScale(m_window, &xscale, &yscale);
        new_dpi_scale = xscale;
    }
#endif

    // 检查DPI是否发生变化
    if (abs(new_dpi_scale - m_last_dpi_scale) > 0.01f) {
        m_dpi_scale = new_dpi_scale;
        m_last_dpi_scale = new_dpi_scale;

        // 重新加载字体
        ReloadFonts();

        // 缩放ImGui样式
        ImGuiStyle &style = ImGui::GetStyle();
        style.ScaleAllSizes(m_dpi_scale);
    }
}

void Manager::ReloadFonts() const {
    ImGuiIO &io = ImGui::GetIO();

    // 清除现有字体
    io.Fonts->Clear();

    // 计算缩放后的字体大小
    float font_size = 16.0f * m_dpi_scale;

    // 添加默认字体
    ImFontConfig font_config;
    font_config.SizePixels = font_size;
    font_config.OversampleH = 2;
    font_config.OversampleV = 2;

    // 尝试加载系统字体，失败则使用默认字体
#ifdef _WIN32
    io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/msyh.ttc", font_size, &font_config,
                                 io.Fonts->GetGlyphRangesChineseFull());
#elif __APPLE__
    io.Fonts->AddFontFromFileTTF("/System/Library/Fonts/PingFang.ttc", font_size, &font_config, io.Fonts->GetGlyphRangesChineseFull());
#else
    // Linux - 尝试常见的中文字体路径
    const char* font_paths[] = {
        "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        nullptr
    };

    bool font_loaded = false;
    for (int i = 0; font_paths[i] && !font_loaded; i++) {
        if (io.Fonts->AddFontFromFileTTF(font_paths[i], font_size, &font_config, io.Fonts->GetGlyphRangesChineseFull())) {
            font_loaded = true;
        }
    }

    if (!font_loaded) {
        io.Fonts->AddFontDefault(&font_config);
    }
#endif

    // 如果没有成功加载任何字体，使用默认字体
    if (io.Fonts->Fonts.empty()) {
        io.Fonts->AddFontDefault(&font_config);
    }

    // 构建字体纹理
    io.Fonts->Build();

    // 重新创建字体纹理
#ifdef USE_WIN32_BACKEND
    if (m_pd3dDevice) {
        ID3D11ShaderResourceView* font_srv = nullptr;
        if (io.Fonts->TexID) {
            ((ID3D11ShaderResourceView*)io.Fonts->TexID)->Release();
        }

        D3D11_TEXTURE2D_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.Width = io.Fonts->TexWidth;
        desc.Height = io.Fonts->TexHeight;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = 0;

        ID3D11Texture2D* pTexture = nullptr;
        D3D11_SUBRESOURCE_DATA subResource;
        subResource.pSysMem = io.Fonts->TexPixelsRGBA32;
        subResource.SysMemPitch = desc.Width * 4;
        subResource.SysMemSlicePitch = 0;

        m_pd3dDevice->CreateTexture2D(&desc, &subResource, &pTexture);
        if (pTexture) {
            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
            ZeroMemory(&srvDesc, sizeof(srvDesc));
            srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = desc.MipLevels;
            srvDesc.Texture2D.MostDetailedMip = 0;

            m_pd3dDevice->CreateShaderResourceView(pTexture, &srvDesc, &font_srv);
            pTexture->Release();
        }

        io.Fonts->SetTexID((ImTextureID)font_srv);
    }
#else
    // OpenGL字体纹理重建会由ImGui自动处理
#endif
}

#ifdef __APPLE__
// macOS 特定的辅助函数声明
extern "C" {
    void* GetMacAppDelegate();
    void* GetMacTrayIcon();
}
#endif
