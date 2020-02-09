#pragma once
#include "tools/config.h"

#include <string>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

/*
    Guildlines for logging:
    signal the start of a function inside the function and signal it's completion after the line it's called in
    try to start log entries with capital letters
*/

class Log {
public:
    static void Init();
    inline static std::shared_ptr<spdlog::logger>& GetLogger() { return sLogger; }

private:
    inline static bool initialized = false;
    static std::shared_ptr<spdlog::logger> sLogger;
};

// LOG MACROS

#ifdef _VERBOSE_OUTPUT
#define LOG_TRACE(...)  Log::GetLogger()->trace(__VA_ARGS__)
#define LOG_INFO(...)   Log::GetLogger()->info(__VA_ARGS__)
#else
#define LOG_TRACE(...)
#define LOG_INFO(...)
#endif
#define LOG_WARN(...)   Log::GetLogger()->warn(__VA_ARGS__)
#define LOG_ERROR(...)  Log::GetLogger()->error("ERROR - " + std::string(__FILE__) + " [line: " + std::to_string(__LINE__) + "]\n" + __VA_ARGS__);\
    _DEBUG_BREAK; throw std::runtime_error("Aidanic crashed! See above error message")
#define LOG_FATAL(...)  Log::GetLogger()->fatal(__VA_ARGS__)