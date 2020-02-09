#pragma once
#include <stdint.h>

#ifdef _DEBUG
#define _VERBOSE_OUTPUT
#endif

namespace _CONFIG {
    const uint32_t initialWindowSize[2] = { 800, 600 };
    const int maxFramesInFlight = 2;
}