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

/*
	Example usage:
	AID_INFO("swap chain image count = {}", imageCount);
	formatting: https://fmt.dev/dev/syntax.html
	https://github.com/fmtlib/fmt
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
#define AID_TRACE(...)  Log::GetLogger()->trace(__VA_ARGS__)
#define AID_INFO(...)   Log::GetLogger()->info(__VA_ARGS__)
#else // _VERBOSE_OUTPUT
#define AID_TRACE(...)
#define AID_INFO(...)
#endif // _VERBOSE_OUTPUT
#define AID_WARN(...)   Log::GetLogger()->warn(__VA_ARGS__)
#define AID_ERROR(...)  Log::GetLogger()->error("ERROR - " + std::string(__FILE__) + \
    " [line: " + std::to_string(__LINE__) + "]\n" + __VA_ARGS__); _DEBUG_BREAK;      \
    throw std::runtime_error("Aidanic crashed! See above error message")
#define AID_FATAL(...)  Log::GetLogger()->fatal(__VA_ARGS__)