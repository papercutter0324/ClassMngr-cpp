include_guard(GLOBAL)

if(NOT WIN32)
    message(FATAL_ERROR
        "windows_native.cmake may only be included for a Windows native target."
    )
endif()

set(CLASSMNGR_NATIVE_REQUIRED_WINDOWS_SDK "10.0.26100.0")
if(NOT CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION STREQUAL
        CLASSMNGR_NATIVE_REQUIRED_WINDOWS_SDK)
    message(FATAL_ERROR
        "Windows native builds require Windows SDK "
        "${CLASSMNGR_NATIVE_REQUIRED_WINDOWS_SDK}; selected "
        "'${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}'."
    )
endif()

include(CheckIncludeFileCXX)
check_include_file_cxx("d2d1_3.h" CLASSMNGR_HAS_D2D13_HEADER)
if(NOT CLASSMNGR_HAS_D2D13_HEADER)
    message(FATAL_ERROR
        "The selected Windows SDK does not provide d2d1_3.h."
    )
endif()

include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_DEFINITIONS
    "-DUNICODE"
    "-D_UNICODE"
    "-DWIN32_LEAN_AND_MEAN"
    "-D_WIN32_WINNT=0x0A00"
    "-DNTDDI_VERSION=0x0A000003"
)
check_cxx_source_compiles([=[
#include <sdkddkver.h>
#include <d2d1_3.h>

static_assert(NTDDI_VERSION >= NTDDI_WIN10_RS2);

int main()
{
    ID2D1Factory3* factory = nullptr;
    ID2D1Device2* device = nullptr;
    ID2D1DeviceContext2* context = nullptr;
    return (factory == nullptr && device == nullptr && context == nullptr)
        ? 0
        : 1;
}
]=] CLASSMNGR_D2D13_CAPABILITY_COMPILES)
if(NOT CLASSMNGR_D2D13_CAPABILITY_COMPILES)
    message(FATAL_ERROR
        "The selected Windows SDK cannot compile the required Direct2D 1.3 "
        "Factory3/Device2/DeviceContext2 capability check."
    )
endif()

add_library(ClassMngrWindowsNativeSdk INTERFACE)
target_compile_definitions(ClassMngrWindowsNativeSdk
    INTERFACE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        _WIN32_WINNT=0x0A00
        NTDDI_VERSION=0x0A000003
)
target_link_libraries(ClassMngrWindowsNativeSdk
    INTERFACE
        d2d1
        dwrite
        d3d11
        dxgi
        dcomp
        windowscodecs
        ole32
        uuid
        shcore
        shell32
        user32
        gdi32
        bcrypt
        crypt32
        psapi
)

add_executable(ClassMngrWindowsNativeSdkCapabilityTests
    "${PROJECT_SOURCE_DIR}/src/platform/windows/native/sdk_capability.cpp"
)
target_link_libraries(ClassMngrWindowsNativeSdkCapabilityTests
    PRIVATE
        ClassMngrCommonBuildSettings
        ClassMngrWindowsNativeSdk
)
add_test(
    NAME ClassMngrWindowsNativeSdkCapabilityTests
    COMMAND ClassMngrWindowsNativeSdkCapabilityTests
)
