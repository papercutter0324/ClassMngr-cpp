#include "pch.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation::MainWindowDetail
{

bool calendarDateLess(
    EngineCalendarDate const& left,
    EngineCalendarDate const& right
    ) noexcept
{
    return std::chrono::sys_days{left} < std::chrono::sys_days{right};
}

bool calendarDateEqual(
    EngineCalendarDate const& left,
    EngineCalendarDate const& right
    ) noexcept
{
    return left.ok() && right.ok()
        && std::chrono::sys_days{left} == std::chrono::sys_days{right};
}

EngineCalendarDate calendarToday()
{
    return EngineCalendarDate{
        std::chrono::floor<std::chrono::days>(
            std::chrono::system_clock::now()
            )
        };
}

int calendarDaysInMonth(EngineCalendarDate const& date) noexcept
{
    if (!date.ok())
    {
        return 0;
    }

    return static_cast<int>(static_cast<unsigned>(
        std::chrono::year_month_day_last{
            date.year(),
            std::chrono::month_day_last{date.month()}
        }.day()
        ));
}

EngineCalendarDate calendarMonthStart(EngineCalendarDate const& date)
{
    return date.ok()
        ? EngineCalendarDate{date.year(), date.month(), std::chrono::day{1}}
        : EngineCalendarDate{};
}

EngineCalendarDate calendarAddDays(
    EngineCalendarDate const& date,
    int count
    )
{
    return date.ok()
        ? EngineCalendarDate{
            std::chrono::sys_days{date} + std::chrono::days{count}
        }
        : EngineCalendarDate{};
}

EngineCalendarDate calendarAddMonths(
    EngineCalendarDate const& date,
    int count
    )
{
    if (!date.ok())
    {
        return {};
    }

    const std::chrono::year_month month = date.year() / date.month()
        + std::chrono::months{count};
    const auto lastDay = std::chrono::year_month_day_last{
        month.year(),
        std::chrono::month_day_last{month.month()}
    }.day();
    const std::chrono::day day{
        std::min(
            static_cast<unsigned>(date.day()),
            static_cast<unsigned>(lastDay)
            )
    };
    return EngineCalendarDate{month.year(), month.month(), day};
}

std::wstring calendarDateText(EngineCalendarDate const& date)
{
    if (!date.ok())
    {
        return {};
    }

    const int year = static_cast<int>(date.year());
    const unsigned month = static_cast<unsigned>(date.month());
    const unsigned day = static_cast<unsigned>(date.day());
    std::wstring result = std::to_wstring(year);
    while (result.size() < 4)
    {
        result.insert(result.begin(), L'0');
    }
    result += L'-';
    result += month < 10 ? L"0" : L"";
    result += std::to_wstring(month);
    result += L'-';
    result += day < 10 ? L"0" : L"";
    result += std::to_wstring(day);
    return result;
}

bool calendarDateFromText(
    std::wstring_view value,
    EngineCalendarDate& result
    ) noexcept
{
    if (value.size() != 10 || value[4] != L'-' || value[7] != L'-')
    {
        return false;
    }

    const auto digits = [](std::wstring_view text) noexcept {
        return std::all_of(
            text.begin(),
            text.end(),
            [](wchar_t value) { return value >= L'0' && value <= L'9'; }
            );
    };
    if (!digits(value.substr(0, 4))
        || !digits(value.substr(5, 2))
        || !digits(value.substr(8, 2)))
    {
        return false;
    }

    const int year = std::stoi(std::wstring(value.substr(0, 4)));
    const unsigned month = static_cast<unsigned>(std::stoi(
        std::wstring(value.substr(5, 2))
        ));
    const unsigned day = static_cast<unsigned>(std::stoi(
        std::wstring(value.substr(8, 2))
        ));
    const EngineCalendarDate parsed{
        std::chrono::year{year},
        std::chrono::month{month},
        std::chrono::day{day}
    };
    if (!parsed.ok())
    {
        return false;
    }
    result = parsed;
    return true;
}

std::wstring calendarMonthTitle(EngineCalendarDate const& date)
{
    if (!date.ok())
    {
        return L"Calendar";
    }

    constexpr std::array<std::wstring_view, 12> names{
        L"January", L"February", L"March", L"April", L"May", L"June",
        L"July", L"August", L"September", L"October", L"November",
        L"December"
    };
    const unsigned month = static_cast<unsigned>(date.month());
    return std::wstring(names[month - 1]) + L" "
        + std::to_wstring(static_cast<int>(date.year()));
}

std::optional<std::chrono::minutes> calendarTimeFromText(
    std::wstring_view value
    ) noexcept
{
    if (value.size() != 5 || value[2] != L':'
        || value[0] < L'0' || value[0] > L'9'
        || value[1] < L'0' || value[1] > L'9'
        || value[3] < L'0' || value[3] > L'9'
        || value[4] < L'0' || value[4] > L'9')
    {
        return std::nullopt;
    }

    const int hours = (value[0] - L'0') * 10 + value[1] - L'0';
    const int minutes = (value[3] - L'0') * 10 + value[4] - L'0';
    if (hours > 23 || minutes > 59)
    {
        return std::nullopt;
    }
    return std::chrono::minutes{hours * 60 + minutes};
}

std::wstring calendarTimeText(
    std::optional<std::chrono::minutes> value
    )
{
    if (!value)
    {
        return {};
    }
    const auto count = value->count();
    const int hours = static_cast<int>(count / 60);
    const int minutes = static_cast<int>(count % 60);
    return (hours < 10 ? L"0" : L"") + std::to_wstring(hours)
        + L":" + (minutes < 10 ? L"0" : L"")
        + std::to_wstring(minutes);
}

bool settingBool(
    classmngr::engine::SettingValue const& value,
    bool fallback
    ) noexcept
{
    if (const auto* integer = std::get_if<std::int64_t>(&value))
    {
        return *integer != 0;
    }
    if (const auto* text = std::get_if<std::string>(&value))
    {
        return *text == "1" || *text == "true" || *text == "TRUE";
    }
    return fallback;
}

std::int64_t settingInteger(
    classmngr::engine::SettingValue const& value,
    std::int64_t fallback
    ) noexcept
{
    if (const auto* integer = std::get_if<std::int64_t>(&value))
    {
        return *integer;
    }
    if (const auto* text = std::get_if<std::string>(&value))
    {
        std::int64_t parsed{};
        const auto result = std::from_chars(
            text->data(),
            text->data() + text->size(),
            parsed
            );
        if (result.ec == std::errc{}
            && result.ptr == text->data() + text->size())
        {
            return parsed;
        }
    }
    return fallback;
}

std::optional<winrt::ClassMngrWinUI::implementation::ClassNavigationLocation>
classNavigationLocationFromSetting(
    classmngr::engine::SettingValue const& value
    ) noexcept
{
    const auto* text = std::get_if<std::string>(&value);
    if (text == nullptr)
    {
        return std::nullopt;
    }
    if (*text == "top")
    {
        return winrt::ClassMngrWinUI::implementation::ClassNavigationLocation::Top;
    }
    if (*text == "bottom")
    {
        return winrt::ClassMngrWinUI::implementation::ClassNavigationLocation::Bottom;
    }
    return std::nullopt;
}

classmngr::engine::Result<std::string> subPrepTextSetting(
    classmngr::engine::ApplicationSettingsService& settings,
    std::string_view key,
    std::string defaultValue
    )
{
    const auto loaded = settings.load(key);
    if (!loaded)
    {
        return std::unexpected(loaded.error());
    }
    if (std::holds_alternative<std::monostate>(*loaded))
    {
        return defaultValue;
    }
    const auto* value = std::get_if<std::string>(&*loaded);
    if (!value)
    {
        return std::unexpected(classmngr::engine::Error{
            classmngr::engine::ErrorCode::Schema,
            "Application setting '" + std::string(key)
                + "' must contain a text value.",
            std::nullopt
        });
    }
    return *value;
}

std::string calendarScheduleKey(
    int termYear,
    int school,
    int term,
    bool winterStart
    )
{
    std::string key = "calendar/academic/" + std::to_string(termYear)
        + (school == 0 ? "/elementary/" : "/middle/");
    if (winterStart)
    {
        return key + "winterStart";
    }
    return key + "weeks/" + std::to_string(term);
}

} // namespace winrt::ClassMngrWinUI::implementation::MainWindowDetail
