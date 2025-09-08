#ifndef APP_STATE_H
#define APP_STATE_H

#include "CLIProcess.h"
#include <string>
#include <map>
#include <imgui.h>

class AppState {
public:
    AppState();
    ~AppState() = default;

    void LoadSettings();
    void SaveSettings();
    void ApplySettings();

    bool show_main_window;
    bool auto_start;
    CLIProcess cli_process;
    char command_input[256]{};
    char send_command[256]{};
    bool auto_scroll_logs;
    int max_log_lines;
    char web_url[256]{};

    // 停止命令相关配置
    char stop_command[256]{};
    int stop_timeout_ms;
    bool use_stop_command;

    // 环境变量相关配置
    std::map<std::string, std::string> environment_variables;
    bool use_custom_environment;

    // 新增：输出编码相关配置
    OutputEncoding output_encoding;

    bool settings_dirty;

private:
    // 环境变量序列化辅助函数
    std::string SerializeEnvironmentVariables() const;
    void DeserializeEnvironmentVariables(const std::string& serialized);

    // 新增：编码序列化辅助函数
    std::string SerializeOutputEncoding() const;
    void DeserializeOutputEncoding(const std::string& serialized);
};

#endif // APP_STATE_H