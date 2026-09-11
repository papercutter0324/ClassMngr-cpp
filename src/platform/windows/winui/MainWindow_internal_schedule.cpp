#include "pch.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation::MainWindowDetail
{

std::wstring boxedString(
    winrt::Windows::Foundation::IInspectable const& value
    )
{
    if (!value)
    {
        return {};
    }

    try
    {
        return asWString(winrt::unbox_value<winrt::hstring>(value));
    }
    catch (...)
    {
        return {};
    }
}

std::wstring selectedComboValue(
    winrt::Microsoft::UI::Xaml::Controls::ComboBox const& combo
    )
{
    const auto item = combo.SelectedItem().try_as<
        winrt::Microsoft::UI::Xaml::Controls::ComboBoxItem>();
    return item
        ? boxedString(item.Tag())
        : boxedString(combo.SelectedItem());
}

int boxedInt(
    winrt::Windows::Foundation::IInspectable const& value
    ) noexcept
{
    if (!value)
    {
        return -1;
    }

    try
    {
        return winrt::unbox_value<int32_t>(value);
    }
    catch (...)
    {
        return -1;
    }
}

std::vector<std::wstring> splitScheduleKey(std::wstring_view value)
{
    std::vector<std::wstring> parts;
    std::size_t start = 0;
    while (start <= value.size())
    {
        const std::size_t separator = value.find(L'|', start);
        const std::size_t end = separator == std::wstring_view::npos
            ? value.size()
            : separator;
        parts.emplace_back(value.substr(start, end - start));
        if (separator == std::wstring_view::npos)
        {
            break;
        }
        start = separator + 1;
    }
    return parts;
}

std::optional<ScheduleSelection> scheduleSelectionFromKey(
    std::wstring_view value
    )
{
    const auto parts = splitScheduleKey(value);
    if (parts.size() != 5 || parts[1].size() != 1)
    {
        return std::nullopt;
    }

    int classId = -1;
    try
    {
        std::size_t parsed = 0;
        classId = std::stoi(parts[0], &parsed);
        if (parsed != parts[0].size())
        {
            return std::nullopt;
        }
    }
    catch (...)
    {
        return std::nullopt;
    }
    if (classId <= 0 || (parts[1][0] != L'r' && parts[1][0] != L'i'))
    {
        return std::nullopt;
    }

    return ScheduleSelection{
        classId,
        parts[1][0] == L'i'
            ? classmngr::engine::ScheduleType::Intensive
            : classmngr::engine::ScheduleType::Regular,
        parts[2],
        parts[3],
        parts[4]
    };
}

std::wstring scheduleSelectionKey(
    int classId,
    classmngr::engine::ScheduleType type,
    std::wstring_view day,
    std::wstring_view startTime,
    std::wstring_view endTime
    )
{
    return std::to_wstring(classId)
        + L'|'
        + (type == classmngr::engine::ScheduleType::Intensive ? L"i" : L"r")
        + L'|' + std::wstring(day)
        + L'|' + std::wstring(startTime)
        + L'|' + std::wstring(endTime);
}

std::wstring scheduleTypeText(classmngr::engine::ScheduleType type)
{
    return type == classmngr::engine::ScheduleType::Intensive
        ? L"Intensive"
        : L"Regular";
}

std::vector<std::wstring> scheduleImportDays(std::wstring_view value)
{
    std::vector<std::wstring> days;
    std::size_t start = 0;
    while (start <= value.size())
    {
        const std::size_t separator = value.find(L',', start);
        const std::size_t end = separator == std::wstring_view::npos
            ? value.size()
            : separator;
        std::wstring day(value.substr(start, end - start));
        const auto first = day.find_first_not_of(L" \t");
        const auto last = day.find_last_not_of(L" \t");
        if (first == std::wstring::npos)
        {
            day.clear();
        }
        else
        {
            day = day.substr(first, last - first + 1);
        }
        if (!day.empty())
        {
            days.push_back(std::move(day));
        }
        if (separator == std::wstring_view::npos)
        {
            break;
        }
        start = separator + 1;
    }
    return days;
}

classmngr::engine::Roster defaultRoster()
{
    classmngr::engine::Roster roster;
    roster.columns.reserve(classmngr::engine::RosterBaseColumns.size());
    for (const std::string_view column : classmngr::engine::RosterBaseColumns)
    {
        roster.columns.emplace_back(column);
    }
    roster.columnWidths = {140, 140, 100, 140, 100, 100};
    padRosterRows(roster);
    return roster;
}

bool validMonthDay(std::wstring_view value) noexcept
{
    if (value.empty())
    {
        return true;
    }
    if (value.size() != 5 || value[2] != L'-')
    {
        return false;
    }
    if (value[0] < L'0' || value[0] > L'9'
        || value[1] < L'0' || value[1] > L'9'
        || value[3] < L'0' || value[3] > L'9'
        || value[4] < L'0' || value[4] > L'9')
    {
        return false;
    }

    const int month = (value[0] - L'0') * 10 + value[1] - L'0';
    const int day = (value[3] - L'0') * 10 + value[4] - L'0';
    constexpr int daysInMonth[] = {
        0, 31, 29, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
    };
    return month >= 1 && month <= 12
        && day >= 1 && day <= daysInMonth[month];
}

winrt::Windows::Foundation::IAsyncAction phase3PresentationWork(
    classmngr::engine::CancellationToken const& cancellation
    )
{
    using namespace std::chrono_literals;

    co_await winrt::resume_after(250ms);
    if (cancellation.isCancellationRequested())
    {
        co_return;
    }

    co_await winrt::resume_after(250ms);
}

} // namespace winrt::ClassMngrWinUI::implementation::MainWindowDetail
