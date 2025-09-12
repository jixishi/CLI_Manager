#include "CLIProcess.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>

#ifdef _WIN32
#include "Units.h"
#else
#include <iconv.h>
#include <locale.h>
#include <langinfo.h>
#include <spawn.h>
extern char **environ;
#endif

CLIProcess::CLIProcess() {
#ifdef _WIN32
    ZeroMemory(&pi_, sizeof(pi_));
    hWritePipe_stdin_ = nullptr;
#else
    process_pid_ = -1;
    pipe_stdout_[0] = pipe_stdout_[1] = -1;
    pipe_stdin_[0] = pipe_stdin_[1] = -1;
    process_running_ = false;
#endif
    max_log_lines_ = 1000;
    stop_timeout_ms_ = 5000;
    output_encoding_ = OutputEncoding::AUTO_DETECT;
    use_auto_working_dir_ = true; // 自动工作目录
}

CLIProcess::~CLIProcess() {
    Stop();
    CleanupResources();
}
void CLIProcess::SetAutoWorkingDir(const bool auto_dir) {
    use_auto_working_dir_ = auto_dir;
}
// 设置工作目录
void CLIProcess::SetWorkingDirectory(const std::string& working_dir) {
    std::lock_guard<std::mutex> lock(working_dir_mutex_);
    if (working_dir.empty()) {
        // use_auto_working_dir_ = true;
        working_directory_.clear();
    } else {
        if (DirectoryExists(working_dir)) {
            working_directory_ = GetAbsolutePath(working_dir);
            use_auto_working_dir_ = false;
            // AddLog("工作目录已设置为: " + working_directory_);
        } else {
            AddLog("警告: 指定的工作目录不存在: " + working_dir);
        }
    }
}

// 获取当前工作目录设置
std::string CLIProcess::GetWorkingDirectory() const {
    std::lock_guard<std::mutex> lock(working_dir_mutex_);
    return working_directory_;
}

// 从命令中提取目录路径
std::string CLIProcess::ExtractDirectoryFromCommand(const std::string& command) {
    if (command.empty()) return "";

    std::string trimmed_command = command;

    // 移除前后空格
    size_t start = trimmed_command.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";

    size_t end = trimmed_command.find_last_not_of(" \t\r\n");
    trimmed_command = trimmed_command.substr(start, end - start + 1);

    std::string executable_path;

    // 处理引号包围的路径
    if (trimmed_command[0] == '"') {
        size_t quote_end = trimmed_command.find('"', 1);
        if (quote_end != std::string::npos) {
            executable_path = trimmed_command.substr(1, quote_end - 1);
        }
    } else {
        // 找到第一个空格前的部分作为可执行文件路径
        size_t space_pos = trimmed_command.find(' ');
        if (space_pos != std::string::npos) {
            executable_path = trimmed_command.substr(0, space_pos);
        } else {
            executable_path = trimmed_command;
        }
    }

    if (executable_path.empty()) return "";

    // 使用 std::filesystem 来处理路径
    try {
        std::filesystem::path path(executable_path);

        // 如果是相对路径，转换为绝对路径
        if (path.is_relative()) {
            path = std::filesystem::absolute(path);
        }

        // 检查文件是否存在
        if (std::filesystem::exists(path) && std::filesystem::is_regular_file(path)) {
            return path.parent_path().string();
        }

        // 如果文件不存在，但路径看起来像一个文件路径，返回其父目录
        if (path.has_parent_path()) {
            auto parent = path.parent_path();
            if (std::filesystem::exists(parent) && std::filesystem::is_directory(parent)) {
                return parent.string();
            }
        }
    } catch (const std::exception& e) {
        // 路径解析失败，返回当前工作目录
        return std::filesystem::current_path().string();
    }

    return "";
}

// 新增：获取绝对路径
std::string CLIProcess::GetAbsolutePath(const std::string& path) {
    try {
        return std::filesystem::absolute(path).string();
    } catch (const std::exception&) {
        return path;
    }
}

// 新增：检查目录是否存在
bool CLIProcess::DirectoryExists(const std::string& path) {
    try {
        return std::filesystem::exists(path) && std::filesystem::is_directory(path);
    } catch (const std::exception&) {
        return false;
    }
}

