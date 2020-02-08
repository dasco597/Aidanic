#pragma once
#include "tools/config.h"

#include <string>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

class Log {
public:
    static void Init();
    inline static std::shared_ptr<spdlog::logger>& GetLogger() { return sLogger; }

private:
    inline static bool initialized = false;
    static std::shared_ptr<spdlog::logger> sLogger;
};

// log macros
#ifdef _VERBOSE_OUTPUT
#define LOG_TRACE(...)  Log::GetLogger()->trace(__VA_ARGS__)
#define LOG_INFO(...)   Log::GetLogger()->info(__VA_ARGS__)
#else
#define LOG_TRACE(...)
#define LOG_INFO(...)
#endif
#define LOG_WARN(...)   Log::GetLogger()->warn(__VA_ARGS__)
#define LOG_ERROR(...)  Log::GetLogger()->error(std::string(__FILE__) + " [line: " + std::to_string(__LINE__) + "] " + __VA_ARGS__)
#define LOG_FATAL(...)  Log::GetLogger()->fatal(__VA_ARGS__)