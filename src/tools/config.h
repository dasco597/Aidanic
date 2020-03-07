#pragma once
#include <stdint.h>

#ifdef _DEBUG

    // allows printing of LOG_TRACE and AID_INFO
    #define _VERBOSE_OUTPUT

    // allows debug breaks
    #ifdef WIN32
    #define _DEBUG_BREAK __debugbreak()
    #endif
    #ifdef LINUX
    #define _DEBUG_BREAK __builtin_trap()
    #endif

#endif // _DEBUG

namespace AIDANIC_CONFIG {
    // general

    const uint32_t initialWindowSize[2] = { 800, 600 };

    // renderer

    const int maxFramesInFlight = 2;

    char* getAssetPath() {
        return "assets/";
    }
}