// 设置输出编码
void CLIProcess::SetOutputEncoding(OutputEncoding encoding) {
    std::lock_guard<std::mutex> lock(encoding_mutex_);
    output_encoding_ = encoding;
    AddLog("输出编码已设置为: " + GetEncodingName(encoding));
}

// 获取输出编码
OutputEncoding CLIProcess::GetOutputEncoding() const {
    std::lock_guard<std::mutex> lock(encoding_mutex_);
    return output_encoding_;
}

// 获取编码名称
std::string CLIProcess::GetEncodingName(const OutputEncoding encoding) {
    switch (encoding) {
        case OutputEncoding::AUTO_DETECT: return "自动检测";
        case OutputEncoding::UTF8: return "UTF-8";
#ifdef _WIN32
        case OutputEncoding::GBK: return "GBK";
        case OutputEncoding::GB2312: return "GB2312";
        case OutputEncoding::BIG5: return "BIG5";
        case OutputEncoding::SHIFT_JIS: return "Shift_JIS";
#else
        case OutputEncoding::ISO_8859_1: return "ISO-8859-1";
        case OutputEncoding::GB18030: return "GB18030";
        case OutputEncoding::BIG5: return "BIG5";
        case OutputEncoding::EUC_JP: return "EUC-JP";
#endif
        default: return "未知编码";
    }
}

// 获取支持的编码列表
std::vector<std::pair<OutputEncoding, std::string>> CLIProcess::GetSupportedEncodings() {
    return {
        {OutputEncoding::AUTO_DETECT, "自动检测"},
        {OutputEncoding::UTF8, "UTF-8"},
#ifdef _WIN32
        {OutputEncoding::GBK, "GBK (简体中文)"},
        {OutputEncoding::GB2312, "GB2312 (简体中文)"},
        {OutputEncoding::BIG5, "Big5 (繁体中文)"},
        {OutputEncoding::SHIFT_JIS, "Shift-JIS (日文)"},
#else
        {OutputEncoding::ISO_8859_1, "ISO-8859-1"},
        {OutputEncoding::GB18030, "GB18030"},
        {OutputEncoding::BIG5, "BIG5"},
        {OutputEncoding::EUC_JP, "EUC-JP"},
#endif
    };
}

// 根据编码获取代码页
UINT CLIProcess::GetCodePageFromEncoding(const OutputEncoding encoding) {
    switch (encoding) {
        case OutputEncoding::GBK: return 936;
        case OutputEncoding::GB2312: return 20936;
        case OutputEncoding::BIG5: return 950;
        case OutputEncoding::SHIFT_JIS: return 932;
        default: return CP_ACP; // 系统默认代码页
    }
}

// 检查是否为有效的UTF-8
bool CLIProcess::IsValidUTF8(const std::string& str) {
    const auto* bytes = reinterpret_cast<const unsigned char*>(str.c_str());
    size_t len = str.length();

    for (size_t i = 0; i < len; ) {
        if (bytes[i] <= 0x7F) {
            // ASCII字符
            i++;
        } else if ((bytes[i] & 0xE0) == 0xC0) {
            // 2字节UTF-8序列
            if (i + 1 >= len || (bytes[i + 1] & 0xC0) != 0x80) return false;
            i += 2;
        } else if ((bytes[i] & 0xF0) == 0xE0) {
            // 3字节UTF-8序列
            if (i + 2 >= len || (bytes[i + 1] & 0xC0) != 0x80 || (bytes[i + 2] & 0xC0) != 0x80) return false;
            i += 3;
        } else if ((bytes[i] & 0xF8) == 0xF0) {
            // 4字节UTF-8序列
            if (i + 3 >= len || (bytes[i + 1] & 0xC0) != 0x80 || (bytes[i + 2] & 0xC0) != 0x80 || (bytes[i + 3] & 0xC0) != 0x80) return false;
            i += 4;
        } else {
            return false;
        }
    }
    return true;
}

