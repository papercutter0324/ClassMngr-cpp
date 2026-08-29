#include "native_application.h"

#include "native_identity.h"

#include <windows.h>

#include <string_view>
#include <vector>

namespace
{
using RtlGetVersionFunction = LONG (WINAPI *)(PRTL_OSVERSIONINFOW);

bool contains(std::wstring_view text, std::wstring_view value)
{
    return text.find(value) != std::wstring_view::npos;
}

std::wstring embeddedManifest(HINSTANCE instance)
{
    HRSRC resource = FindResourceW(
        instance,
        MAKEINTRESOURCEW(1),
        RT_MANIFEST
        );
    if (!resource)
    {
        return {};
    }

    HGLOBAL loadedResource = LoadResource(instance, resource);
    if (!loadedResource)
    {
        return {};
    }

    const DWORD size = SizeofResource(instance, resource);
    const auto* bytes = static_cast<const char*>(LockResource(loadedResource));
    if (!bytes || size == 0)
    {
        return {};
    }

    const int utf16Length = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        bytes,
        static_cast<int>(size),
        nullptr,
        0
        );
    if (utf16Length > 0)
    {
        std::wstring manifest(static_cast<std::size_t>(utf16Length), L'\0');
        MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            bytes,
            static_cast<int>(size),
            manifest.data(),
            utf16Length
            );
        return manifest;
    }

    return std::wstring(
        reinterpret_cast<const wchar_t*>(bytes),
        reinterpret_cast<const wchar_t*>(bytes) + (size / sizeof(wchar_t))
        );
}
}

NativeApplication::NativeApplication(HINSTANCE instance)
    : m_instance(instance)
{
}

int NativeApplication::run(const std::vector<std::wstring>& arguments)
{
    if (!isSupportedWindowsVersion())
    {
        MessageBoxW(
            nullptr,
            L"ClassMngr Native requires Windows 10 version 1809 (build 17763) or later.",
            L"ClassMngr Native",
            MB_OK | MB_ICONERROR
            );
        return ERROR_OLD_WIN_VERSION;
    }

    bool smokeTest = false;
    bool manifestTest = false;
    for (const std::wstring& argument : arguments)
    {
        smokeTest = smokeTest || argument == L"--phase1-smoke-test";
        manifestTest = manifestTest || argument == L"--phase1-manifest-test";
    }

    if (manifestTest)
    {
        return verifyEmbeddedManifest() ? 0 : ERROR_BAD_EXE_FORMAT;
    }

    if (!registerWindowClass() || !createWindow())
    {
        return static_cast<int>(GetLastError());
    }

    if (smokeTest)
    {
        DestroyWindow(m_window);
        m_window = nullptr;
        UnregisterClassW(
            ClassMngrNativeIdentity::WindowClassName,
            m_instance
            );
        return 0;
    }

    ShowWindow(m_window, SW_SHOW);
    UpdateWindow(m_window);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    UnregisterClassW(
        ClassMngrNativeIdentity::WindowClassName,
        m_instance
        );
    return static_cast<int>(message.wParam);
}

bool NativeApplication::registerWindowClass() const
{
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.hInstance = m_instance;
    windowClass.lpfnWndProc = &NativeApplication::windowProcedure;
    windowClass.lpszClassName = ClassMngrNativeIdentity::WindowClassName;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hIcon = LoadIconW(
        m_instance,
        MAKEINTRESOURCEW(ClassMngrNativeIdentity::IconResourceId)
        );
    windowClass.hbrBackground = static_cast<HBRUSH>(
        GetStockObject(WHITE_BRUSH)
        );

    return RegisterClassExW(&windowClass) != 0
        || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

bool NativeApplication::createWindow()
{
    m_window = CreateWindowExW(
        0,
        ClassMngrNativeIdentity::WindowClassName,
        L"ClassMngr Native",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        960,
        640,
        nullptr,
        nullptr,
        m_instance,
        nullptr
        );
    return m_window != nullptr;
}

bool NativeApplication::isSupportedWindowsVersion() const
{
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll)
    {
        return false;
    }

    const auto getVersion = reinterpret_cast<RtlGetVersionFunction>(
        GetProcAddress(ntdll, "RtlGetVersion")
        );
    if (!getVersion)
    {
        return false;
    }

    RTL_OSVERSIONINFOW version{};
    version.dwOSVersionInfoSize = sizeof(version);
    if (getVersion(&version) != 0)
    {
        return false;
    }

    return version.dwMajorVersion > 10
        || (version.dwMajorVersion == 10
            && version.dwBuildNumber >= 17763);
}

bool NativeApplication::verifyEmbeddedManifest() const
{
    const std::wstring manifest = embeddedManifest(m_instance);
    return contains(
               manifest,
               L"{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}"
               )
        && contains(manifest, L"PerMonitorV2, PerMonitor, System")
        && contains(manifest, L"longPathAware>true")
        && contains(manifest, L"requestedExecutionLevel level=\"asInvoker\"");
}

LRESULT CALLBACK NativeApplication::windowProcedure(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam
    )
{
    switch (message)
    {
    case WM_NCDESTROY:
        PostQuitMessage(0);
        break;
    case WM_CLOSE:
        DestroyWindow(window);
        break;
    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }

    static_cast<void>(lParam);
    return 0;
}
