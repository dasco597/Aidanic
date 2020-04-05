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

#define _WINDOW_SIZE_X 1200
#define _WINDOW_SIZE_Y 800

#define _MAX_FRAMES_IN_FLIGHT 2

#define _AID_PI 3.14159f

namespace _CONFIG {
    static char* getAssetPath() {
        return "assets/";
    }
}