#include "AppState.h"
#include <fstream>
#include <algorithm>
#include <sstream>

AppState::AppState() :
    show_main_window(true),
    auto_start(false),
    auto_scroll_logs(true),
    auto_working_dir(true),
    enable_colored_logs(true),
    max_log_lines(1000),
    stop_timeout_ms(5000),
    use_stop_command(false),
    use_custom_environment(false),
    output_encoding(OutputEncoding::AUTO_DETECT),
    max_command_history(20), // 新增：最大历史记录数量
    settings_dirty(false) {
    strcpy_s(command_input, "cmd.exe");
    strcpy_s(web_url, "http://localhost:8080");
    strcpy_s(stop_command, "exit");
    memset(send_command, 0, sizeof(send_command));
}

// 新增：添加命令到历史记录
void AppState::AddCommandToHistory(const std::string& command) {
    if (command.empty()) return;

    // 移除重复的命令
    auto it = std::find(command_history.begin(), command_history.end(), command);
    if (it != command_history.end()) {
        command_history.erase(it);
    }

    // 添加到开头
    command_history.insert(command_history.begin(), command);

    // 限制历史记录数量
    if (command_history.size() > static_cast<size_t>(max_command_history)) {
        command_history.resize(max_command_history);
    }

    settings_dirty = true;
}

// 新增：从历史记录中移除命令
void AppState::RemoveCommandFromHistory(int index) {
    if (index >= 0 && index < static_cast<int>(command_history.size())) {
        command_history.erase(command_history.begin() + index);
        settings_dirty = true;
    }
}

// 新增：清空命令历史记录
void AppState::ClearCommandHistory() {
    command_history.clear();
    settings_dirty = true;
}

// 新增：序列化命令历史记录
std::string AppState::SerializeCommandHistory() const {
    std::ostringstream oss;
    bool first = true;
    for (const auto& command : command_history) {
        if (!first) {
            oss << "|";
        }
        // 转义分隔符
        std::string escaped_command = command;
        size_t pos = 0;
        while ((pos = escaped_command.find("|", pos)) != std::string::npos) {
            escaped_command.replace(pos, 1, "\\|");
            pos += 2;
        }
        oss << escaped_command;
        first = false;
    }
    return oss.str();
}

// 反序列化命令历史记录
void AppState::DeserializeCommandHistory(const std::string& serialized) {
    command_history.clear();
    if (serialized.empty()) return;

    std::istringstream iss(serialized);
    std::string command;
    std::string current;

    for (size_t i = 0; i < serialized.length(); ++i) {
        if (serialized[i] == '\\' && i + 1 < serialized.length() && serialized[i + 1] == '|') {
            current += '|';
            ++i; // 跳过下一个字符
        } else if (serialized[i] == '|') {
            if (!current.empty()) {
                command_history.push_back(current);
                current.clear();
            }
        } else {
            current += serialized[i];
        }
    }

    if (!current.empty()) {
        command_history.push_back(current);
    }
}

std::string AppState::SerializeEnvironmentVariables() const {
    std::ostringstream oss;
    bool first = true;
    for (const auto& pair : environment_variables) {
        if (!first) {
            oss << "|";
        }
        oss << pair.first << "=" << pair.second;
        first = false;
    }
    return oss.str();
}

void AppState::DeserializeEnvironmentVariables(const std::string& serialized) {
    environment_variables.clear();
    if (serialized.empty()) return;

    std::istringstream iss(serialized);
    std::string pair;

    while (std::getline(iss, pair, '|')) {
        size_t equalPos = pair.find('=');
        if (equalPos != std::string::npos && equalPos > 0) {
            std::string key = pair.substr(0, equalPos);
            std::string value = pair.substr(equalPos + 1);
            environment_variables[key] = value;
        }
    }
}

// 新增：序列化输出编码
std::string AppState::SerializeOutputEncoding() const {
    return std::to_string(static_cast<int>(output_encoding));
}

// 新增：反序列化输出编码
void AppState::DeserializeOutputEncoding(const std::string& serialized) {
    if (serialized.empty()) {
        output_encoding = OutputEncoding::AUTO_DETECT;
        return;
    }

    try {
        int encodingValue = std::stoi(serialized);
        if (encodingValue >= 0 && encodingValue <= static_cast<int>(OutputEncoding::AUTO_DETECT)) {
            output_encoding = static_cast<OutputEncoding>(encodingValue);
        } else {
            output_encoding = OutputEncoding::AUTO_DETECT;
        }
    } catch (const std::exception&) {
        output_encoding = OutputEncoding::AUTO_DETECT;
    }
}