// 转换到UTF-8
std::string CLIProcess::ConvertToUTF8(std::string& input, const OutputEncoding encoding) {
    if (input.empty()) return input;

    // 如果已经是UTF-8编码，直接返回
    if (encoding == OutputEncoding::UTF8) {
        return input;
    }

    UINT codePage = GetCodePageFromEncoding(encoding);

    // 先转换为宽字符
    int wideSize = MultiByteToWideChar(codePage, 0, input.c_str(), -1, nullptr, 0);
    if (wideSize <= 0) {
        // 转换失败，返回原始字符串
        return input;
    }

    std::vector<wchar_t> wideStr(wideSize);
    if (MultiByteToWideChar(codePage, 0, input.c_str(), -1, wideStr.data(), wideSize) <= 0) {
        return input;
    }

    // 再从宽字符转换为UTF-8
    int utf8Size = WideCharToMultiByte(CP_UTF8, 0, wideStr.data(), -1, nullptr, 0, nullptr, nullptr);
    if (utf8Size <= 0) {
        return input;
    }

    std::vector<char> utf8Str(utf8Size);
    if (WideCharToMultiByte(CP_UTF8, 0, wideStr.data(), -1, utf8Str.data(), utf8Size, nullptr, nullptr) <= 0) {
        return input;
    }

    return std::string(utf8Str.data());
}

// 自动检测并转换到UTF-8
std::string CLIProcess::DetectAndConvertToUTF8(std::string& input) {
    if (input.empty()) return input;

    // 首先检查是否已经是有效的UTF-8
    if (IsValidUTF8(input)) {
        return input;
    }

    // 尝试不同的编码进行转换
    const std::vector encodingsToTry = {
        OutputEncoding::GBK,
        OutputEncoding::GB2312,
        OutputEncoding::BIG5,
        OutputEncoding::SHIFT_JIS
    };

    for (OutputEncoding encoding : encodingsToTry) {
        std::string converted = ConvertToUTF8(input, encoding);
        if (converted != input && IsValidUTF8(converted)) {
            // 转换成功且结果是有效的UTF-8
            return converted;
        }
    }

    // 如果所有编码都失败，尝试使用系统默认代码页
    return ConvertToUTF8(input, OutputEncoding::GBK); // 默认使用GBK
}
void CLIProcess::SetStopCommand(const std::string& command, int timeout_ms) {
    std::lock_guard<std::mutex> lock(stop_mutex_);
    stop_command_ = command;
    stop_timeout_ms_ = (timeout_ms > 0) ? timeout_ms : 5000;

    if (!command.empty()) {
        AddLog("已设置停止命令: " + command + " (超时: " + std::to_string(timeout_ms) + "ms)");
    } else {
    }
}

void CLIProcess::SetMaxLogLines(int max_lines) {
    std::lock_guard<std::mutex> lock(logs_mutex_);
    max_log_lines_ = max_lines;
    if (logs_.size() > max_log_lines_) {
        logs_.erase(logs_.begin(), logs_.end() - max_log_lines_);
    }
}

void CLIProcess::SetEnvironmentVariables(const std::map<std::string, std::string>& env_vars) {
    std::lock_guard<std::mutex> lock(env_mutex_);
    environment_variables_.clear();

    // 验证所有环境变量
    for (const auto& pair : env_vars) {
        if (pair.first.empty()) {
            AddLog("警告: 跳过空的环境变量名");
            continue;
        }

        if (pair.first.find('=') != std::string::npos || pair.first.find('\0') != std::string::npos) {
            AddLog("警告: 跳过包含无效字符的环境变量: " + pair.first);
            continue;
        }

        environment_variables_[pair.first] = pair.second;
    }

    if (!environment_variables_.empty()) {
        // AddLog("已设置 " + std::to_string(environment_variables_.size()) + " 个有效环境变量");
        for (const auto& pair : environment_variables_) {
            // AddLog("  " + pair.first + "=" + pair.second);
        }
    } else {
        // AddLog("已清空所有自定义环境变量");
    }
}

const std::map<std::string, std::string>& CLIProcess::GetEnvironmentVariables() const {
    std::lock_guard<std::mutex> lock(env_mutex_);
    return environment_variables_;
}

void CLIProcess::AddEnvironmentVariable(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(env_mutex_);

    // 验证环境变量名
    if (key.empty()) {
        AddLog("错误: 环境变量名不能为空");
        return;
    }

    // 检查是否包含无效字符
    if (key.find('=') != std::string::npos || key.find('\0') != std::string::npos) {
        AddLog("错误: 环境变量名包含无效字符: " + key);
        return;
    }

    environment_variables_[key] = value;
    // AddLog("添加环境变量: " + key + "=" + value);
}
void CLIProcess::RemoveEnvironmentVariable(const std::string& key) {
    std::lock_guard<std::mutex> lock(env_mutex_);
    auto it = environment_variables_.find(key);
    if (it != environment_variables_.end()) {
        environment_variables_.erase(it);
        // AddLog("移除环境变量: " + key);
    }
}

