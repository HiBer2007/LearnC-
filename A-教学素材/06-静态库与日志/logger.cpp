#include "logger.h"

#include <cstdlib>
#include <cstdio>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#else
#include <unistd.h>
#include <fcntl.h>
#endif

std::shared_ptr<spdlog::logger> CLogger::instance_;

std::shared_ptr<spdlog::logger> CLogger::Resolve()
{
    if (instance_)
        return instance_;
    // 插件 DLL 内 instance_ 为本模块独立静态 (CommonLoggerCPP 是 STATIC 库), 恒为空:
    // 回退 spdlog 共享 DLL 全局 registry 的 default_logger (宿主 Init 时注入)。
    // 第二次取时缓存到本模块 instance_ 避免每次查 registry。
    instance_ = spdlog::default_logger();
    return instance_;
}

void CLogger::HoldConsole() {
#ifdef _WIN32
    // 幂等: 多次调用无副作用。stdout 的唯一"持有者"应是这里——
    // 其它模块不要再重复设置码页/VT/流模式，避免颜色丢失。
    ::SetConsoleOutputCP(CP_UTF8);
    ::SetConsoleCP(CP_UTF8);
    HANDLE hOut = ::GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut && hOut != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (::GetConsoleMode(hOut, &mode)) {
            ::SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
#endif
    // 二进制无缓冲: 颜色/UTF-8 字节不被 CRT 按 ANSI 码页改写/缓冲吞掉。
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
#endif
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
}

void CLogger::Init(const std::string& logFile, const std::string& loggerName) {
    if (instance_) {
        return;
    }
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFile, true);

    spdlog::sinks_init_list sinks = { consoleSink, fileSink };
    instance_ = std::make_shared<spdlog::logger>(loggerName, sinks);

    // 默认 info; NSUM_LOG_LEVEL 环境变量可提级 trace/debug (详细日志排障用)
    spdlog::level::level_enum level = spdlog::level::info;
    if (const char* env = std::getenv("NSUM_LOG_LEVEL")) {
        const std::string v = env;
        if (v == "trace") level = spdlog::level::trace;
        else if (v == "debug") level = spdlog::level::debug;
        else if (v == "info") level = spdlog::level::info;
        else if (v == "warn") level = spdlog::level::warn;
        else if (v == "error") level = spdlog::level::err;
        else if (v == "off") level = spdlog::level::off;
    }
    instance_->set_level(level);
    instance_->flush_on(spdlog::level::info);
    spdlog::register_logger(instance_);
    // Cross-module shared channel: plugin DLL CLogger::Resolve reaches this logger via default_logger
    if (!spdlog::default_logger() || spdlog::default_logger()->name() != loggerName)
        spdlog::set_default_logger(instance_);
}

void CLogger::SetLevel(Level level) {
    auto l = Resolve();
    if (l)
        l->set_level(static_cast<spdlog::level::level_enum>(level));
}

void CLogger::AddSink(const std::shared_ptr<spdlog::sinks::sink>& sink) {
    auto l = Resolve();
    if (l && sink)
        l->sinks().push_back(sink);
}