void AppState::LoadSettings() {
    std::ifstream file("climanager_settings.ini");
    if (!file.is_open()) return;

    std::string line;
    std::string section;

    while (std::getline(file, line)) {
        if (!line.empty() && line[line.size() - 1] == '\r') {
            line.erase(line.size() - 1);
        }

        if (line.empty()) continue;

        if (line[0] == '[' && line[line.size() - 1] == ']') {
            section = line.substr(1, line.size() - 2);
        }
        else if (section == "Settings") {
            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                std::string key = line.substr(0, pos);
                std::string value = line.substr(pos + 1);

                if (key == "CommandInput") {
                    strncpy_s(command_input, value.c_str(), sizeof(command_input) - 1);
                }
                else if (key == "WorkingDirectory") {
                    strncpy_s(working_directory, value.c_str(), sizeof(working_directory) - 1);
                }
                else if (key == "MaxLogLines") {
                    max_log_lines = std::stoi(value);
                    max_log_lines = std::max(100, std::min(max_log_lines, 10000));
                }
                else if (key == "AutoScrollLogs") {
                    auto_scroll_logs = (value == "1");
                }
                else if (key == "EnableColoredLogs") {
                    enable_colored_logs = (value == "1");
                }
                else if (key == "AutoStart") {
                    auto_start = (value == "1");
                }
                else if (key == "AutoWorkDirectory") {
                    auto_working_dir = (value == "1");
                }
                else if (key == "WebUrl") {
                    strncpy_s(web_url, value.c_str(), sizeof(web_url) - 1);
                }
                else if (key == "StopCommand") {
                    strncpy_s(stop_command, value.c_str(), sizeof(stop_command) - 1);
                }
                else if (key == "StopTimeoutMs") {
                    stop_timeout_ms = std::stoi(value);
                    stop_timeout_ms = std::max(1000, std::min(stop_timeout_ms, 60000));
                }
                else if (key == "UseStopCommand") {
                    use_stop_command = (value == "1");
                }
                else if (key == "UseCustomEnvironment") {
                    use_custom_environment = (value == "1");
                }
                else if (key == "EnvironmentVariables") {
                    DeserializeEnvironmentVariables(value);
                }
                else if (key == "OutputEncoding") {
                    DeserializeOutputEncoding(value);
                }
                // 新增：命令历史记录配置的加载
                else if (key == "CommandHistory") {
                    DeserializeCommandHistory(value);
                }
                else if (key == "MaxCommandHistory") {
                    max_command_history = std::stoi(value);
                    max_command_history = std::max(5, std::min(max_command_history, 100));
                }
            }
        }
    }
    file.close();
}

void AppState::SaveSettings() {
    std::ofstream file("climanager_settings.ini");
    if (!file.is_open()) return;

    file << "[Settings]\n";
    file << "CommandInput=" << command_input << "\n";
    file << "WorkingDirectory=" << working_directory << "\n";
    file << "MaxLogLines=" << max_log_lines << "\n";
    file << "AutoScrollLogs=" << (auto_scroll_logs ? "1" : "0") << "\n";
    file << "EnableColoredLogs=" << (enable_colored_logs ? "1" : "0") << "\n";
    file << "AutoStart=" << (auto_start ? "1" : "0") << "\n";
    file << "AutoWorkDirectory=" << (auto_working_dir ? "1" : "0") << "\n";
    file << "WebUrl=" << web_url << "\n";

    // 停止命令相关配置的保存
    file << "StopCommand=" << stop_command << "\n";
    file << "StopTimeoutMs=" << stop_timeout_ms << "\n";
    file << "UseStopCommand=" << (use_stop_command ? "1" : "0") << "\n";

    // 环境变量相关配置的保存
    file << "UseCustomEnvironment=" << (use_custom_environment ? "1" : "0") << "\n";
    file << "EnvironmentVariables=" << SerializeEnvironmentVariables() << "\n";

    // 输出编码配置的保存
    file << "OutputEncoding=" << SerializeOutputEncoding() << "\n";

    // 新增：命令历史记录配置的保存
    file << "CommandHistory=" << SerializeCommandHistory() << "\n";
    file << "MaxCommandHistory=" << max_command_history << "\n";

    file.close();

    settings_dirty = false;
}

void AppState::ApplySettings() {
    cli_process.SetMaxLogLines(max_log_lines);

    // 应用停止命令设置
    if (use_stop_command && strlen(stop_command) > 0) {
        cli_process.SetStopCommand(stop_command, stop_timeout_ms);
    } else {
        cli_process.SetStopCommand("", 0);
    }

    // 应用环境变量设置
    if (use_custom_environment) {
        cli_process.SetEnvironmentVariables(environment_variables);
    } else {
        cli_process.SetEnvironmentVariables({});
    }

    if (strlen(working_directory)>0) {
        cli_process.SetWorkingDirectory(working_directory);
    }

    cli_process.SetAutoWorkingDir(auto_working_dir);
    // 应用输出编码设置
    cli_process.SetOutputEncoding(output_encoding);
}