void CLIProcess::ClearEnvironmentVariables() {
    std::lock_guard<std::mutex> lock(env_mutex_);
    environment_variables_.clear();
    // AddLog("已清空所有自定义环境变量");
}

void CLIProcess::Start(const std::string& command) {
    Stop();

    // 确定工作目录
    std::wstring working_dir;
    {
        std::lock_guard<std::mutex> lock(working_dir_mutex_);
        if (use_auto_working_dir_) {
            working_dir = StringToWide(ExtractDirectoryFromCommand(command));
            if (working_dir.empty()) {
                working_dir = StringToWide(std::filesystem::current_path().string());
            }
            // AddLog("自动检测工作目录: " + working_dir);
        } else {
            working_dir = StringToWide(working_directory_);
            if (!working_dir.empty()) {
                // AddLog("使用指定工作目录: " + working_dir);
            }
        }
    }

#ifdef _WIN32
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    if (!CreatePipe(&hReadPipe_, &hWritePipe_, &sa, 0)) {
        AddLog("创建输出管道失败");
        return;
    }

    if (!CreatePipe(&hReadPipe_stdin_, &hWritePipe_stdin_, &sa, 0)) {
        AddLog("创建输入管道失败");
        CloseHandle(hReadPipe_);
        CloseHandle(hWritePipe_);
        return;
    }

    STARTUPINFO si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWritePipe_;
    si.hStdError = hWritePipe_;
    si.hStdInput = hReadPipe_stdin_;
    si.wShowWindow = SW_HIDE;
    ZeroMemory(&pi_, sizeof(pi_));

    // 转换命令为宽字符
    std::wstring wcmd = StringToWide(command);

    // CreateProcess需要可修改的字符串
    std::vector<wchar_t> cmdBuffer(wcmd.begin(), wcmd.end());
    cmdBuffer.push_back(L'\0');

    // 使用Windows API设置环境变量
    std::vector<std::pair<std::string, std::string>> originalEnvVars;
    bool envVarsSet = false;

    if (!environment_variables_.empty()) {
        envVarsSet = true;

        for (const auto &pair: environment_variables_) {
            if (!pair.first.empty()) {
                // 保存原始值（如果存在）
                DWORD bufferSize = GetEnvironmentVariableA(pair.first.c_str(), nullptr, 0);
                if (bufferSize > 0) {
                    // 变量存在，保存原始值
                    std::vector<char> buffer(bufferSize);
                    if (GetEnvironmentVariableA(pair.first.c_str(), buffer.data(), bufferSize) > 0) {
                        originalEnvVars.emplace_back(pair.first, std::string(buffer.data()));
                    } else {
                        originalEnvVars.emplace_back(pair.first, "");
                    }
                } else {
                    // 变量不存在，标记为新变量（使用空字符串表示原来不存在）
                    originalEnvVars.emplace_back(pair.first, "");
                }

                // 设置新的环境变量值
                if (SetEnvironmentVariableA(pair.first.c_str(), pair.second.c_str())) {
                    // AddLog("设置环境变量: " + pair.first + "=" + pair.second);
                } else {
                    AddLog("设置环境变量失败: " + pair.first + " (错误代码: " + std::to_string(GetLastError()) + ")");
                }
            }
        }

        // AddLog("环境变量设置完成，数量: " + std::to_string(environment_variables_.size()));
    } else {
        // AddLog("未设置自定义环境变量，使用默认环境 PWD:" + WideToString(working_dir));
    }

    BOOL result = CreateProcess(
            nullptr,                    // lpApplicationName
            cmdBuffer.data(),          // lpCommandLine
            nullptr,                   // lpProcessAttributes
            nullptr,                   // lpThreadAttributes
            TRUE,                      // bInheritHandles
            CREATE_NO_WINDOW,          // dwCreationFlags
            nullptr,                   // lpEnvironment (使用nullptr让子进程继承当前环境)
            working_dir.empty() ? nullptr : working_dir.data(),                   // lpCurrentDirectory
            &si,                       // lpStartupInfo
            &pi_                       // lpProcessInformation
    );

    // 恢复原始环境变量
    if (envVarsSet) {
        for (const auto &pair: originalEnvVars) {
            if (pair.second.empty()) {
                // 原来不存在，删除变量
                SetEnvironmentVariableA(pair.first.c_str(), nullptr);
            } else {
                // 恢复原始值
                SetEnvironmentVariableA(pair.first.c_str(), pair.second.c_str());
            }
        }
    }

    if (result) {
        AddLog("进程已启动: " + command + " PID: " + std::to_string(pi_.dwProcessId));

        CloseHandle(hWritePipe_);
        CloseHandle(hReadPipe_stdin_);
        hWritePipe_ = nullptr;
        hReadPipe_stdin_ = nullptr;

        output_thread_ = std::thread([this]() {
            ReadOutput();
        });
    } else {
        DWORD err = GetLastError();

        // 获取详细的错误信息
        LPWSTR messageBuffer = nullptr;
        size_t size = FormatMessageW(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPWSTR) &messageBuffer, 0, nullptr);

        std::string errorMsg = "CreateProcess 失败 (错误代码: " + std::to_string(err) + ")";
        if (messageBuffer) {
            std::wstring wErrorMsg(messageBuffer);
            errorMsg += " - " + WideToString(wErrorMsg);
            LocalFree(messageBuffer);
        }

        AddLog(errorMsg);

        // 清理资源
        CloseHandle(hReadPipe_);
        CloseHandle(hWritePipe_);
        CloseHandle(hReadPipe_stdin_);
        CloseHandle(hWritePipe_stdin_);
        hReadPipe_ = hWritePipe_ = hReadPipe_stdin_ = hWritePipe_stdin_ = nullptr;
    }
