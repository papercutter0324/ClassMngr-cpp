#include "pch.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation::MainWindowDetail
{

void setAutomationName(
    winrt::Microsoft::UI::Xaml::DependencyObject const& element,
    std::wstring_view name
    )
{
    winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(
        element,
        winrt::hstring(name)
    );
}

void applyResourceStyle(
    winrt::Microsoft::UI::Xaml::FrameworkElement const& element,
    std::wstring_view key
    )
{
    const auto application =
        winrt::Microsoft::UI::Xaml::Application::Current();
    if (!application || key.empty())
    {
        return;
    }

    const auto resource = application.Resources().Lookup(
        winrt::box_value(winrt::hstring(key))
        );
    if (resource)
    {
        element.Style(resource.as<winrt::Microsoft::UI::Xaml::Style>());
    }
}

winrt::Windows::UI::Color uiColorFromHex(std::string_view value) noexcept
{
    if (value.size() != 7 || value.front() != '#')
    {
        return winrt::Windows::UI::Color{255, 128, 128, 128};
    }

    const auto hexDigit = [](char character) noexcept -> int {
        if (character >= '0' && character <= '9')
        {
            return character - '0';
        }
        if (character >= 'a' && character <= 'f')
        {
            return character - 'a' + 10;
        }
        if (character >= 'A' && character <= 'F')
        {
            return character - 'A' + 10;
        }
        return -1;
    };
    const auto component = [&hexDigit](char high, char low) noexcept -> std::uint8_t {
        const int highValue = hexDigit(high);
        const int lowValue = hexDigit(low);
        return highValue < 0 || lowValue < 0
            ? static_cast<std::uint8_t>(128)
            : static_cast<std::uint8_t>(highValue * 16 + lowValue);
    };
    if (hexDigit(value[1]) < 0 || hexDigit(value[2]) < 0
        || hexDigit(value[3]) < 0 || hexDigit(value[4]) < 0
        || hexDigit(value[5]) < 0 || hexDigit(value[6]) < 0)
    {
        return winrt::Windows::UI::Color{255, 128, 128, 128};
    }
    return winrt::Windows::UI::Color{
        255,
        component(value[1], value[2]),
        component(value[3], value[4]),
        component(value[5], value[6])
    };
}

std::string uiHexFromColor(winrt::Windows::UI::Color color)
{
    constexpr char digits[] = "0123456789ABCDEF";
    std::string value("#000000");
    value[1] = digits[(color.R >> 4) & 0x0f];
    value[2] = digits[color.R & 0x0f];
    value[3] = digits[(color.G >> 4) & 0x0f];
    value[4] = digits[color.G & 0x0f];
    value[5] = digits[(color.B >> 4) & 0x0f];
    value[6] = digits[color.B & 0x0f];
    return value;
}

std::vector<std::wstring> splitPastedRangeRow(std::wstring_view row)
{
    std::vector<std::wstring> values;
    size_t start = 0;
    while (start <= row.size())
    {
        // WinUI TextBox normalizes clipboard tab characters to spaces. Treat
        // both representations as column separators so a TSV range remains
        // usable after the platform text-control boundary.
        const size_t separator = row.find_first_of(L"\t ", start);
        const size_t end = separator == std::wstring_view::npos
            ? row.size()
            : separator;
        values.emplace_back(row.substr(start, end - start));
        if (separator == std::wstring_view::npos)
        {
            break;
        }
        start = separator + 1;
    }
    return values;
}

std::vector<std::vector<std::wstring>> parsePastedRange(
    std::wstring_view text
    )
{
    std::vector<std::vector<std::wstring>> rows;
    size_t start = 0;
    while (start <= text.size())
    {
        const size_t separator = text.find_first_of(L"\r\n", start);
        const size_t end = separator == std::wstring_view::npos
            ? text.size()
            : separator;
        const auto row = text.substr(start, end - start);
        if (!row.empty())
        {
            rows.emplace_back(splitPastedRangeRow(row));
        }
        if (separator == std::wstring_view::npos)
        {
            break;
        }
        start = separator + 1;
        if (text[separator] == L'\r' && start < text.size()
            && text[start] == L'\n')
        {
            ++start;
        }
    }
    return rows;
}

} // namespace winrt::ClassMngrWinUI::implementation::MainWindowDetail
