#ifndef PLATFORM_H
#define PLATFORM_H

// =========================================================
// Platform Enum
// =========================================================

enum class Platform
{
    WINDOWS,
    MAC,
    LINUX
};



// =========================================================
// Platform Helpers
// =========================================================

Platform getPlatform();

bool isWindows();

bool isMac();

bool isLinux();



#endif // PLATFORM_H