#else
    // Unix/Linux implementation
    int pipe_out[2];
    int pipe_in[2];

    if (pipe(pipe_out) < 0 || pipe(pipe_in) < 0) {
        AddLog("创建管道失败: " + std::string(strerror(errno)));
        return;
    }

    pid_t pid = fork();
    if (pid == 0) {
        // child process
        close(pipe_out[0]);
        dup2(pipe_out[1], STDOUT_FILENO);
        dup2(pipe_out[1], STDERR_FILENO);
        close(pipe_out[1]);

        close(pipe_in[1]);
        dup2(pipe_in[0], STDIN_FILENO);
        close(pipe_in[0]);

        // 设置工作目录
        if (!working_dir.empty()) {
            if (!DirectoryExists(working_dir)) {
                fprintf(stderr, "警告: 工作目录不存在: %s\n", working_dir.c_str());
                working_dir = std::filesystem::current_path().string();
            }

            if (chdir(working_dir.c_str()) != 0) {
                fprintf(stderr, "无法切换到工作目录: %s, 错误: %s\n",
                       working_dir.c_str(), strerror(errno));
            }
        }

        // Prepare environment variables
        {
            std::lock_guard<std::mutex> lock(env_mutex_);
            if (!environment_variables_.empty()) {
                for (const auto& kv : environment_variables_) {
                    setenv(kv.first.c_str(), kv.second.c_str(), 1);
                }
            }
        }

        execl("/bin/sh", "sh", "-c", command.c_str(), (char*)nullptr);
        _exit(127);
    }
    else if (pid > 0) {
        // parent process
        close(pipe_out[1]);
        close(pipe_in[0]);

        process_pid_ = pid;
        pipe_stdout_[0] = pipe_out[0];
        pipe_stdout_[1] = pipe_out[1]; // closed already in parent, but keep for safety
        pipe_stdin_[0] = pipe_in[0];   // closed already in parent, but keep for safety
        pipe_stdin_[1] = pipe_in[1];

        process_running_ = true;

        AddLog("进程已启动，PID: " + std::to_string(pid));
        if (!working_dir.empty()) {
            AddLog("工作目录: " + working_dir);
        }

        // Start output reading thread
        output_thread_ = std::thread(&CLIProcess::ReadOutput, this);
    } else {
        AddLog("fork失败，无法启动进程: " + std::string(strerror(errno)));
        close(pipe_out[0]);
        close(pipe_out[1]);
        close(pipe_in[0]);
        close(pipe_in[1]);
    }
