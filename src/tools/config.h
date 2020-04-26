#pragma once
#include <stdint.h>

#ifdef _DEBUG

// allows printing of LOG_TRACE and AID_INFO
#define _VERBOSE_OUTPUT

// allows debug breaks
#ifdef WIN32
#define _DEBUG_BREAK __debugbreak()
#endif // WIN32
#ifdef LINUX
#define _DEBUG_BREAK __builtin_trap()
#endif // LINUX

#else // _DEBUG
#define _DEBUG_BREAK
#endif // _DEBUG

#define WINDOW_SIZE_X 1200
#define WINDOW_SIZE_Y 800

#define MAX_FRAMES_IN_FLIGHT 2
#define AID_PI 3.14159f

// Size of a static C-style array. Don't use on pointers!
#define ARRAY_SIZE(_ARR) static_cast<uint32_t>(sizeof(_ARR) / sizeof(*_ARR))

namespace _CONFIG {
    static char* getAssetsPath() {
        return "assets/";
    }
}