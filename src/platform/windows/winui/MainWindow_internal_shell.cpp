#include "pch.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation::MainWindowDetail
{

bool readRegistryString(
    HKEY key,
    wchar_t const* valueName,
    std::wstring& value
    ) noexcept
{
    DWORD type{};
    DWORD byteCount{};
    if (RegQueryValueExW(
            key,
            valueName,
            nullptr,
            &type,
            nullptr,
            &byteCount
            ) != ERROR_SUCCESS
        || type != REG_SZ
        || byteCount < sizeof(wchar_t))
    {
        return false;
    }

    std::wstring result(byteCount / sizeof(wchar_t), L'\0');
    if (RegQueryValueExW(
            key,
            valueName,
            nullptr,
            &type,
            reinterpret_cast<LPBYTE>(result.data()),
            &byteCount
            ) != ERROR_SUCCESS)
    {
        return false;
    }

    if (!result.empty() && result.back() == L'\0')
    {
        result.pop_back();
    }
    value = std::move(result);
    return true;
}

bool readRegistryDword(
    HKEY key,
    wchar_t const* valueName,
    DWORD& value
    ) noexcept
{
    DWORD type{};
    DWORD byteCount = sizeof(value);
    return RegQueryValueExW(
               key,
               valueName,
               nullptr,
               &type,
               reinterpret_cast<LPBYTE>(&value),
               &byteCount
               ) == ERROR_SUCCESS
        && type == REG_DWORD
        && byteCount == sizeof(value);
}

void writeRegistryString(
    HKEY key,
    wchar_t const* valueName,
    std::wstring const& value
    ) noexcept
{
    const DWORD byteCount = static_cast<DWORD>(
        (value.size() + 1) * sizeof(wchar_t)
        );
    RegSetValueExW(
        key,
        valueName,
        0,
        REG_SZ,
        reinterpret_cast<BYTE const*>(value.c_str()),
        byteCount
        );
}

void writeRegistryDword(
    HKEY key,
    wchar_t const* valueName,
    DWORD value
    ) noexcept
{
    RegSetValueExW(
        key,
        valueName,
        0,
        REG_DWORD,
        reinterpret_cast<BYTE const*>(&value),
        sizeof(value)
        );
}

PersistedShellState loadShellState() noexcept
{
    PersistedShellState state;
    HKEY key{};
    if (RegOpenKeyExW(
            HKEY_CURRENT_USER,
            ClassMngrWinUIIdentity::ShellStateRegistrySubkey,
            0,
            KEY_READ,
            &key
            ) != ERROR_SUCCESS)
    {
        return state;
    }

    std::wstring selectedPage;
    if (readRegistryString(key, L"SelectedPage", selectedPage)
        && isKnownPageId(selectedPage))
    {
        state.selectedPage = std::move(selectedPage);
    }
    readRegistryString(key, L"NavigationState", state.navigationState);

    for (std::size_t index = 0; index < maximumRecentDatabasePaths; ++index)
    {
        const std::wstring valueName =
            L"RecentDatabase" + std::to_wstring(index);
        std::wstring path;
        if (readRegistryString(key, valueName.c_str(), path))
        {
            state.recentDatabasePaths.emplace_back(std::move(path));
        }
    }
    state.recentDatabasePaths = pruneRecentDatabasePaths(
        state.recentDatabasePaths
        );

    DWORD value{};
    const bool hasLeft = readRegistryDword(key, L"WindowLeft", value);
    if (hasLeft)
    {
        state.windowBounds.left = static_cast<LONG>(value);
    }
    const bool hasTop = readRegistryDword(key, L"WindowTop", value);
    if (hasTop)
    {
        state.windowBounds.top = static_cast<LONG>(value);
    }
    const bool hasRight = readRegistryDword(key, L"WindowRight", value);
    if (hasRight)
    {
        state.windowBounds.right = static_cast<LONG>(value);
    }
    const bool hasBottom = readRegistryDword(key, L"WindowBottom", value);
    if (hasBottom)
    {
        state.windowBounds.bottom = static_cast<LONG>(value);
    }
    state.hasWindowBounds =
        hasLeft && hasTop && hasRight && hasBottom;

    RegCloseKey(key);
    return state;
}

HKEY openShellStateForWrite() noexcept
{
    HKEY key{};
    if (RegCreateKeyExW(
            HKEY_CURRENT_USER,
            ClassMngrWinUIIdentity::ShellStateRegistrySubkey,
            0,
            nullptr,
            REG_OPTION_NON_VOLATILE,
            KEY_WRITE,
            nullptr,
            &key,
            nullptr
            ) != ERROR_SUCCESS)
    {
        return nullptr;
    }
    return key;
}

HWND windowHandle(
    winrt::ClassMngrWinUI::implementation::MainWindow* window
    ) noexcept
{
    try
    {
        const auto inspectable = static_cast<
            winrt::Windows::Foundation::IInspectable>(*window);
        winrt::com_ptr<::IWindowNative> nativeWindow;
        if (inspectable
            && SUCCEEDED(winrt::get_unknown(inspectable)->QueryInterface(
                __uuidof(::IWindowNative),
                nativeWindow.put_void())))
        {
            HWND handle{};
            if (SUCCEEDED(nativeWindow->get_WindowHandle(&handle)))
            {
                return handle;
            }
        }
    }
    catch (...)
    {
    }
    return nullptr;
}

bool isUsableWindowBounds(RECT const& bounds) noexcept
{
    const LONG width = bounds.right - bounds.left;
    const LONG height = bounds.bottom - bounds.top;
    return width >= minimumShellWidth
        && width <= 10000
        && height >= minimumShellHeight
        && height <= 10000
        && bounds.left > -100000
        && bounds.left < 100000
        && bounds.top > -100000
        && bounds.top < 100000;
}

bool moveWindowBoundsIntoWorkArea(RECT* bounds) noexcept
{
    if (!bounds)
    {
        return false;
    }

    const HMONITOR monitor = MonitorFromRect(
        bounds,
        MONITOR_DEFAULTTONEAREST
        );
    if (!monitor)
    {
        return false;
    }

    MONITORINFO monitorInfo{sizeof(monitorInfo)};
    if (!GetMonitorInfoW(monitor, &monitorInfo))
    {
        return false;
    }

    const LONG width = bounds->right - bounds->left;
    const LONG height = bounds->bottom - bounds->top;
    const LONG workAreaWidth =
        monitorInfo.rcWork.right - monitorInfo.rcWork.left;
    const LONG workAreaHeight =
        monitorInfo.rcWork.bottom - monitorInfo.rcWork.top;

    // Keep the entire window in the work area when it fits.  If a saved
    // window is larger than the available work area, keep its top-left corner
    // visible so the user can resize or move it back into place.
    if (width <= workAreaWidth)
    {
        bounds->left = std::clamp(
            bounds->left,
            monitorInfo.rcWork.left,
            monitorInfo.rcWork.right - width
            );
    }
    else
    {
        bounds->left = monitorInfo.rcWork.left;
    }
    if (height <= workAreaHeight)
    {
        bounds->top = std::clamp(
            bounds->top,
            monitorInfo.rcWork.top,
            monitorInfo.rcWork.bottom - height
            );
    }
    else
    {
        bounds->top = monitorInfo.rcWork.top;
    }
    bounds->right = bounds->left + width;
    bounds->bottom = bounds->top + height;
    return true;
}

} // namespace winrt::ClassMngrWinUI::implementation::MainWindowDetail