#endif
}

void CLIProcess::Stop() {
#ifdef _WIN32
    std::lock_guard<std::mutex> lock(stop_mutex_);
    if (pi_.hProcess != nullptr) {
        if (!stop_command_.empty()) {
            SendCommand(stop_command_);
            // Wait for process to exit within timeout
            WaitForSingleObject(pi_.hProcess, stop_timeout_ms_);
        }
        TerminateProcess(pi_.hProcess, 0);
        WaitForSingleObject(pi_.hProcess, INFINITE);
        CloseProcessHandles();
    }
#else
    if (process_running_) {
        std::lock_guard<std::mutex> lock(stop_mutex_);
        if (!stop_command_.empty()) {
            SendCommand(stop_command_);
            // Wait for termination or timeout
            int status = 0;
            for (int i = 0; i < stop_timeout_ms_/100; ++i) {
                pid_t result = waitpid(process_pid_, &status, WNOHANG);
                if (result == process_pid_) {
                    process_running_ = false;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        if (process_running_) {
            kill(process_pid_, SIGKILL);
            waitpid(process_pid_, nullptr, 0);
            process_running_ = false;
        }
        CloseProcessHandles();
    }
#endif

    if (output_thread_.joinable()) {
        output_thread_.join();
    }
}

// 关闭进程句柄的辅助函数
void CLIProcess::CloseProcessHandles() {
    if (pi_.hProcess) {
        CloseHandle(pi_.hProcess);
        pi_.hProcess = nullptr;
    }
    if (pi_.hThread) {
        CloseHandle(pi_.hThread);
        pi_.hThread = nullptr;
    }
}

// 清理资源的辅助函数
void CLIProcess::CleanupResources() {
    // 关闭输入管道写入端（通知进程停止）
    if (hWritePipe_stdin_) {
        CloseHandle(hWritePipe_stdin_);
        hWritePipe_stdin_ = nullptr;
    }

    // 等待输出线程结束
    if (output_thread_.joinable()) {
        output_thread_.join();
    }

    // 关闭输出管道读取端
    if (hReadPipe_) {
        CloseHandle(hReadPipe_);
        hReadPipe_ = nullptr;
    }

    // 确保所有句柄都已关闭
    if (hWritePipe_) {
        CloseHandle(hWritePipe_);
        hWritePipe_ = nullptr;
    }

    if (hReadPipe_stdin_) {
        CloseHandle(hReadPipe_stdin_);
        hReadPipe_stdin_ = nullptr;
    }
}

void CLIProcess::Restart(const std::string& command) {
    Stop();
    Start(command);
}

void CLIProcess::ClearLogs() {
    std::lock_guard<std::mutex> lock(logs_mutex_);
    logs_.clear();
}

void CLIProcess::AddLog(const std::string& log) {
    std::lock_guard<std::mutex> lock(logs_mutex_);
    logs_.push_back(log);
    if (logs_.size() > max_log_lines_) {
        logs_.erase(logs_.begin(), logs_.begin() + (logs_.size() - max_log_lines_));
    }
}

const std::vector<std::string>& CLIProcess::GetLogs() const {
    std::lock_guard<std::mutex> lock(logs_mutex_);
    return logs_;
}


bool CLIProcess::SendCommand(const std::string &command) {
#ifdef _WIN32
    if (!IsRunning() || !hWritePipe_stdin_) {
        return false;
    }

    DWORD bytesWritten;
    std::string fullCommand = command + "\n";

    if (WriteFile(hWritePipe_stdin_, fullCommand.c_str(),
                 static_cast<DWORD>(fullCommand.length()), &bytesWritten, nullptr)) {
        AddLog("> " + command);
        return true;
                 }
    return false;
#else
        if (!process_running_ || pipe_stdin_[1] < 0) return false;

        std::string cmd = command + "\n";
        ssize_t written = write(pipe_stdin_[1], cmd.c_str(), cmd.size());
        return written == (ssize_t)cmd.size();
#endif
}


void CLIProcess::CopyLogsToClipboard() const {
#ifdef _WIN32
    std::lock_guard<std::mutex> lock(logs_mutex_);
    if (logs_.empty()) return;

    // 构建完整的日志字符串（使用\r\n确保跨平台兼容）
    std::wstring allLogs;
    for (const auto& log : logs_) {
        allLogs += StringToWide(log) ;
        allLogs.resize(allLogs.size() - sizeof(wchar_t));
        allLogs += L"\n";
    }

    if (OpenClipboard(nullptr)) {
        EmptyClipboard();

        // 计算正确的内存大小（包括终止空字符）
        const size_t dataSize = (allLogs.length() + 1) * sizeof(wchar_t);
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, dataSize);

        if (hMem) {
            wchar_t* pMem = static_cast<wchar_t*>(GlobalLock(hMem));
            if (pMem) {
                // 安全复制宽字符串数据
                wcscpy_s(pMem, allLogs.length() + 1, allLogs.c_str());
                GlobalUnlock(hMem);
                SetClipboardData(CF_UNICODETEXT, hMem);
            } else {
                GlobalFree(hMem); // 锁定失败时释放内存
            }
        }
        CloseClipboard();
    }
#else
    // Unix / macOS, use xclip or pbcopy
    std::string clipboard_text;
    {
        std::lock_guard<std::mutex> lock(logs_mutex_);
        for (const auto& line : logs_) {
            clipboard_text += line + "\n";
        }
    }
    FILE* pipe = popen("pbcopy", "w");
    if (!pipe) {
        pipe = popen("xclip -selection clipboard", "w");
    }
    if (pipe) {
        fwrite(clipboard_text.c_str(), 1, clipboard_text.size(), pipe);
        pclose(pipe);
    }
#endif
}

bool CLIProcess::IsRunning() const {
#ifdef _WIN32
    if (pi_.hProcess == nullptr) return false;

    DWORD status = WaitForSingleObject(pi_.hProcess, 0);
    return status == WAIT_TIMEOUT;
#else
    if (!process_running_) return false;

    int status;
    pid_t result = waitpid(process_pid_, &status, WNOHANG);
    if (result == 0) return true; // still running
    process_running_ = false;
    return false;
#endif
}


void CLIProcess::ReadOutput() {
#ifdef _WIN32
    constexpr int BUFFER_SIZE = 4096;
    char buffer[BUFFER_SIZE];
    DWORD bytesRead;
    std::string partialLine;

    while (true) {
        if (!ReadFile(hReadPipe_, buffer, BUFFER_SIZE - 1, &bytesRead, nullptr) || bytesRead == 0) {
            break;
        }

        buffer[bytesRead] = '\0';
        std::string output(buffer);

        // 根据设置的编码转换输出
        OutputEncoding currentEncoding;
        {
            std::lock_guard<std::mutex> lock(encoding_mutex_);
            currentEncoding = output_encoding_;
        }

        std::string convertedOutput;
        if (currentEncoding == OutputEncoding::AUTO_DETECT) {
            convertedOutput = DetectAndConvertToUTF8(output);
        } else {
            convertedOutput = ConvertToUTF8(output, currentEncoding);
        }

        size_t start = 0;
        size_t end = convertedOutput.find('\n');

        while (end != std::string::npos) {
            std::string line = partialLine + convertedOutput.substr(start, end - start);
            partialLine.clear();

            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            if (!line.empty()) {
                AddLog(line);
            }

            start = end + 1;
            end = convertedOutput.find('\n', start);
        }

        if (start < convertedOutput.size()) {
            partialLine = convertedOutput.substr(start);
        }
    }
#else
const int buffer_size = 4096;
char buffer[buffer_size];
ssize_t bytes_read = 0;
while (process_running_) {
    bytes_read = read(pipe_stdout_[0], buffer, buffer_size - 1);
    if (bytes_read <= 0) break;
    buffer[bytes_read] = '\0';

    std::string utf8_str;
    {
        std::lock_guard<std::mutex> lock(encoding_mutex_);
        if (output_encoding_ == OutputEncoding::AUTO_DETECT) {
            utf8_str = DetectAndConvertToUTF8(std::string(buffer));
        } else {
            utf8_str = ConvertToUTF8(std::string(buffer), output_encoding_);
        }
    }
    AddLog(utf8_str);
}
#endif
}

std::wstring CLIProcess::GetPid() const {
    if (pi_.hProcess == nullptr) return L"";
    return StringToWide(std::to_string(pi_.dwProcessId));
}
