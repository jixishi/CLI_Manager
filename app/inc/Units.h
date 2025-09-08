#ifndef UNITS_H
#define UNITS_H
#include <string>
#include <imgui.h>
#include <vector>

std::wstring StringToWide(const std::string& str);
std::string WideToString(const std::wstring& wstr);
void SetAutoStart(bool enable);
bool IsAutoStartEnabled();

// 结构体定义
struct ColoredTextSegment {
    std::string text;   // 文本内容
    ImVec4 color;      // 文本颜色
};

// 日志颜色处理方法
ImVec4 GetLogLevelColor(const std::string &log);       // 获取日志级别颜色
void RenderColoredLogLine(const std::string &log);     // 渲染彩色日志行
std::vector<ColoredTextSegment> ParseAnsiColorCodes(const std::string &text);  // 解析ANSI颜色代码
std::pair<ImVec4, bool>
ParseAnsiColorCode(const std::string &code, const ImVec4 &currentColor, bool currentBold);  // 解析单个ANSI颜色代码
ImVec4 GetAnsiColor(int colorIndex, bool bright);      // 获取ANSI颜色

#endif //UNITS_H
