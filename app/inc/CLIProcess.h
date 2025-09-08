#ifndef CLIPROCESS_H
#define CLIPROCESS_H

#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <map>
#include <windows.h>

// 新增：输出编码枚举
enum class OutputEncoding {
    UTF8 = 0,
    GBK,
    GB2312,
    BIG5,
    SHIFT_JIS,
    AUTO_DETECT
};

class CLIProcess {
public:
    CLIProcess();
    ~CLIProcess();

    void SetMaxLogLines(int max_lines);
    void SetStopCommand(const std::string& command, int timeout_ms = 5000);
    void SetEnvironmentVariables(const std::map<std::string, std::string>& env_vars);
    void SetOutputEncoding(OutputEncoding encoding); // 新增：设置输出编码

    void Start(const std::string& command);
    void Stop();
    void Restart(const std::string& command);

    void ClearLogs();
    void AddLog(const std::string& log);
    const std::vector<std::string>& GetLogs() const;

    bool SendCommand(const std::string& command);
    void CopyLogsToClipboard() const;

    bool IsRunning() const;

    // 环境变量管理接口
    const std::map<std::string, std::string>& GetEnvironmentVariables() const;
    void AddEnvironmentVariable(const std::string& key, const std::string& value);
    void RemoveEnvironmentVariable(const std::string& key);
    void ClearEnvironmentVariables();

    // 新增：编码相关接口
    OutputEncoding GetOutputEncoding() const;
    static std::string GetEncodingName(OutputEncoding encoding);
    static std::vector<std::pair<OutputEncoding, std::string>> GetSupportedEncodings();

private:
    void ReadOutput();
    void CloseProcessHandles();
    void CleanupResources();

    // 新增：编码转换相关方法
    std::string ConvertToUTF8(const std::string& input, OutputEncoding encoding);
    std::string DetectAndConvertToUTF8(const std::string& input);
    UINT GetCodePageFromEncoding(OutputEncoding encoding);
    bool IsValidUTF8(const std::string& str);

    PROCESS_INFORMATION pi_{};
    HANDLE hReadPipe_{};
    HANDLE hWritePipe_{};
    HANDLE hReadPipe_stdin_{};
    HANDLE hWritePipe_stdin_;

    mutable std::mutex logs_mutex_;
    std::vector<std::string> logs_;
    int max_log_lines_;

    std::thread output_thread_;

    // 停止命令相关
    std::mutex stop_mutex_;
    std::string stop_command_;
    int stop_timeout_ms_;

    // 环境变量相关
    mutable std::mutex env_mutex_;
    std::map<std::string, std::string> environment_variables_;

    // 新增：编码相关
    mutable std::mutex encoding_mutex_;
    OutputEncoding output_encoding_;
};

#endif // CLIPROCESS_H