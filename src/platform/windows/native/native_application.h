#pragma once

#include <windows.h>

#include <string>
#include <vector>

class NativeApplication
{
public:
    explicit NativeApplication(HINSTANCE instance);

    [[nodiscard]] int run(const std::vector<std::wstring>& arguments);

private:
    [[nodiscard]] bool registerWindowClass() const;
    [[nodiscard]] bool createWindow();
    [[nodiscard]] bool isSupportedWindowsVersion() const;
    [[nodiscard]] bool verifyEmbeddedManifest() const;

    static LRESULT CALLBACK windowProcedure(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam
        );

    HINSTANCE m_instance = nullptr;
    HWND m_window = nullptr;
};
