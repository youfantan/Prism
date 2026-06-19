#include <utils.h>
#include <mlog.h>

std::wstring ConvertStringToWstring(const std::string& src) {
    if (src.empty()) return {};
    int len = MultiByteToWideChar(CP_ACP, 0, src.data(), -1, nullptr, 0);
    if (len == 0) {
        LFATAL("Cannot convert string to wstring, Win32 API MultiByteToWideChar() returns length == 0");
        return {};
    }
    wchar_t* dst = (wchar_t*)malloc(len * sizeof(wchar_t));
    MultiByteToWideChar(CP_ACP, 0, src.data(), -1, dst, len);
    return dst;
}

std::string ConvertWstringToString(const std::wstring& src) {
    if (src.empty()) {};
    int len = WideCharToMultiByte(CP_ACP, 0, src.data(), -1, nullptr, 0, nullptr, nullptr);
    if (len == 0) {
        LFATAL("Cannot convert wstring to string, Win32 API WideCharToMultiByte() returns length == 0");
        return {};
    }
    char* dst = (char*)malloc(len);
    WideCharToMultiByte( CP_ACP, 0, src.data(), -1, dst, len, nullptr, nullptr);
    return dst;
}
