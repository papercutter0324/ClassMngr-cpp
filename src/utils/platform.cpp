#include "platform.h"



// =========================================================
// Platform Detection
// =========================================================

Platform getPlatform()
{
#ifdef Q_OS_WIN
    return Platform::WINDOWS;

#elif defined(Q_OS_MACOS)
    return Platform::MAC;

#else
    return Platform::LINUX;

#endif
}



// =========================================================
// Helpers
// =========================================================

bool isWindows()
{
    return getPlatform() == Platform::WINDOWS;
}


bool isMac()
{
    return getPlatform() == Platform::MAC;
}


bool isLinux()
{
    return getPlatform() == Platform::LINUX;
}