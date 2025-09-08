#include "Units.h"
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <windows.h>
#include <sstream>

std::wstring StringToWide(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    std::wstring wstr(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size);
    return wstr;
}

std::string WideToString(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string str(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], size, nullptr, nullptr);
    return str;
}

void SetAutoStart(bool enable) {
    HKEY hKey;
    LPCWSTR path = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

    if (RegOpenKeyEx(HKEY_CURRENT_USER, path, 0, KEY_WRITE, &hKey) != ERROR_SUCCESS)
        return;

    WCHAR exePath[MAX_PATH];
    GetModuleFileName(NULL, exePath, MAX_PATH);

    if (enable) {
        RegSetValueEx(hKey, L"CLIManager", 0, REG_SZ,
            (BYTE*)exePath, (wcslen(exePath) + 1) * sizeof(WCHAR));
    }
    else {
        RegDeleteValue(hKey, L"CLIManager");
    }
    RegCloseKey(hKey);
}

bool IsAutoStartEnabled() {
    HKEY hKey;
    LPCWSTR path = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

    if (RegOpenKeyEx(HKEY_CURRENT_USER, path, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return false;

    DWORD type, size = 0;
    bool exists = (RegQueryValueEx(hKey, L"CLIManager", NULL, &type, NULL, &size) == ERROR_SUCCESS);

    RegCloseKey(hKey);
    return exists;
}

ImVec4 GetLogLevelColor(const std::string &log) {
    // 简单的日志级别颜色区分
    if (log.find("错误") != std::string::npos || log.find("[E]") != std::string::npos ||
        log.find("[ERROR]") != std::string::npos || log.find("error") != std::string::npos) {
        return ImVec4(1.0f, 0.4f, 0.4f, 1.0f); // 红色
    } else if (log.find("警告") != std::string::npos || log.find("[W]") != std::string::npos ||
               log.find("[WARN]") != std::string::npos || log.find("warning") != std::string::npos) {
        return ImVec4(1.0f, 1.0f, 0.4f, 1.0f); // 黄色
    } else if (log.find("信息") != std::string::npos || log.find("[I]") != std::string::npos ||
               log.find("[INFO]") != std::string::npos || log.find("info") != std::string::npos) {
        return ImVec4(0.4f, 1.0f, 0.4f, 1.0f); // 绿色
    }
    return ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // 默认白色
}


void RenderColoredLogLine(const std::string &log) {
    auto segments = ParseAnsiColorCodes(log);

    if (segments.empty()) {
        // 如果没有ANSI代码，使用简单的日志级别颜色
        ImVec4 textColor = GetLogLevelColor(log);
        ImGui::TextColored(textColor, "%s", log.c_str());
        return;
    }

    // 渲染带颜色的文本段
    bool first = true;
    for (const auto &segment: segments) {
        if (!first) {
            ImGui::SameLine(0, 0); // 在同一行继续显示
        }
        first = false;

        if (!segment.text.empty()) {
            ImGui::TextColored(segment.color, "%s", segment.text.c_str());
        }
    }
}


std::vector<ColoredTextSegment> ParseAnsiColorCodes(const std::string &text) {
    std::vector<ColoredTextSegment> segments;

    if (text.empty()) {
        return segments;
    }

    size_t pos = 0;
    ImVec4 currentColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // 默认白色
    bool isBold = false;

    while (pos < text.length()) {
        size_t escapePos = text.find('\033', pos);

        if (escapePos == std::string::npos) {
            // 没有更多转义序列，添加剩余文本
            if (pos < text.length()) {
                segments.push_back({text.substr(pos), currentColor});
            }
            break;
        }

        // 添加转义序列之前的文本
        if (escapePos > pos) {
            segments.push_back({text.substr(pos, escapePos - pos), currentColor});
        }

        // 解析ANSI转义序列
        size_t codeStart = escapePos + 1;
        if (codeStart < text.length() && text[codeStart] == '[') {
            size_t codeEnd = text.find('m', codeStart);
            if (codeEnd != std::string::npos) {
                std::string colorCode = text.substr(codeStart + 1, codeEnd - codeStart - 1);
                auto newColor = ParseAnsiColorCode(colorCode, currentColor, isBold);
                currentColor = newColor.first;
                isBold = newColor.second;
                pos = codeEnd + 1;
            } else {
                // 无效的转义序列，跳过
                pos = codeStart;
            }
        } else {
            // 无效的转义序列，跳过
            pos = codeStart;
        }
    }

    return segments;
}

std::pair<ImVec4, bool>
ParseAnsiColorCode(const std::string &code, const ImVec4 &currentColor, bool currentBold) {
    ImVec4 newColor = currentColor;
    bool newBold = currentBold;

    // 分割多个颜色代码(用分号分隔)
    std::vector<int> codes;
    std::stringstream ss(code);
    std::string item;

    while (std::getline(ss, item, ';')) {
        if (!item.empty()) {
            try {
                codes.push_back(std::stoi(item));
            } catch (const std::exception &) {
                // 忽略无效的代码
            }
        }
    }

    // 如果没有代码，默认为0(重置)
    if (codes.empty()) {
        codes.push_back(0);
    }

    for (int colorCode: codes) {
        switch (colorCode) {
            case 0:  // 重置
                newColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                newBold = false;
                break;
            case 1:  // 粗体/亮色
                newBold = true;
                break;
            case 22: // 正常强度
                newBold = false;
                break;

                // 前景色 (30-37)
            case 30:
                newColor = GetAnsiColor(0, newBold);
                break; // 黑色
            case 31:
                newColor = GetAnsiColor(1, newBold);
                break; // 红色
            case 32:
                newColor = GetAnsiColor(2, newBold);
                break; // 绿色
            case 33:
                newColor = GetAnsiColor(3, newBold);
                break; // 黄色
            case 34:
                newColor = GetAnsiColor(4, newBold);
                break; // 蓝色
            case 35:
                newColor = GetAnsiColor(5, newBold);
                break; // 洋红
            case 36:
                newColor = GetAnsiColor(6, newBold);
                break; // 青色
            case 37:
                newColor = GetAnsiColor(7, newBold);
                break; // 白色
            case 39:
                newColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                break; // 默认前景色

                // 亮色前景色 (90-97)
            case 90:
                newColor = GetAnsiColor(8, false);
                break;  // 亮黑色(灰色)
            case 91:
                newColor = GetAnsiColor(9, false);
                break;  // 亮红色
            case 92:
                newColor = GetAnsiColor(10, false);
                break; // 亮绿色
            case 93:
                newColor = GetAnsiColor(11, false);
                break; // 亮黄色
            case 94:
                newColor = GetAnsiColor(12, false);
                break; // 亮蓝色
            case 95:
                newColor = GetAnsiColor(13, false);
                break; // 亮洋红
            case 96:
                newColor = GetAnsiColor(14, false);
                break; // 亮青色
            case 97:
                newColor = GetAnsiColor(15, false);
                break; // 亮白色

                // 背景色暂时忽略 (40-47, 100-107)
            default:
                // 处理256色和RGB色彩(38;5;n 和 38;2;r;g;b)
                // 这里可以根据需要扩展
                break;
        }
    }

    return {newColor, newBold};
}

ImVec4 GetAnsiColor(int colorIndex, bool bright) {
    // ANSI标准颜色表
    static const ImVec4 ansiColors[16] = {
            // 标准颜色 (0-7)
            ImVec4(0.0f, 0.0f, 0.0f, 1.0f),     // 0: 黑色
            ImVec4(0.8f, 0.0f, 0.0f, 1.0f),     // 1: 红色
            ImVec4(0.0f, 0.8f, 0.0f, 1.0f),     // 2: 绿色
            ImVec4(0.8f, 0.8f, 0.0f, 1.0f),     // 3: 黄色
            ImVec4(0.0f, 0.0f, 0.8f, 1.0f),     // 4: 蓝色
            ImVec4(0.8f, 0.0f, 0.8f, 1.0f),     // 5: 洋红
            ImVec4(0.0f, 0.8f, 0.8f, 1.0f),     // 6: 青色
            ImVec4(0.8f, 0.8f, 0.8f, 1.0f),     // 7: 白色

            // 亮色 (8-15)
            ImVec4(0.5f, 0.5f, 0.5f, 1.0f),     // 8: 亮黑色(灰色)
            ImVec4(1.0f, 0.0f, 0.0f, 1.0f),     // 9: 亮红色
            ImVec4(0.0f, 1.0f, 0.0f, 1.0f),     // 10: 亮绿色
            ImVec4(1.0f, 1.0f, 0.0f, 1.0f),     // 11: 亮黄色
            ImVec4(0.0f, 0.0f, 1.0f, 1.0f),     // 12: 亮蓝色
            ImVec4(1.0f, 0.0f, 1.0f, 1.0f),     // 13: 亮洋红
            ImVec4(0.0f, 1.0f, 1.0f, 1.0f),     // 14: 亮青色
            ImVec4(1.0f, 1.0f, 1.0f, 1.0f),     // 15: 亮白色
    };

    if (colorIndex >= 0 && colorIndex < 16) {
        ImVec4 color = ansiColors[colorIndex];

        // 如果是粗体且是标准颜色(0-7)，增加亮度
        if (bright && colorIndex < 8) {
            color.x = std::min(1.0f, color.x + 0.3f);
            color.y = std::min(1.0f, color.y + 0.3f);
            color.z = std::min(1.0f, color.z + 0.3f);
        }

        return color;
    }

    return ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // 默认白色
}
