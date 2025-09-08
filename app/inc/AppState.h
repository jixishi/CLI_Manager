#ifndef APP_STATE_H
#define APP_STATE_H

#include "CLIProcess.h"
#include <string>
#include <map>
#include <vector>
#include <imgui.h>

class AppState {
public:
    AppState();
    ~AppState() = default;

    void LoadSettings();
    void SaveSettings();
    void ApplySettings();

    // 新增：启动命令历史记录管理
    void AddCommandToHistory(const std::string& command);
    void RemoveCommandFromHistory(int index);
    void ClearCommandHistory();
    const std::vector<std::string>& GetCommandHistory() const { return command_history; }

    bool show_main_window;
    bool auto_start;
    CLIProcess cli_process;
    char command_input[256]{};
    char send_command[256]{};
    bool auto_scroll_logs;
    bool enable_colored_logs;
    int max_log_lines;
    char web_url[256]{};

    // 停止命令相关配置
    char stop_command[256]{};
    int stop_timeout_ms;
    bool use_stop_command;

    // 环境变量相关配置
    std::map<std::string, std::string> environment_variables;
    bool use_custom_environment;

    // 输出编码相关配置
    OutputEncoding output_encoding;

    // 新增：启动命令历史记录
    std::vector<std::string> command_history;
    int max_command_history;

    bool settings_dirty;

private:
    // 环境变量序列化辅助函数
    std::string SerializeEnvironmentVariables() const;
    void DeserializeEnvironmentVariables(const std::string& serialized);

    // 编码序列化辅助函数
    std::string SerializeOutputEncoding() const;
    void DeserializeOutputEncoding(const std::string& serialized);

    // 新增：命令历史记录序列化辅助函数
    std::string SerializeCommandHistory() const;
    void DeserializeCommandHistory(const std::string& serialized);
};

#endif // APP_STATE_H