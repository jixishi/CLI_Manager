#ifndef UNITS_H
#define UNITS_H
#include <string>

std::wstring StringToWide(const std::string& str);
std::string WideToString(const std::wstring& wstr);
void SetAutoStart(bool enable);
bool IsAutoStartEnabled();


#endif //UNITS_H
