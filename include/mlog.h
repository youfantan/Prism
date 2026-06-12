#pragma once

#include <format>
#include <source_location>
#include <string>
#include <chrono>

enum class log_level
{
    info,
    debug,
    error,
    fatal,
};

using mlog_sth_init_t = struct
{
    std::string log_directory;
    std::string log_file_name;
};

using mlog_sth_ctx_t = struct
{
    FILE* logstdout;
    FILE* logfile;
};

static inline mlog_sth_ctx_t mlog_ctx;

template<typename T>
std::string get_formatted_time(const T& timepoint, std::string_view format = "{:%F %T}")
{
    auto tp = std::chrono::time_point_cast<std::chrono::seconds>(timepoint);
    return std::vformat(format, std::make_format_args(tp));
}

template<log_level lv, typename... Args>
void log(std::source_location location, const std::string& format, Args&&... args)
{
    std::string prefix;
    std::string color;
    if constexpr (lv == log_level::info)
    {
        prefix = "INFO";
        color = "\033[37m";
    }
    if constexpr (lv == log_level::debug)
    {
        prefix = "DEBUG";
        color = "\033[32m";
    }
    if constexpr (lv == log_level::error)
    {
        prefix = "ERROR";
        color = "\033[31m";
    }
    if constexpr (lv == log_level::fatal)
    {
        prefix = "FATAL";
        color = "\033[31m";
    }

    auto now = std::chrono::system_clock::now();
    std::string log_string = std::vformat(format, std::make_format_args(args...));
    std::string leading_string = std::format("[{}][{}][{}:{}] ", prefix, get_formatted_time(now), location.file_name(), location.column());
    std::string console_leading_string = color + leading_string;
    std::string console_end_string = "\033[37m\n";
    fwrite(console_leading_string.c_str(), 1, console_leading_string.size(), mlog_ctx.logstdout);
    fwrite(log_string.c_str(), 1, log_string.size(), mlog_ctx.logstdout);
    fwrite(console_end_string.c_str(), 1, console_end_string.size(), mlog_ctx.logstdout);
    fwrite(leading_string.c_str(), 1, leading_string.size(), mlog_ctx.logfile);
    fwrite(log_string.c_str(), 1, log_string.size(), mlog_ctx.logfile);
    fwrite("\n", 1, 1, mlog_ctx.logfile);
}

inline int mlog_sth_init(const mlog_sth_init_t& init)
{
    std::string path = init.log_directory + "/" + init.log_file_name;
    mlog_ctx.logstdout = stdout;
    mlog_ctx.logfile = fopen(path.c_str(), "w+");
    if (mlog_ctx.logfile == nullptr)
    {
        return -1;
    }
    return 0;
}

inline void mlog_sth_close()
{
    fclose(mlog_ctx.logfile);
}

#ifdef WIN32
#include <Windows.h>
#include <io.h>

inline bool mlog_enable_win32_console() {
    if (!AllocConsole()) return false;
    HANDLE hOut = CreateFile(L"CONOUT$", GENERIC_READ | GENERIC_WRITE,
                             FILE_SHARE_READ | FILE_SHARE_WRITE,
                             nullptr, OPEN_EXISTING, 0, 0);
    if (hOut == INVALID_HANDLE_VALUE) return false;
    SetStdHandle(STD_OUTPUT_HANDLE, hOut);
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    return true;
}

inline bool mlog_enable_win32_vansi() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return false;
    dwMode |= 0x0004;
    return SetConsoleMode(hOut, dwMode) != FALSE;
}

#endif

#define LINFO(fmt, ...) log<log_level::info>(std::source_location::current(), fmt, ##__VA_ARGS__)
#define LDEBUG(fmt, ...) log<log_level::debug>(std::source_location::current(), fmt, ##__VA_ARGS__)
#define LERROR(fmt, ...) log<log_level::error>(std::source_location::current(), fmt, ##__VA_ARGS__)
#define LFATAL(fmt, ...) log<log_level::fatal>(std::source_location::current(), fmt, ##__VA_ARGS__)