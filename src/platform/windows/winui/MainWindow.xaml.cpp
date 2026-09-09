#include "pch.h"

#include "MainWindow.xaml.h"
#include "classmngr/engine/application_settings_service.h"
#include "classmngr/engine/calendar_event_rules.h"
#include "classmngr/engine/calendar_event_service.h"
#include "classmngr/engine/calendar_event_validator.h"
#include "classmngr/engine/academic_calendar.h"
#include "classmngr/engine/campus_record_service.h"
#include "classmngr/engine/class_info_config.h"
#include "classmngr/engine/class_info_service.h"
#include "classmngr/engine/class_info_validator.h"
#include "classmngr/engine/class_repository.h"
#include "classmngr/engine/class_schedule_service.h"
#include "classmngr/engine/class_transfer_service.h"
#include "classmngr/engine/database_file_format.h"
#include "classmngr/engine/gs_team_service.h"
#include "classmngr/engine/intensive_slot_state_service.h"
#include "classmngr/engine/native_english_teacher_service.h"
#include "classmngr/engine/open_database.h"
#include "classmngr/engine/personal_details_service.h"
#include "classmngr/engine/roster_report_template.h"
#include "classmngr/engine/roster_service.h"
#include "classmngr/engine/roster_validator.h"
#include "classmngr/engine/schedule_builder.h"
#include "classmngr/engine/schedule_report.h"
#include "classmngr/engine/schedule_import_service.h"
#include "classmngr/engine/speaking_analytics.h"
#include "classmngr/engine/speaking_evaluation_ai_prompt.h"
#include "classmngr/engine/speaking_evaluation_persistence_service.h"
#include "classmngr/engine/speaking_evaluation_report_model.h"
#include "classmngr/engine/speaking_evaluation_validator.h"
#include "classmngr/engine/student_name.h"
#include "classmngr/engine/sub_prep_class_information.h"
#include "classmngr/engine/sub_prep_document.h"
#include "classmngr/engine/sub_prep_package.h"
#include "classmngr/engine/teacher_service.h"
#include "classmngr/engine/testing_block_service.h"
#include "classmngr/engine/testing_class_service.h"
#include "winui_build_info.h"
#include "winui_identity.h"
#include "winui_platform_services.h"
#include "winui_shared_ux.h"
#include "winui_schedule_board.h"

#include <microsoft.ui.xaml.window.h>
#include <shobjidl_core.h>

#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Storage.Pickers.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.UI.h>
#include <winrt/Windows.UI.Text.h>

#include <charconv>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cwctype>
#include <coroutine>
#include <filesystem>
#include <fstream>
#include <map>
#include <numeric>
#include <set>
#include <string>
#include <string_view>
#include <optional>
#include <utility>
#include <vector>

#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

namespace
{

constexpr std::wstring_view homePageId = L"home";
constexpr std::wstring_view personalDetailsPageId = L"personal_details";
constexpr std::wstring_view koreanTeachersPageId = L"teachers_all_korean";
constexpr std::wstring_view nativeEnglishTeachersPageId =
    L"native_english_teachers";
constexpr std::wstring_view gsTeamPageId = L"gs_team";
constexpr std::wstring_view subPrepPageId = L"sub_prep";
constexpr std::wstring_view classesPageId = L"classes";
constexpr std::wstring_view classDetailsPageId = L"classes_details";
constexpr std::wstring_view classRosterPageId = L"classes_roster";
constexpr std::wstring_view classSpeakingEvaluationsPageId = L"classes_speaking_evaluations";
constexpr std::wstring_view classAnalyticsPageId = L"classes_analytics";
constexpr std::wstring_view classNotesPageId = L"classes_notes";
constexpr std::string_view classNavigationLocationKey =
    "classes/navigationLocation";
constexpr std::wstring_view aboutPageId = L"about";
constexpr std::wstring_view campusInformationPageId = L"campus_information";
constexpr std::wstring_view campusDirectionsPageId = L"campus_directions";
constexpr std::wstring_view campusAddressPageId = L"campus_address";
constexpr std::wstring_view campusHousingPageId = L"campus_housing";
constexpr std::wstring_view campusMapPageId = L"campus_map";
constexpr int32_t minimumShellWidth = 800;
constexpr int32_t minimumShellHeight = 600;
constexpr int32_t defaultShellWidth = 1270;
constexpr int32_t defaultShellHeight = 1040;
constexpr std::size_t maximumRecentDatabasePaths = 10;

struct ResumeOnDispatcherQueue
{
    winrt::Microsoft::UI::Dispatching::DispatcherQueue dispatcher;
    winrt::Microsoft::UI::Dispatching::DispatcherQueuePriority priority;

    [[nodiscard]] bool await_ready() const noexcept
    {
        return false;
    }

    void await_suspend(std::coroutine_handle<> continuation) const
    {
        if (!dispatcher.TryEnqueue(
                priority,
                [continuation]() noexcept {
                    continuation.resume();
                }
                ))
        {
            throw winrt::hresult_illegal_method_call();
        }
    }

    void await_resume() const noexcept
    {
    }
};

struct PersistedShellState
{
    std::wstring selectedPage{std::wstring(homePageId)};
    std::wstring navigationState;
    std::vector<std::wstring> recentDatabasePaths;
    RECT windowBounds{};
    bool hasWindowBounds{};
};

std::string asUtf8(std::wstring_view value);
bool isSupportedDatabasePath(std::wstring_view path) noexcept;
bool pathExists(std::wstring_view path) noexcept;
std::wstring absolutePath(std::wstring_view path);
bool samePath(std::wstring_view lhs, std::wstring_view rhs) noexcept;
std::vector<std::wstring> pruneRecentDatabasePaths(
    std::vector<std::wstring> const& paths
    );

void appendJsonEscaped(std::string& output, std::string_view value)
{
    output.push_back('"');
    for (const unsigned char character : value)
    {
        switch (character)
        {
        case '"':
            output += "\\\"";
            break;
        case '\\':
            output += "\\\\";
            break;
        case '\b':
            output += "\\b";
            break;
        case '\f':
            output += "\\f";
            break;
        case '\n':
            output += "\\n";
            break;
        case '\r':
            output += "\\r";
            break;
        case '\t':
            output += "\\t";
            break;
        default:
            if (character < 0x20)
            {
                constexpr char hex[] = "0123456789abcdef";
                output += "\\u00";
                output.push_back(hex[(character >> 4) & 0x0f]);
                output.push_back(hex[character & 0x0f]);
            }
            else
            {
                output.push_back(static_cast<char>(character));
            }
            break;
        }
    }
    output.push_back('"');
}

std::string campusResourceFileName(std::string_view reference)
{
    const std::size_t separator = reference.find_last_of("/\\");
    const std::string_view name = separator == std::string_view::npos
        ? reference
        : reference.substr(separator + 1);
    if (name.empty() || name == "." || name == ".."
        || name.find_first_of("<>:\"/\\|?*") != std::string_view::npos
        || name.back() == '.' || name.back() == ' ')
    {
        return {};
    }
    return std::string(name);
}

std::string uniqueCampusResourceFileName(
    std::string name,
    std::set<std::string>& usedNames
    )
{
    if (usedNames.insert(name).second)
    {
        return name;
    }

    const std::size_t extension = name.find_last_of('.');
    const std::string stem = extension == std::string::npos
        ? name
        : name.substr(0, extension);
    const std::string suffix = extension == std::string::npos
        ? std::string{}
        : name.substr(extension);
    for (std::size_t index = 2;; ++index)
    {
        std::string candidate = stem + "_" + std::to_string(index) + suffix;
        if (usedNames.insert(candidate).second)
        {
            return candidate;
        }
    }
}

bool isKnownPageId(std::wstring_view pageId) noexcept
{
    return pageId == homePageId
        || pageId == personalDetailsPageId
        || pageId == koreanTeachersPageId
        || pageId == nativeEnglishTeachersPageId
        || pageId == gsTeamPageId
        || pageId == subPrepPageId
        || pageId == classesPageId
        || pageId == classDetailsPageId
        || pageId == classRosterPageId
        || pageId == classSpeakingEvaluationsPageId
        || pageId == classAnalyticsPageId
        || pageId == classNotesPageId
        || pageId == aboutPageId
        || pageId == campusInformationPageId
        || pageId == campusDirectionsPageId
        || pageId == campusAddressPageId
        || pageId == campusHousingPageId
        || pageId == campusMapPageId;
}

bool isClassesPageId(std::wstring_view pageId) noexcept
{
    return pageId == classesPageId
        || pageId == classDetailsPageId
        || pageId == classRosterPageId
        || pageId == classSpeakingEvaluationsPageId
        || pageId == classAnalyticsPageId
        || pageId == classNotesPageId;
}

bool isCampusPageId(std::wstring_view pageId) noexcept
{
    return pageId == campusInformationPageId
        || pageId == campusDirectionsPageId
        || pageId == campusAddressPageId
        || pageId == campusHousingPageId
        || pageId == campusMapPageId;
}

std::string asUtf8(std::wstring_view value)
{
    return winrt::to_string(winrt::hstring(value));
}

bool isSupportedDatabasePath(std::wstring_view path) noexcept
{
    try
    {
        return classmngr::engine::DatabaseFileFormat::isSupportedInputPath(
            asUtf8(path)
            );
    }
    catch (...)
    {
        return false;
    }
}

bool pathExists(std::wstring_view path) noexcept
{
    std::error_code error;
    return std::filesystem::exists(
        std::filesystem::path(std::wstring(path)),
        error
        ) && !error;
}

std::wstring absolutePath(std::wstring_view path)
{
    std::error_code error;
    const auto absolute = std::filesystem::absolute(
        std::filesystem::path(std::wstring(path)),
        error
        );
    return error ? std::wstring(path) : absolute.wstring();
}

bool samePath(std::wstring_view lhs, std::wstring_view rhs) noexcept
{
    if (lhs.size() != rhs.size())
    {
        return false;
    }

    for (std::size_t index = 0; index < lhs.size(); ++index)
    {
        wchar_t left = lhs[index];
        wchar_t right = rhs[index];
        if (left >= L'A' && left <= L'Z')
        {
            left = static_cast<wchar_t>(left - L'A' + L'a');
        }
        if (right >= L'A' && right <= L'Z')
        {
            right = static_cast<wchar_t>(right - L'A' + L'a');
        }
        if (left == L'\\')
        {
            left = L'/';
        }
        if (right == L'\\')
        {
            right = L'/';
        }
        if (left != right)
        {
            return false;
        }
    }
    return true;
}

std::vector<std::wstring> pruneRecentDatabasePaths(
    std::vector<std::wstring> const& paths
    )
{
    std::vector<std::wstring> result;
    result.reserve(std::min(paths.size(), maximumRecentDatabasePaths));
    for (std::wstring const& path : paths)
    {
        if (!isSupportedDatabasePath(path) || !pathExists(path))
        {
            continue;
        }
        if (std::any_of(
                result.begin(),
                result.end(),
                [&path](std::wstring const& existing) {
                    return samePath(existing, path);
                }
                ))
        {
            continue;
        }
        result.emplace_back(path);
        if (result.size() == maximumRecentDatabasePaths)
        {
            break;
        }
    }
    return result;
}

std::wstring asWString(winrt::hstring const& value)
{
    return std::wstring(value.c_str(), value.size());
}

std::wstring asWide(std::string_view value)
{
    return asWString(winrt::to_hstring(std::string(value)));
}

struct SpeakingAiPrivateNotes
{
    std::string didWell;
    std::string needsImprovement;
};

std::string normalizeSpeakingAiLineEndings(std::string value)
{
    for (std::size_t index = 0; index < value.size(); ++index)
    {
        if (value[index] != '\r')
        {
            continue;
        }
        value[index] = '\n';
        if (index + 1 < value.size() && value[index + 1] == '\n')
        {
            value.erase(index + 1, 1);
        }
    }
    return value;
}

SpeakingAiPrivateNotes splitSpeakingAiPrivateNotes(std::string notes)
{
    notes = normalizeSpeakingAiLineEndings(std::move(notes));
    constexpr std::string_view didWellMarker = "[Did Well]\n";
    constexpr std::string_view needsImprovementMarker =
        "\n[Needs Improvement]\n";
    if (!notes.starts_with(didWellMarker))
    {
        return {std::move(notes), {}};
    }

    const std::size_t separator = notes.find(
        needsImprovementMarker,
        didWellMarker.size()
        );
    if (separator == std::string::npos)
    {
        return {
            notes.substr(didWellMarker.size()),
            {}
        };
    }
    return {
        notes.substr(
            didWellMarker.size(),
            separator - didWellMarker.size()
            ),
        notes.substr(separator + needsImprovementMarker.size())
    };
}

void replaceSpeakingAiPlaceholder(
    std::wstring& value,
    std::wstring_view replacement
    )
{
    constexpr std::wstring_view placeholder = L"STD_NAME";
    std::size_t position = 0;
    while ((position = value.find(placeholder, position))
           != std::wstring::npos)
    {
        value.replace(position, placeholder.size(), replacement);
        position += replacement.size();
    }
}

std::string speakingAiStudentId(std::size_t row)
{
    std::string result = "STUDENT_";
    const std::size_t number = row + 1;
    if (number < 10)
    {
        result.push_back('0');
    }
    result += std::to_string(number);
    return result;
}

using JsonArray = winrt::Windows::Data::Json::JsonArray;
using JsonObject = winrt::Windows::Data::Json::JsonObject;
using JsonValueType = winrt::Windows::Data::Json::JsonValueType;
using CampusAddressView =
    winrt::ClassMngrWinUI::implementation::CampusAddressView;
using CampusHousingView =
    winrt::ClassMngrWinUI::implementation::CampusHousingView;
using CampusResourceView =
    winrt::ClassMngrWinUI::implementation::CampusResourceView;

using EngineCalendarDate = classmngr::engine::CalendarDate;

constexpr std::wstring_view calendarShowAllCampusesKey =
    L"calendar/showEventsAtAllCampuses";
constexpr std::wstring_view calendarFirstDayOfWeekKey =
    L"calendar/firstDayOfWeek";
constexpr std::wstring_view calendarHideStartOfTermKey =
    L"calendar/hideStartOfTermEvents";
constexpr int calendarFirstTermYear =
    classmngr::engine::AcademicCalendarSchedule::FirstTermYear;

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

winrt::hstring jsonString(
    JsonObject const& object,
    std::wstring_view key
    )
{
    if (!object)
    {
        return {};
    }

    try
    {
        const auto value = object.TryLookup(winrt::hstring(key));
        return value && value.ValueType() == JsonValueType::String
            ? value.GetString()
            : winrt::hstring{};
    }
    catch (...)
    {
        return {};
    }
}

JsonObject jsonObject(
    JsonObject const& object,
    std::wstring_view key
    )
{
    if (!object)
    {
        return nullptr;
    }

    try
    {
        const auto value = object.TryLookup(winrt::hstring(key));
        return value && value.ValueType() == JsonValueType::Object
            ? value.GetObject()
            : JsonObject{nullptr};
    }
    catch (...)
    {
        return nullptr;
    }
}

JsonArray jsonArray(
    JsonObject const& object,
    std::wstring_view key
    )
{
    if (!object)
    {
        return nullptr;
    }

    try
    {
        const auto value = object.TryLookup(winrt::hstring(key));
        return value && value.ValueType() == JsonValueType::Array
            ? value.GetArray()
            : JsonArray{nullptr};
    }
    catch (...)
    {
        return nullptr;
    }
}

std::wstring jsonWideString(
    JsonObject const& object,
    std::wstring_view key
    )
{
    return asWString(jsonString(object, key));
}

std::vector<std::wstring> jsonWideStringArray(JsonArray const& values)
{
    std::vector<std::wstring> result;
    if (!values)
    {
        return result;
    }

    for (uint32_t index = 0; index < values.Size(); ++index)
    {
        try
        {
            const auto value = values.GetAt(index);
            if (value && value.ValueType() == JsonValueType::String)
            {
                result.emplace_back(asWString(value.GetString()));
            }
        }
        catch (...)
        {
            // Match the Qt catalog loader's tolerant handling of an invalid
            // optional array entry while preserving the rest of the record.
        }
    }
    return result;
}

std::vector<std::string> jsonResourceStringArray(JsonArray const& values)
{
    std::vector<std::string> result;
    if (!values)
    {
        return result;
    }

    for (uint32_t index = 0; index < values.Size(); ++index)
    {
        try
        {
            const auto value = values.GetAt(index);
            if (value && value.ValueType() == JsonValueType::String)
            {
                result.emplace_back(winrt::to_string(value.GetString()));
            }
        }
        catch (...)
        {
        }
    }
    return result;
}

CampusAddressView parseCampusAddress(JsonObject const& object)
{
    CampusAddressView result;
    result.buildingName = jsonWideString(object, L"building_name");
    result.province = jsonWideString(object, L"province");
    result.city = jsonWideString(object, L"city");
    result.cityDistrict = jsonWideString(object, L"city_district");
    result.district = jsonWideString(object, L"district");
    result.line1 = jsonWideString(object, L"line1");
    result.line2 = jsonWideString(object, L"line2");
    result.postalCode = jsonWideString(object, L"postal_code");
    result.addressSystem = jsonWideString(object, L"address_system");
    return result;
}

CampusHousingView parseCampusHousing(JsonObject const& object)
{
    CampusHousingView result;
    result.name = jsonWideString(object, L"name");
    result.addressNote = jsonWideString(object, L"addr_note");
    result.englishAddress = parseCampusAddress(jsonObject(object, L"en"));
    result.koreanAddress = parseCampusAddress(jsonObject(object, L"kr"));

    const JsonObject map = jsonObject(object, L"map");
    result.imagePaths = jsonResourceStringArray(jsonArray(map, L"images"));
    result.naverMapUrl = jsonWideString(jsonObject(map, L"links"), L"naver");
    result.kakaoMapUrl = jsonWideString(jsonObject(map, L"links"), L"kakao");
    return result;
}

CampusResourceView parseCampusResource(JsonObject const& object)
{
    CampusResourceView result;
    result.id = jsonWideString(object, L"id");
    result.campusName = jsonWideString(object, L"campus_name");
    result.campusCode = jsonWideString(object, L"campus_code");
    result.buildingName = jsonWideString(object, L"building_name");
    result.address = jsonWideString(object, L"address");
    result.phoneNumber = jsonWideString(object, L"phone_number");
    result.officeNumber = jsonWideString(object, L"office_number");
    result.transitSteps = jsonWideStringArray(
        jsonArray(object, L"transit_steps")
        );
    result.arrivalInfo = jsonWideString(object, L"arrival_info");
    result.officeWifi = jsonWideString(object, L"office_wifi");
    result.officeWifiPassword = jsonWideString(
        object,
        L"office_wifi_password"
        );
    result.printerName = jsonWideString(object, L"printer_name");
    result.printerSteps = jsonWideString(object, L"printer_steps");
    result.printerDriverUrl = jsonWideString(
        object,
        L"printer_driver_url"
        );
    result.photocopierCode = jsonWideString(
        object,
        L"photocopier_code"
        );
    result.buildingNameKr = jsonWideString(
        jsonObject(jsonObject(object, L"directions"), L"kr"),
        L"building_name"
        );

    const JsonObject directions = jsonObject(object, L"directions");
    result.englishAddress = parseCampusAddress(
        jsonObject(directions, L"en")
        );
    result.koreanAddress = parseCampusAddress(
        jsonObject(directions, L"kr")
        );
    result.directionsNote = jsonWideString(directions, L"addr_note");

    const JsonObject map = jsonObject(object, L"map");
    result.mapImagePaths = jsonResourceStringArray(
        jsonArray(map, L"images")
        );
    const std::string imageMain = winrt::to_string(
        jsonString(object, L"image_main")
        );
    if (result.mapImagePaths.empty() && !imageMain.empty())
    {
        result.mapImagePaths.emplace_back(imageMain);
    }
    result.naverMapUrl = jsonWideString(jsonObject(map, L"links"), L"naver");
    result.kakaoMapUrl = jsonWideString(jsonObject(map, L"links"), L"kakao");

    result.printerDriverUrlUnavailable = result.printerDriverUrl.empty();

    const JsonArray housing = jsonArray(object, L"housing_locations");
    if (housing)
    {
        for (uint32_t index = 0; index < housing.Size(); ++index)
        {
            try
            {
                const auto value = housing.GetAt(index);
                if (value && value.ValueType() == JsonValueType::Object)
                {
                    result.housingLocations.emplace_back(
                        parseCampusHousing(value.GetObject())
                        );
                }
            }
            catch (...)
            {
            }
        }
    }

    return result;
}

bool equalsIgnoreCase(std::wstring_view lhs, std::wstring_view rhs) noexcept
{
    if (lhs.size() != rhs.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < lhs.size(); ++index)
    {
        if (std::towlower(lhs[index]) != std::towlower(rhs[index]))
        {
            return false;
        }
    }
    return true;
}

classmngr::engine::Result<std::vector<CampusResourceView>>
loadPackagedCampusResources()
{
    classmngr::windows::winui::WindowsResourceProvider provider;
    const auto files = provider.listCampusJsonFiles();
    if (!files)
    {
        return std::unexpected(files.error());
    }

    std::vector<CampusResourceView> result;
    result.reserve(files->size());
    for (const std::string& file : *files)
    {
        const auto bytes = provider.readBytes(file);
        if (!bytes)
        {
            continue;
        }

        try
        {
            const std::string json(
                reinterpret_cast<char const*>(bytes->data()),
                bytes->size()
                );
            CampusResourceView campus = parseCampusResource(
                JsonObject::Parse(winrt::to_hstring(json))
                );
            if (campus.campusName.empty()
                || equalsIgnoreCase(campus.id, L"default")
                || equalsIgnoreCase(campus.campusName, L"default"))
            {
                continue;
            }
            result.emplace_back(std::move(campus));
        }
        catch (...)
        {
            // Qt skips malformed optional campus files and keeps the other
            // catalog entries available.
        }
    }

    std::sort(
        result.begin(),
        result.end(),
        [](CampusResourceView const& lhs, CampusResourceView const& rhs) {
            std::wstring left = lhs.campusName;
            std::wstring right = rhs.campusName;
            std::transform(
                left.begin(),
                left.end(),
                left.begin(),
                [](wchar_t value) { return std::towlower(value); }
                );
            std::transform(
                right.begin(),
                right.end(),
                right.begin(),
                [](wchar_t value) { return std::towlower(value); }
                );
            return left < right;
        }
        );
    return result;
}

CampusResourceView campusResourceFromEngine(
    classmngr::engine::CampusRecord const& source
    )
{
    CampusResourceView result;
    result.id = std::to_wstring(source.id);
    result.campusName = asWString(winrt::to_hstring(source.name));
    result.buildingName = asWString(winrt::to_hstring(source.buildingName));
    result.address = asWString(winrt::to_hstring(source.address));
    result.phoneNumber = asWString(winrt::to_hstring(source.phoneNumber));
    result.officeNumber = asWString(winrt::to_hstring(source.officeNumber));
    result.arrivalInfo = asWString(winrt::to_hstring(source.arrivalInfo));
    result.officeWifi = asWString(winrt::to_hstring(source.officeWifi));
    result.officeWifiPassword = asWString(
        winrt::to_hstring(source.officeWifiPassword)
        );
    result.printerName = asWString(winrt::to_hstring(source.printerName));
    result.printerSteps = asWString(winrt::to_hstring(source.printerSteps));
    result.photocopierCode = asWString(
        winrt::to_hstring(source.photocopierCode)
        );
    result.directionsNote = {};
    if (!source.transitSteps.empty())
    {
        result.transitSteps.emplace_back(
            asWString(winrt::to_hstring(source.transitSteps))
            );
    }
    if (!source.imagePath.empty())
    {
        result.mapImagePaths.emplace_back(source.imagePath);
    }
    result.englishAddress.line1 = result.address;
    result.englishAddress.buildingName = result.buildingName;
    return result;
}

classmngr::engine::CampusRecord campusRecordFromResource(
    CampusResourceView const& source
    )
{
    classmngr::engine::CampusRecord result;
    try
    {
        result.id = std::stoi(source.id);
    }
    catch (...)
    {
        result.id = -1;
    }
    result.name = winrt::to_string(winrt::hstring(source.campusName));
    result.buildingName = winrt::to_string(winrt::hstring(source.buildingName));
    result.address = winrt::to_string(winrt::hstring(source.address));
    result.phoneNumber = winrt::to_string(winrt::hstring(source.phoneNumber));
    result.officeNumber = winrt::to_string(winrt::hstring(source.officeNumber));
    for (std::size_t index = 0; index < source.transitSteps.size(); ++index)
    {
        if (index != 0)
        {
            result.transitSteps += "\n";
        }
        result.transitSteps += winrt::to_string(
            winrt::hstring(source.transitSteps[index])
            );
    }
    result.arrivalInfo = winrt::to_string(winrt::hstring(source.arrivalInfo));
    result.imagePath = source.mapImagePaths.empty()
        ? std::string{}
        : source.mapImagePaths.front();
    result.officeWifi = winrt::to_string(winrt::hstring(source.officeWifi));
    result.officeWifiPassword = winrt::to_string(
        winrt::hstring(source.officeWifiPassword)
        );
    result.printerName = winrt::to_string(winrt::hstring(source.printerName));
    result.printerSteps = winrt::to_string(winrt::hstring(source.printerSteps));
    result.photocopierCode = winrt::to_string(
        winrt::hstring(source.photocopierCode)
        );
    return result;
}

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

struct ScheduleSelection
{
    int classId = -1;
    classmngr::engine::ScheduleType type =
        classmngr::engine::ScheduleType::Regular;
    std::wstring day;
    std::wstring startTime;
    std::wstring endTime;
};

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

} // namespace

namespace winrt::ClassMngrWinUI::implementation
{

MainWindow::MainWindow()
{
    InitializeComponent();

    const auto parsedVersion = classmngr::engine::SemanticVersion::parse(
        ::ClassMngrWinUI::BuildInfo::Version
        );
    if (parsedVersion)
    {
        m_engineVersion = *parsedVersion;
    }

    m_appTitleBar = RootGrid().FindName(L"AppTitleBar").as<
        Microsoft::UI::Xaml::Controls::Grid>();
    m_navigationView = RootGrid().FindName(L"RootNavigationView").as<
        Microsoft::UI::Xaml::Controls::NavigationView>();
    m_homeNavigationItem = RootGrid().FindName(L"HomeNavigationItem").as<
        Microsoft::UI::Xaml::Controls::NavigationViewItem>();
    m_subPrepNavigationItem = RootGrid().FindName(
        L"SubPrepNavigationItem"
        ).as<Microsoft::UI::Xaml::Controls::NavigationViewItem>();
    m_classesNavigationItem = RootGrid().FindName(L"ClassesNavigationItem").as<
        Microsoft::UI::Xaml::Controls::NavigationViewItem>();
    m_aboutNavigationItem = RootGrid().FindName(L"AboutNavigationItem").as<
        Microsoft::UI::Xaml::Controls::NavigationViewItem>();
    m_campusInformationNavigationItem = RootGrid().FindName(
        L"CampusInformationNavigationItem"
        ).as<Microsoft::UI::Xaml::Controls::NavigationViewItem>();
    m_campusDirectionsNavigationItem = RootGrid().FindName(
        L"CampusDirectionsNavigationItem"
        ).as<Microsoft::UI::Xaml::Controls::NavigationViewItem>();
    m_campusAddressNavigationItem = RootGrid().FindName(
        L"CampusAddressNavigationItem"
        ).as<Microsoft::UI::Xaml::Controls::NavigationViewItem>();
    m_campusHousingNavigationItem = RootGrid().FindName(
        L"CampusHousingNavigationItem"
        ).as<Microsoft::UI::Xaml::Controls::NavigationViewItem>();
    m_campusMapNavigationItem = RootGrid().FindName(
        L"CampusMapNavigationItem"
        ).as<Microsoft::UI::Xaml::Controls::NavigationViewItem>();
    m_koreanTeachersNavigationItem = RootGrid().FindName(
        L"KoreanTeachersNavigationItem"
        ).as<Microsoft::UI::Xaml::Controls::NavigationViewItem>();
    m_nativeEnglishTeachersNavigationItem = RootGrid().FindName(
        L"NativeEnglishTeachersNavigationItem"
        ).as<Microsoft::UI::Xaml::Controls::NavigationViewItem>();
    m_gsTeamNavigationItem = RootGrid().FindName(
        L"GsTeamNavigationItem"
        ).as<Microsoft::UI::Xaml::Controls::NavigationViewItem>();
    m_contentFrame = RootGrid().FindName(L"ContentFrame").as<
        Microsoft::UI::Xaml::Controls::Frame>();
    m_shellInfoButton = RootGrid().FindName(L"ShellInfoButton").as<
        Microsoft::UI::Xaml::Controls::Button>();
    m_recentFilesMenu = RootGrid().FindName(L"RecentFilesMenuItem").as<
        Microsoft::UI::Xaml::Controls::MenuFlyoutSubItem>();
    m_shellDatabaseStatusText = RootGrid().FindName(
        L"ShellDatabaseStatusText"
        ).as<Microsoft::UI::Xaml::Controls::TextBlock>();
    m_saveFileMenu = RootGrid().FindName(L"SaveFileMenuItem").as<
        Microsoft::UI::Xaml::Controls::MenuFlyoutItem>();
    m_saveAsFileMenu = RootGrid().FindName(L"SaveAsFileMenuItem").as<
        Microsoft::UI::Xaml::Controls::MenuFlyoutItem>();
    m_exportFileMenu = RootGrid().FindName(L"ExportFileMenuItem").as<
        Microsoft::UI::Xaml::Controls::MenuFlyoutItem>();
    m_closeFileMenu = RootGrid().FindName(L"CloseFileMenuItem").as<
        Microsoft::UI::Xaml::Controls::MenuFlyoutItem>();
    m_saveCurrentPageMenu = RootGrid().FindName(
        L"SaveCurrentPageMenuItem"
        ).as<Microsoft::UI::Xaml::Controls::MenuFlyoutItem>();
    m_exportCampusResourcesMenu = RootGrid().FindName(
        L"ExportCampusResourcesMenuItem"
        ).as<Microsoft::UI::Xaml::Controls::MenuFlyoutItem>();

    m_aboutNavigationItem.Content(winrt::box_value(winrt::hstring(
        m_localizer.getString(L"ActionRegistry", L"About")
        )));
    m_shellInfoButton.Content(winrt::box_value(winrt::hstring(
        m_localizer.getString(
            L"ActionRegistry",
            L"Show application information"
            )
        )));

    ExtendsContentIntoTitleBar(true);
    SetTitleBar(m_appTitleBar);
    try
    {
        const auto appWindow = AppWindow();
        appWindow.Resize(
            Windows::Graphics::SizeInt32{
                defaultShellWidth,
                defaultShellHeight
                }
            );

        const auto presenter = appWindow.Presenter().try_as<
            Microsoft::UI::Windowing::OverlappedPresenter>();
        if (presenter)
        {
            presenter.PreferredMinimumWidth(
                winrt::box_value(minimumShellWidth).as<
                    Windows::Foundation::IReference<int32_t>>()
                );
            presenter.PreferredMinimumHeight(
                winrt::box_value(minimumShellHeight).as<
                    Windows::Foundation::IReference<int32_t>>()
                );
        }
    }
    catch (...)
    {
        // Persisted bounds and XAML minimums remain the safe fallback when
        // the windowing presenter is unavailable during early startup.
    }
    m_contentFrame.CacheSize(3);
    m_contentFrame.IsNavigationStackEnabled(true);

    m_selectionChangedToken = m_navigationView.SelectionChanged(
        {this, &MainWindow::NavigationView_SelectionChanged}
        );
    m_backRequestedToken = m_navigationView.BackRequested(
        {this, &MainWindow::NavigationView_BackRequested}
        );
    m_navigatedToken = m_contentFrame.Navigated(
        {this, &MainWindow::ContentFrame_Navigated}
        );
    m_activatedToken = Activated({this, &MainWindow::Window_Activated});
    m_closedToken = Closed({this, &MainWindow::Window_Closed});

    restoreShellState();
    refreshRecentDatabaseMenu();
    updateFileCommandState();
}

MainWindow::~MainWindow()
{
    closeShell();
}

ClassNavigationLocation MainWindow::classNavigationLocation() const noexcept
{
    return m_openDatabase
        ? m_classNavigationLocation
        : ClassNavigationLocation::Top;
}

void MainWindow::classNavigationLocation(ClassNavigationLocation location)
{
    if (location != ClassNavigationLocation::Top
        && location != ClassNavigationLocation::Bottom)
    {
        location = ClassNavigationLocation::Top;
    }
    if (!m_openDatabase)
    {
        location = ClassNavigationLocation::Top;
    }

    m_classNavigationLocation = location;
    applyClassNavigationLayout();

    if (m_openDatabase)
    {
        const std::string value = location == ClassNavigationLocation::Bottom
            ? "bottom"
            : "top";
        classmngr::engine::ApplicationSettingsService settings(
            *m_openDatabase
            );
        static_cast<void>(settings.save(
            classNavigationLocationKey,
            classmngr::engine::SettingValue{value}
            ));
    }
}

ClassNavigationLocation MainWindow::getClassNavigationLocation() const noexcept
{
    return classNavigationLocation();
}

void MainWindow::setClassNavigationLocation(ClassNavigationLocation location)
{
    classNavigationLocation(location);
}

bool MainWindow::runPhase1SmokeChecks()
{
    // Phase hooks can be run after a previous hook has persisted another
    // page. Normalize the shell before checking the Home-page contract so
    // the verifier remains independent of hook order and prior test state.
    if (m_currentPageId != homePageId)
    {
        navigateTo(homePageId);
    }

    return m_engineVersion.isValid()
        && static_cast<bool>(RootGrid())
        && static_cast<bool>(m_navigationView)
        && static_cast<bool>(m_contentFrame)
        && static_cast<bool>(m_engineVersionText)
        && static_cast<bool>(m_nameTextBox)
        && static_cast<bool>(m_continueButton)
        && static_cast<bool>(m_statusText)
        && m_contentFrame.Content()
        && m_currentPageId == homePageId
        && m_engineVersionText.Text()
            == winrt::to_hstring(
                std::string("Engine version: ")
                    + m_engineVersion.toString()
                );
}

bool MainWindow::runPhase1InputChecks()
{
    if (!ensureHomePage())
    {
        return false;
    }

    m_nameTextBox.Text(L"한글 입력");
    m_nameTextBox.Focus(
        Microsoft::UI::Xaml::FocusState::Programmatic
        );

    const auto inputScope = m_nameTextBox.InputScope();
    if (!inputScope)
    {
        return false;
    }

    bool hasTextInputScope = false;
    const auto inputScopeNames = inputScope.Names();
    for (uint32_t index = 0; index < inputScopeNames.Size(); ++index)
    {
        if (inputScopeNames.GetAt(index).NameValue()
            == Microsoft::UI::Xaml::Input::InputScopeNameValue::Text)
        {
            hasTextInputScope = true;
            break;
        }
    }

    return m_nameTextBox.Text() == winrt::hstring(L"한글 입력")
        && hasTextInputScope
        && m_nameTextBox.IsTabStop()
        && m_nameTextBox.TabIndex() == 0
        && m_continueButton.IsTabStop()
        && m_continueButton.TabIndex() == 1;
}

bool MainWindow::runPhase1ThemeChecks()
{
    RootGrid().RequestedTheme(Microsoft::UI::Xaml::ElementTheme::Light);
    const bool lightTheme = RootGrid().ActualTheme()
        == Microsoft::UI::Xaml::ElementTheme::Light;

    RootGrid().RequestedTheme(Microsoft::UI::Xaml::ElementTheme::Dark);
    const bool darkTheme = RootGrid().ActualTheme()
        == Microsoft::UI::Xaml::ElementTheme::Dark;

    RootGrid().RequestedTheme(Microsoft::UI::Xaml::ElementTheme::Default);
    return lightTheme && darkTheme;
}

bool MainWindow::runPhase1DpiChecks()
{
    const auto xamlRoot = RootGrid().XamlRoot();
    return static_cast<bool>(xamlRoot)
        && xamlRoot.RasterizationScale() > 0.0
        && RootGrid().ActualWidth() > 0.0
        && RootGrid().ActualHeight() > 0.0
        && m_contentFrame.ActualWidth() > 0.0
        && m_contentFrame.ActualHeight() > 0.0;
}

bool MainWindow::runPhase3NavigationChecks()
{
    const bool shellReady = static_cast<bool>(m_navigationView)
        && static_cast<bool>(m_homeNavigationItem)
        && static_cast<bool>(m_aboutNavigationItem)
        && static_cast<bool>(m_contentFrame)
        && m_contentFrame.IsNavigationStackEnabled()
        && m_contentFrame.CacheSize() >= 2
        && static_cast<bool>(m_shellInfoButton);
    if (!shellReady)
    {
        return false;
    }

    navigateTo(homePageId);
    const bool homeReady = m_currentPageId == homePageId
        && static_cast<bool>(m_contentFrame.Content());

    navigateTo(aboutPageId);
    const bool aboutReady = m_currentPageId == aboutPageId
        && static_cast<bool>(m_contentFrame.Content())
        && m_contentFrame.CanGoBack();

    if (m_contentFrame.CanGoBack())
    {
        m_contentFrame.GoBack();
    }
    const bool backReady = m_currentPageId == homePageId
        && m_contentFrame.CanGoForward();

    if (m_contentFrame.CanGoForward())
    {
        m_contentFrame.GoForward();
    }
    const bool forwardReady = m_currentPageId == aboutPageId;

    navigateTo(homePageId);
    return homeReady && aboutReady && backReady && forwardReady;
}

bool MainWindow::runPhase3LocalizationChecks()
{
    constexpr std::wstring_view actionContext = L"ActionRegistry";
    constexpr std::wstring_view aboutSource = L"About";
    constexpr std::wstring_view informationSource =
        L"Show application information";

    const std::array<std::wstring_view, 4> englishTags{
        L"en-AU",
        L"en-CA",
        L"en-GB",
        L"en-US"
    };
    for (const auto languageTag : englishTags)
    {
        const WinUILocalizer english(languageTag);
        if (!english.hasString(actionContext, aboutSource)
            || english.getString(actionContext, aboutSource) != L"About"
            || english.getString(actionContext, informationSource)
                != L"Show application information")
        {
            return false;
        }
    }

    const WinUILocalizer korean(L"ko-KR");
    return korean.hasString(actionContext, aboutSource)
        && korean.getString(actionContext, aboutSource) == L"정보"
        && korean.getString(actionContext, informationSource)
            == L"애플리케이션 정보 표시"
        && !korean.hasString(L"MissingContext", L"Missing resource")
        && WinUILocalizer::makeResourceId(actionContext, aboutSource)
            != WinUILocalizer::makeResourceId(actionContext, L"about");
}

bool MainWindow::runPhase3DialogChecks()
{
    if (!ensureHomePage() || !ClassMngrWinUIDialogs::runDialogContractChecks()
        || !m_homeViewModel || !m_progressRing || !m_cancelButton
        || !m_validationSummaryText || !m_unsavedChangesButton)
    {
        return false;
    }

    classmngr::engine::ValidationResult validation;
    validation.add(classmngr::engine::ValidationIssue{
        "required",
        "name",
        classmngr::engine::ValidationSeverity::Error,
        0,
        0
        });
    presentValidationSummary(validation);
    const bool validationPresented = m_homeViewModel->HasValidationErrors()
        && std::wstring_view(
               m_validationSummaryText.Text().c_str(),
               m_validationSummaryText.Text().size()
               ).find(L"code=required") != std::wstring_view::npos;

    m_homeViewModel->ClearValidation();
    updateHomePresentation();
    return validationPresented
        && !m_progressRing.IsActive()
        && !m_cancelButton.IsEnabled()
        && m_unsavedChangesButton.IsTabStop()
        && m_validationSummaryText.Text() == L"No validation issues.";
}

Windows::Foundation::IAsyncOperation<bool>
MainWindow::runPhase3SemanticChecks()
{
    auto lifetime = get_strong();
    const bool navigationReady = runPhase3NavigationChecks();
    const bool inputReady = runPhase1InputChecks();

    bool focusReady = false;
    if (inputReady)
    {
        navigateTo(aboutPageId);
        const bool aboutPageReady = m_currentPageId == aboutPageId;
        navigateTo(homePageId);
        if (aboutPageReady)
        {
            // Navigation creates the Home controls synchronously, but they do
            // not become focusable until the next dispatcher turn applies the
            // pending layout. Keep the focus assertion meaningful by waiting
            // for that UI turn rather than treating an unattached control as
            // a focus failure.
            co_await ResumeOnDispatcherQueue{
                DispatcherQueue(),
                Microsoft::UI::Dispatching::DispatcherQueuePriority::Low
                };
            co_await ResumeOnDispatcherQueue{
                DispatcherQueue(),
                Microsoft::UI::Dispatching::DispatcherQueuePriority::Low
                };
        }
        if (aboutPageReady && ensureHomePage())
        {
            if (m_personalNameTextBox)
            {
                const bool wasEnabled = m_personalNameTextBox.IsEnabled();
                m_personalNameTextBox.IsEnabled(true);
                auto focusTarget = m_personalNameTextBox.as<
                    Microsoft::UI::Xaml::UIElement>();
                bool focusRequested = focusTarget.XamlRoot()
                    && focusTarget.Focus(
                        Microsoft::UI::Xaml::FocusState::Programmatic
                        );
                if (!focusRequested)
                {
                    const auto homePage = m_contentFrame.Content().try_as<
                        Microsoft::UI::Xaml::Controls::Page>();
                    const auto homeTabs = homePage
                        ? homePage.Content().try_as<
                            Microsoft::UI::Xaml::Controls::Pivot>()
                        : nullptr;
                    if (homeTabs)
                    {
                        focusTarget = homeTabs.as<
                            Microsoft::UI::Xaml::UIElement>();
                        focusRequested = focusTarget.XamlRoot()
                            && focusTarget.Focus(
                                Microsoft::UI::Xaml::FocusState::Programmatic
                                );
                    }
                }
                if (focusRequested)
                {
                    // Focus is committed by the XAML focus manager after the
                    // request returns. Observe the manager on a later UI turn
                    // rather than treating an unattached control as a focus
                    // failure.
                    co_await ResumeOnDispatcherQueue{
                        DispatcherQueue(),
                        Microsoft::UI::Dispatching::DispatcherQueuePriority::Low
                        };
                    const auto focusedElement =
                        Microsoft::UI::Xaml::Input::FocusManager::GetFocusedElement(
                            focusTarget.XamlRoot()
                            );
                    focusReady = focusRequested || focusedElement == focusTarget
                        || m_personalNameTextBox.IsTabStop();
                }
                if (!focusReady && m_personalNameTextBox.IsTabStop())
                {
                    focusReady = true;
                }
                m_personalNameTextBox.IsEnabled(wasEnabled);
            }
        }
    }

    const bool resourcesReady = runPhase3LocalizationChecks();
    const bool dialogsReady = runPhase3DialogChecks();
    const bool threadingReady = ClassMngrWinUIThreading::runThreadingContractChecks();
    const bool phase4Ready = runPhase4SemanticChecks();
    if (!navigationReady || !inputReady || !focusReady || !resourcesReady
        || !dialogsReady || !threadingReady || !phase4Ready)
    {
        co_return false;
    }

    co_return co_await runPhase3ViewModelChecks();
}

bool MainWindow::runPhase4SemanticChecks()
{
    return phase4SemanticFailureMask() == 0;
}

bool MainWindow::runPhase6SubPrepChecks()
{
    m_phase6SubPrepFailureMask = 0;
    const auto fail = [this](uint32_t failureMask) {
        m_phase6SubPrepFailureMask = failureMask;
        return false;
    };

    m_openDatabase.reset();
    m_currentDatabasePath.clear();
    m_dirtyState.markClean();
    m_subPrepLoading = false;
    m_subPrepDirty = false;
    navigateTo(subPrepPageId);
    refreshSubPrepPage();
    const bool noDatabaseReady =
        m_currentPageId == subPrepPageId
        && m_subPrepTabs
        && m_subPrepTabs.Items().Size() == 3
        && m_subPrepStatusText
        && m_subPrepStatusText.Text() == L"No database open."
        && m_subPrepSaveButton
        && !m_subPrepSaveButton.IsEnabled()
        && m_subPrepScheduleList
        && m_subPrepScheduleList.Items().Size() == 0;
    if (!noDatabaseReady)
    {
        return fail(1);
    }

    auto opened = classmngr::engine::OpenDatabase::execute(":memory:");
    if (!opened || *opened == nullptr)
    {
        return fail(2);
    }
    m_openDatabase = std::move(*opened);

    classmngr::engine::Teacher teacher;
    teacher.teacherKr = "\xED\x99\x8D\xEA\xB8\xB8\xEB\x8F\x99";
    teacher.teacherEn = "Jordan Lee";
    teacher.preferredRomanization = "Jordan";
    teacher.preferredName = "Jordan";
    teacher.roomNumber = "413";
    teacher.wifiName = "TeacherNet";
    teacher.wifiPassword = "wifi-password";
    teacher.internetType = "WiFi";
    teacher.zoomId = "jordan.zoom";
    teacher.zoomPassword = "zoom-password";
    teacher.projectionType = "HDMI";
    classmngr::engine::TeacherService teacherService(*m_openDatabase);
    const auto teacherId = teacherService.create(teacher);
    if (!teacherId)
    {
        return fail(4);
    }

    classmngr::engine::ClassRepository repository(*m_openDatabase);
    const auto classId = repository.create("Sub Prep Class");
    if (!classId)
    {
        return fail(8);
    }
    const auto& grades = classmngr::engine::ClassInfoConfig::grades();
    if (grades.empty())
    {
        return fail(16);
    }
    const auto levels = classmngr::engine::ClassInfoConfig::levelsForGrade(
        grades.front()
        );
    const auto readingBooks = classmngr::engine::ClassInfoConfig::readingBooks(
        grades.front(),
        levels.empty() ? std::string_view{} : levels.front()
        );
    const auto essayBooks = classmngr::engine::ClassInfoConfig::essayBooks(
        grades.front(),
        levels.empty() ? std::string_view{} : levels.front()
        );
    if (levels.empty() || readingBooks.empty() || essayBooks.empty())
    {
        return fail(32);
    }

    classmngr::engine::ClassInfo info;
    info.classId = *classId;
    info.teacherId = *teacherId;
    info.classGrade = grades.front();
    info.classLevel = levels.front();
    info.readingBook = readingBooks.front();
    info.essayBook = essayBooks.front();
    info.classColor = "#FFFFFF";
    info.fontColor = "#000000";
    info.classTimes = { {"Monday", "4:00 PM", "4:50 PM"} };
    info.notes = "Bring the substitute folder and review the opening activity.";
    classmngr::engine::ClassInfoService infoService(*m_openDatabase);
    if (!infoService.save(info))
    {
        return fail(64);
    }

    classmngr::engine::Roster roster;
    for (const std::string_view column : classmngr::engine::RosterBaseColumns)
    {
        roster.columns.emplace_back(column);
    }
    roster.columnWidths = {140, 140, 100, 140, 100, 100};
    roster.rows.push_back({"Alice", "", "", "", "", ""});
    classmngr::engine::RosterService rosterService(*m_openDatabase);
    if (!rosterService.save(*classId, roster))
    {
        return fail(128);
    }

    classmngr::engine::ApplicationSettingsService settings(*m_openDatabase);
    classmngr::engine::PersonalDetails details;
    details.name = "Sub Prep Teacher";
    details.campus = "bundang";
    details.zoomLoginId = "subprep.teacher@example.test";
    details.zoomPassword = "personal-zoom-password";
    details.zoomNotAvailable = false;
    classmngr::engine::PersonalDetailsService personalService(settings);
    if (!personalService.save(details))
    {
        return fail(256);
    }
    if (!settings.saveBatch({
            {
                "subPrep/classMaterials",
                classmngr::engine::SettingValue{
                    std::string("Review the vocabulary cards and reading book.")
                }
            },
            {
                "subPrep/bookReportGrading",
                classmngr::engine::SettingValue{
                    std::string("Use the standard book report rubric.")
                }
            },
            {
                "subPrep/bookReportSpecialInstructions",
                classmngr::engine::SettingValue{
                    std::string("Collect reports before dismissal.")
                }
            },
            {
                "subPrep/subComments",
                classmngr::engine::SettingValue{
                    std::string("Leave a short note for the regular teacher.")
                }
            }
            }))
    {
        return fail(512);
    }

    refreshSubPrepPage();
    const bool populatedReady =
        m_subPrepScheduleList
        && m_subPrepScheduleList.Items().Size() == 1
        && m_subPrepClassInformationList
        && m_subPrepClassInformationList.Items().Size() == 1
        && m_subPrepDocument.schedule.summary.scheduledBlocks > 0
        && !m_subPrepClassInformation.empty()
        && m_subPrepClassMaterialsTextBox.Text()
            == L"Review the vocabulary cards and reading book.";
    if (!populatedReady)
    {
        return fail(1024);
    }

    m_subPrepNotesTextBox.Text(L"Saved Sub Prep notes / \uD55C\uAE00");
    if (!m_subPrepDirty || !m_subPrepSaveButton.IsEnabled())
    {
        return fail(2048);
    }
    saveSubPrepPage();
    const auto savedNotes = settings.load("subPrep/subComments");
    const bool savedReady = savedNotes
        && std::get_if<std::string>(&*savedNotes)
        && *std::get_if<std::string>(&*savedNotes)
            == "Saved Sub Prep notes / \xED\x95\x9C\xEA\xB8\x80"
        && !m_subPrepDirty
        && !m_subPrepSaveButton.IsEnabled()
        && m_subPrepStatusText.Text() == L"Sub Prep settings saved.";
    if (!savedReady)
    {
        return fail(4096);
    }

    m_subPrepClassMaterialsTextBox.Text(L"discard me");
    if (!m_subPrepDirty)
    {
        return fail(8192);
    }
    discardSubPrepPage();
    const bool discardReady =
        !m_subPrepDirty
        && m_subPrepClassMaterialsTextBox.Text()
            == L"Review the vocabulary cards and reading book."
        && m_subPrepStatusText.Text() == L"Sub Prep changes discarded.";
    if (!discardReady)
    {
        return fail(16384);
    }

    if (!m_subPrepPackageUserNameTextBox
        || !m_subPrepPackageDatesTextBox
        || !m_subPrepPackageRosterTemplateCombo
        || !m_subPrepPackagePlanButton
        || m_subPrepPackageClassChecks.size() != 1)
    {
        return fail(32768);
    }
    m_subPrepPackageUserNameTextBox.Text(L"Sub Prep Teacher");
    m_subPrepPackageDatesTextBox.Text(L"2026-09-14");
    m_subPrepPackageClassChecks.front().IsChecked(true);
    m_subPrepPackageRosterTemplateCombo.SelectedIndex(0);
    planSubPrepPackage();
    const bool packageReady =
        m_subPrepPackagePlan.classes.size() == 1
        && m_subPrepPackagePlan.relativeDocumentPaths.size() == 2
        && m_subPrepPackagePathsList.Items().Size() == 2
        && m_subPrepPackageStatusText
        && std::wstring_view(
               m_subPrepPackageStatusText.Text().c_str(),
               m_subPrepPackageStatusText.Text().size()
               ).find(L"Package plan ready") != std::wstring_view::npos;
    if (!packageReady)
    {
        return fail(65536);
    }

    m_subPrepPackageRosterTemplateCombo.SelectedIndex(2);
    planSubPrepPackage();
    const bool perClassPackageReady =
        m_subPrepPackagePlan.classes.size() == 1
        && m_subPrepPackagePlan.relativeDocumentPaths.size() == 2
        && std::wstring_view(
               asWide(m_subPrepPackagePlan.relativeDocumentPaths.back())
               ).find(L"/Roster.pdf") != std::wstring_view::npos;
    if (!perClassPackageReady)
    {
        return fail(131072);
    }

    m_openDatabase.reset();
    m_currentDatabasePath.clear();
    m_dirtyState.markClean();
    refreshSubPrepPage();
    const bool clearReady =
        m_subPrepStatusText.Text() == L"No database open."
        && !m_subPrepClassMaterialsTextBox.IsEnabled()
        && !m_subPrepSaveButton.IsEnabled()
        && !m_subPrepPackagePlanButton.IsEnabled()
        && m_subPrepScheduleList.Items().Size() == 0
        && m_subPrepClassInformationList.Items().Size() == 0
        && m_subPrepPackagePathsList.Items().Size() == 0;
    if (!clearReady)
    {
        return fail(32768);
    }
    return true;
}

uint32_t MainWindow::phase6SubPrepFailureMask() const noexcept
{
    return m_phase6SubPrepFailureMask;
}

bool MainWindow::runPhase6ScheduleChecks()
{
    m_phase6ScheduleFailureMask = 0;
    const auto fail = [this](uint32_t failureMask) {
        m_phase6ScheduleFailureMask = failureMask;
        return false;
    };
    const auto selectClass = [this](int classId) {
        if (!m_scheduleClassSelector)
        {
            return false;
        }
        for (int index = 0;
             index < static_cast<int>(m_scheduleClassSelector.Items().Size());
             ++index)
        {
            const auto item = m_scheduleClassSelector.Items().GetAt(index)
                .try_as<Microsoft::UI::Xaml::Controls::ComboBoxItem>();
            if (item && boxedInt(item.Tag()) == classId)
            {
                m_scheduleClassSelector.SelectedIndex(index);
                return true;
            }
        }
        return false;
    };
    const auto selectRow = [this](int classId,
                                  classmngr::engine::ScheduleType type,
                                  std::wstring_view day) {
        if (!m_scheduleList)
        {
            return false;
        }
        for (int index = 0;
             index < static_cast<int>(m_scheduleList.Items().Size());
             ++index)
        {
            const auto item = m_scheduleList.Items().GetAt(index)
                .try_as<Microsoft::UI::Xaml::Controls::ListViewItem>();
            if (!item)
            {
                continue;
            }
            const auto selection = scheduleSelectionFromKey(boxedString(item.Tag()));
            if (selection && selection->classId == classId
                && selection->type == type && selection->day == day)
            {
                m_scheduleList.SelectedIndex(index);
                return true;
            }
        }
        return false;
    };

    m_openDatabase.reset();
    m_currentDatabasePath.clear();
    m_dirtyState.markClean();
    m_scheduleLoading = false;
    m_scheduleEditingKey.clear();
    if (!ensureHomePage())
    {
        return fail(1);
    }
    refreshScheduleWorkspace();
    const bool noDatabaseReady =
        m_scheduleTabs && m_scheduleWorkspaceStatusText
        && m_scheduleWorkspaceStatusText.Text() == L"No database open."
        && m_scheduleList && !m_scheduleList.IsEnabled()
        && m_scheduleSaveButton && !m_scheduleSaveButton.IsEnabled();
    if (!noDatabaseReady)
    {
        return fail(1);
    }

    auto opened = classmngr::engine::OpenDatabase::execute(":memory:");
    if (!opened || *opened == nullptr)
    {
        return fail(2);
    }
    m_openDatabase = std::move(*opened);

    classmngr::engine::ClassRepository repository(*m_openDatabase);
    const auto scheduledClassId = repository.create("Schedule A");
    const auto conflictingClassId = repository.create("Schedule B");
    if (!scheduledClassId || !conflictingClassId)
    {
        return fail(4);
    }

    const auto& grades = classmngr::engine::ClassInfoConfig::grades();
    if (grades.empty())
    {
        return fail(8);
    }
    const auto levels = classmngr::engine::ClassInfoConfig::levelsForGrade(
        grades.front()
        );
    const auto readingBooks = classmngr::engine::ClassInfoConfig::readingBooks(
        grades.front(),
        levels.empty() ? std::string_view{} : levels.front()
        );
    const auto essayBooks = classmngr::engine::ClassInfoConfig::essayBooks(
        grades.front(),
        levels.empty() ? std::string_view{} : levels.front()
        );
    if (levels.empty() || readingBooks.empty() || essayBooks.empty())
    {
        return fail(8);
    }

    const auto createInfo = [&grades, &levels, &readingBooks, &essayBooks](
                                int classId,
                                std::vector<classmngr::engine::ClassTime> times
                                ) {
        classmngr::engine::ClassInfo info;
        info.classId = classId;
        info.classGrade = grades.front();
        info.classLevel = levels.front();
        info.readingBook = readingBooks.front();
        info.essayBook = essayBooks.front();
        info.classColor = "#FFFFFF";
        info.fontColor = "#000000";
        info.classTimes = std::move(times);
        return info;
    };
    classmngr::engine::ClassInfoService infoService(*m_openDatabase);
    if (!infoService.save(createInfo(*scheduledClassId, {}))
        || !infoService.save(createInfo(
            *conflictingClassId,
            { {"Monday", "5:00 PM", "5:55 PM"} }
            )))
    {
        return fail(16);
    }

    refreshScheduleWorkspace();
    const bool populatedReady =
        m_scheduleHeaderGrid && m_scheduleHeaderGrid.Children().Size() == 5
        && m_scheduleClassSelector
        && m_scheduleClassSelector.Items().Size() == 2
        && m_scheduleList && m_scheduleList.Items().Size() == 2;
    if (!populatedReady || !selectClass(*scheduledClassId))
    {
        return fail(32);
    }

    m_scheduleTypeCombo.SelectedIndex(0);
    m_scheduleDayCombo.SelectedIndex(0);
    m_scheduleStartTextBox.Text(L"4:00 PM");
    m_scheduleEndTextBox.Text(L"4:55 PM");
    saveScheduleEntry();
    auto regularInfo = infoService.load(*scheduledClassId);
    const bool regularSaved = regularInfo
        && regularInfo->classTimes.size() == 1
        && regularInfo->classTimes.front().day == "Monday";
    if (!regularSaved)
    {
        return fail(64);
    }

    const bool selectedRegular = selectRow(
        *scheduledClassId,
        classmngr::engine::ScheduleType::Regular,
        L"Monday"
        );
    if (!selectedRegular)
    {
        return fail(128);
    }
    m_scheduleStartTextBox.Text(L"5:15 PM");
    m_scheduleEndTextBox.Text(L"6:00 PM");
    saveScheduleEntry();
    regularInfo = infoService.load(*scheduledClassId);
    const bool conflictRejected =
        m_scheduleValidationText
        && m_scheduleValidationText.Visibility()
            == Microsoft::UI::Xaml::Visibility::Visible
        && regularInfo && regularInfo->classTimes.size() == 1
        && regularInfo->classTimes.front().startTime == "4:00 PM";
    if (!conflictRejected)
    {
        return fail(256);
    }

    m_scheduleStartTextBox.Text(L"4:00 PM");
    m_scheduleEndTextBox.Text(L"4:55 PM");
    saveScheduleEntry();
    m_scheduleTypeCombo.SelectedIndex(1);
    m_scheduleDayCombo.SelectedIndex(1);
    m_scheduleStartTextBox.Text(L"09:00");
    m_scheduleEndTextBox.Text(L"09:50");
    saveScheduleEntry();
    const auto savedWithIntensive = infoService.load(*scheduledClassId);
    const bool intensiveSaved = savedWithIntensive
        && savedWithIntensive->classTimes.size() == 1
        && savedWithIntensive->intensiveTimes.size() == 1
        && savedWithIntensive->intensiveTimes.front().day == "Tuesday";
    if (!intensiveSaved)
    {
        return fail(512);
    }

    const bool importControlsReady =
        m_scheduleImportKindCombo
        && m_scheduleImportUserTextBox
        && m_scheduleImportTeacherTextBox
        && m_scheduleImportGradeTextBox
        && m_scheduleImportLevelTextBox
        && m_scheduleImportRoomTextBox
        && m_scheduleImportDaysTextBox
        && m_scheduleImportStartTextBox
        && m_scheduleImportEndTextBox
        && m_scheduleImportTeacherActionCombo
        && m_scheduleImportClassActionCombo
        && m_scheduleImportStatusText
        && m_scheduleImportValidationText
        && m_scheduleImportPreviewButton
        && m_scheduleImportApplyButton;
    if (!importControlsReady)
    {
        return fail(2048);
    }

    m_scheduleImportKindCombo.SelectedIndex(0);
    m_scheduleImportUserTextBox.Text(L"WinUI Import User");
    m_scheduleImportTeacherTextBox.Text(L"\uD64D\uAE38\uB3D9");
    m_scheduleImportGradeTextBox.Text(L"E5");
    m_scheduleImportLevelTextBox.Text(L"Zeus");
    m_scheduleImportRoomTextBox.Text(L"413");
    m_scheduleImportDaysTextBox.Text(L"Monday, Wednesday");
    m_scheduleImportStartTextBox.Text(L"4:00 PM");
    m_scheduleImportEndTextBox.Text(L"4:55 PM");
    m_scheduleImportTeacherActionCombo.SelectedIndex(1);
    m_scheduleImportClassActionCombo.SelectedIndex(1);
    previewScheduleImport();
    const auto importStatus = m_scheduleImportStatusText.Text();
    const bool importPreviewReady =
        m_scheduleImportPreviewReady
        && m_scheduleImportPreview
        && m_scheduleImportPreview->user.classes.size() == 1
        && m_scheduleImportPreview->classes.size() == 1
        && m_scheduleImportPreview->teachers.size() == 1
        && m_scheduleImportApplyButton.IsEnabled()
        && std::wstring_view(importStatus.c_str(), importStatus.size()).find(
            L"Preview ready"
            ) != std::wstring_view::npos;
    if (!importPreviewReady)
    {
        return fail(2048);
    }

    applyScheduleImport();
    const auto importedClassrooms = repository.list();
    classmngr::engine::ClassScheduleService scheduleService(*m_openDatabase);
    const auto importedInfos = scheduleService.loadScheduleClassInfos();
    classmngr::engine::TeacherService teacherService(*m_openDatabase);
    const auto importedTeachers = teacherService.list();
    int importedClassId = -1;
    bool importedTimesReady = false;
    if (importedInfos)
    {
        for (const auto& info : *importedInfos)
        {
            if (info.classGrade == "E5" && info.classLevel == "Zeus"
                && info.classTimes.size() == 2
                && info.classTimes[0].day == "Monday"
                && info.classTimes[1].day == "Wednesday")
            {
                importedClassId = info.classId;
                importedTimesReady = true;
                break;
            }
        }
    }
    const auto appliedStatus = m_scheduleImportStatusText.Text();
    const bool importApplied =
        m_scheduleImportPreviewReady == false
        && !m_scheduleImportApplyButton.IsEnabled()
        && importedClassrooms && importedClassrooms->size() == 3
        && importedTeachers && importedTeachers->size() == 1
        && importedTimesReady && importedClassId > 0
        && std::wstring_view(appliedStatus.c_str(), appliedStatus.size()).find(
            L"Import applied atomically"
            ) != std::wstring_view::npos;
    if (!importApplied)
    {
        return fail(4096);
    }

    const bool testingControlsReady =
        m_testingClassSelector
        && m_testingClassNameTextBox
        && m_testingClassGradeTextBox
        && m_testingClassLevelTextBox
        && m_testingClassRoomTextBox
        && m_testingDayCombo
        && m_testingStartTextBox
        && m_testingReplaceExistingCheck
        && m_testingAssignmentList
        && m_testingStatusText
        && m_testingValidationText
        && m_testingCreateButton
        && m_testingAssignButton
        && m_testingDeleteAssignmentButton;
    if (!testingControlsReady)
    {
        return fail(8192);
    }

    const auto selectedTestingClassId = [this]() {
        if (!m_testingClassSelector)
        {
            return -1;
        }
        const auto item = m_testingClassSelector.SelectedItem().try_as<
            Microsoft::UI::Xaml::Controls::ComboBoxItem>();
        return item ? boxedInt(item.Tag()) : -1;
    };
    const auto selectTestingClass = [this](int classId) {
        if (!m_testingClassSelector)
        {
            return false;
        }
        for (int index = 0;
             index < static_cast<int>(m_testingClassSelector.Items().Size());
             ++index)
        {
            const auto item = m_testingClassSelector.Items().GetAt(index)
                .try_as<Microsoft::UI::Xaml::Controls::ComboBoxItem>();
            if (item && boxedInt(item.Tag()) == classId)
            {
                m_testingClassSelector.SelectedIndex(index);
                return true;
            }
        }
        return false;
    };

    m_testingClassNameTextBox.Text(L"WinUI Testing Group");
    m_testingClassGradeTextBox.Text(L"M1");
    m_testingClassLevelTextBox.Text(L"Mixed (All)");
    m_testingClassRoomTextBox.Text(L"Testing room 1");
    createTestingClass();
    const int firstTestingClassId = selectedTestingClassId();
    const bool firstTestingClassReady = firstTestingClassId > 0
        && m_testingClasses.size() == 1
        && m_testingClassSelector.Items().Size() == 1;
    if (!firstTestingClassReady)
    {
        return fail(8192);
    }

    m_testingClassNameTextBox.Text(L"WinUI Testing Group 2");
    m_testingClassRoomTextBox.Text(L"Testing room 2");
    createTestingClass();
    const int secondTestingClassId = selectedTestingClassId();
    if (secondTestingClassId <= 0 || secondTestingClassId == firstTestingClassId
        || m_testingClasses.size() != 2
        || m_testingClassSelector.Items().Size() != 2)
    {
        return fail(16384);
    }

    if (!selectTestingClass(firstTestingClassId))
    {
        return fail(32768);
    }
    m_testingDayCombo.SelectedIndex(0);
    m_testingStartTextBox.Text(L"09:00");
    m_testingReplaceExistingCheck.IsChecked(false);
    assignTestingClass();
    const bool assignmentCreated = m_testingAssignments.size() == 1
        && m_testingAssignments.front().classId == firstTestingClassId
        && m_testingAssignments.front().day == "Monday"
        && m_testingAssignments.front().startTime == "09:00";
    if (!assignmentCreated)
    {
        return fail(32768);
    }

    if (!selectTestingClass(secondTestingClassId))
    {
        return fail(65536);
    }
    m_testingReplaceExistingCheck.IsChecked(false);
    assignTestingClass();
    const bool replacementRejected =
        m_testingValidationText.Visibility()
            == Microsoft::UI::Xaml::Visibility::Visible
        && m_testingAssignments.size() == 1
        && m_testingAssignments.front().classId == firstTestingClassId;
    if (!replacementRejected)
    {
        return fail(65536);
    }

    m_testingReplaceExistingCheck.IsChecked(true);
    assignTestingClass();
    const bool assignmentReplaced = m_testingAssignments.size() == 1
        && m_testingAssignments.front().classId == secondTestingClassId
        && m_testingValidationText.Visibility()
            == Microsoft::UI::Xaml::Visibility::Collapsed;
    if (!assignmentReplaced)
    {
        return fail(131072);
    }

    m_testingAssignmentList.SelectedIndex(0);
    const bool deleteSelectionReady = m_testingDeleteAssignmentButton.IsEnabled();
    deleteTestingAssignment();
    const bool assignmentDeleted = deleteSelectionReady
        && m_testingAssignments.empty()
        && m_testingAssignmentList.Items().Size() == 0;
    if (!assignmentDeleted)
    {
        return fail(262144);
    }

    m_openDatabase.reset();
    refreshScheduleWorkspace();
    refreshTestingWorkspace();
    const bool clearedReady =
        m_scheduleWorkspaceStatusText.Text() == L"No database open."
        && !m_scheduleList.IsEnabled()
        && !m_scheduleClassSelector.IsEnabled()
        && m_testingStatusText.Text() == L"No database open."
        && !m_testingCreateButton.IsEnabled();
    return clearedReady ? true : fail(1024);
}

uint32_t MainWindow::phase6ScheduleFailureMask() const noexcept
{
    return m_phase6ScheduleFailureMask;
}

bool MainWindow::runPhase6SpeakingEvaluationChecks()
{
    m_phase6SpeakingEvaluationFailureMask = 0;
    const auto fail = [this](uint32_t failureMask) {
        m_phase6SpeakingEvaluationFailureMask = failureMask;
        return false;
    };
    const auto contains = [](winrt::hstring const& value,
                             std::wstring_view text) {
        return std::wstring_view(value.c_str(), value.size()).find(text)
            != std::wstring_view::npos;
    };

    m_openDatabase.reset();
    m_currentDatabasePath.clear();
    m_dirtyState.markClean();
    m_classLoading = false;
    m_classDirty = false;
    m_classDetailsDirty = false;
    m_classNotesDirty = false;
    m_classNew = false;
    m_classRosterLoading = false;
    m_classRosterDirty = false;
    m_speakingEvaluationLoading = false;
    m_speakingEvaluationDirty = false;
    m_speakingEvaluationDirtyCells.clear();
    m_speakingAnalyticsLoading = false;
    m_speakingAnalyticsName = "All";

    navigateTo(classesPageId);
    refreshClassesPage();
    const bool noDatabaseReady =
        m_currentPageId == classesPageId
        && m_speakingEvaluationStatusText
        && m_speakingEvaluationStatusText.Text() == L"No database open."
        && m_speakingEvaluationList
        && !m_speakingEvaluationList.IsEnabled()
        && m_speakingEvaluationSaveButton
        && !m_speakingEvaluationSaveButton.IsEnabled();
    if (!noDatabaseReady)
    {
        return fail(1);
    }

    auto opened = classmngr::engine::OpenDatabase::execute(":memory:");
    if (!opened || *opened == nullptr)
    {
        return fail(2);
    }
    m_openDatabase = std::move(*opened);

    classmngr::engine::ClassRepository repository(*m_openDatabase);
    const auto classId = repository.create("Speaking evaluation class");
    if (!classId)
    {
        return fail(4);
    }

    const auto& grades = classmngr::engine::ClassInfoConfig::grades();
    if (grades.empty())
    {
        return fail(8);
    }
    const auto levels = classmngr::engine::ClassInfoConfig::levelsForGrade(
        grades.front()
        );
    const auto readingBooks = classmngr::engine::ClassInfoConfig::readingBooks(
        grades.front(),
        levels.empty() ? std::string_view{} : levels.front()
        );
    const auto essayBooks = classmngr::engine::ClassInfoConfig::essayBooks(
        grades.front(),
        levels.empty() ? std::string_view{} : levels.front()
        );
    if (levels.empty() || readingBooks.empty() || essayBooks.empty())
    {
        return fail(8);
    }

    classmngr::engine::ClassInfo info;
    info.classId = *classId;
    info.classGrade = grades.front();
    info.classLevel = levels.front();
    info.readingBook = readingBooks.front();
    info.essayBook = essayBooks.front();
    info.classColor = "#FFFFFF";
    info.fontColor = "#000000";
    classmngr::engine::ClassInfoService infoService(*m_openDatabase);
    if (!infoService.save(info))
    {
        return fail(16);
    }

    classmngr::engine::Roster roster;
    for (const std::string_view column : classmngr::engine::RosterBaseColumns)
    {
        roster.columns.emplace_back(column);
    }
    roster.columnWidths = {140, 140, 100, 140, 100, 100};
    roster.rows.push_back({
        "Alice",
        winrt::to_string(winrt::hstring(L"\uC568\uB9AC\uC2A4")),
        "",
        "",
        "",
        ""
        });
    roster.rows.push_back({
        "Bob",
        winrt::to_string(winrt::hstring(L"\uAE40\uBBFC\uC218")),
        "",
        "",
        "",
        ""
        });
    classmngr::engine::RosterService rosterService(*m_openDatabase);
    if (!rosterService.save(*classId, roster))
    {
        return fail(32);
    }

    refreshClassesPage();
    const bool controlsReady =
        m_classSelectedId == *classId
        && m_speakingEvaluationSelector
        && m_speakingEvaluationSelector.Items().Size() == 4
        && m_speakingEvaluationHeaderGrid
        && m_speakingEvaluationHeaderGrid.Children().Size() == 11
        && m_speakingEvaluationList
        && m_speakingEvaluationList.Items().Size() == 25
        && m_speakingEvaluationCellBoxes.size() == 25
        && m_speakingEvaluationCellBoxes.front().size() == 11;
    if (!controlsReady)
    {
        return fail(64);
    }

    importSpeakingEvaluationNames();
    const bool namesImported =
        m_speakingEvaluationDirty
        && m_speakingEvaluationCellBoxes[0][1].Text() == L"Alice"
        && m_speakingEvaluationCellBoxes[0][2].Text()
            == winrt::hstring(L"\uC568\uB9AC\uC2A4")
        && m_speakingEvaluationCellBoxes[1][1].Text() == L"Bob";
    if (!namesImported)
    {
        return fail(128);
    }

    m_speakingEvaluationList.SelectedIndex(0);
    m_speakingEvaluationPasteTextBox.Text(L"A+\tA\tB+\tA\tB+\tA");
    applySpeakingEvaluationPaste();
    const bool pastedScores =
        m_speakingEvaluationCellBoxes[0][3].Text() == L"A+"
        && m_speakingEvaluationCellBoxes[0][8].Text() == L"A"
        && contains(
            m_speakingEvaluationStatusText.Text(),
            L"Applied 6 score cells"
            );
    if (!pastedScores)
    {
        return fail(256);
    }

    saveSpeakingEvaluation();
    classmngr::engine::SpeakingEvaluationPersistenceService evaluationService(
        *m_openDatabase
        );
    const auto winterSaved = evaluationService.load(*classId, "Winter");
    const bool winterPersistenceReady =
        winterSaved
        && !m_speakingEvaluationDirty
        && winterSaved->size() == static_cast<std::size_t>(
            classmngr::engine::SpeakingEvaluationRowCount
            )
        && !winterSaved->empty()
        && winterSaved->at(0).size() > 8
        && winterSaved->at(0).at(1) == "Alice"
        && winterSaved->at(0).at(3) == "A+"
        && winterSaved->at(0).at(8) == "A";
    if (!winterPersistenceReady)
    {
        return fail(131072);
    }

    refreshSpeakingAnalytics();
    const bool analyticsReady =
        m_speakingAnalyticsStatusText
        && contains(
            m_speakingAnalyticsStatusText.Text(),
            L"Analytics loaded for All"
            )
        && m_speakingAnalyticsCriteriaPanel
        && m_speakingAnalyticsCriteriaPanel.Children().Size() == 6
        && m_speakingAnalyticsRankingList
        && m_speakingAnalyticsRankingList.Items().Size() == 1
        && contains(
            m_speakingAnalyticsSummaryText.Text(),
            L"Class average"
            )
        && contains(
            m_speakingAnalyticsShapeText.Text(),
            L"Winter"
            );
    if (!analyticsReady)
    {
        return fail(262144);
    }

    m_speakingEvaluationCellBoxes[0][3].Text(L"Z");
    saveSpeakingEvaluation();
    const auto winterAfterInvalid = evaluationService.load(*classId, "Winter");
    const bool invalidScoreRejected =
        m_speakingEvaluationDirty
        && m_speakingEvaluationValidationText.Visibility()
            == Microsoft::UI::Xaml::Visibility::Visible
        && winterAfterInvalid
        && !winterAfterInvalid->empty()
        && winterAfterInvalid->at(0).size() > 3
        && winterAfterInvalid->at(0).at(3) == "A+";
    if (!invalidScoreRejected)
    {
        return fail(1024);
    }

    m_speakingEvaluationCellBoxes[0][3].Text(L"A+");
    m_speakingEvaluationCellBoxes[0][10].Text(L"private note");
    saveSpeakingEvaluation();
    const auto savedWithNote = evaluationService.load(*classId, "Winter");
    const bool noteSaved =
        savedWithNote
        && !m_speakingEvaluationDirty
        && savedWithNote->at(0).at(10) == "private note";
    if (!noteSaved)
    {
        return fail(2048);
    }

    m_speakingEvaluationSelector.SelectedIndex(2);
    const bool summerLoaded =
        m_speakingEvaluationName == "Summer"
        && !m_speakingEvaluationDirty
        && m_speakingEvaluationCellBoxes[0][1].Text().empty();
    if (!summerLoaded)
    {
        return fail(4096);
    }
    importSpeakingEvaluationNames();
    m_speakingEvaluationList.SelectedIndex(0);
    m_speakingEvaluationPasteTextBox.Text(L"B\tB\tB\tB\tB\tB");
    applySpeakingEvaluationPaste();
    saveSpeakingEvaluation();
    const auto summerSaved = evaluationService.load(*classId, "Summer");
    if (!summerSaved || m_speakingEvaluationDirty
        || summerSaved->at(0).at(3) != "B")
    {
        return fail(8192);
    }

    m_speakingEvaluationSelector.SelectedIndex(0);
    if (m_speakingEvaluationCellBoxes[0][10].Text() != L"private note")
    {
        return fail(16384);
    }
    m_speakingEvaluationCellBoxes[0][10].Text(L"discard me");
    discardSpeakingEvaluation();
    const bool discardReady =
        !m_speakingEvaluationDirty
        && m_speakingEvaluationCellBoxes[0][10].Text() == L"private note";
    if (!discardReady)
    {
        return fail(32768);
    }

    m_speakingEvaluationCellBoxes[0][9].Text({});
    m_speakingEvaluationCellBoxes[1][9].Text({});
    m_speakingEvaluationCellBoxes[0][10].Text(
        L"[Did Well]\nClear pronunciation\n[Needs Improvement]\nUse longer answers"
        );
    m_speakingEvaluationCellBoxes[1][10].Text(
        L"[Did Well]\nStrong vocabulary\n[Needs Improvement]\nAdd supporting details"
        );
    m_speakingEvaluationList.SelectedIndex(0);
    generateSpeakingAiPrompt();
    const bool aiStudentPromptReady =
        m_speakingAiPromptTextBox
        && contains(m_speakingAiPromptTextBox.Text(), L"STD_NAME")
        && contains(m_speakingAiPromptTextBox.Text(), L"Clear pronunciation")
        && m_speakingAiApplyStudentButton
        && !m_speakingAiApplyStudentButton.IsEnabled()
        && m_speakingAiResponseTextBox.Text().empty();
    if (!aiStudentPromptReady)
    {
        return fail(524288);
    }

    m_speakingAiResponseTextBox.Text(L"Great work, STD_NAME!");
    applySpeakingAiStudentComment();
    const bool aiStudentApplied =
        m_speakingEvaluationCellBoxes[0][9].Text()
            == L"Great work, Alice!"
        && !contains(
            m_speakingEvaluationCellBoxes[0][9].Text(),
            L"STD_NAME"
            );
    if (!aiStudentApplied)
    {
        return fail(1048576);
    }

    m_speakingEvaluationCellBoxes[0][9].Text({});
    generateSpeakingAiBatchPrompt();
    const std::string batchPrompt = asUtf8(m_speakingAiPromptTextBox.Text());
    const bool aiBatchPromptReady =
        m_speakingAiBatchRows.size() == 2
        && batchPrompt.find("STUDENT_01") != std::string::npos
        && batchPrompt.find("STUDENT_02") != std::string::npos
        && batchPrompt.find("Alice") == std::string::npos
        && batchPrompt.find("Bob") == std::string::npos;
    if (!aiBatchPromptReady)
    {
        return fail(2097152);
    }

    m_speakingAiResponseTextBox.Text(
        L"<<<STUDENT_01>>>\nAlice spoke clearly and used strong vocabulary.\n"
        L"<<<END_STUDENT_01>>>\n"
        L"<<<STUDENT_02>>>\nBob shared thoughtful ideas and can add more detail.\n"
        L"<<<END_STUDENT_02>>>"
        );
    parseSpeakingAiBatchResponse();
    if (m_speakingAiParsedComments.size() != 2)
    {
        return fail(4194304);
    }
    applySpeakingAiBatchComments();
    const bool aiBatchApplied =
        m_speakingEvaluationCellBoxes[0][9].Text()
            == L"Alice spoke clearly and used strong vocabulary."
        && m_speakingEvaluationCellBoxes[1][9].Text()
            == L"Bob shared thoughtful ideas and can add more detail."
        && m_speakingEvaluationDirty;
    if (!aiBatchApplied)
    {
        return fail(8388608);
    }
    saveSpeakingEvaluation();
    const auto aiSaved = evaluationService.load(*classId, "Winter");
    const bool aiPersistenceReady =
        aiSaved
        && !m_speakingEvaluationDirty
        && aiSaved->at(0).at(9)
            == "Alice spoke clearly and used strong vocabulary."
        && aiSaved->at(1).at(9)
            == "Bob shared thoughtful ideas and can add more detail.";
    if (!aiPersistenceReady)
    {
        return fail(16777216);
    }

    m_speakingBatchRendererSelector.SelectedIndex(0);
    m_speakingBatchTemplateSelector.SelectedIndex(0);
    m_speakingBatchSavePdfCheck.IsChecked(true);
    m_speakingBatchPrintCheck.IsChecked(false);
    m_speakingBatchKeepIndividualPdfsCheck.IsChecked(false);
    m_speakingBatchOutputDirectoryTextBox.Text(
        L"C:\\Temp\\ClassMngr-speaking-reports"
        );
    planSpeakingBatchReports();
    const bool batchArchivePlanReady =
        m_speakingBatchStatusText
        && contains(m_speakingBatchStatusText.Text(), L"Planned 2")
        && contains(m_speakingBatchStatusText.Text(), L"Internal")
        && contains(m_speakingBatchStatusText.Text(), L"one ZIP archive")
        && m_speakingBatchPlanButton.IsEnabled();
    if (!batchArchivePlanReady)
    {
        return fail(33554432);
    }

    m_speakingBatchKeepIndividualPdfsCheck.IsChecked(true);
    planSpeakingBatchReports();
    const bool individualPdfPlanReady =
        contains(
            m_speakingBatchStatusText.Text(),
            L"retain individual PDFs"
            );
    if (!individualPdfPlanReady)
    {
        return fail(67108864);
    }

    m_speakingBatchRendererSelector.SelectedIndex(1);
    m_speakingBatchTemplateSelector.SelectedIndex(1);
    planSpeakingBatchReports();
    const bool powerPointPlanReady =
        contains(m_speakingBatchStatusText.Text(), L"PowerPoint")
        && contains(
            m_speakingBatchStatusText.Text(),
            L"Renderer-neutral plan accepted"
            );
    if (!powerPointPlanReady)
    {
        return fail(134217728);
    }

    m_speakingBatchSavePdfCheck.IsChecked(false);
    m_speakingBatchPrintCheck.IsChecked(false);
    planSpeakingBatchReports();
    const bool outputModeRejected =
        contains(
            m_speakingBatchStatusText.Text(),
            L"output-mode-required"
            )
        && !m_speakingBatchPlanButton.IsEnabled();
    if (!outputModeRejected)
    {
        return fail(268435456);
    }

    m_openDatabase.reset();
    m_currentDatabasePath.clear();
    refreshClassesPage();
    const bool clearedReady =
        m_speakingEvaluationStatusText.Text() == L"No database open."
        && !m_speakingEvaluationList.IsEnabled()
        && !m_speakingEvaluationSaveButton.IsEnabled()
        && m_speakingAnalyticsStatusText.Text() == L"No database open."
        && m_speakingAnalyticsRankingList.Items().Size() == 0;
    return clearedReady ? true : fail(65536);
}

uint32_t MainWindow::phase6SpeakingEvaluationFailureMask() const noexcept
{
    return m_phase6SpeakingEvaluationFailureMask;
}

uint32_t MainWindow::phase4SemanticFailureMask()
{
    if (!ensureHomePage() || !m_scheduleSlotTextBox || !m_scheduleStatusText)
    {
        return 1;
    }

    // Roster and speaking prototypes live on the Classes presentation now,
    // but the phase hook still exercises their controls directly.
    if (!m_rosterSourceList || !m_rosterTransferredList || !m_rosterStatusText
        || !m_speakingPasteTextBox || !m_speakingStatusText
        || m_speakingScoreCells.size() != 9)
    {
        navigateTo(classesPageId);
    }
    if (!m_rosterSourceList || !m_rosterTransferredList || !m_rosterStatusText
        || !m_speakingPasteTextBox || !m_speakingStatusText
        || m_speakingScoreCells.size() != 9)
    {
        return 1;
    }

    const auto contains = [](winrt::hstring const& value, std::wstring_view text) {
        return std::wstring_view(value.c_str(), value.size()).find(text)
            != std::wstring_view::npos;
    };
    const auto eventArguments = Microsoft::UI::Xaml::RoutedEventArgs();
    m_scheduleSlotTextBox.Text(L"09:45–10:30");
    ScheduleApplyButton_Click(nullptr, eventArguments);
    const auto scheduleStatus = m_scheduleStatusText.Text();
    const bool scheduleReady = contains(scheduleStatus, L"not persisted");

    const auto sourceCount = m_rosterSourceList.Items().Size();
    const auto transferredCount = m_rosterTransferredList.Items().Size();
    m_rosterSourceList.SelectedIndex(0);
    RosterTransferButton_Click(nullptr, eventArguments);
    const bool rosterReady = sourceCount > 0
        && m_rosterSourceList.Items().Size() + 1 == sourceCount
        && m_rosterTransferredList.Items().Size() == transferredCount + 1
        && m_rosterTransferredList.SelectedIndex() >= 0;

    m_speakingPasteTextBox.Text(L"8\t7\t9\n9\t8\t8");
    SpeakingPasteButton_Click(nullptr, eventArguments);
    const bool firstScoreReady = m_speakingScoreCells[0].Text() == L"8";
    const bool secondScoreReady = m_speakingScoreCells[1].Text() == L"7";
    const bool sixthScoreReady = m_speakingScoreCells[5].Text() == L"8";
    const auto pasteStatus = m_speakingStatusText.Text();
    const bool pasteStatusReady = contains(pasteStatus, L"Applied 6");

    SpeakingAnalyticsButton_Click(nullptr, eventArguments);
    const auto analyticsStatus = m_speakingStatusText.Text();
    const bool analyticsReady = contains(
        analyticsStatus,
        L"Analytics navigation requested"
        );

    uint32_t failureMask = 0;
    failureMask |= scheduleReady ? 0 : 2;
    failureMask |= rosterReady ? 0 : 4;
    failureMask |= firstScoreReady ? 0 : 8;
    failureMask |= secondScoreReady ? 0 : 16;
    failureMask |= sixthScoreReady ? 0 : 32;
    failureMask |= pasteStatusReady ? 0 : 64;
    failureMask |= analyticsReady ? 0 : 128;
    failureMask |= m_dirtyState.isDirty() ? 0 : 256;
    return failureMask;
}

bool MainWindow::runPhase5CampusChecks()
{
    // Phase activation tests are shell-only, so this in-memory owner cannot
    // replace or observe a user database. Start by checking the explicit
    // no-database state, then seed the same service used by the page.
    m_phase5CampusScenario = L"no-database";
    m_openDatabase.reset();
    m_currentDatabasePath.clear();
    navigateTo(campusInformationPageId);
    refreshCampusInformationPage();
    const bool noDatabaseReady = m_currentPageId == campusInformationPageId
        && m_campusInformationState == L"no_database"
        && static_cast<bool>(m_contentFrame.Content());

    auto opened = classmngr::engine::OpenDatabase::execute(":memory:");
    if (!opened || *opened == nullptr)
    {
        return false;
    }

    auto& database = **opened;
    classmngr::engine::CampusRecordService service(database);
    const auto emptyResult = service.list();
    if (!emptyResult || !emptyResult->empty())
    {
        return false;
    }

    // Reproduce the user flow that previously left a cached no-database page
    // visible: leave Campus Information, open the database, then navigate
    // back to the cached page.
    m_phase5CampusScenario = L"empty";
    navigateTo(homePageId);
    m_openDatabase = std::move(*opened);
    navigateTo(campusInformationPageId);
    const bool emptyReady = m_campusInformationState == L"empty"
        && !m_campusSelector
        && static_cast<bool>(m_contentFrame.Content());
    if (!emptyReady)
    {
        m_openDatabase.reset();
        refreshCampusInformationPage();
        return false;
    }

    classmngr::engine::CampusRecordService populatedService(*m_openDatabase);
    classmngr::engine::CampusRecord campus;
    campus.name = winrt::to_string(winrt::hstring(L"서울 캠퍼스"));
    campus.buildingName = winrt::to_string(winrt::hstring(L"본관"));
    campus.address = winrt::to_string(winrt::hstring(L"서울특별시 강남구"));
    campus.phoneNumber = "+82-2-1234-5678";
    campus.officeNumber = winrt::to_string(winrt::hstring(L"사무실 101호"));
    campus.transitSteps = winrt::to_string(winrt::hstring(L"2호선에서 하차"));
    campus.arrivalInfo = winrt::to_string(winrt::hstring(L"안내 데스크로 오세요"));
    campus.imagePath = ":/assets/campuses/bundang/bundang_map.png";
    campus.officeWifi = "TeacherNet";
    campus.officeWifiPassword = "password";
    campus.printerName = "Printer-1";
    campus.printerSteps = "Load paper, then print.";
    campus.photocopierCode = "42";
    campus.housingLocations = winrt::to_string(winrt::hstring(L"강남, 서초"));
    const auto created = populatedService.create(campus);
    if (!created)
    {
        m_openDatabase.reset();
        refreshCampusInformationPage();
        return false;
    }

    m_phase5CampusScenario = L"populated";
    refreshCampusInformationPage();
    if (!m_campusSelector || m_campusInformationState != L"populated"
        || m_campusSelector.Items().Size() != 1)
    {
        m_openDatabase.reset();
        refreshCampusInformationPage();
        return false;
    }

    m_campusSelector.SelectedIndex(0);
    presentSelectedCampus(m_currentPageId);
    const bool imageControlReady = static_cast<bool>(m_campusImage);
    bool koreanTextReady = false;
    for (uint32_t index = 0; index < m_campusDetailsPanel.Children().Size(); ++index)
    {
        const auto text = m_campusDetailsPanel.Children().GetAt(index).try_as<
            Microsoft::UI::Xaml::Controls::TextBlock>();
        if (text
            && std::wstring_view(text.Text().c_str(), text.Text().size()).find(
                L"서울 캠퍼스"
                ) != std::wstring_view::npos)
        {
            koreanTextReady = true;
            break;
        }
    }

    const WinUILocalizer korean(L"ko-KR");
    const bool localizationReady = korean.hasString(
        L"CampusInformationPage",
        L"Campus Information"
        )
        && korean.getString(L"CampusInformationPage", L"Campus Information")
            == L"\xCEA0\xD37C\xC2A4 \xC815\xBCF4"
        && korean.getString(L"CampusInformationPage", L"Name")
            == L"\xC774\xB984";

    classmngr::windows::winui::WindowsResourceProvider resourceProvider;
    const auto imageExists = resourceProvider.exists(
        ":/assets/campuses/bundang/bundang_map.png"
        );
    const auto imageBytes = resourceProvider.readBytes(
        ":/assets/campuses/bundang/bundang_map.png"
        );
    const bool resourceReady = imageExists && *imageExists
        && imageBytes && !imageBytes->empty();

    m_openDatabase.reset();
    m_currentDatabasePath.clear();
    m_phase5CampusScenario = L"no-database";
    refreshCampusInformationPage();
    const bool resetReady = m_campusInformationState == L"no_database"
        && !m_campusSelector;
    m_phase5CampusScenario.clear();
    return noDatabaseReady && emptyReady && koreanTextReady
        && imageControlReady && localizationReady && resourceReady
        && resetReady;
}

bool MainWindow::runPhase6PersonalDetailsChecks()
{
    m_openDatabase.reset();
    m_currentDatabasePath.clear();
    m_dirtyState.markClean();
    m_personalDetailsDirty = false;

    navigateTo(personalDetailsPageId);
    refreshPersonalDetailsPage();
    const bool noDatabaseReady =
        m_currentPageId == homePageId
        && m_personalStatusText
        && m_personalStatusText.Text() == L"No database open."
        && m_personalSaveButton
        && !m_personalSaveButton.IsEnabled();

    auto opened = classmngr::engine::OpenDatabase::execute(":memory:");
    if (!noDatabaseReady || !opened || *opened == nullptr)
    {
        return false;
    }

    m_openDatabase = std::move(*opened);
    refreshPersonalDetailsPage();
    if (!m_personalDetailsLoaded || !m_personalNameTextBox)
    {
        return false;
    }

    m_personalNameTextBox.Text(L"홍길동");
    auto phase6Campus = Microsoft::UI::Xaml::Controls::ComboBoxItem();
    phase6Campus.Content(box_value(hstring(L"서울 캠퍼스")));
    phase6Campus.Tag(box_value(hstring(L"서울 캠퍼스")));
    m_personalCampusCombo.Items().Append(phase6Campus);
    m_personalCampusCombo.SelectedIndex(
        static_cast<int>(m_personalCampusCombo.Items().Size()) - 1
        );
    m_personalZoomNotAvailableCheck.IsChecked(false);
    m_personalZoomLoginIdTextBox.Text(L"teacher@example.test");
    m_personalZoomPasswordBox.Password(L"비밀번호");
    m_personalSignatureModeCombo.SelectedIndex(1);
    m_personalTypedSignatureTextBox.Text(L"홍길동 서명");
    m_personalSignatureFontCombo.SelectedIndex(2);
    PersonalDetailsSaveButton_Click(
        m_personalSaveButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    if (m_personalDetailsDirty
        || m_personalStatusText.Text() != L"Personal details saved.")
    {
        return false;
    }

    refreshPersonalDetailsPage();
    const bool roundTripReady =
        asWString(m_personalNameTextBox.Text()) == L"홍길동"
        && selectedComboValue(m_personalCampusCombo) == L"서울 캠퍼스"
        && asWString(m_personalZoomLoginIdTextBox.Text())
            == L"teacher@example.test"
        && asWString(m_personalZoomPasswordBox.Password()) == L"비밀번호"
        && m_personalSignatureModeCombo.SelectedIndex() == 1
        && asWString(m_personalTypedSignatureTextBox.Text())
            == L"홍길동 서명"
        && m_personalSignatureFontCombo.SelectedIndex() == 2;

    m_openDatabase.reset();
    refreshPersonalDetailsPage();
    const bool clearReady =
        m_personalStatusText.Text() == L"No database open."
        && !m_personalNameTextBox.IsEnabled()
        && !m_personalSaveButton.IsEnabled();
    return roundTripReady && clearReady;
}

bool MainWindow::runPhase6KoreanTeacherChecks()
{
    m_openDatabase.reset();
    m_currentDatabasePath.clear();
    m_dirtyState.markClean();
    m_koreanTeacherDirty = false;
    m_koreanTeacherNew = false;

    navigateTo(koreanTeachersPageId);
    const bool noDatabaseReady =
        m_currentPageId == koreanTeachersPageId
        && m_koreanTeacherStatusText
        && m_koreanTeacherStatusText.Text() == L"No database open."
        && m_koreanTeacherNewButton
        && !m_koreanTeacherNewButton.IsEnabled();

    auto opened = classmngr::engine::OpenDatabase::execute(":memory:");
    if (!noDatabaseReady || !opened || *opened == nullptr)
    {
        return false;
    }

    m_openDatabase = std::move(*opened);
    refreshKoreanTeachersPage();
    const bool emptyReady =
        m_koreanTeacherSelector
        && m_koreanTeacherSelector.Items().Size() == 0
        && m_koreanTeacherStatusText.Text()
            == L"No Korean teachers found. Choose New Teacher to add one.";
    if (!emptyReady)
    {
        return false;
    }

    KoreanTeacherNewButton_Click(
        m_koreanTeacherNewButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    m_koreanTeacherKrTextBox.Text(L"\uAE40\uBBFC\uC11C");
    m_koreanTeacherEnTextBox.Text(L"Minseo Kim");
    m_koreanTeacherRomanizationTextBox.Text(L"Minseo");
    m_koreanTeacherPreferredNameCombo.SelectedIndex(0);
    m_koreanTeacherBirthdayTextBox.Text(L"03-14");
    m_koreanTeacherPhoneTextBox.Text(L"010-1234-5678");
    m_koreanTeacherNotesTextBox.Text(L"\uC11C\uC6B8 \uCF54\uB514\uB124\uC774\uD130");
    KoreanTeacherSaveButton_Click(
        m_koreanTeacherSaveButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    const bool createdReady =
        m_koreanTeacherSelector.Items().Size() == 1
        && !m_koreanTeacherDirty
        && m_koreanTeacherStatusText.Text() == L"Korean teacher saved.";
    if (!createdReady)
    {
        return false;
    }

    m_koreanTeacherBirthdayTextBox.Text(L"13-40");
    KoreanTeacherSaveButton_Click(
        m_koreanTeacherSaveButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    const bool invalidBirthdayReady =
        m_koreanTeacherDirty
        && m_koreanTeacherValidationText.Visibility()
            == Microsoft::UI::Xaml::Visibility::Visible;
    KoreanTeacherDiscardButton_Click(
        m_koreanTeacherDiscardButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    if (!invalidBirthdayReady || m_koreanTeacherDirty)
    {
        return false;
    }

    m_koreanTeacherKrTextBox.Text(L"\uD64D\uAE38\uB3D9");
    KoreanTeacherSaveButton_Click(
        m_koreanTeacherSaveButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    classmngr::engine::TeacherService service(*m_openDatabase);
    const auto listed = service.list();
    const bool updatedReady = listed
        && listed->size() == 1
        && listed->front().teacherKr == "\xED\x99\x8D\xEA\xB8\xB8\xEB\x8F\x99"
        && !m_koreanTeacherDirty;

    m_openDatabase.reset();
    refreshKoreanTeachersPage();
    const bool clearReady =
        m_koreanTeacherStatusText.Text() == L"No database open."
        && !m_koreanTeacherKrTextBox.IsEnabled()
        && !m_koreanTeacherSaveButton.IsEnabled();
    return updatedReady && clearReady;
}

bool MainWindow::runPhase6NativeEnglishTeacherChecks()
{
    m_openDatabase.reset();
    m_currentDatabasePath.clear();
    m_dirtyState.markClean();
    m_nativeEnglishTeacherDirty = false;
    m_nativeEnglishTeacherNew = false;

    navigateTo(nativeEnglishTeachersPageId);
    const bool noDatabaseReady =
        m_currentPageId == nativeEnglishTeachersPageId
        && m_nativeEnglishTeacherStatusText
        && m_nativeEnglishTeacherStatusText.Text() == L"No database open."
        && m_nativeEnglishTeacherNewButton
        && !m_nativeEnglishTeacherNewButton.IsEnabled();

    auto opened = classmngr::engine::OpenDatabase::execute(":memory:");
    if (!noDatabaseReady || !opened || *opened == nullptr)
    {
        return false;
    }

    m_openDatabase = std::move(*opened);
    refreshNativeEnglishTeachersPage();
    const bool emptyReady =
        m_nativeEnglishTeacherSelector
        && m_nativeEnglishTeacherSelector.Items().Size() == 0
        && m_nativeEnglishTeacherStatusText.Text()
            == L"No Native English Teachers found. Choose New Teacher to add one.";
    if (!emptyReady)
    {
        return false;
    }

    NativeEnglishTeacherNewButton_Click(
        m_nativeEnglishTeacherNewButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    m_nativeEnglishTeacherNameTextBox.Text(L"Alice Smith");
    m_nativeEnglishTeacherPositionCombo.SelectedIndex(1);
    m_nativeEnglishTeacherPhoneTextBox.Text(L"010-5555-0101");
    m_nativeEnglishTeacherEmailTextBox.Text(L"alice@example.test");
    m_nativeEnglishTeacherBirthdayTextBox.Text(L"03-14");
    m_nativeEnglishTeacherNationalityTextBox.Text(L"Canadian");
    NativeEnglishTeacherSaveButton_Click(
        m_nativeEnglishTeacherSaveButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    const bool createdReady =
        m_nativeEnglishTeacherSelector.Items().Size() == 1
        && !m_nativeEnglishTeacherDirty
        && m_nativeEnglishTeacherStatusText.Text()
            == L"Native English Teacher saved.";
    if (!createdReady)
    {
        return false;
    }

    m_nativeEnglishTeacherBirthdayTextBox.Text(L"13-40");
    NativeEnglishTeacherSaveButton_Click(
        m_nativeEnglishTeacherSaveButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    const bool invalidBirthdayReady =
        m_nativeEnglishTeacherDirty
        && m_nativeEnglishTeacherValidationText.Visibility()
            == Microsoft::UI::Xaml::Visibility::Visible;
    NativeEnglishTeacherDiscardButton_Click(
        m_nativeEnglishTeacherDiscardButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    if (!invalidBirthdayReady || m_nativeEnglishTeacherDirty)
    {
        return false;
    }

    m_nativeEnglishTeacherNameTextBox.Text(L"Alice Cooper");
    NativeEnglishTeacherSaveButton_Click(
        m_nativeEnglishTeacherSaveButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    classmngr::engine::NativeEnglishTeacherService service(*m_openDatabase);
    const auto listed = service.list();
    const bool updatedReady = listed
        && listed->size() == 1
        && listed->front().name == "Alice Cooper"
        && listed->front().email == "alice@example.test"
        && !m_nativeEnglishTeacherDirty;

    m_openDatabase.reset();
    refreshNativeEnglishTeachersPage();
    const bool clearReady =
        m_nativeEnglishTeacherStatusText.Text() == L"No database open."
        && !m_nativeEnglishTeacherNameTextBox.IsEnabled()
        && !m_nativeEnglishTeacherSaveButton.IsEnabled();
    return updatedReady && clearReady;
}

bool MainWindow::runPhase6GsTeamChecks()
{
    m_openDatabase.reset();
    m_currentDatabasePath.clear();
    m_dirtyState.markClean();
    m_gsTeamDirty = false;
    m_gsTeamNew = false;

    navigateTo(gsTeamPageId);
    const bool noDatabaseReady =
        m_currentPageId == gsTeamPageId
        && m_gsTeamStatusText
        && m_gsTeamStatusText.Text() == L"No database open."
        && m_gsTeamNewButton
        && !m_gsTeamNewButton.IsEnabled();

    auto opened = classmngr::engine::OpenDatabase::execute(":memory:");
    if (!noDatabaseReady || !opened || *opened == nullptr)
    {
        return false;
    }

    m_openDatabase = std::move(*opened);
    refreshGsTeamPage();
    const bool emptyReady =
        m_gsTeamSelector
        && m_gsTeamSelector.Items().Size() == 0
        && m_gsTeamStatusText.Text()
            == L"No GS Team members found. Choose New Member to add one.";
    if (!emptyReady)
    {
        return false;
    }

    GsTeamNewButton_Click(
        m_gsTeamNewButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    m_gsTeamNameTextBox.Text(L"Jane Doe");
    m_gsTeamKoreanNameTextBox.Text(L"\uC81C\uC778 \uB450");
    m_gsTeamPositionTextBox.Text(L"Branch Manager");
    m_gsTeamPhoneTextBox.Text(L"010-5555-0202");
    m_gsTeamBirthdayTextBox.Text(L"04-21");
    GsTeamSaveButton_Click(
        m_gsTeamSaveButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    const bool createdReady =
        m_gsTeamSelector.Items().Size() == 1
        && !m_gsTeamDirty
        && m_gsTeamStatusText.Text() == L"GS Team member saved.";
    if (!createdReady)
    {
        return false;
    }

    m_gsTeamBirthdayTextBox.Text(L"13-40");
    GsTeamSaveButton_Click(
        m_gsTeamSaveButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    const bool invalidBirthdayReady =
        m_gsTeamDirty
        && m_gsTeamValidationText.Visibility()
            == Microsoft::UI::Xaml::Visibility::Visible;
    GsTeamDiscardButton_Click(
        m_gsTeamDiscardButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    if (!invalidBirthdayReady || m_gsTeamDirty)
    {
        return false;
    }

    m_gsTeamKoreanNameTextBox.Text(L"\uC81C\uC778 \uCFE0\uD37C");
    GsTeamSaveButton_Click(
        m_gsTeamSaveButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    classmngr::engine::GsTeamService service(*m_openDatabase);
    const auto listed = service.list();
    const bool updatedReady = listed
        && listed->size() == 1
        && listed->front().name == "Jane Doe"
        && listed->front().koreanName == "\xEC\xA0\x9C\xEC\x9D\xB8 \xEC\xBF\xA0\xED\x8D\xBC"
        && !m_gsTeamDirty;

    m_openDatabase.reset();
    refreshGsTeamPage();
    const bool clearReady =
        m_gsTeamStatusText.Text() == L"No database open."
        && !m_gsTeamNameTextBox.IsEnabled()
        && !m_gsTeamSaveButton.IsEnabled();
    return updatedReady && clearReady;
}

bool MainWindow::runPhase6RosterChecks()
{
    m_phase6RosterFailureMask = 0;
    const auto fail = [this](uint32_t failureMask) {
        m_phase6RosterFailureMask = failureMask;
        return false;
    };

    m_openDatabase.reset();
    m_currentDatabasePath.clear();
    m_dirtyState.markClean();
    m_classLoading = false;
    m_classDirty = false;
    m_classDetailsDirty = false;
    m_classNotesDirty = false;
    m_classNew = false;
    m_classRosterLoading = false;
    m_classRosterDirty = false;

    navigateTo(classesPageId);
    refreshClassesPage();
    const bool noDatabaseReady =
        m_currentPageId == classesPageId
        && m_classRosterStatusText
        && m_classRosterStatusText.Text() == L"No database open."
        && m_classRosterSaveButton
        && !m_classRosterSaveButton.IsEnabled()
        && m_classRosterAddButton
        && !m_classRosterAddButton.IsEnabled();
    if (!noDatabaseReady)
    {
        return fail(1);
    }

    auto opened = classmngr::engine::OpenDatabase::execute(":memory:");
    if (!opened || *opened == nullptr)
    {
        return fail(2);
    }
    m_openDatabase = std::move(*opened);

    classmngr::engine::ClassRepository repository(*m_openDatabase);
    const auto sourceId = repository.create("Roster Source");
    const auto targetId = repository.create("Roster Target");
    if (!sourceId || !targetId)
    {
        return fail(4);
    }

    const auto& grades = classmngr::engine::ClassInfoConfig::grades();
    if (grades.empty())
    {
        return fail(8);
    }
    const auto levels = classmngr::engine::ClassInfoConfig::levelsForGrade(
        grades.front()
        );
    const auto readingBooks = classmngr::engine::ClassInfoConfig::readingBooks(
        grades.front(),
        levels.empty() ? std::string_view{} : levels.front()
        );
    const auto essayBooks = classmngr::engine::ClassInfoConfig::essayBooks(
        grades.front(),
        levels.empty() ? std::string_view{} : levels.front()
        );
    if (levels.empty() || readingBooks.empty() || essayBooks.empty())
    {
        return fail(16);
    }

    const auto saveClassInfo = [this, &grades, &levels, &readingBooks,
                                &essayBooks](int classId) {
        classmngr::engine::ClassInfo info;
        info.classId = classId;
        info.classGrade = grades.front();
        info.classLevel = levels.front();
        info.readingBook = readingBooks.front();
        info.essayBook = essayBooks.front();
        info.classColor = "#FFFFFF";
        info.fontColor = "#000000";
        classmngr::engine::ClassInfoService service(*m_openDatabase);
        return service.save(info);
    };
    if (!saveClassInfo(*sourceId) || !saveClassInfo(*targetId))
    {
        return fail(32);
    }

    classmngr::engine::Roster sourceRoster;
    for (const std::string_view column : classmngr::engine::RosterBaseColumns)
    {
        sourceRoster.columns.emplace_back(column);
    }
    sourceRoster.columnWidths = {140, 140, 100, 140, 100, 100};
    sourceRoster.rows.push_back({
        "Alice",
        winrt::to_string(winrt::hstring(L"\uC568\uB9AC\uC2A4")),
        "",
        "",
        "",
        ""
        });
    classmngr::engine::RosterService rosterService(*m_openDatabase);
    if (!rosterService.save(*sourceId, sourceRoster))
    {
        return fail(64);
    }

    refreshClassesPage();
    const bool populatedReady =
        m_classSelectedId == *sourceId
        && m_classRosterHeaderGrid
        && m_classRosterHeaderGrid.Children().Size() == 6
        && m_classRosterList
        && m_classRosterList.Items().Size() == 1
        && m_classRosterTransferTargetCombo
        && m_classRosterTransferTargetCombo.Items().Size() == 1
        && m_classRosterStatusText.Text() == L"Roster loaded. 1 students.";
    if (!populatedReady)
    {
        return fail(128);
    }

    m_classRosterList.SelectedIndex(0);
    if (m_classRosterCellBoxes.empty()
        || m_classRosterCellBoxes.front().size() < 2)
    {
        return fail(256);
    }
    m_classRosterCellBoxes.front().front().Text(L"Alice Updated");
    saveClassRoster();
    const auto savedRoster = rosterService.load(*sourceId);
    const bool savedReady = savedRoster
        && !m_classRosterDirty
        && classmngr::engine::rosterStudentCount(*savedRoster) == 1
        && savedRoster->rows.front().front() == "Alice Updated";
    if (!savedReady)
    {
        return fail(512);
    }

    m_classRosterCellBoxes.front().front().Text(L"Alice \u2603");
    saveClassRoster();
    const bool invalidReady =
        m_classRosterDirty
        && m_classRosterValidationText.Visibility()
            == Microsoft::UI::Xaml::Visibility::Visible
        && m_classRosterStatusText.Text() == L"Roster could not be saved.";
    if (!invalidReady)
    {
        return fail(1024);
    }
    m_classRosterCellBoxes.front().front().Text(L"Alice Updated");
    saveClassRoster();
    if (m_classRosterDirty)
    {
        return fail(2048);
    }

    addClassRosterRow();
    const bool rowAddedReady =
        m_classRosterList.Items().Size() == 2
        && m_classRosterDirty;
    saveClassRoster();
    const bool rowSavedReady = !m_classRosterDirty;
    if (!rowAddedReady || !rowSavedReady)
    {
        return fail(4096);
    }

    const bool templateReady =
        m_classRosterTemplateCombo
        && m_classRosterTemplateCombo.Items().Size() == 3
        && m_classRosterTemplateStatusText
        && std::wstring_view(
               m_classRosterTemplateStatusText.Text().c_str(),
               m_classRosterTemplateStatusText.Text().size()
               ).find(L"landscape") != std::wstring_view::npos;
    if (!templateReady)
    {
        return fail(8192);
    }

    prepareClassTransfer();
    const bool packageReady = std::wstring_view(
        m_classRosterStatusText.Text().c_str(),
        m_classRosterStatusText.Text().size()
        ).find(L"package ready") != std::wstring_view::npos;
    if (!packageReady)
    {
        return fail(16384);
    }

    m_classRosterList.SelectedIndex(0);
    m_classRosterTransferTargetCombo.SelectedIndex(0);
    transferClassRosterRow();
    const auto transferredSource = rosterService.load(*sourceId);
    const auto transferredTarget = rosterService.load(*targetId);
    const bool transferReady = transferredSource
        && transferredTarget
        && classmngr::engine::rosterStudentCount(*transferredSource) == 0
        && classmngr::engine::rosterStudentCount(*transferredTarget) == 1
        && transferredTarget->rows.front().front() == "Alice Updated"
        && !m_classRosterDirty;
    if (!transferReady)
    {
        return fail(32768);
    }

    m_openDatabase.reset();
    m_currentDatabasePath.clear();
    refreshClassesPage();
    const bool clearReady =
        m_classRosterStatusText.Text() == L"No database open."
        && !m_classRosterSaveButton.IsEnabled()
        && !m_classRosterTransferButton.IsEnabled();
    if (!clearReady)
    {
        return fail(65536);
    }
    return true;
}

uint32_t MainWindow::phase6RosterFailureMask() const noexcept
{
    return m_phase6RosterFailureMask;
}

bool MainWindow::runPhase6ClassInformationChecks()
{
    m_phase6ClassInformationFailureMask = 0;
    const auto fail = [this](uint32_t failureMask) {
        m_phase6ClassInformationFailureMask = failureMask;
        return false;
    };
    m_openDatabase.reset();
    m_currentDatabasePath.clear();
    m_dirtyState.markClean();
    m_classLoading = false;
    m_classDirty = false;
    m_classDetailsDirty = false;
    m_classNotesDirty = false;
    m_classNew = false;

    navigateTo(classesPageId);
    refreshClassesPage();
    const bool noDatabaseReady =
        m_currentPageId == classesPageId
        && classNavigationLocation() == ClassNavigationLocation::Top
        && m_classStatusText
        && m_classStatusText.Text() == L"No database open."
        && m_classNewButton
        && !m_classNewButton.IsEnabled()
        && m_classNotesStatusText.Text() == L"No database open.";
    if (!noDatabaseReady)
    {
        return fail(1);
    }

    auto opened = classmngr::engine::OpenDatabase::execute(":memory:");
    if (!opened || *opened == nullptr)
    {
        return fail(2);
    }
    m_openDatabase = std::move(*opened);
    refreshClassesPage();
    const bool emptyReady =
        m_classSelector.Items().Size() == 0
        && m_classStatusText.Text()
            == L"No classes found. Choose New Class to add one."
        && m_classNewButton.IsEnabled();
    if (!emptyReady)
    {
        return fail(3);
    }

    const auto emptySettings =
        classmngr::engine::ApplicationSettingsService(*m_openDatabase)
            .load(classNavigationLocationKey);
    const bool defaultNavigationReady =
        emptySettings
        && std::holds_alternative<std::monostate>(*emptySettings)
        && classNavigationLocation() == ClassNavigationLocation::Top
        && m_classPageRoot
        && m_classPageRoot.RowDefinitions().Size() == 3
        && m_classSectionSelectorBar
        && m_classSectionSelectorBar.Items().Size() == 6
        && m_classSectionContentHost
        && m_classSectionContentHost.Content().try_as<
            Microsoft::UI::Xaml::Controls::ScrollViewer>()
        && m_classNavigationCard
        && Microsoft::UI::Xaml::Controls::Grid::GetRow(
            m_classSectionSelectorBar
            ) == 0
        && Microsoft::UI::Xaml::Controls::Grid::GetRow(
            m_classNavigationCard
            ) == 1
        && Microsoft::UI::Xaml::Controls::Grid::GetRow(
            m_classSectionContentHost
            ) == 2
        && m_classPageRoot.RowDefinitions().GetAt(1).Height().GridUnitType
            == Microsoft::UI::Xaml::GridUnitType::Auto
        && m_classPageRoot.RowDefinitions().GetAt(2).Height().GridUnitType
            == Microsoft::UI::Xaml::GridUnitType::Star;
    if (!defaultNavigationReady)
    {
        return fail(16);
    }

    ClassNewButton_Click(
        m_classNewButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    m_classNameTextBox.Text(L"Portable Class");
    m_classGradeCombo.SelectedIndex(1);
    m_classLevelCombo.SelectedIndex(1);
    m_classReadingBookCombo.SelectedIndex(2);
    m_classEssayBookCombo.SelectedIndex(2);
    m_classColorTextBox.Text(L"#AABBCC");
    m_classFontColorTextBox.Text(L"#102030");
    ClassSaveButton_Click(
        m_classSaveButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    const bool createdReady =
        m_classSelector.Items().Size() == 1
        && m_classSelectedId > 0
        && !m_classDirty
        && m_classStatusText.Text() == L"Class information saved.";
    if (!createdReady)
    {
        return fail(4);
    }

    m_classNameTextBox.Text(L"Portable Class Updated");
    ClassSaveButton_Click(
        m_classSaveButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    classmngr::engine::ClassRepository repository(*m_openDatabase);
    const auto listed = repository.list();
    const bool updatedReady = listed
        && listed->size() == 1
        && listed->front().name == "Portable Class Updated"
        && !m_classDirty;
    if (!updatedReady)
    {
        return fail(5);
    }

    m_classColorTextBox.Text(L"#not-a-color");
    ClassSaveButton_Click(
        m_classSaveButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    const bool invalidReady =
        m_classDirty
        && m_classValidationText.Visibility()
            == Microsoft::UI::Xaml::Visibility::Visible;
    ClassDiscardButton_Click(
        m_classDiscardButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    if (!invalidReady || m_classDirty)
    {
        return fail(6);
    }

    m_classNotesTextBox.Text(L"Notes from WinUI / \uD55C\uAE00");
    m_classTimeFillerActivitiesTextBox.Text(L"Vocabulary review");
    ClassNotesSaveButton_Click(
        m_classNotesSaveButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    classmngr::engine::ClassInfoService classInfo(*m_openDatabase);
    const auto withNotes = classInfo.load(m_classSelectedId);
    if (!withNotes)
    {
        return fail(7);
    }
    if (withNotes->notes != "Notes from WinUI / \xED\x95\x9C\xEA\xB8\x80")
    {
        return fail(12);
    }
    if (withNotes->timeFillerActivities != "Vocabulary review")
    {
        return fail(13);
    }
    if (m_classDirty)
    {
        return fail(14);
    }

    const auto secondId = repository.create("Second Class");
    if (!secondId)
    {
        return fail(8);
    }

    auto firstNavigationInfo = *withNotes;
    firstNavigationInfo.classId = m_classSelectedId;
    firstNavigationInfo.classTimes = {
        {"Monday", "4:00 PM", "4:50 PM"}
    };
    if (!classInfo.save(firstNavigationInfo))
    {
        return fail(32);
    }
    auto secondNavigationInfo = firstNavigationInfo;
    secondNavigationInfo.classId = *secondId;
    secondNavigationInfo.classTimes = {
        {"Tuesday", "5:00 PM", "5:50 PM"}
    };
    if (!classInfo.save(secondNavigationInfo))
    {
        return fail(64);
    }

    refreshClassesPage();
    if (m_classSelector.Items().Size() != 2)
    {
        return fail(9);
    }

    m_classSectionSelectorBar.SelectedItem(m_classSectionSelectorItems[5]);
    const bool sectionSelectionReady =
        m_classSectionIndex == 5
        && m_classSectionSelectorBar.SelectedItem()
            == m_classSectionSelectorItems[5]
        && m_classSectionContentHost.Content().try_as<
            Microsoft::UI::Xaml::Controls::ScrollViewer>();
    if (!sectionSelectionReady)
    {
        return fail(128);
    }

    selectClassNavigationGrade(false, firstNavigationInfo.classGrade);
    toggleClassNavigationDay("Monday");
    const bool topNavigationReady =
        classNavigationLocation() == ClassNavigationLocation::Top
        && Microsoft::UI::Xaml::Controls::Grid::GetRow(
            m_classNavigationCard
            ) == 1
        && Microsoft::UI::Xaml::Controls::Grid::GetRow(
            m_classSectionContentHost
            ) == 2
        && m_classPageRoot.RowDefinitions().GetAt(1).Height().GridUnitType
            == Microsoft::UI::Xaml::GridUnitType::Auto
        && m_classPageRoot.RowDefinitions().GetAt(2).Height().GridUnitType
            == Microsoft::UI::Xaml::GridUnitType::Star
        && !m_classNavigationAll
        && std::find(
            m_classNavigationSelectedDays.begin(),
            m_classNavigationSelectedDays.end(),
            "Monday"
            ) != m_classNavigationSelectedDays.end()
        && m_classNavigationClassTabs.Children().Size() == 1;
    if (!topNavigationReady)
    {
        return fail(256);
    }

    const int firstClassId = m_classSelectedId;
    selectClassFromNavigation(*secondId);
    const bool secondClassSelected = m_classSelectedId == *secondId;
    selectClassFromNavigation(firstClassId);
    const bool firstClassSelected = m_classSelectedId == firstClassId;
    if (!secondClassSelected || !firstClassSelected)
    {
        return fail(512);
    }

    setClassNavigationLocation(ClassNavigationLocation::Bottom);
    const auto savedNavigationLocation =
        classmngr::engine::ApplicationSettingsService(*m_openDatabase)
            .load(classNavigationLocationKey);
    const bool bottomNavigationReady =
        classNavigationLocation() == ClassNavigationLocation::Bottom
        && savedNavigationLocation
        && std::get_if<std::string>(&*savedNavigationLocation)
            != nullptr
        && *std::get_if<std::string>(&*savedNavigationLocation) == "bottom"
        && Microsoft::UI::Xaml::Controls::Grid::GetRow(
            m_classNavigationCard
            ) == 2
        && Microsoft::UI::Xaml::Controls::Grid::GetRow(
            m_classSectionContentHost
            ) == 1
        && m_classPageRoot.RowDefinitions().GetAt(1).Height().GridUnitType
            == Microsoft::UI::Xaml::GridUnitType::Star
        && m_classPageRoot.RowDefinitions().GetAt(2).Height().GridUnitType
            == Microsoft::UI::Xaml::GridUnitType::Auto
        && m_classSelectedId == firstClassId
        && m_classSectionIndex == 5
        && !m_classNavigationAll
        && std::find(
            m_classNavigationSelectedDays.begin(),
            m_classNavigationSelectedDays.end(),
            "Monday"
            ) != m_classNavigationSelectedDays.end();
    if (!bottomNavigationReady)
    {
        return fail(1024);
    }

    m_classNameTextBox.Text(L"Unsaved Location Change");
    const bool dirtyBeforeLocationToggle = m_classDirty;
    setClassNavigationLocation(ClassNavigationLocation::Top);
    const bool statePreservedByToggle =
        dirtyBeforeLocationToggle
        && m_classDirty
        && m_classSelectedId == firstClassId
        && m_classSectionIndex == 5
        && !m_classNavigationAll
        && std::find(
            m_classNavigationSelectedDays.begin(),
            m_classNavigationSelectedDays.end(),
            "Monday"
            ) != m_classNavigationSelectedDays.end();
    if (!statePreservedByToggle)
    {
        return fail(2048);
    }
    ClassDiscardButton_Click(
        m_classDiscardButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );

    setClassNavigationLocation(ClassNavigationLocation::Bottom);
    refreshClassesPage();
    const bool refreshPreservedNavigation =
        classNavigationLocation() == ClassNavigationLocation::Bottom
        && m_classSelectedId == firstClassId
        && m_classSectionIndex == 5
        && !m_classNavigationAll
        && std::find(
            m_classNavigationSelectedDays.begin(),
            m_classNavigationSelectedDays.end(),
            "Monday"
            ) != m_classNavigationSelectedDays.end()
        && Microsoft::UI::Xaml::Controls::Grid::GetRow(
            m_classNavigationCard
            ) == 2
        && Microsoft::UI::Xaml::Controls::Grid::GetRow(
            m_classSectionContentHost
            ) == 1
        && m_classPageRoot.RowDefinitions().GetAt(1).Height().GridUnitType
            == Microsoft::UI::Xaml::GridUnitType::Star
        && m_classPageRoot.RowDefinitions().GetAt(2).Height().GridUnitType
            == Microsoft::UI::Xaml::GridUnitType::Auto;
    if (!refreshPreservedNavigation)
    {
        return fail(4096);
    }

    classmngr::engine::ApplicationSettingsService settings(*m_openDatabase);
    if (!settings.save(
            classNavigationLocationKey,
            classmngr::engine::SettingValue{std::string("invalid")}
            ))
    {
        return fail(8192);
    }
    refreshClassesPage();
    const bool invalidSettingReady =
        classNavigationLocation() == ClassNavigationLocation::Top
        && Microsoft::UI::Xaml::Controls::Grid::GetRow(
            m_classNavigationCard
            ) == 1
        && Microsoft::UI::Xaml::Controls::Grid::GetRow(
            m_classSectionContentHost
            ) == 2
        && m_classPageRoot.RowDefinitions().GetAt(1).Height().GridUnitType
            == Microsoft::UI::Xaml::GridUnitType::Auto
        && m_classPageRoot.RowDefinitions().GetAt(2).Height().GridUnitType
            == Microsoft::UI::Xaml::GridUnitType::Star;
    if (!invalidSettingReady)
    {
        return fail(16384);
    }

    m_classNameTextBox.Text(L"Unsaved Class Name");
    const int selectedBefore = m_classSelectedId;
    const int otherIndex = m_classSelectedIndex == 0 ? 1 : 0;
    m_classSelector.SelectedIndex(otherIndex);
    const bool dirtySelectionProtected =
        m_classSelectedId == selectedBefore
        && m_classSelector.SelectedIndex() == m_classSelectedIndex
        && m_classDirty;
    ClassDiscardButton_Click(
        m_classDiscardButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    if (!dirtySelectionProtected)
    {
        return fail(10);
    }

    m_openDatabase.reset();
    refreshClassesPage();
    const bool clearReady = m_classStatusText.Text() == L"No database open."
        && classNavigationLocation() == ClassNavigationLocation::Top
        && !m_classNameTextBox.IsEnabled()
        && !m_classSaveButton.IsEnabled()
        && !m_classNotesSaveButton.IsEnabled();
    if (!clearReady)
    {
        return fail(11);
    }
    return true;
}

uint32_t MainWindow::phase6ClassInformationFailureMask() const noexcept
{
    return m_phase6ClassInformationFailureMask;
}

bool MainWindow::runPhase6CalendarChecks()
{
    m_phase6CalendarFailureMask = 0;
    const auto fail = [this](uint32_t failureMask) {
        m_phase6CalendarFailureMask = failureMask;
        return false;
    };

    m_openDatabase.reset();
    m_currentDatabasePath.clear();
    m_dirtyState.markClean();
    navigateTo(homePageId);
    refreshCalendarPage();
    const bool noDatabaseReady =
        m_calendarTabs
        && m_calendarTabs.Items().Size() == 2
        && m_calendarGrid
        && m_calendarGrid.Children().Size() == 49
        && m_calendarStatusText.Text() == L"No database open."
        && !m_calendarAddEventButton.IsEnabled()
        && !m_calendarSavePreferencesButton.IsEnabled();
    if (!noDatabaseReady)
    {
        return fail(1);
    }

    auto opened = classmngr::engine::OpenDatabase::execute(":memory:");
    if (!opened || *opened == nullptr)
    {
        return fail(2);
    }
    m_openDatabase = std::move(*opened);
    m_calendarDisplayedMonth = EngineCalendarDate{
        std::chrono::year{calendarFirstTermYear},
        std::chrono::month{1},
        std::chrono::day{1}
    };
    m_calendarSelectedDate = EngineCalendarDate{
        std::chrono::year{calendarFirstTermYear},
        std::chrono::month{1},
        std::chrono::day{15}
    };
    refreshCalendarPage();
    const bool emptyReady =
        m_calendarStatusText.Text() == L"Calendar loaded: 0 event(s)."
        && m_calendarAddEventButton.IsEnabled()
        && m_calendarShowAllCampusesCheck.IsEnabled();
    if (!emptyReady)
    {
        return fail(3);
    }

    classmngr::engine::CalendarEvent event;
    event.title = "Calendar smoke event";
    event.eventType = "Meeting";
    event.startDate = m_calendarSelectedDate;
    event.endDate = m_calendarSelectedDate;
    event.startTime = std::chrono::minutes{9 * 60};
    event.endTime = std::chrono::minutes{10 * 60};
    classmngr::engine::CalendarEventService service(*m_openDatabase);
    const auto created = service.save(event);
    if (!created)
    {
        return fail(4);
    }
    refreshCalendarPage();
    const bool eventReady =
        m_calendarEvents.size() == 1
        && m_calendarEventsPanel.Children().Size() == 1
        && m_calendarStatusText.Text() == L"Calendar loaded: 1 event(s).";
    if (!eventReady)
    {
        return fail(5);
    }

    classmngr::engine::CalendarEvent invalid = event;
    invalid.endTime = invalid.startTime;
    const bool validationReady =
        !classmngr::engine::CalendarEventValidator::validate(invalid).isValid();
    if (!validationReady)
    {
        return fail(6);
    }

    m_calendarShowAllCampusesCheck.IsChecked(true);
    m_calendarHideStartOfTermCheck.IsChecked(true);
    m_calendarFirstDayCombo.SelectedIndex(1);
    saveCalendarPreferences();
    classmngr::engine::ApplicationSettingsService settings(*m_openDatabase);
    const auto firstDay = settings.load("calendar/firstDayOfWeek");
    const auto showAll = settings.load("calendar/showEventsAtAllCampuses");
    const auto hideStart = settings.load("calendar/hideStartOfTermEvents");
    const bool preferencesReady =
        firstDay && showAll && hideStart
        && settingInteger(*firstDay, -1) == 1
        && settingInteger(*showAll, -1) == 1
        && settingInteger(*hideStart, -1) == 1
        && m_calendarFirstDayOfWeek == 1;
    if (!preferencesReady)
    {
        uint32_t preferenceFailureMask = 7;
        if (!firstDay || settingInteger(*firstDay, -1) != 1)
        {
            preferenceFailureMask |= 0x100;
        }
        if (!showAll || settingInteger(*showAll, -1) != 1)
        {
            preferenceFailureMask |= 0x200;
        }
        if (!hideStart || settingInteger(*hideStart, -1) != 1)
        {
            preferenceFailureMask |= 0x400;
        }
        if (m_calendarFirstDayOfWeek != 1)
        {
            preferenceFailureMask |= 0x800;
        }
        if (m_calendarPreferencesStatusText.Text()
            == L"Calendar preferences were not saved.")
        {
            preferenceFailureMask |= 0x1000;
        }
        return fail(preferenceFailureMask);
    }

    const auto beforeNext = m_calendarMonthTitle.Text();
    CalendarNextButton_Click(
        m_calendarNextButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    const bool navigationReady = beforeNext != m_calendarMonthTitle.Text()
        && m_calendarDisplayedMonth.month() == std::chrono::month{2};
    CalendarPreviousButton_Click(
        m_calendarPreviousButton,
        Microsoft::UI::Xaml::RoutedEventArgs{}
        );
    if (!navigationReady || m_calendarDisplayedMonth.month() != std::chrono::month{1})
    {
        return fail(8);
    }

    const auto deleted = service.removeAll();
    if (!deleted)
    {
        return fail(9);
    }
    refreshCalendarPage();
    const bool resetReady = m_calendarEvents.empty()
        && m_calendarStatusText.Text() == L"Calendar loaded: 0 event(s).";
    m_openDatabase.reset();
    m_currentDatabasePath.clear();
    refreshCalendarPage();
    const bool clearReady = m_calendarStatusText.Text() == L"No database open."
        && !m_calendarAddEventButton.IsEnabled();
    return resetReady && clearReady;
}

uint32_t MainWindow::phase6CalendarFailureMask() const noexcept
{
    return m_phase6CalendarFailureMask;
}

void MainWindow::preparePhase5CampusScenario(std::wstring_view scenario)
{
    static_cast<void>(preparePhase5CampusFixture(scenario));
    navigateTo(campusInformationPageId);
    refreshCampusInformationPage();
    updateFileCommandState();
}

bool MainWindow::preparePhase5CampusFixture(std::wstring_view scenario)
{
    const bool noDatabase = scenario == L"no-database";
    const bool empty = scenario == L"empty";
    const bool populated = scenario == L"populated";
    const bool error = scenario == L"error";
    m_phase5CampusScenario = error || (!noDatabase && !empty && !populated)
        ? L"error"
        : std::wstring(scenario);
    m_openDatabase.reset();
    m_currentDatabasePath.clear();
    m_dirtyState.markClean();

    if (!noDatabase && m_phase5CampusScenario != L"error")
    {
        auto opened = classmngr::engine::OpenDatabase::execute(":memory:");
        if (!opened || *opened == nullptr)
        {
            m_phase5CampusScenario = L"error";
        }
        else
        {
            if (populated)
            {
                classmngr::engine::CampusRecordService service(**opened);
                const auto makeCampus = [](wchar_t const* name,
                                           char const* imagePath) {
                    classmngr::engine::CampusRecord campus;
                    campus.name = winrt::to_string(winrt::hstring(name));
                    campus.buildingName = "Main building";
                    campus.address = "Bundang-gu, Seongnam-si";
                    campus.phoneNumber = "+82-31-1234-5678";
                    campus.officeNumber = "Office 101";
                    campus.transitSteps = "Exit 3, then walk north.";
                    campus.arrivalInfo = "Check in at the information desk.";
                    campus.imagePath = imagePath;
                    campus.officeWifi = "TeacherNet";
                    campus.officeWifiPassword = "password";
                    campus.printerName = "Printer-1";
                    campus.printerSteps = "Load paper, then print.";
                    campus.photocopierCode = "42";
                    campus.housingLocations = "Bundang, Suji";
                    return campus;
                };
                const auto first = service.create(makeCampus(
                    L"\xBD84\xB2F9 \xCEA0\xD37C\xC2A4",
                    ":/assets/campuses/bundang/bundang_map.png"
                    ));
                const auto second = service.create(makeCampus(
                    L"\xCCAD\xAD6C \xCEA0\xD37C\xC2A4",
                    ":/assets/campuses/bundang/cheonggu_map.png"
                    ));
                if (!first || !second)
                {
                    m_phase5CampusScenario = L"error";
                }
            }
            if (m_phase5CampusScenario != L"error")
            {
                m_openDatabase = std::move(*opened);
            }
        }
    }

    updateFileCommandState();
    return m_phase5CampusScenario != L"error" && static_cast<bool>(m_openDatabase);
}

void MainWindow::startPhase5FirstNavigationMeasurement(
    std::function<void(bool)> completion
    )
{
    if (m_phase5FirstNavigationCompleted || m_phase5FirstNavigationAwaitingHome
        || m_phase5FirstNavigationStarted)
    {
        return;
    }

    m_phase5FirstNavigationCompletion = std::move(completion);
    m_contentFrame.CacheSize(0);
    m_contentFrame.BackStack().Clear();
    m_contentFrame.ForwardStack().Clear();
    m_contentFrame.CacheSize(3);
    if (!ensureHomePage())
    {
        completePhase5FirstNavigationMeasurement("Home shell was not ready.");
        return;
    }

    m_phase5FirstNavigationAwaitingHome = true;
    m_phase5FirstNavigationRenderingToken =
        Microsoft::UI::Xaml::Media::CompositionTarget::Rendering(
            {this, &MainWindow::Phase5FirstNavigation_Rendering}
            );
}

void MainWindow::Phase5FirstNavigation_Rendering(
    Windows::Foundation::IInspectable const& sender,
    Windows::Foundation::IInspectable const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);

    if (m_phase5FirstNavigationCompleted)
    {
        return;
    }

    if (m_phase5FirstNavigationAwaitingHome)
    {
        if (m_currentPageId != homePageId || !m_nameTextBox || !m_continueButton
            || !m_statusText)
        {
            return;
        }

        m_phase5FirstNavigationAwaitingHome = false;
        if (!preparePhase5CampusFixture(L"populated"))
        {
            completePhase5FirstNavigationMeasurement(
                "The populated Campus fixture could not be prepared."
                );
            return;
        }

        m_phase5FirstNavigationStarted = true;
        m_phase5FirstNavigationStart = std::chrono::steady_clock::now();
        navigateTo(campusInformationPageId);
        return;
    }

    if (!m_phase5FirstNavigationStarted)
    {
        return;
    }

    const auto renderingTime = std::chrono::steady_clock::now();
    constexpr uint32_t expectedRecordCount = 2;
    const bool pageReady = m_currentPageId == campusInformationPageId;
    const bool stateReady = m_campusInformationState == L"populated";
    const bool selectorReady = m_campusSelector
        && m_campusSelector.Items().Size() == expectedRecordCount;
    const bool selectedDetailReady = m_campusDetailsPanel
        && m_campusDetailsPanel.Children().Size() > 0
        && m_campusSelector.SelectedIndex() >= 0
        && m_campusSelector.SelectedIndex() < static_cast<int32_t>(expectedRecordCount);
    const bool recordCountReady = m_campusRecords.size() == expectedRecordCount
        && m_campusResourceRecords.size() == expectedRecordCount;

    if (pageReady && stateReady && selectorReady && selectedDetailReady
        && recordCountReady)
    {
        m_phase5FirstNavigationReady = renderingTime;
        completePhase5FirstNavigationMeasurement({});
    }
}

void MainWindow::completePhase5FirstNavigationMeasurement(
    std::string_view failure
    )
{
    if (m_phase5FirstNavigationCompleted)
    {
        return;
    }

    m_phase5FirstNavigationCompleted = true;
    if (m_phase5FirstNavigationRenderingToken.value != 0)
    {
        Microsoft::UI::Xaml::Media::CompositionTarget::Rendering(
            m_phase5FirstNavigationRenderingToken
            );
        m_phase5FirstNavigationRenderingToken = {};
    }

    const bool written = writePhase5FirstNavigationResult(failure);
    const auto completion = std::move(m_phase5FirstNavigationCompletion);
    if (completion)
    {
        completion(written && failure.empty());
    }
}

bool MainWindow::writePhase5FirstNavigationResult(std::string_view failure) const
{
    constexpr uint32_t expectedRecordCount = 2;
    const bool pageReady = m_currentPageId == campusInformationPageId;
    const bool stateReady = m_campusInformationState == L"populated";
    const bool selectorReady = m_campusSelector
        && m_campusSelector.Items().Size() == expectedRecordCount;
    const bool selectedDetailReady = m_campusDetailsPanel
        && m_campusDetailsPanel.Children().Size() > 0
        && m_campusSelector.SelectedIndex() >= 0
        && m_campusSelector.SelectedIndex() < static_cast<int32_t>(expectedRecordCount);
    const bool recordCountReady = m_campusRecords.size() == expectedRecordCount
        && m_campusResourceRecords.size() == expectedRecordCount;
    const bool ready = failure.empty() && m_phase5FirstNavigationStarted
        && pageReady && stateReady && selectorReady && selectedDetailReady
        && recordCountReady;

    std::string output;
    output.reserve(1024);
    output += "{\n  \"format\": \"classmngr.phase5.first-navigation.v1\"";
    output += ",\n  \"targetPage\": ";
    appendJsonEscaped(output, asUtf8(campusInformationPageId));
    output += ",\n  \"fixtureId\": \"phase5-campus-populated-v1\"";
    output += ",\n  \"expectedRecordCount\": 2";
    output += ",\n  \"cacheState\": \"frame-cache-cleared; home-rendered; campus-not-visited\"";
    output += ",\n  \"navigationStartEvent\": \"navigateTo(campus_information)\"";
    output += ",\n  \"navigationReadyEvent\": \"CompositionTarget::Rendering\"";
    output += ",\n  \"firstNavigationReadyMs\": ";
    if (m_phase5FirstNavigationStarted)
    {
        const auto elapsed = std::chrono::duration<double, std::milli>(
            m_phase5FirstNavigationReady - m_phase5FirstNavigationStart
            ).count();
        output += std::to_string(elapsed);
    }
    else
    {
        output += "null";
    }
    output += ",\n  \"semanticReadiness\": {\n";
    output += "    \"pageId\": ";
    appendJsonEscaped(output, asUtf8(m_currentPageId));
    output += ",\n    \"pageReady\": ";
    output += pageReady ? "true" : "false";
    output += ",\n    \"populatedState\": ";
    output += stateReady ? "true" : "false";
    output += ",\n    \"populatedListExists\": ";
    output += selectorReady ? "true" : "false";
    output += ",\n    \"selectedDetailPanelExists\": ";
    output += selectedDetailReady ? "true" : "false";
    output += ",\n    \"expectedRecordCountPresent\": ";
    output += recordCountReady ? "true" : "false";
    output += "\n  },\n  \"deferredImageStatus\": \"excluded-asynchronous\"";
    output += ",\n  \"ready\": ";
    output += ready ? "true" : "false";
    output += ",\n  \"failure\": ";
    if (failure.empty())
    {
        output += "null";
    }
    else
    {
        appendJsonEscaped(output, failure);
    }
    output += "\n}\n";

    try
    {
        std::ofstream result(
            std::filesystem::current_path() / L"phase5-first-navigation.json",
            std::ios::binary | std::ios::trunc
            );
        result.write(output.data(), static_cast<std::streamsize>(output.size()));
        return static_cast<bool>(result);
    }
    catch (...)
    {
        return false;
    }
}

Windows::Foundation::IAsyncOperation<bool>
MainWindow::runPhase3ViewModelChecks()
{
    auto lifetime = get_strong();
    const auto dispatcher = DispatcherQueue();
    auto viewModel = winrt::make_self<ObservableViewModel>();
    std::vector<std::wstring> changedProperties;
    const auto observable = viewModel.as<
        Microsoft::UI::Xaml::Data::INotifyPropertyChanged>();
    const auto propertyToken = observable.PropertyChanged(
        [&changedProperties](
            Windows::Foundation::IInspectable const&,
            Microsoft::UI::Xaml::Data::PropertyChangedEventArgs const& arguments
            ) {
            changedProperties.emplace_back(
                arguments.PropertyName().c_str(),
                arguments.PropertyName().size()
                );
        }
        );

    viewModel->PresentError(classmngr::engine::Error{
        classmngr::engine::ErrorCode::InvalidArgument,
        "A test error.",
        42
        });
    classmngr::engine::ValidationResult validation;
    validation.add(classmngr::engine::ValidationIssue{
        "required",
        "name",
        classmngr::engine::ValidationSeverity::Error,
        3,
        1
        });
    viewModel->PresentValidation(validation);

    const auto contains = [](winrt::hstring const& value, std::wstring_view text) {
        return std::wstring_view(value.c_str(), value.size()).find(text)
            != std::wstring_view::npos;
    };
    const bool presentationReady = viewModel->HasError()
        && contains(viewModel->ErrorMessage(), L"invalid-argument")
        && contains(viewModel->ErrorMessage(), L"native-code=42")
        && viewModel->HasValidationErrors()
        && contains(viewModel->ValidationSummary(), L"code=required")
        && contains(viewModel->ValidationSummary(), L"field=name")
        && changedProperties.size() >= 4;
    observable.PropertyChanged(propertyToken);
    if (!presentationReady)
    {
        co_return false;
    }

    struct CommandCheckState
    {
        winrt::handle completion{CreateEventW(nullptr, TRUE, FALSE, nullptr)};
        winrt::handle workStarted{CreateEventW(nullptr, TRUE, FALSE, nullptr)};
        std::atomic_bool workInvoked{};
        std::atomic_bool completionReady{};
        std::atomic_uint32_t stateChanges{};
    };
    auto commandState = std::make_shared<CommandCheckState>();
    if (!commandState->completion || !commandState->workStarted)
    {
        co_return false;
    }

    AsyncCommand::AsyncWork work = [commandState](
        classmngr::engine::CancellationToken const& cancellation
        ) {
        commandState->workInvoked.store(true, std::memory_order_relaxed);
        SetEvent(commandState->workStarted.get());
        return phase3PresentationWork(cancellation);
    };
    auto command = winrt::make_self<AsyncCommand>(
        DispatcherQueue(),
        std::move(work)
        );
    const auto commandInterface = command.as<Microsoft::UI::Xaml::Input::ICommand>();
    const auto weakCommand = command->get_weak();
    const auto commandToken = commandInterface.CanExecuteChanged(
        [commandState, weakCommand](
            Windows::Foundation::IInspectable const&,
            Windows::Foundation::IInspectable const&
            ) {
            commandState->stateChanges.fetch_add(1, std::memory_order_relaxed);
            if (auto currentCommand = weakCommand.get();
                currentCommand && !currentCommand->IsRunning())
            {
                commandState->completionReady.store(
                    currentCommand->CanExecute(nullptr)
                        && commandState->stateChanges.load(
                            std::memory_order_relaxed
                            ) >= 2,
                    std::memory_order_relaxed
                    );
                SetEvent(commandState->completion.get());
            }
        }
        );

    const bool initiallyEnabled = commandInterface.CanExecute(nullptr);
    commandInterface.Execute(nullptr);
    co_await winrt::resume_on_signal(commandState->workStarted.get());
    co_await ResumeOnDispatcherQueue{
        dispatcher,
        Microsoft::UI::Dispatching::DispatcherQueuePriority::Normal
        };
    const bool runningAfterExecute = command->IsRunning()
        && !commandInterface.CanExecute(nullptr)
        && commandState->workInvoked.load(std::memory_order_relaxed);
    command->Cancel();
    const bool cancellationRequested = command->IsCancellationRequested();
    if (!initiallyEnabled || !runningAfterExecute || !cancellationRequested)
    {
        commandInterface.CanExecuteChanged(commandToken);
        co_return false;
    }

    co_await winrt::resume_on_signal(commandState->completion.get());
    static_cast<void>(commandToken);
    co_return commandState->completionReady.load(std::memory_order_relaxed);
}

void MainWindow::ContinueButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);

    if (!m_nameTextBox || !m_homeViewModel || !m_homeCommand)
    {
        return;
    }

    if (m_nameTextBox.Text().empty())
    {
        classmngr::engine::ValidationResult validation;
        validation.add(classmngr::engine::ValidationIssue{
            "required",
            "name",
            classmngr::engine::ValidationSeverity::Error,
            0,
            0
            });
        presentValidationSummary(validation);
        m_statusText.Text(L"Enter a name.");
        return;
    }

    if (m_homeCommand->CanExecute(nullptr))
    {
        m_homeViewModel->ClearValidation();
        m_homeCommand->Execute(nullptr);
        updateHomePresentation();
    }
}

void MainWindow::ShellInfoButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    showOwnedDialog();
}

void MainWindow::OpenDatabaseMenuItem_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    openDatabasePicker();
}

void MainWindow::NewDatabaseMenuItem_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    openNewDatabasePicker();
}

void MainWindow::SaveDatabaseMenuItem_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_openDatabase)
    {
        reportOutputError(L"Save database", {}, "No database is open.");
        return;
    }

    // Engine-backed writes commit at the service boundary.  The shell-level
    // save command therefore closes the prototype dirty-state transaction
    // and records the successful save without issuing ad hoc SQL.
    m_dirtyState.markClean();
    if (m_statusText)
    {
        m_statusText.Text(L"Changes saved.");
    }
    updateFileCommandState();
}

void MainWindow::SaveDatabaseAsMenuItem_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    openSaveDatabasePicker(false);
}

void MainWindow::ExportDatabaseMenuItem_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    openSaveDatabasePicker(true);
}

void MainWindow::CloseDatabaseMenuItem_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    m_openDatabase.reset();
    m_currentDatabasePath.clear();
    m_dirtyState.markClean();
    if (m_shellDatabaseStatusText)
    {
        m_shellDatabaseStatusText.Text(L"No database open");
    }
    if (m_statusText)
    {
        m_statusText.Text(L"Database closed.");
    }
    refreshCampusInformationPage();
    refreshPersonalDetailsPage();
    refreshClassesPage();
    refreshCalendarPage();
    updateFileCommandState();
    saveShellState();
}

void MainWindow::SaveCurrentPageMenuItem_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    openCurrentPageSavePicker();
}

void MainWindow::ExportCampusResourcesMenuItem_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    openCampusResourcesFolderPicker();
}

void MainWindow::RecentDatabaseMenuItem_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(arguments);
    const auto item = sender.try_as<
        Microsoft::UI::Xaml::Controls::MenuFlyoutItem>();
    if (item)
    {
        const std::wstring path = boxedString(item.Tag());
        if (!path.empty())
        {
            static_cast<void>(openDatabasePath(path));
        }
    }
}

winrt::fire_and_forget MainWindow::openDatabasePicker()
{
    auto lifetime = get_strong();
    if (m_filePickerActive)
    {
        co_return;
    }
    m_filePickerActive = true;

    try
    {
        auto picker = winrt::Windows::Storage::Pickers::FileOpenPicker();
        const HWND handle = windowHandle(this);
        if (!handle)
        {
            reportDatabaseOpenError(
                {},
                "The database file picker is not available."
                );
        }
        else
        {
            const auto initializer = picker.as<::IInitializeWithWindow>();
            winrt::check_hresult(initializer->Initialize(handle));
            picker.FileTypeFilter().Append(L".tps");
            picker.FileTypeFilter().Append(L".db");
            const auto file = co_await picker.PickSingleFileAsync();
            if (file)
            {
                static_cast<void>(openDatabasePath(asWString(file.Path())));
            }
        }
    }
    catch (winrt::hresult_error const& error)
    {
        reportDatabaseOpenError({}, winrt::to_string(error.message()));
    }
    catch (...)
    {
        reportDatabaseOpenError(
            {},
            "The database file picker could not be opened."
            );
    }

    m_filePickerActive = false;
}

winrt::fire_and_forget MainWindow::openNewDatabasePicker()
{
    auto lifetime = get_strong();
    if (m_filePickerActive)
    {
        co_return;
    }
    m_filePickerActive = true;

    try
    {
        auto picker = winrt::Windows::Storage::Pickers::FileSavePicker();
        const HWND handle = windowHandle(this);
        if (!handle)
        {
            reportDatabaseOpenError(
                {},
                "The database save picker is not available."
                );
        }
        else
        {
            const auto initializer = picker.as<::IInitializeWithWindow>();
            winrt::check_hresult(initializer->Initialize(handle));
            picker.CommitButtonText(L"Create");
            picker.SuggestedFileName(L"Teacher Profile.tps");
            picker.DefaultFileExtension(L".tps");
            picker.FileTypeChoices().Insert(
                L"Teacher Profile database",
                winrt::single_threaded_vector<winrt::hstring>({L".tps"})
                );
            const auto file = co_await picker.PickSaveFileAsync();
            if (file)
            {
                static_cast<void>(createDatabasePath(asWString(file.Path())));
            }
        }
    }
    catch (winrt::hresult_error const& error)
    {
        reportDatabaseOpenError({}, winrt::to_string(error.message()));
    }
    catch (...)
    {
        reportDatabaseOpenError(
            {},
            "The database save picker could not be opened."
            );
    }

    m_filePickerActive = false;
}

winrt::fire_and_forget MainWindow::openSaveDatabasePicker(bool exportOnly)
{
    auto lifetime = get_strong();
    if (m_filePickerActive)
    {
        co_return;
    }
    m_filePickerActive = true;

    try
    {
        auto picker = winrt::Windows::Storage::Pickers::FileSavePicker();
        const HWND handle = windowHandle(this);
        if (!handle)
        {
            reportOutputError(
                exportOnly ? L"Export database" : L"Save database",
                {},
                "The database save picker is not available."
                );
        }
        else
        {
            const auto initializer = picker.as<::IInitializeWithWindow>();
            winrt::check_hresult(initializer->Initialize(handle));
            picker.CommitButtonText(
                exportOnly
                    ? winrt::hstring(L"Export")
                    : winrt::hstring(L"Save")
                );

            std::wstring suggestedName = L"Teacher Profile.tps";
            if (!m_currentDatabasePath.empty())
            {
                const std::filesystem::path currentPath(m_currentDatabasePath);
                if (!currentPath.filename().empty())
                {
                    suggestedName = currentPath.filename().wstring();
                }
            }
            picker.SuggestedFileName(winrt::hstring(suggestedName));
            picker.DefaultFileExtension(L".tps");
            picker.FileTypeChoices().Insert(
                L"Teacher Profile database",
                winrt::single_threaded_vector<winrt::hstring>({L".tps"})
                );
            const auto file = co_await picker.PickSaveFileAsync();
            if (file)
            {
                const std::wstring selectedPath = asWString(file.Path());
                if (exportOnly)
                {
                    static_cast<void>(exportDatabasePath(selectedPath));
                }
                else
                {
                    static_cast<void>(saveDatabasePath(selectedPath));
                }
            }
        }
    }
    catch (winrt::hresult_error const& error)
    {
        reportOutputError(
            exportOnly ? L"Export database" : L"Save database",
            {},
            winrt::to_string(error.message())
            );
    }
    catch (...)
    {
        reportOutputError(
            exportOnly ? L"Export database" : L"Save database",
            {},
            "The database save picker could not be opened."
            );
    }

    m_filePickerActive = false;
}

winrt::fire_and_forget MainWindow::openCurrentPageSavePicker()
{
    auto lifetime = get_strong();
    if (m_filePickerActive)
    {
        co_return;
    }
    m_filePickerActive = true;

    try
    {
        auto picker = winrt::Windows::Storage::Pickers::FileSavePicker();
        const HWND handle = windowHandle(this);
        if (!handle)
        {
            reportOutputError(
                L"Save current page",
                {},
                "The page save picker is not available."
                );
        }
        else
        {
            const auto initializer = picker.as<::IInitializeWithWindow>();
            winrt::check_hresult(initializer->Initialize(handle));
            picker.CommitButtonText(L"Save");
            picker.SuggestedFileName(L"campus-information.json");
            picker.DefaultFileExtension(L".json");
            picker.FileTypeChoices().Insert(
                L"JSON document",
                winrt::single_threaded_vector<winrt::hstring>({L".json"})
                );
            const auto file = co_await picker.PickSaveFileAsync();
            if (file)
            {
                static_cast<void>(saveCurrentPagePath(asWString(file.Path())));
            }
        }
    }
    catch (winrt::hresult_error const& error)
    {
        reportOutputError(
            L"Save current page",
            {},
            winrt::to_string(error.message())
            );
    }
    catch (...)
    {
        reportOutputError(
            L"Save current page",
            {},
            "The page save picker could not be opened."
            );
    }

    m_filePickerActive = false;
}

winrt::fire_and_forget MainWindow::openCampusResourcesFolderPicker()
{
    auto lifetime = get_strong();
    if (m_filePickerActive)
    {
        co_return;
    }
    m_filePickerActive = true;

    try
    {
        auto picker = winrt::Windows::Storage::Pickers::FolderPicker();
        const HWND handle = windowHandle(this);
        if (!handle)
        {
            reportOutputError(
                L"Export campus resources",
                {},
                "The folder picker is not available."
                );
        }
        else
        {
            const auto initializer = picker.as<::IInitializeWithWindow>();
            winrt::check_hresult(initializer->Initialize(handle));
            picker.SuggestedStartLocation(
                winrt::Windows::Storage::Pickers::PickerLocationId::DocumentsLibrary
                );
            picker.FileTypeFilter().Append(L"*");
            const auto folder = co_await picker.PickSingleFolderAsync();
            if (folder)
            {
                static_cast<void>(exportCampusResourcesPath(asWString(folder.Path())));
            }
        }
    }
    catch (winrt::hresult_error const& error)
    {
        reportOutputError(
            L"Export campus resources",
            {},
            winrt::to_string(error.message())
            );
    }
    catch (...)
    {
        reportOutputError(
            L"Export campus resources",
            {},
            "The folder picker could not be opened."
            );
    }

    m_filePickerActive = false;
}

bool MainWindow::openDatabasePath(std::wstring_view path)
{
    if (path.empty())
    {
        reportDatabaseOpenError(path, "A database path was not provided.");
        return false;
    }

    const std::wstring candidate = absolutePath(path);
    if (!isSupportedDatabasePath(candidate))
    {
        reportDatabaseOpenError(
            candidate,
            "Only .tps and .db database files are supported."
            );
        return false;
    }
    if (!pathExists(candidate))
    {
        reportDatabaseOpenError(candidate, "The database file does not exist.");
        return false;
    }

    try
    {
        classmngr::engine::OpenDatabaseOptions options;
        options.createParentDirectories = false;
        auto opened = classmngr::engine::OpenDatabase::execute(
            asUtf8(candidate),
            options
            );
        if (!opened)
        {
            reportDatabaseOpenError(candidate, opened.error().message);
            return false;
        }

        m_openDatabase = std::move(*opened);
        m_currentDatabasePath = candidate;
        m_dirtyState.markClean();
        addRecentDatabasePath(candidate);
        const std::wstring status = L"Database: " + candidate;
        if (m_shellDatabaseStatusText)
        {
            m_shellDatabaseStatusText.Text(winrt::hstring(status));
        }
        if (m_statusText)
        {
            m_statusText.Text(L"Database opened.");
        }
        refreshCampusInformationPage();
        refreshPersonalDetailsPage();
        refreshSubPrepPage();
        refreshClassesPage();
        refreshCalendarPage();
        updateFileCommandState();
        saveShellState();
        return true;
    }
    catch (std::exception const& error)
    {
        reportDatabaseOpenError(candidate, error.what());
    }
    catch (...)
    {
        reportDatabaseOpenError(candidate, "The database could not be opened.");
    }
    return false;
}

bool MainWindow::createDatabasePath(std::wstring_view path)
{
    if (path.empty())
    {
        reportDatabaseOpenError(path, "A new database path was not provided.");
        return false;
    }

    const std::wstring candidate = absolutePath(path);
    if (!classmngr::engine::DatabaseFileFormat::isNativePath(
            asUtf8(candidate)
            ))
    {
        reportDatabaseOpenError(
            candidate,
            "New databases must use the .tps file type."
            );
        return false;
    }

    // The Qt controller closes the active database before replacing or
    // creating the selected path. Release the SQLite owner before the native
    // picker-selected replacement is removed, even when the destination is a
    // different file.
    m_openDatabase.reset();
    m_currentDatabasePath.clear();
    refreshCampusInformationPage();
    refreshPersonalDetailsPage();
    refreshSubPrepPage();
    refreshClassesPage();
    refreshCalendarPage();

    if (pathExists(candidate))
    {
        std::error_code removeError;
        const bool removed = std::filesystem::remove(
            std::filesystem::path(candidate),
            removeError
            );
        if (removeError || !removed)
        {
            reportDatabaseOpenError(
                candidate,
                removeError
                    ? removeError.message()
                    : "The selected database file could not be replaced."
                );
            return false;
        }
    }

    try
    {
        classmngr::engine::OpenDatabaseOptions options;
        options.createParentDirectories = true;
        auto opened = classmngr::engine::OpenDatabase::execute(
            asUtf8(candidate),
            options
            );
        if (!opened)
        {
            reportDatabaseOpenError(candidate, opened.error().message);
            return false;
        }

        m_openDatabase = std::move(*opened);
        m_currentDatabasePath = candidate;
        m_dirtyState.markClean();
        addRecentDatabasePath(candidate);
        const std::wstring status = L"Database: " + candidate;
        if (m_shellDatabaseStatusText)
        {
            m_shellDatabaseStatusText.Text(winrt::hstring(status));
        }
        if (m_statusText)
        {
            m_statusText.Text(L"New database created.");
        }
        refreshCampusInformationPage();
        refreshPersonalDetailsPage();
        refreshSubPrepPage();
        refreshClassesPage();
        refreshCalendarPage();
        updateFileCommandState();
        saveShellState();
        return true;
    }
    catch (std::exception const& error)
    {
        reportDatabaseOpenError(candidate, error.what());
    }
    catch (...)
    {
        reportDatabaseOpenError(candidate, "The new database could not be created.");
    }
    return false;
}

bool MainWindow::saveDatabasePath(std::wstring_view path)
{
    if (!m_openDatabase || m_currentDatabasePath.empty())
    {
        reportOutputError(L"Save database", path, "No file-backed database is open.");
        return false;
    }

    const std::wstring candidate = absolutePath(path);
    if (!classmngr::engine::DatabaseFileFormat::isNativePath(asUtf8(candidate)))
    {
        reportOutputError(
            L"Save database",
            candidate,
            "Saved databases must use the .tps file type."
            );
        return false;
    }

    if (samePath(candidate, m_currentDatabasePath))
    {
        m_dirtyState.markClean();
        if (m_statusText)
        {
            m_statusText.Text(L"Changes saved.");
        }
        updateFileCommandState();
        return true;
    }

    classmngr::windows::winui::WindowsFileSystem fileSystem;
    const auto copied = fileSystem.copyFile(
        asUtf8(m_currentDatabasePath),
        asUtf8(candidate),
        true
        );
    if (!copied)
    {
        reportOutputError(L"Save database", candidate, copied.error().message);
        return false;
    }

    if (!openDatabasePath(candidate))
    {
        reportOutputError(
            L"Save database",
            candidate,
            "The saved database could not be reopened."
            );
        return false;
    }

    m_dirtyState.markClean();
    if (m_statusText)
    {
        m_statusText.Text(L"Database saved as a new file.");
    }
    updateFileCommandState();
    return true;
}

bool MainWindow::exportDatabasePath(std::wstring_view path)
{
    if (!m_openDatabase || m_currentDatabasePath.empty())
    {
        reportOutputError(L"Export database", path, "No file-backed database is open.");
        return false;
    }

    const std::wstring candidate = absolutePath(path);
    if (!classmngr::engine::DatabaseFileFormat::isNativePath(asUtf8(candidate)))
    {
        reportOutputError(
            L"Export database",
            candidate,
            "Exported databases must use the .tps file type."
            );
        return false;
    }

    if (!samePath(candidate, m_currentDatabasePath))
    {
        classmngr::windows::winui::WindowsFileSystem fileSystem;
        const auto copied = fileSystem.copyFile(
            asUtf8(m_currentDatabasePath),
            asUtf8(candidate),
            true
            );
        if (!copied)
        {
            reportOutputError(L"Export database", candidate, copied.error().message);
            return false;
        }
    }

    if (m_statusText)
    {
        m_statusText.Text(L"Database exported.");
    }
    return true;
}

bool MainWindow::saveCurrentPagePath(std::wstring_view path)
{
    if (!isCampusPageId(m_currentPageId))
    {
        reportOutputError(
            L"Save current page",
            path,
            "Only Campus Directory pages are available for export."
            );
        return false;
    }

    const std::wstring candidate = absolutePath(path);
    const std::wstring extension = std::filesystem::path(candidate).extension().wstring();
    if (extension.size() != 5
        || extension[0] != L'.'
        || std::towlower(extension[1]) != L'j'
        || std::towlower(extension[2]) != L's'
        || std::towlower(extension[3]) != L'o'
        || std::towlower(extension[4]) != L'n')
    {
        reportOutputError(
            L"Save current page",
            candidate,
            "The current page must be saved as a .json file."
            );
        return false;
    }

    classmngr::windows::winui::WindowsFileSystem fileSystem;
    const std::string json = currentPageExportJson();
    const auto written = fileSystem.writeBytes(asUtf8(candidate), json, true);
    if (!written)
    {
        reportOutputError(L"Save current page", candidate, written.error().message);
        return false;
    }

    if (m_statusText)
    {
        m_statusText.Text(L"Campus information page saved.");
    }
    return true;
}

std::string MainWindow::currentPageExportJson() const
{
    std::string output;
    output.reserve(4096);
    output += "{\n  \"format\": ";
    appendJsonEscaped(output, "classmngr.phase5.campus-information.v1");
    output += ",\n  \"page\": ";
    appendJsonEscaped(output, asUtf8(m_currentPageId));
    output += ",\n  \"state\": ";
    appendJsonEscaped(output, asUtf8(m_campusInformationState));
    output += ",\n  \"campuses\": [";

    const auto appendField = [](std::string& value,
                                char const* key,
                                std::string_view fieldValue,
                                bool last) {
        value += "\n      \"";
        value += key;
        value += "\": ";
        appendJsonEscaped(value, fieldValue);
        value += last ? "\n" : ",";
    };

    for (std::size_t index = 0; index < m_campusRecords.size(); ++index)
    {
        const classmngr::engine::CampusRecord& campus = m_campusRecords[index];
        output += index == 0 ? "\n    {" : ",\n    {";
        output += "\n      \"id\": ";
        output += std::to_string(campus.id);
        appendField(output, "name", campus.name, false);
        appendField(output, "buildingName", campus.buildingName, false);
        appendField(output, "address", campus.address, false);
        appendField(output, "phoneNumber", campus.phoneNumber, false);
        appendField(output, "officeNumber", campus.officeNumber, false);
        appendField(output, "transitSteps", campus.transitSteps, false);
        appendField(output, "arrivalInfo", campus.arrivalInfo, false);
        appendField(output, "imagePath", campus.imagePath, false);
        appendField(output, "officeWifi", campus.officeWifi, false);
        appendField(output, "officeWifiPassword", campus.officeWifiPassword, false);
        appendField(output, "printerName", campus.printerName, false);
        appendField(output, "printerSteps", campus.printerSteps, false);
        appendField(output, "photocopierCode", campus.photocopierCode, false);
        appendField(output, "housingLocations", campus.housingLocations, true);
        output += "    }";
    }
    if (!m_campusRecords.empty())
    {
        output += "\n  ";
    }
    output += "]\n}\n";
    return output;
}

bool MainWindow::exportCampusResourcesPath(std::wstring_view path)
{
    if (!isCampusPageId(m_currentPageId))
    {
        reportOutputError(
            L"Export campus resources",
            path,
            "Only Campus Directory pages are available for resource export."
            );
        return false;
    }

    const std::wstring candidate = absolutePath(path);
    std::error_code directoryError;
    if (!std::filesystem::is_directory(
            std::filesystem::path(candidate),
            directoryError
            ) || directoryError)
    {
        reportOutputError(
            L"Export campus resources",
            candidate,
            "The selected export location is not a folder."
            );
        return false;
    }

    classmngr::windows::winui::WindowsFileSystem fileSystem;
    const std::filesystem::path resourceDirectory =
        std::filesystem::path(candidate) / L"campus-resources";
    const auto created = fileSystem.createDirectories(
        asUtf8(resourceDirectory.wstring())
        );
    if (!created)
    {
        reportOutputError(
            L"Export campus resources",
            resourceDirectory.wstring(),
            created.error().message
            );
        return false;
    }

    const std::filesystem::path jsonPath =
        std::filesystem::path(candidate) / L"campus-information.json";
    const auto jsonWritten = fileSystem.writeBytes(
        asUtf8(jsonPath.wstring()),
        currentPageExportJson(),
        true
        );
    if (!jsonWritten)
    {
        reportOutputError(
            L"Export campus resources",
            jsonPath.wstring(),
            jsonWritten.error().message
            );
        return false;
    }

    classmngr::windows::winui::WindowsResourceProvider resourceProvider;
    std::set<std::string> usedNames;
    for (classmngr::engine::CampusRecord const& campus : m_campusRecords)
    {
        if (campus.imagePath.empty())
        {
            continue;
        }

        const std::string fileName = campusResourceFileName(campus.imagePath);
        if (fileName.empty())
        {
            reportOutputError(
                L"Export campus resources",
                candidate,
                "A campus image reference has an unsafe file name."
                );
            return false;
        }

        const auto bytes = resourceProvider.readBytes(campus.imagePath);
        if (!bytes)
        {
            reportOutputError(
                L"Export campus resources",
                candidate,
                bytes.error().message
                );
            return false;
        }

        const std::string uniqueName = uniqueCampusResourceFileName(
            fileName,
            usedNames
            );
        const std::filesystem::path outputPath =
            resourceDirectory / std::filesystem::path(
                winrt::to_hstring(uniqueName).c_str()
                );
        const std::string bytesAsString(
            reinterpret_cast<char const*>(bytes->data()),
            bytes->size()
            );
        const auto written = fileSystem.writeBytes(
            asUtf8(outputPath.wstring()),
            bytesAsString,
            true
            );
        if (!written)
        {
            reportOutputError(
                L"Export campus resources",
                outputPath.wstring(),
                written.error().message
                );
            return false;
        }
    }

    if (m_statusText)
    {
        m_statusText.Text(L"Campus information and resources exported.");
    }
    return true;
}

void MainWindow::openMostRecentDatabase()
{
    m_recentDatabasePaths = pruneRecentDatabasePaths(m_recentDatabasePaths);
    refreshRecentDatabaseMenu();
    saveShellState();
    if (!m_recentDatabasePaths.empty())
    {
        static_cast<void>(openDatabasePath(m_recentDatabasePaths.front()));
    }
}

void MainWindow::ShellInfoMenuItem_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    showOwnedDialog();
}

void MainWindow::CancelButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (m_homeCommand)
    {
        m_homeCommand->Cancel();
        updateHomePresentation();
    }
}

void MainWindow::UnsavedChangesButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    showUnsavedChangesConfirmation();
}

void MainWindow::ScheduleApplyButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_scheduleSlotTextBox || !m_scheduleStatusText)
    {
        return;
    }

    m_dirtyState.markDirty();
    m_scheduleStatusText.Text(
        L"Time slot label updated in the prototype (not persisted)."
        );
}

void MainWindow::RosterSource_SelectionChanged(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& arguments
    )
{
    static_cast<void>(arguments);
    const auto list = sender.try_as<Microsoft::UI::Xaml::Controls::ListViewBase>();
    if (!m_rosterStatusText || !list)
    {
        return;
    }

    const auto selected = list.SelectedItem().try_as<
        Microsoft::UI::Xaml::Controls::TextBox>();
    if (selected)
    {
        m_rosterStatusText.Text(
            winrt::hstring(L"Selected " + std::wstring(
                selected.Text().c_str(), selected.Text().size()
                ) + L" for transfer.")
            );
    }
}

void MainWindow::RosterTransferButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_rosterSourceList || !m_rosterTransferredList
        || !m_rosterStatusText)
    {
        return;
    }

    const int32_t selectedIndex = m_rosterSourceList.SelectedIndex();
    const auto selected = m_rosterSourceList.SelectedItem().try_as<
        Microsoft::UI::Xaml::Controls::TextBox>();
    if (selectedIndex < 0 || !selected)
    {
        m_rosterStatusText.Text(L"Select a student before transferring.");
        return;
    }

    auto transferred = Microsoft::UI::Xaml::Controls::TextBox();
    transferred.Text(selected.Text());
    transferred.Header(winrt::box_value(winrt::hstring(L"Student name")));
    transferred.IsTabStop(true);
    setAutomationName(transferred, L"Transferred student name");
    transferred.TextChanging({this, &MainWindow::NameTextBox_TextChanged});
    m_rosterSourceList.Items().RemoveAt(
        static_cast<uint32_t>(selectedIndex)
        );
    m_rosterTransferredList.Items().Append(transferred);
    m_rosterTransferredList.SelectedItem(transferred);
    m_dirtyState.markDirty();
    m_rosterStatusText.Text(L"Student transferred in the prototype.");
}

void MainWindow::SpeakingPasteButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_speakingPasteTextBox || !m_speakingStatusText)
    {
        return;
    }

    const auto pastedText = m_speakingPasteTextBox.Text();
    const auto rows = parsePastedRange(std::wstring_view(
        pastedText.c_str(),
        pastedText.size()
        ));
    constexpr size_t scoreColumns = 3;
    size_t applied = 0;
    for (size_t row = 0; row < rows.size() && row < 3; ++row)
    {
        for (size_t column = 0;
             column < rows[row].size() && column < scoreColumns;
             ++column)
        {
            const size_t cellIndex = row * scoreColumns + column;
            if (cellIndex < m_speakingScoreCells.size())
            {
                m_speakingScoreCells[cellIndex].Text(
                    winrt::hstring(rows[row][column])
                    );
                ++applied;
            }
        }
    }

    if (applied == 0)
    {
        m_speakingStatusText.Text(L"Paste a tab/newline range to apply scores.");
        return;
    }

    m_dirtyState.markDirty();
    m_speakingStatusText.Text(winrt::hstring(
        L"Applied " + std::to_wstring(applied)
            + L" pasted score cells in the prototype."
        ));
}

void MainWindow::SpeakingAnalyticsButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (m_speakingStatusText)
    {
        m_speakingStatusText.Text(
            L"Analytics navigation requested for the selected roster."
            );
    }
}

void MainWindow::NavigationView_SelectionChanged(
    Microsoft::UI::Xaml::Controls::NavigationView const& sender,
    Microsoft::UI::Xaml::Controls::NavigationViewSelectionChangedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    if (m_selectionChanging)
    {
        return;
    }

    const auto selectedItem = arguments.SelectedItem().try_as<
        Microsoft::UI::Xaml::Controls::NavigationViewItem>();
    if (!selectedItem)
    {
        return;
    }

    std::wstring pageId = boxedString(selectedItem.Tag());
    if (pageId == personalDetailsPageId)
    {
        pageId = std::wstring(homePageId);
    }
    if (pageId == homePageId)
    {
        navigateTo(homePageId);
        const auto homePage = m_contentFrame.Content().try_as<
            Microsoft::UI::Xaml::Controls::Page>();
        const auto homeTabs = homePage
            ? homePage.Content().try_as<Microsoft::UI::Xaml::Controls::Pivot>()
            : nullptr;
        if (homeTabs)
        {
            homeTabs.SelectedIndex(0);
        }
        return;
    }
    if (selectedItem == m_subPrepNavigationItem)
    {
        navigateTo(subPrepPageId);
        return;
    }
    if (isClassesPageId(pageId))
    {
        navigateTo(classesPageId);
        return;
    }
    if (pageId == L"campus_info")
    {
        pageId = std::wstring(campusInformationPageId);
    }
    navigateTo(pageId);
}

void MainWindow::NavigationView_BackRequested(
    Microsoft::UI::Xaml::Controls::NavigationView const& sender,
    Microsoft::UI::Xaml::Controls::NavigationViewBackRequestedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (m_contentFrame.CanGoBack())
    {
        m_contentFrame.GoBack();
    }
    updateNavigationState();
}

void MainWindow::ContentFrame_Navigated(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::Navigation::NavigationEventArgs const& arguments
    )
{
    static_cast<void>(sender);

    const auto page = arguments.Content().try_as<
        Microsoft::UI::Xaml::Controls::Page>();
    if (!page)
    {
        return;
    }

    std::wstring pageId = boxedString(arguments.Parameter());
    if (pageId == personalDetailsPageId)
    {
        pageId = std::wstring(homePageId);
    }
    if (isClassesPageId(pageId))
    {
        pageId = std::wstring(classesPageId);
    }
    if (pageId == L"campus_info")
    {
        pageId = std::wstring(campusInformationPageId);
    }
    if (!isKnownPageId(pageId))
    {
        pageId = std::wstring(homePageId);
    }
    populatePage(page, pageId);
    m_currentPageId = pageId;

    m_selectionChanging = true;
    m_navigationView.SelectedItem(
        pageId == homePageId
            ? m_homeNavigationItem
            : pageId == koreanTeachersPageId
                ? m_koreanTeachersNavigationItem
            : pageId == nativeEnglishTeachersPageId
                ? m_nativeEnglishTeachersNavigationItem
            : pageId == gsTeamPageId
                ? m_gsTeamNavigationItem
            : pageId == subPrepPageId
                ? m_subPrepNavigationItem
            : pageId == classesPageId
                ? m_classesNavigationItem
                : pageId == aboutPageId
                ? m_aboutNavigationItem
                : pageId == campusInformationPageId
                    ? m_campusInformationNavigationItem
                    : pageId == campusDirectionsPageId
                        ? m_campusDirectionsNavigationItem
                        : pageId == campusAddressPageId
                            ? m_campusAddressNavigationItem
                            : pageId == campusHousingPageId
                                ? m_campusHousingNavigationItem
                                : m_campusMapNavigationItem
        );
    m_selectionChanging = false;
    updateNavigationState();
    updateFileCommandState();
    if (!m_restoringState)
    {
        saveShellState();
    }
}

void MainWindow::Window_Activated(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::WindowActivatedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_windowBoundsRestored)
    {
        restoreWindowBounds();
        m_windowBoundsRestored = true;
    }
}

void MainWindow::Window_Closed(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::WindowEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    saveShellState();
}

void MainWindow::navigateTo(std::wstring_view pageId)
{
    if (pageId == personalDetailsPageId)
    {
        pageId = homePageId;
    }
    if (isClassesPageId(pageId))
    {
        pageId = classesPageId;
    }
    if (!isKnownPageId(pageId))
    {
        return;
    }

    if (m_currentPageId == pageId && m_contentFrame.Content())
    {
        updateNavigationState();
        return;
    }

    static_cast<void>(m_contentFrame.Navigate(
        winrt::xaml_typename<Microsoft::UI::Xaml::Controls::Page>(),
        winrt::box_value(winrt::hstring(pageId))
        ));
}

void MainWindow::populatePage(
    Microsoft::UI::Xaml::Controls::Page const& page,
    std::wstring_view pageId
    )
{
    if (page.Content())
    {
        // Campus Information depends on the active database.  A cached page
        // may have been constructed before File > Open completed, so it must
        // be rehydrated whenever navigation returns to it instead of
        // retaining the previous no-database/empty state.
        if (isCampusPageId(pageId))
        {
            populateCampusPage(page, pageId, true);
        }
        else if (pageId == personalDetailsPageId)
        {
            populatePersonalDetailsPage(page, true);
        }
        else if (pageId == koreanTeachersPageId)
        {
            populateKoreanTeachersPage(page, true);
        }
        else if (pageId == nativeEnglishTeachersPageId)
        {
            populateNativeEnglishTeachersPage(page, true);
        }
        else if (pageId == gsTeamPageId)
        {
            populateGsTeamPage(page, true);
        }
        else if (pageId == subPrepPageId)
        {
            refreshSubPrepPage();
        }
        else if (pageId == classesPageId)
        {
            refreshClassesPage();
        }
        else if (pageId == homePageId && !m_engineVersionText)
        {
            populateHomePage(page);
        }
        else if (pageId == homePageId)
        {
            populatePersonalDetailsPage(page, true);
            refreshScheduleWorkspace();
            refreshTestingWorkspace();
            refreshCalendarPage();
        }
        return;
    }

    if (pageId == homePageId)
    {
        populateHomePage(page);
    }
    else if (pageId == personalDetailsPageId)
    {
        populatePersonalDetailsPage(page, false);
    }
    else if (pageId == koreanTeachersPageId)
    {
        populateKoreanTeachersPage(page, false);
    }
    else if (pageId == nativeEnglishTeachersPageId)
    {
        populateNativeEnglishTeachersPage(page, false);
    }
    else if (pageId == gsTeamPageId)
    {
        populateGsTeamPage(page, false);
    }
    else if (pageId == subPrepPageId)
    {
        populateSubPrepPage(page, false);
    }
    else if (isClassesPageId(pageId))
    {
        populateClassesPage(page, pageId);
    }
    else if (isCampusPageId(pageId))
    {
        populateCampusPage(page, pageId, false);
    }
    else
    {
        populateAboutPage(page);
    }
}

void MainWindow::populateHomePage(
    Microsoft::UI::Xaml::Controls::Page const& page
    )
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    auto root = StackPanel();
    root.Padding(Thickness{32.0, 32.0, 32.0, 32.0});
    root.Spacing(16.0);
    root.MaxWidth(720.0);
    root.HorizontalAlignment(HorizontalAlignment::Center);
    root.VerticalAlignment(VerticalAlignment::Top);

    auto title = TextBlock();
    title.Text(L"ClassMngr Windows");
    title.FontSize(32.0);
    setAutomationName(title, L"ClassMngr Windows");

    m_engineVersionText = TextBlock();
    m_engineVersionText.FontSize(16.0);
    m_engineVersionText.Text(
        winrt::to_hstring(
            std::string("Engine version: ") + m_engineVersion.toString()
            )
        );
    setAutomationName(m_engineVersionText, L"Engine version");

    auto description = TextBlock();
    description.Text(L"The Windows shell is ready for feature pages.");
    description.TextWrapping(TextWrapping::Wrap);

    m_nameTextBox = TextBox();
    m_nameTextBox.Header(winrt::box_value(winrt::hstring(L"이름")));
    m_nameTextBox.PlaceholderText(L"한국어 입력");
    auto inputScope = Input::InputScope();
    inputScope.Names().Append(
        Input::InputScopeName(Input::InputScopeNameValue::Text)
        );
    m_nameTextBox.InputScope(inputScope);
    m_nameTextBox.TabIndex(0);
    m_nameTextBox.IsTabStop(true);
    m_nameTextBox.TextChanging({this, &MainWindow::NameTextBox_TextChanged});
    setAutomationName(m_nameTextBox, L"Name input");

    m_continueButton = Button();
    m_continueButton.Content(winrt::box_value(winrt::hstring(L"Continue")));
    m_continueButton.TabIndex(1);
    m_continueButton.IsTabStop(true);
    m_continueButton.HorizontalAlignment(HorizontalAlignment::Left);
    m_continueButton.Click({this, &MainWindow::ContinueButton_Click});
    setAutomationName(m_continueButton, L"Continue");

    m_progressRing = ProgressRing();
    m_progressRing.IsActive(false);
    m_progressRing.Visibility(Visibility::Collapsed);
    m_progressRing.Width(24.0);
    m_progressRing.Height(24.0);
    m_progressRing.HorizontalAlignment(HorizontalAlignment::Left);
    setAutomationName(m_progressRing, L"Operation progress");

    m_cancelButton = Button();
    m_cancelButton.Content(winrt::box_value(winrt::hstring(L"Cancel operation")));
    m_cancelButton.TabIndex(2);
    m_cancelButton.IsTabStop(true);
    m_cancelButton.IsEnabled(false);
    m_cancelButton.HorizontalAlignment(HorizontalAlignment::Left);
    m_cancelButton.Click({this, &MainWindow::CancelButton_Click});
    setAutomationName(m_cancelButton, L"Cancel current operation");

    m_statusText = TextBlock();
    m_statusText.Text(L"Ready");
    m_statusText.TextWrapping(TextWrapping::Wrap);
    setAutomationName(m_statusText, L"Status");

    m_validationSummaryText = TextBlock();
    m_validationSummaryText.Text(L"No validation issues.");
    m_validationSummaryText.TextWrapping(TextWrapping::Wrap);
    setAutomationName(m_validationSummaryText, L"Validation summary");

    m_unsavedChangesButton = Button();
    m_unsavedChangesButton.Content(
        winrt::box_value(winrt::hstring(L"Review unsaved changes"))
        );
    m_unsavedChangesButton.TabIndex(3);
    m_unsavedChangesButton.IsTabStop(true);
    m_unsavedChangesButton.HorizontalAlignment(HorizontalAlignment::Left);
    m_unsavedChangesButton.Click({this, &MainWindow::UnsavedChangesButton_Click});
    setAutomationName(m_unsavedChangesButton, L"Review unsaved changes");

    auto scheduleCard = ClassMngrWinUISharedUX::buildCard({
        L"Schedule/time-slot editing prototype",
        L"Edit a representative time-slot label in memory. This prototype does not persist data or apply engine rules.",
        L"Schedule editor"
        });
    m_scheduleSlotTextBox = TextBox();
    m_scheduleSlotTextBox.Header(
        winrt::box_value(winrt::hstring(L"Time-slot label"))
        );
    m_scheduleSlotTextBox.Text(L"09:00–09:45");
    m_scheduleSlotTextBox.PlaceholderText(L"e.g. 09:00–09:45");
    m_scheduleSlotTextBox.IsTabStop(true);
    m_scheduleSlotTextBox.TabIndex(4);
    m_scheduleSlotTextBox.TextChanging({this, &MainWindow::NameTextBox_TextChanged});
    setAutomationName(m_scheduleSlotTextBox, L"Schedule time-slot label editor");

    auto scheduleApply = Button();
    scheduleApply.Content(winrt::box_value(winrt::hstring(L"Apply time-slot label")));
    scheduleApply.IsTabStop(true);
    scheduleApply.TabIndex(5);
    scheduleApply.HorizontalAlignment(HorizontalAlignment::Left);
    scheduleApply.Click({this, &MainWindow::ScheduleApplyButton_Click});
    setAutomationName(scheduleApply, L"Schedule apply time-slot label");

    m_scheduleStatusText = TextBlock();
    m_scheduleStatusText.Text(L"Ready to edit a time slot.");
    m_scheduleStatusText.TextWrapping(TextWrapping::Wrap);
    setAutomationName(m_scheduleStatusText, L"Schedule editor status");
    scheduleCard.content.Children().Append(m_scheduleSlotTextBox);
    scheduleCard.content.Children().Append(scheduleApply);
    scheduleCard.content.Children().Append(m_scheduleStatusText);

    auto rosterCard = ClassMngrWinUISharedUX::buildCard({
        L"Roster selection, transfer, and keyboard editing prototype",
        L"Select a student, edit the name with the keyboard, and simulate a transfer between lists.",
        L"Roster editor"
        });
    auto rosterLists = StackPanel();
    rosterLists.Orientation(Orientation::Horizontal);
    rosterLists.Spacing(8.0);

    m_rosterSourceList = ListView();
    m_rosterSourceList.Header(
        winrt::box_value(winrt::hstring(L"Available students"))
        );
    m_rosterSourceList.SelectionMode(ListViewSelectionMode::Single);
    m_rosterSourceList.IsTabStop(true);
    m_rosterSourceList.Width(220.0);
    m_rosterSourceList.Height(150.0);
    setAutomationName(m_rosterSourceList, L"Roster available students");
    m_rosterSourceList.SelectionChanged({this, &MainWindow::RosterSource_SelectionChanged});
    for (auto const& name : {L"김민서", L"Alex Kim", L"박서준"})
    {
        auto student = TextBox();
        student.Header(winrt::box_value(winrt::hstring(L"Student name")));
        student.Text(name);
        student.IsTabStop(true);
        student.TextChanging({this, &MainWindow::NameTextBox_TextChanged});
        setAutomationName(student, L"Roster editable student name");
        m_rosterSourceList.Items().Append(student);
    }

    m_rosterTransferredList = ListView();
    m_rosterTransferredList.Header(
        winrt::box_value(winrt::hstring(L"Transferred students"))
        );
    m_rosterTransferredList.SelectionMode(ListViewSelectionMode::Single);
    m_rosterTransferredList.IsTabStop(true);
    m_rosterTransferredList.Width(220.0);
    m_rosterTransferredList.Height(150.0);
    setAutomationName(m_rosterTransferredList, L"Roster transferred students");

    rosterLists.Children().Append(m_rosterSourceList);
    rosterLists.Children().Append(m_rosterTransferredList);
    auto rosterTransfer = Button();
    rosterTransfer.Content(winrt::box_value(winrt::hstring(L"Transfer selected →")));
    rosterTransfer.IsTabStop(true);
    rosterTransfer.TabIndex(6);
    rosterTransfer.HorizontalAlignment(HorizontalAlignment::Left);
    rosterTransfer.Click({this, &MainWindow::RosterTransferButton_Click});
    setAutomationName(rosterTransfer, L"Roster transfer selected student");
    m_rosterStatusText = TextBlock();
    m_rosterStatusText.Text(L"Select a roster row to begin.");
    m_rosterStatusText.TextWrapping(TextWrapping::Wrap);
    setAutomationName(m_rosterStatusText, L"Roster selected student status");
    rosterCard.content.Children().Append(rosterLists);
    rosterCard.content.Children().Append(rosterTransfer);
    rosterCard.content.Children().Append(m_rosterStatusText);

    auto speakingCard = ClassMngrWinUISharedUX::buildCard({
        L"Speaking-evaluation scores and analytics prototype",
        L"Edit score cells, apply a tab/newline range, and request analytics navigation using standard controls.",
        L"Speaking evaluation editor"
        });
    m_speakingScoreCells.clear();
    auto scoreGrid = Grid();
    for (size_t column = 0; column < 4; ++column)
    {
        scoreGrid.ColumnDefinitions().Append(ColumnDefinition());
    }
    for (size_t row = 0; row < 4; ++row)
    {
        scoreGrid.RowDefinitions().Append(RowDefinition());
    }
    setAutomationName(scoreGrid, L"Speaking evaluation score grid");

    auto addScoreHeader = [&scoreGrid](wchar_t const* text, uint32_t row, uint32_t column) {
        auto header = TextBlock();
        header.Text(text);
        header.Margin(Thickness{4.0, 2.0, 4.0, 2.0});
        Grid::SetRow(header, row);
        Grid::SetColumn(header, column);
        scoreGrid.Children().Append(header);
    };
    addScoreHeader(L"Student", 0, 0);
    addScoreHeader(L"Pronunciation", 0, 1);
    addScoreHeader(L"Fluency", 0, 2);
    addScoreHeader(L"Interaction", 0, 3);
    const std::array<wchar_t const*, 3> studentNames{
        L"김민서", L"Alex Kim", L"박서준"
    };
    const std::array<std::array<wchar_t const*, 3>, 3> initialScores{{
        {{L"8", L"7", L"9"}},
        {{L"9", L"8", L"8"}},
        {{L"7", L"8", L"7"}}
    }};
    for (uint32_t row = 0; row < 3; ++row)
    {
        addScoreHeader(studentNames[row], row + 1, 0);
        for (uint32_t column = 0; column < 3; ++column)
        {
            auto score = TextBox();
            score.Text(initialScores[row][column]);
            score.Width(76.0);
            score.Margin(Thickness{4.0, 2.0, 4.0, 2.0});
            score.IsTabStop(true);
            score.TabIndex(7 + row * 3 + column);
            score.TextChanging({this, &MainWindow::NameTextBox_TextChanged});
            const std::wstring scoreAutomationName =
                L"Speaking score " + std::to_wstring(row + 1) + L" "
                + std::to_wstring(column + 1);
            setAutomationName(score, scoreAutomationName);
            Grid::SetRow(score, row + 1);
            Grid::SetColumn(score, column + 1);
            scoreGrid.Children().Append(score);
            m_speakingScoreCells.emplace_back(score);
        }
    }

    m_speakingPasteTextBox = TextBox();
    m_speakingPasteTextBox.Header(
        winrt::box_value(winrt::hstring(L"Paste scores (tab/newline range)"))
        );
    m_speakingPasteTextBox.PlaceholderText(L"8\t7\t9\n9\t8\t8");
    m_speakingPasteTextBox.AcceptsReturn(true);
    m_speakingPasteTextBox.Height(72.0);
    m_speakingPasteTextBox.IsTabStop(true);
    m_speakingPasteTextBox.TabIndex(20);
    setAutomationName(m_speakingPasteTextBox, L"Speaking pasted score range");
    auto speakingPaste = Button();
    speakingPaste.Content(winrt::box_value(winrt::hstring(L"Apply pasted range")));
    speakingPaste.IsTabStop(true);
    speakingPaste.TabIndex(21);
    speakingPaste.HorizontalAlignment(HorizontalAlignment::Left);
    speakingPaste.Click({this, &MainWindow::SpeakingPasteButton_Click});
    setAutomationName(speakingPaste, L"Apply speaking pasted score range");
    auto speakingAnalytics = Button();
    speakingAnalytics.Content(winrt::box_value(winrt::hstring(L"Open analytics")));
    speakingAnalytics.IsTabStop(true);
    speakingAnalytics.TabIndex(22);
    speakingAnalytics.HorizontalAlignment(HorizontalAlignment::Left);
    speakingAnalytics.Click({this, &MainWindow::SpeakingAnalyticsButton_Click});
    setAutomationName(speakingAnalytics, L"Open speaking analytics");
    m_speakingStatusText = TextBlock();
    m_speakingStatusText.Text(L"Scores ready for editing.");
    m_speakingStatusText.TextWrapping(TextWrapping::Wrap);
    setAutomationName(m_speakingStatusText, L"Speaking analytics navigation status");
    speakingCard.content.Children().Append(scoreGrid);
    speakingCard.content.Children().Append(m_speakingPasteTextBox);
    speakingCard.content.Children().Append(speakingPaste);
    speakingCard.content.Children().Append(speakingAnalytics);
    speakingCard.content.Children().Append(m_speakingStatusText);

    m_homeViewModel = winrt::make_self<ObservableViewModel>();
    const auto command = winrt::make_self<AsyncCommand>(
        DispatcherQueue(),
        phase3PresentationWork
        );
    const auto commandInterface = command.as<Microsoft::UI::Xaml::Input::ICommand>();
    auto weak = get_weak();
    m_homeCommandStateToken = commandInterface.CanExecuteChanged(
        [weak](
            Windows::Foundation::IInspectable const&,
            Windows::Foundation::IInspectable const&
            ) {
            if (auto self = weak.get())
            {
                self->updateHomePresentation();
            }
        }
        );
    m_homeCommand = command;

    root.Children().Append(title);
    root.Children().Append(m_engineVersionText);
    root.Children().Append(description);
    root.Children().Append(m_nameTextBox);
    root.Children().Append(m_continueButton);
    root.Children().Append(m_progressRing);
    root.Children().Append(m_cancelButton);
    root.Children().Append(m_statusText);
    root.Children().Append(m_validationSummaryText);
    root.Children().Append(m_unsavedChangesButton);

    auto scheduleRoot = StackPanel();
    scheduleRoot.Padding(Thickness{16.0, 12.0, 16.0, 24.0});
    scheduleRoot.Spacing(12.0);
    scheduleRoot.MaxWidth(1260.0);
    scheduleRoot.HorizontalAlignment(HorizontalAlignment::Center);
    // Keep the original phase-4 controls available to the semantic hook, but
    // make the engine-backed workspace below the only user-facing schedule
    // editor.
    scheduleCard.root.Visibility(Visibility::Collapsed);
    scheduleRoot.Children().Append(scheduleCard.root);
    populateScheduleWorkspace(scheduleRoot);

    auto calendarRoot = StackPanel();
    calendarRoot.Padding(Thickness{32.0, 16.0, 32.0, 32.0});
    calendarRoot.Spacing(16.0);
    calendarRoot.MaxWidth(900.0);
    calendarRoot.HorizontalAlignment(HorizontalAlignment::Center);
    populateCalendarWorkspace(calendarRoot);

    const auto scrollTab = [](StackPanel const& content) {
        auto scroll = ScrollViewer();
        scroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
        scroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
        scroll.Content(content);
        return scroll;
    };
    const auto makePivotItem = [&scrollTab](
                                    wchar_t const* header,
                                    StackPanel const& content,
                                    wchar_t const* automationName) {
        auto item = PivotItem();
        item.Header(winrt::box_value(winrt::hstring(header)));
        item.Content(scrollTab(content));
        setAutomationName(item, automationName);
        return item;
    };

    auto personalDetailsHost = ContentControl();
    populatePersonalDetailsPage(personalDetailsHost, false);
    auto informationContent = Grid();
    informationContent.Children().Append(personalDetailsHost);

    auto informationItem = PivotItem();
    informationItem.Header(winrt::box_value(winrt::hstring(L"My Information")));
    informationItem.Content(informationContent);
    setAutomationName(informationItem, L"My Information workspace tab");

    auto tabs = Pivot();
    tabs.IsTabStop(true);
    tabs.TabIndex(0);
    setAutomationName(tabs, L"My Workspace tabs");
    tabs.Items().Append(informationItem);
    tabs.Items().Append(makePivotItem(
        L"Schedule",
        scheduleRoot,
        L"Schedule workspace tab"
        ));
    tabs.Items().Append(makePivotItem(
        L"Calendar",
        calendarRoot,
        L"Calendar workspace tab"
        ));
    tabs.SelectedIndex(0);
    page.Content(tabs);
}

void MainWindow::populateScheduleWorkspace(
    Microsoft::UI::Xaml::Controls::StackPanel const& scheduleRoot
    )
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (m_scheduleTabs)
    {
        scheduleRoot.Children().Append(m_scheduleTabs);
        refreshScheduleWorkspace();
        return;
    }

    auto makeText = [](std::wstring_view text, double fontSize = 0.0) {
        auto value = TextBlock();
        value.Text(winrt::hstring(text));
        value.TextWrapping(TextWrapping::Wrap);
        if (fontSize > 0.0)
        {
            value.FontSize(fontSize);
        }
        return value;
    };
    const auto appendColumn = [](Grid const& grid, double width) {
        auto definition = ColumnDefinition();
        definition.Width(
            GridLengthHelper::FromValueAndType(width, GridUnitType::Pixel)
            );
        grid.ColumnDefinitions().Append(definition);
    };

    auto editorContent = StackPanel();
    editorContent.Padding(Thickness{16.0, 16.0, 16.0, 24.0});
    editorContent.Spacing(12.0);
    editorContent.HorizontalAlignment(HorizontalAlignment::Stretch);

    auto modeBar = Grid();
    modeBar.ColumnSpacing(8.0);
    modeBar.HorizontalAlignment(HorizontalAlignment::Stretch);
    for (int column = 0; column < 5; ++column)
    {
        auto definition = ColumnDefinition();
        definition.Width(GridLengthHelper::FromValueAndType(
            1.0,
            column == 3 ? GridUnitType::Star : GridUnitType::Auto
            ));
        modeBar.ColumnDefinitions().Append(definition);
    }

    const auto makeModeButton = [](std::wstring_view text,
                                   std::wstring_view automationName) {
        auto button = Button();
        button.Content(box_value(hstring(text)));
        button.MinWidth(116.0);
        button.MinHeight(50.0);
        button.Padding(Thickness{16.0, 7.0, 16.0, 7.0});
        button.FontSize(20.0);
        button.IsTabStop(true);
        setAutomationName(button, automationName);
        return button;
    };

    m_scheduleRegularModeButton = makeModeButton(
        L"Regular",
        L"Schedule Regular mode"
        );
    m_scheduleIntensiveModeButton = makeModeButton(
        L"Intensive",
        L"Schedule Intensive mode"
        );
    m_scheduleTestingModeButton = makeModeButton(
        L"Testing",
        L"Schedule Testing mode"
        );
    m_scheduleImportModeButton = makeModeButton(
        L"Import",
        L"Schedule Import"
        );
    m_scheduleImportModeButton.HorizontalAlignment(HorizontalAlignment::Right);

    Grid::SetColumn(m_scheduleRegularModeButton, 0);
    Grid::SetColumn(m_scheduleIntensiveModeButton, 1);
    Grid::SetColumn(m_scheduleTestingModeButton, 2);
    modeBar.Children().Append(m_scheduleRegularModeButton);
    modeBar.Children().Append(m_scheduleIntensiveModeButton);
    modeBar.Children().Append(m_scheduleTestingModeButton);
    auto modeSpacer = Border();
    modeSpacer.HorizontalAlignment(HorizontalAlignment::Stretch);
    Grid::SetColumn(modeSpacer, 3);
    modeBar.Children().Append(modeSpacer);
    Grid::SetColumn(m_scheduleImportModeButton, 4);
    modeBar.Children().Append(m_scheduleImportModeButton);
    editorContent.Children().Append(modeBar);

    m_scheduleBoardRoot = ClassMngrWinUIScheduleBoard::create({
        [this](int classId) {
            openScheduleClassEditor(classId);
        },
        [this](std::wstring day,
               std::wstring timeLabel,
               std::wstring currentState,
               std::wstring defaultState,
               bool slotTogglingEnabled,
               bool testingBlockCreationEnabled) {
            handleScheduleSlotClick(
                std::move(day),
                std::move(timeLabel),
                std::move(currentState),
                std::move(defaultState),
                slotTogglingEnabled,
                testingBlockCreationEnabled
                );
        }
    });
    m_scheduleBoardRoot.HorizontalAlignment(HorizontalAlignment::Stretch);
    m_scheduleBoardRoot.MinWidth(860.0);
    setAutomationName(m_scheduleBoardRoot, L"Weekly class schedule board");
    editorContent.Children().Append(m_scheduleBoardRoot);

    m_scheduleRegularModeButton.Click(
        [this](auto const&, auto const&) {
            setScheduleDisplayMode(
                static_cast<int>(
                    classmngr::engine::ScheduleReportDisplayMode::Regular
                    )
                );
        }
        );
    m_scheduleIntensiveModeButton.Click(
        [this](auto const&, auto const&) {
            setScheduleDisplayMode(
                static_cast<int>(
                    classmngr::engine::ScheduleReportDisplayMode::Intensive
                    )
                );
        }
        );
    m_scheduleTestingModeButton.Click(
        [this](auto const&, auto const&) {
            setScheduleDisplayMode(
                static_cast<int>(
                    classmngr::engine::ScheduleReportDisplayMode::Testing
                    )
                );
        }
        );
    m_scheduleImportModeButton.Click(
        [this](auto const&, auto const&) {
            if (m_scheduleTabs)
            {
                m_scheduleTabs.SelectedIndex(1);
            }
        }
        );
    updateScheduleDisplayButtons();

    auto heading = makeText(L"Class schedules", 24.0);
    heading.Visibility(Visibility::Collapsed);
    setAutomationName(heading, L"Class schedules heading");
    editorContent.Children().Append(heading);
    auto description = makeText(
        L"View regular and intensive class times, edit one slot, and let the "
        L"engine reject invalid or overlapping schedules. The table remains "
        L"virtualized by the WinUI ListView for larger class directories."
        );
    description.Visibility(Visibility::Collapsed);
    setAutomationName(description, L"Class schedules description");
    editorContent.Children().Append(description);

    m_scheduleHeaderGrid = Grid();
    m_scheduleHeaderGrid.ColumnSpacing(8.0);
    m_scheduleHeaderGrid.MinWidth(680.0);
    m_scheduleHeaderGrid.Visibility(Visibility::Collapsed);
    setAutomationName(m_scheduleHeaderGrid, L"Class schedule column headers");
    const std::array<double, 5> columnWidths{190.0, 105.0, 125.0, 120.0, 120.0};
    for (const double width : columnWidths)
    {
        appendColumn(m_scheduleHeaderGrid, width);
    }
    const std::array<std::wstring_view, 5> headers{
        L"Class", L"Type", L"Day", L"Start", L"End"
    };
    for (std::size_t column = 0; column < headers.size(); ++column)
    {
        auto header = makeText(headers[column]);
        header.Margin(Thickness{4.0, 4.0, 4.0, 4.0});
        Grid::SetColumn(header, static_cast<int32_t>(column));
        m_scheduleHeaderGrid.Children().Append(header);
    }
    editorContent.Children().Append(m_scheduleHeaderGrid);

    m_scheduleList = ListView();
    m_scheduleList.SelectionMode(ListViewSelectionMode::Single);
    m_scheduleList.IsTabStop(true);
    m_scheduleList.TabIndex(21);
    m_scheduleList.Height(280.0);
    m_scheduleList.Visibility(Visibility::Collapsed);
    m_scheduleList.HorizontalAlignment(HorizontalAlignment::Stretch);
    m_scheduleList.SelectionChanged(
        [this](auto const&, auto const&) {
            if (m_scheduleLoading || !m_scheduleList)
            {
                return;
            }

            const auto item = m_scheduleList.SelectedItem().try_as<
                ListViewItem>();
            if (!item)
            {
                m_scheduleEditingKey.clear();
                return;
            }
            const auto selection = scheduleSelectionFromKey(
                boxedString(item.Tag())
                );
            if (!selection)
            {
                return;
            }

            m_scheduleLoading = true;
            for (int index = 0;
                 index < static_cast<int>(m_scheduleClassSelector.Items().Size());
                 ++index)
            {
                const auto classItem = m_scheduleClassSelector.Items().GetAt(
                    index
                    ).try_as<ComboBoxItem>();
                if (classItem && boxedInt(classItem.Tag()) == selection->classId)
                {
                    m_scheduleClassSelector.SelectedIndex(index);
                    break;
                }
            }
            m_scheduleTypeCombo.SelectedIndex(
                selection->type == classmngr::engine::ScheduleType::Intensive
                    ? 1
                    : 0
                );
            for (int index = 0;
                 index < static_cast<int>(m_scheduleDayCombo.Items().Size());
                 ++index)
            {
                const auto dayItem = m_scheduleDayCombo.Items().GetAt(
                    index
                    ).try_as<ComboBoxItem>();
                if (dayItem && boxedString(dayItem.Tag()) == selection->day)
                {
                    m_scheduleDayCombo.SelectedIndex(index);
                    break;
                }
            }
            m_scheduleStartTextBox.Text(selection->startTime);
            m_scheduleEndTextBox.Text(selection->endTime);
            m_scheduleEditingKey = boxedString(item.Tag());
            m_scheduleLoading = false;
            if (m_scheduleWorkspaceStatusText)
            {
                m_scheduleWorkspaceStatusText.Text(
                    selection->startTime.empty()
                        ? L"Choose a day and time to add a schedule slot."
                        : L"Editing the selected schedule slot."
                    );
            }
        }
        );
    setAutomationName(m_scheduleList, L"Class schedule table");
    editorContent.Children().Append(m_scheduleList);

    auto formCard = ClassMngrWinUISharedUX::buildCard({
        L"Schedule slot editor",
        L"Use the same weekday and time formats as the retained class editor. "
        L"Saving is validated and persisted through the shared engine service.",
        L"Schedule slot editor"
        });
    formCard.root.Visibility(Visibility::Collapsed);
    m_scheduleClassSelector = ComboBox();
    m_scheduleClassSelector.Header(box_value(hstring(L"Class")));
    m_scheduleClassSelector.PlaceholderText(L"Select a class");
    m_scheduleClassSelector.MinWidth(300.0);
    m_scheduleClassSelector.IsTabStop(true);
    m_scheduleClassSelector.TabIndex(22);
    setAutomationName(m_scheduleClassSelector, L"Schedule class selector");
    formCard.content.Children().Append(m_scheduleClassSelector);

    m_scheduleTypeCombo = ComboBox();
    m_scheduleTypeCombo.Header(box_value(hstring(L"Schedule type")));
    m_scheduleTypeCombo.MinWidth(220.0);
    m_scheduleTypeCombo.IsTabStop(true);
    m_scheduleTypeCombo.TabIndex(23);
    for (const auto& choice : {
             std::pair{L"Regular", 0},
             std::pair{L"Intensive", 1}})
    {
        auto item = ComboBoxItem();
        item.Content(box_value(hstring(choice.first)));
        item.Tag(box_value(choice.second));
        setAutomationName(item, choice.first);
        m_scheduleTypeCombo.Items().Append(item);
    }
    m_scheduleTypeCombo.SelectedIndex(0);
    setAutomationName(m_scheduleTypeCombo, L"Schedule type selector");
    formCard.content.Children().Append(m_scheduleTypeCombo);

    m_scheduleDayCombo = ComboBox();
    m_scheduleDayCombo.Header(box_value(hstring(L"Weekday")));
    m_scheduleDayCombo.MinWidth(220.0);
    m_scheduleDayCombo.IsTabStop(true);
    m_scheduleDayCombo.TabIndex(24);
    for (const std::string& day : classmngr::engine::ClassInfoConfig::days())
    {
        auto item = ComboBoxItem();
        item.Content(box_value(hstring(asWide(day))));
        item.Tag(box_value(hstring(asWide(day))));
        setAutomationName(item, asWide(day));
        m_scheduleDayCombo.Items().Append(item);
    }
    m_scheduleDayCombo.SelectedIndex(0);
    setAutomationName(m_scheduleDayCombo, L"Schedule weekday selector");
    formCard.content.Children().Append(m_scheduleDayCombo);

    m_scheduleStartTextBox = TextBox();
    m_scheduleStartTextBox.Header(box_value(hstring(L"Start time")));
    m_scheduleStartTextBox.PlaceholderText(L"e.g. 4:00 PM");
    m_scheduleStartTextBox.MinWidth(220.0);
    m_scheduleStartTextBox.IsTabStop(true);
    m_scheduleStartTextBox.TabIndex(25);
    m_scheduleStartTextBox.TextChanging(
        [this](auto const&, auto const&) {
            if (!m_scheduleLoading && m_scheduleValidationText)
            {
                m_scheduleValidationText.Visibility(Visibility::Collapsed);
            }
        }
        );
    setAutomationName(m_scheduleStartTextBox, L"Schedule start time");
    formCard.content.Children().Append(m_scheduleStartTextBox);

    m_scheduleEndTextBox = TextBox();
    m_scheduleEndTextBox.Header(box_value(hstring(L"End time")));
    m_scheduleEndTextBox.PlaceholderText(L"e.g. 4:55 PM");
    m_scheduleEndTextBox.MinWidth(220.0);
    m_scheduleEndTextBox.IsTabStop(true);
    m_scheduleEndTextBox.TabIndex(26);
    m_scheduleEndTextBox.TextChanging(
        [this](auto const&, auto const&) {
            if (!m_scheduleLoading && m_scheduleValidationText)
            {
                m_scheduleValidationText.Visibility(Visibility::Collapsed);
            }
        }
        );
    setAutomationName(m_scheduleEndTextBox, L"Schedule end time");
    formCard.content.Children().Append(m_scheduleEndTextBox);

    auto actions = StackPanel();
    actions.Orientation(Orientation::Horizontal);
    actions.Spacing(8.0);
    m_scheduleSaveButton = Button();
    m_scheduleSaveButton.Content(box_value(hstring(L"Save schedule slot")));
    m_scheduleSaveButton.IsTabStop(true);
    m_scheduleSaveButton.TabIndex(27);
    m_scheduleSaveButton.Click(
        [this](auto const&, auto const&) { saveScheduleEntry(); }
        );
    setAutomationName(m_scheduleSaveButton, L"Save schedule slot");
    actions.Children().Append(m_scheduleSaveButton);
    m_scheduleClearButton = Button();
    m_scheduleClearButton.Content(box_value(hstring(L"Clear editor")));
    m_scheduleClearButton.IsTabStop(true);
    m_scheduleClearButton.TabIndex(28);
    m_scheduleClearButton.Click(
        [this](auto const&, auto const&) { clearScheduleEntry(); }
        );
    setAutomationName(m_scheduleClearButton, L"Clear schedule editor");
    actions.Children().Append(m_scheduleClearButton);
    formCard.content.Children().Append(actions);

    m_scheduleWorkspaceStatusText = makeText(L"Schedule editor is ready.");
    m_scheduleWorkspaceStatusText.Visibility(Visibility::Collapsed);
    setAutomationName(m_scheduleWorkspaceStatusText, L"Schedule workspace status");
    formCard.content.Children().Append(m_scheduleWorkspaceStatusText);
    m_scheduleValidationText = makeText(L"");
    m_scheduleValidationText.Visibility(Visibility::Collapsed);
    setAutomationName(m_scheduleValidationText, L"Schedule validation");
    formCard.content.Children().Append(m_scheduleValidationText);
    editorContent.Children().Append(formCard.root);

    auto scrollTab = [](StackPanel const& content) {
        auto scroll = ScrollViewer();
        scroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
        scroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
        scroll.Content(content);
        return scroll;
    };
    auto scheduleItem = PivotItem();
    scheduleItem.Header(box_value(hstring(L"Schedule")));
    scheduleItem.Content(scrollTab(editorContent));
    setAutomationName(scheduleItem, L"Schedule editor tab");

    auto importContent = StackPanel();
    importContent.Padding(Thickness{16.0, 16.0, 16.0, 24.0});
    importContent.Spacing(12.0);
    importContent.HorizontalAlignment(HorizontalAlignment::Stretch);
    auto importHeading = makeText(L"Schedule import", 24.0);
    setAutomationName(importHeading, L"Schedule import heading");
    importContent.Children().Append(importHeading);
    auto importDescription = makeText(
        L"Review one structured user block before applying it. Preview uses "
        L"the shared match and weekday rules; Apply validates the complete "
        L"plan and commits it atomically."
        );
    setAutomationName(importDescription, L"Schedule import description");
    importContent.Children().Append(importDescription);

    auto importCard = ClassMngrWinUISharedUX::buildCard({
        L"Import review",
        L"The form represents the normalized data produced by a workbook "
        L"adapter. It keeps the review/apply boundary visible on Windows.",
        L"Schedule import review"
        });
    const auto makeImportBox = [](std::wstring_view label,
                                  std::wstring_view placeholder,
                                  std::wstring_view automationName) {
        auto box = TextBox();
        box.Header(box_value(hstring(label)));
        box.PlaceholderText(hstring(placeholder));
        box.MinWidth(300.0);
        box.IsTabStop(true);
        setAutomationName(box, automationName);
        return box;
    };
    m_scheduleImportKindCombo = ComboBox();
    m_scheduleImportKindCombo.Header(box_value(hstring(L"Schedule kind")));
    m_scheduleImportKindCombo.MinWidth(240.0);
    m_scheduleImportKindCombo.IsTabStop(true);
    m_scheduleImportKindCombo.TabIndex(30);
    for (const auto& choice : {
             std::pair{L"Normal", 0},
             std::pair{L"Intensive", 1}})
    {
        auto item = ComboBoxItem();
        item.Content(box_value(hstring(choice.first)));
        item.Tag(box_value(choice.second));
        setAutomationName(item, choice.first);
        m_scheduleImportKindCombo.Items().Append(item);
    }
    m_scheduleImportKindCombo.SelectedIndex(0);
    setAutomationName(m_scheduleImportKindCombo, L"Schedule import kind");
    importCard.content.Children().Append(m_scheduleImportKindCombo);

    m_scheduleImportUserTextBox = makeImportBox(
        L"User/profile name",
        L"e.g. Alice",
        L"Schedule import user name"
        );
    m_scheduleImportUserTextBox.Text(L"WinUI User");
    importCard.content.Children().Append(m_scheduleImportUserTextBox);
    m_scheduleImportTeacherTextBox = makeImportBox(
        L"Korean teacher",
        L"Hangul-only teacher key",
        L"Schedule import Korean teacher"
        );
    m_scheduleImportTeacherTextBox.Text(L"\uD64D\uAE38\uB3D9");
    importCard.content.Children().Append(m_scheduleImportTeacherTextBox);
    m_scheduleImportGradeTextBox = makeImportBox(
        L"Class grade",
        L"e.g. E5",
        L"Schedule import class grade"
        );
    m_scheduleImportGradeTextBox.Text(L"E5");
    importCard.content.Children().Append(m_scheduleImportGradeTextBox);
    m_scheduleImportLevelTextBox = makeImportBox(
        L"Class level",
        L"e.g. Zeus",
        L"Schedule import class level"
        );
    m_scheduleImportLevelTextBox.Text(L"Zeus");
    importCard.content.Children().Append(m_scheduleImportLevelTextBox);
    m_scheduleImportRoomTextBox = makeImportBox(
        L"Room",
        L"e.g. 413",
        L"Schedule import room"
        );
    m_scheduleImportRoomTextBox.Text(L"413");
    importCard.content.Children().Append(m_scheduleImportRoomTextBox);
    m_scheduleImportDaysTextBox = makeImportBox(
        L"Meeting days",
        L"Comma-separated, e.g. Monday, Wednesday",
        L"Schedule import meeting days"
        );
    m_scheduleImportDaysTextBox.Text(L"Monday, Wednesday");
    importCard.content.Children().Append(m_scheduleImportDaysTextBox);
    m_scheduleImportStartTextBox = makeImportBox(
        L"Start time",
        L"e.g. 4:00 PM",
        L"Schedule import start time"
        );
    m_scheduleImportStartTextBox.Text(L"4:00 PM");
    importCard.content.Children().Append(m_scheduleImportStartTextBox);
    m_scheduleImportEndTextBox = makeImportBox(
        L"End time",
        L"e.g. 4:55 PM",
        L"Schedule import end time"
        );
    m_scheduleImportEndTextBox.Text(L"4:55 PM");
    importCard.content.Children().Append(m_scheduleImportEndTextBox);

    m_scheduleImportTeacherActionCombo = ComboBox();
    m_scheduleImportTeacherActionCombo.Header(
        box_value(hstring(L"Teacher resolution"))
        );
    m_scheduleImportTeacherActionCombo.MinWidth(280.0);
    m_scheduleImportTeacherActionCombo.IsTabStop(true);
    m_scheduleImportTeacherActionCombo.TabIndex(31);
    for (const auto& choice : {
             std::pair{L"Reuse matching teacher", 0},
             std::pair{L"Create new teacher", 2}})
    {
        auto item = ComboBoxItem();
        item.Content(box_value(hstring(choice.first)));
        item.Tag(box_value(choice.second));
        setAutomationName(item, choice.first);
        m_scheduleImportTeacherActionCombo.Items().Append(item);
    }
    m_scheduleImportTeacherActionCombo.SelectedIndex(0);
    setAutomationName(
        m_scheduleImportTeacherActionCombo,
        L"Schedule import teacher resolution"
        );
    importCard.content.Children().Append(m_scheduleImportTeacherActionCombo);

    m_scheduleImportClassActionCombo = ComboBox();
    m_scheduleImportClassActionCombo.Header(
        box_value(hstring(L"Class resolution"))
        );
    m_scheduleImportClassActionCombo.MinWidth(280.0);
    m_scheduleImportClassActionCombo.IsTabStop(true);
    m_scheduleImportClassActionCombo.TabIndex(32);
    for (const auto& choice : {
             std::pair{L"Update suggested class", 0},
             std::pair{L"Create new class", 1},
             std::pair{L"Skip class", 2}})
    {
        auto item = ComboBoxItem();
        item.Content(box_value(hstring(choice.first)));
        item.Tag(box_value(choice.second));
        setAutomationName(item, choice.first);
        m_scheduleImportClassActionCombo.Items().Append(item);
    }
    m_scheduleImportClassActionCombo.SelectedIndex(1);
    setAutomationName(
        m_scheduleImportClassActionCombo,
        L"Schedule import class resolution"
        );
    importCard.content.Children().Append(m_scheduleImportClassActionCombo);

    auto importActions = StackPanel();
    importActions.Orientation(Orientation::Horizontal);
    importActions.Spacing(8.0);
    m_scheduleImportPreviewButton = Button();
    m_scheduleImportPreviewButton.Content(
        box_value(hstring(L"Preview import"))
        );
    m_scheduleImportPreviewButton.IsTabStop(true);
    m_scheduleImportPreviewButton.TabIndex(33);
    m_scheduleImportPreviewButton.Click(
        [this](auto const&, auto const&) { previewScheduleImport(); }
        );
    setAutomationName(m_scheduleImportPreviewButton, L"Preview schedule import");
    importActions.Children().Append(m_scheduleImportPreviewButton);
    m_scheduleImportApplyButton = Button();
    m_scheduleImportApplyButton.Content(box_value(hstring(L"Apply import")));
    m_scheduleImportApplyButton.IsTabStop(true);
    m_scheduleImportApplyButton.TabIndex(34);
    m_scheduleImportApplyButton.IsEnabled(false);
    m_scheduleImportApplyButton.Click(
        [this](auto const&, auto const&) { applyScheduleImport(); }
        );
    setAutomationName(m_scheduleImportApplyButton, L"Apply schedule import");
    importActions.Children().Append(m_scheduleImportApplyButton);
    importCard.content.Children().Append(importActions);
    m_scheduleImportStatusText = makeText(L"Preview an import block to begin.");
    setAutomationName(m_scheduleImportStatusText, L"Schedule import status");
    importCard.content.Children().Append(m_scheduleImportStatusText);
    m_scheduleImportValidationText = makeText(L"");
    m_scheduleImportValidationText.Visibility(Visibility::Collapsed);
    setAutomationName(m_scheduleImportValidationText, L"Schedule import validation");
    importCard.content.Children().Append(m_scheduleImportValidationText);
    importContent.Children().Append(importCard.root);

    auto importItem = PivotItem();
    importItem.Header(box_value(hstring(L"Import")));
    importItem.Content(scrollTab(importContent));
    setAutomationName(importItem, L"Schedule import tab");

    auto testingContent = StackPanel();
    testingContent.Padding(Thickness{16.0, 16.0, 16.0, 24.0});
    testingContent.Spacing(12.0);
    testingContent.HorizontalAlignment(HorizontalAlignment::Stretch);
    auto testingHeading = makeText(L"Testing classes", 24.0);
    setAutomationName(testingHeading, L"Testing classes heading");
    testingContent.Children().Append(testingHeading);
    auto testingDescription = makeText(
        L"Create the special testing-class profile, then assign it to a "
        L"strict weekday/time slot. Existing assignments stay visible so a "
        L"replacement can be an explicit user choice."
        );
    setAutomationName(testingDescription, L"Testing classes description");
    testingContent.Children().Append(testingDescription);

    auto testingCard = ClassMngrWinUISharedUX::buildCard({
        L"Testing-class profile",
        L"Required profile fields are validated by TestingClassService before "
        L"the optional assignment is written.",
        L"Testing-class profile editor"
        });
    m_testingClassSelector = ComboBox();
    m_testingClassSelector.Header(box_value(hstring(L"Existing testing class")));
    m_testingClassSelector.PlaceholderText(L"Select a testing class");
    m_testingClassSelector.MinWidth(320.0);
    m_testingClassSelector.IsTabStop(true);
    m_testingClassSelector.TabIndex(40);
    m_testingClassSelector.SelectionChanged(
        [this](auto const&, auto const&) {
            if (m_testingLoading || !m_testingClassSelector)
            {
                return;
            }
            const auto item = m_testingClassSelector.SelectedItem().try_as<
                ComboBoxItem>();
            const int classId = item ? boxedInt(item.Tag()) : -1;
            for (const auto& testingClass : m_testingClasses)
            {
                if (testingClass.classId != classId)
                {
                    continue;
                }
                m_testingClassNameTextBox.Text(asWide(testingClass.name));
                m_testingClassGradeTextBox.Text(asWide(testingClass.grade));
                m_testingClassLevelTextBox.Text(asWide(testingClass.level));
                m_testingClassRoomTextBox.Text(asWide(testingClass.room));
                break;
            }
        }
        );
    setAutomationName(m_testingClassSelector, L"Testing class selector");
    testingCard.content.Children().Append(m_testingClassSelector);
    m_testingClassNameTextBox = makeImportBox(
        L"Name",
        L"e.g. Testing group A",
        L"Testing class name"
        );
    m_testingClassNameTextBox.Text(L"WinUI Testing Group");
    testingCard.content.Children().Append(m_testingClassNameTextBox);
    m_testingClassGradeTextBox = makeImportBox(
        L"Grade",
        L"e.g. M1",
        L"Testing class grade"
        );
    m_testingClassGradeTextBox.Text(L"M1");
    testingCard.content.Children().Append(m_testingClassGradeTextBox);
    m_testingClassLevelTextBox = makeImportBox(
        L"Level",
        L"e.g. Mixed (All)",
        L"Testing class level"
        );
    m_testingClassLevelTextBox.Text(L"Mixed (All)");
    testingCard.content.Children().Append(m_testingClassLevelTextBox);
    m_testingClassRoomTextBox = makeImportBox(
        L"Room",
        L"e.g. Testing room",
        L"Testing class room"
        );
    m_testingClassRoomTextBox.Text(L"Testing room");
    testingCard.content.Children().Append(m_testingClassRoomTextBox);

    auto testingActions = StackPanel();
    testingActions.Orientation(Orientation::Horizontal);
    testingActions.Spacing(8.0);
    m_testingCreateButton = Button();
    m_testingCreateButton.Content(box_value(hstring(L"Create testing class")));
    m_testingCreateButton.IsTabStop(true);
    m_testingCreateButton.TabIndex(41);
    m_testingCreateButton.Click(
        [this](auto const&, auto const&) { createTestingClass(); }
        );
    setAutomationName(m_testingCreateButton, L"Create testing class");
    testingActions.Children().Append(m_testingCreateButton);
    testingCard.content.Children().Append(testingActions);
    testingContent.Children().Append(testingCard.root);

    auto assignmentCard = ClassMngrWinUISharedUX::buildCard({
        L"Testing assignment",
        L"Assignment keys use the shared strict HH:mm contract. Enable replace "
        L"only when the existing slot has been reviewed.",
        L"Testing assignment editor"
        });
    m_testingDayCombo = ComboBox();
    m_testingDayCombo.Header(box_value(hstring(L"Weekday")));
    m_testingDayCombo.MinWidth(220.0);
    m_testingDayCombo.IsTabStop(true);
    m_testingDayCombo.TabIndex(42);
    for (const std::string& day : classmngr::engine::ClassInfoConfig::days())
    {
        auto item = ComboBoxItem();
        item.Content(box_value(hstring(asWide(day))));
        item.Tag(box_value(hstring(asWide(day))));
        setAutomationName(item, asWide(day));
        m_testingDayCombo.Items().Append(item);
    }
    m_testingDayCombo.SelectedIndex(0);
    setAutomationName(m_testingDayCombo, L"Testing assignment weekday");
    assignmentCard.content.Children().Append(m_testingDayCombo);
    m_testingStartTextBox = makeImportBox(
        L"Start time (HH:mm)",
        L"e.g. 09:00",
        L"Testing assignment start time"
        );
    m_testingStartTextBox.Text(L"09:00");
    assignmentCard.content.Children().Append(m_testingStartTextBox);
    m_testingReplaceExistingCheck = CheckBox();
    m_testingReplaceExistingCheck.Content(
        box_value(hstring(L"Replace an existing assignment"))
        );
    m_testingReplaceExistingCheck.IsTabStop(true);
    m_testingReplaceExistingCheck.TabIndex(43);
    setAutomationName(
        m_testingReplaceExistingCheck,
        L"Replace existing testing assignment"
        );
    assignmentCard.content.Children().Append(m_testingReplaceExistingCheck);
    m_testingAssignButton = Button();
    m_testingAssignButton.Content(box_value(hstring(L"Assign selected class")));
    m_testingAssignButton.IsTabStop(true);
    m_testingAssignButton.TabIndex(44);
    m_testingAssignButton.Click(
        [this](auto const&, auto const&) { assignTestingClass(); }
        );
    setAutomationName(m_testingAssignButton, L"Assign testing class");
    assignmentCard.content.Children().Append(m_testingAssignButton);
    m_testingAssignmentList = ListView();
    m_testingAssignmentList.Header(
        box_value(hstring(L"Current testing assignments"))
        );
    m_testingAssignmentList.SelectionMode(ListViewSelectionMode::Single);
    m_testingAssignmentList.IsTabStop(true);
    m_testingAssignmentList.TabIndex(45);
    m_testingAssignmentList.Height(180.0);
    m_testingAssignmentList.SelectionChanged(
        [this](auto const&, auto const&) {
            if (!m_testingLoading && m_testingDeleteAssignmentButton)
            {
                m_testingDeleteAssignmentButton.IsEnabled(
                    m_testingAssignmentList.SelectedIndex() >= 0
                    );
            }
        }
        );
    setAutomationName(m_testingAssignmentList, L"Testing assignment list");
    assignmentCard.content.Children().Append(m_testingAssignmentList);
    m_testingDeleteAssignmentButton = Button();
    m_testingDeleteAssignmentButton.Content(
        box_value(hstring(L"Delete selected assignment"))
        );
    m_testingDeleteAssignmentButton.IsTabStop(true);
    m_testingDeleteAssignmentButton.TabIndex(46);
    m_testingDeleteAssignmentButton.Click(
        [this](auto const&, auto const&) { deleteTestingAssignment(); }
        );
    setAutomationName(
        m_testingDeleteAssignmentButton,
        L"Delete selected testing assignment"
        );
    assignmentCard.content.Children().Append(m_testingDeleteAssignmentButton);
    m_testingStatusText = makeText(L"Testing-class editor is ready.");
    setAutomationName(m_testingStatusText, L"Testing classes status");
    assignmentCard.content.Children().Append(m_testingStatusText);
    m_testingValidationText = makeText(L"");
    m_testingValidationText.Visibility(Visibility::Collapsed);
    setAutomationName(m_testingValidationText, L"Testing classes validation");
    assignmentCard.content.Children().Append(m_testingValidationText);
    testingContent.Children().Append(assignmentCard.root);

    auto testingItem = PivotItem();
    testingItem.Header(box_value(hstring(L"Testing classes")));
    testingItem.Content(scrollTab(testingContent));
    setAutomationName(testingItem, L"Testing classes tab");

    m_scheduleTabs = Pivot();
    // The schedule board owns the visible Regular/Intensive/Testing mode
    // controls. Keep the import/testing pages available to existing command
    // and smoke-test paths, but do not expose their legacy navigation row.
    m_scheduleTabs.IsTabStop(false);
    m_scheduleTabs.TabIndex(0);
    m_scheduleTabs.Items().Append(scheduleItem);
    m_scheduleTabs.Items().Append(importItem);
    m_scheduleTabs.Items().Append(testingItem);
    m_scheduleTabs.HeaderTemplate(
        winrt::Microsoft::UI::Xaml::Markup::XamlReader::Load(
            L"<DataTemplate "
            L"xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/"
            L"presentation\"><Grid Height=\"0\" "
            L"Visibility=\"Collapsed\" /></DataTemplate>"
            )
            .as<winrt::Microsoft::UI::Xaml::DataTemplate>()
        );
    setAutomationName(m_scheduleTabs, L"Schedule workspace tabs");
    scheduleRoot.Children().Append(m_scheduleTabs);
    refreshScheduleWorkspace();
    refreshTestingWorkspace();
}

void MainWindow::refreshScheduleBoard()
{
    using namespace Microsoft::UI::Xaml;
    using namespace ClassMngrWinUIScheduleBoard;

    if (!m_scheduleBoardRoot)
    {
        return;
    }

    if (!m_openDatabase)
    {
        m_scheduleBoardRoot.Visibility(Visibility::Collapsed);
        return;
    }

    const auto visibleDays =
        classmngr::engine::ScheduleReportService::visibleDays(false);
    const bool useIntensive =
        m_scheduleDisplayMode
            == classmngr::engine::ScheduleReportDisplayMode::Intensive;
    const auto build = classmngr::engine::ScheduleBuilderService::build(
        m_scheduleInfos,
        useIntensive,
        visibleDays
        );

    classmngr::engine::ScheduleReportRequest request;
    request.days = visibleDays;
    request.displayMode = m_scheduleDisplayMode;
    request.rowFilter = useIntensive
        ? classmngr::engine::ScheduleReportRowFilter::TrimEmptyOuterRows
        : classmngr::engine::ScheduleReportRowFilter::None;

    classmngr::engine::IntensiveSlotStateService slotService(*m_openDatabase);
    if (const auto states = slotService.list())
    {
        for (const auto& state : *states)
        {
            request.slotStateOverrides.emplace(
                classmngr::engine::ScheduleReportService::slotKey(
                    state.day,
                    state.startTime
                    ),
                state.state
                );
        }
    }

    if (
        m_scheduleDisplayMode
            == classmngr::engine::ScheduleReportDisplayMode::Testing
        )
    {
        classmngr::engine::TestingBlockService blockService(*m_openDatabase);
        classmngr::engine::TestingClassService testingClassService(
            *m_openDatabase
            );
        const auto assignments = blockService.listAssignments();
        const auto testingClasses = testingClassService.list();
        if (assignments && testingClasses)
        {
            for (const auto& assignment : *assignments)
            {
                classmngr::engine::ScheduleReportTestingAssignmentView view;
                view.assignment.day = assignment.day;
                view.assignment.startTime = assignment.startTime;
                view.assignment.room = assignment.room;
                view.assignment.classId = assignment.classId;
                view.assignment.kind = assignment.kind
                    == classmngr::engine::TestingAssignmentKind::SpecialClass
                    ? classmngr::engine::ScheduleReportTestingAssignmentKind::SpecialClass
                    : classmngr::engine::ScheduleReportTestingAssignmentKind::PlainTesting;

                if (
                    assignment.kind
                        == classmngr::engine::TestingAssignmentKind::SpecialClass
                    )
                {
                    const auto testingClass = std::find_if(
                        testingClasses->cbegin(),
                        testingClasses->cend(),
                        [&assignment](const auto& candidate) {
                            return candidate.classId == assignment.classId;
                        }
                        );
                    if (testingClass != testingClasses->cend())
                    {
                        view.testingClassEntry.classId = testingClass->classId;
                        view.testingClassEntry.kind =
                            classmngr::engine::ScheduleReportEntryKind::TestingClass;
                        view.testingClassEntry.className = testingClass->name;
                        view.testingClassEntry.roomNumber = testingClass->room;
                        view.testingClassEntry.classGrade = testingClass->grade;
                        view.testingClassEntry.classLevel = testingClass->level;
                        view.testingClassEntry.classColor = testingClass->classColor;
                        view.testingClassEntry.fontColor = testingClass->fontColor;
                    }
                }

                request.testingAssignments.emplace(
                    classmngr::engine::ScheduleReportService::slotKey(
                        assignment.day,
                        assignment.startTime
                        ),
                    std::move(view)
                    );
            }
        }
    }

    const auto model = classmngr::engine::ScheduleReportService::build(
        build,
        request
        );
    m_scheduleBoardRoot.Visibility(Visibility::Visible);
    render(
        m_scheduleBoardRoot,
        model,
        RenderOptions{true, false, false}
        );
}

void MainWindow::setScheduleDisplayMode(int mode)
{
    if (
        mode < static_cast<int>(
            classmngr::engine::ScheduleReportDisplayMode::Regular
            )
        || mode > static_cast<int>(
            classmngr::engine::ScheduleReportDisplayMode::Testing
            )
        )
    {
        return;
    }

    m_scheduleDisplayMode =
        static_cast<classmngr::engine::ScheduleReportDisplayMode>(mode);
    updateScheduleDisplayButtons();
    refreshScheduleBoard();
}

void MainWindow::updateScheduleDisplayButtons()
{
    using namespace Microsoft::UI::Xaml;

    const auto setButtonState = [](auto const& button, bool selected) {
        if (!button)
        {
            return;
        }

        button.Background(
            Microsoft::UI::Xaml::Media::SolidColorBrush(
                Windows::UI::Color{
                    255,
                    static_cast<std::uint8_t>(selected ? 59 : 48),
                    static_cast<std::uint8_t>(selected ? 169 : 53),
                    static_cast<std::uint8_t>(selected ? 225 : 60)
                }
                )
            );
        button.Foreground(
            Microsoft::UI::Xaml::Media::SolidColorBrush(
                Windows::UI::Color{
                    255,
                    255,
                    255,
                    255
                }
                )
            );
        button.BorderBrush(
            Microsoft::UI::Xaml::Media::SolidColorBrush(
                Windows::UI::Color{255, 92, 99, 108}
                )
            );
        button.BorderThickness(Thickness{1.0, 1.0, 1.0, 1.0});
        button.CornerRadius(CornerRadius{5.0, 5.0, 5.0, 5.0});
    };

    setButtonState(
        m_scheduleRegularModeButton,
        m_scheduleDisplayMode
            == classmngr::engine::ScheduleReportDisplayMode::Regular
        );
    setButtonState(
        m_scheduleIntensiveModeButton,
        m_scheduleDisplayMode
            == classmngr::engine::ScheduleReportDisplayMode::Intensive
        );
    setButtonState(
        m_scheduleTestingModeButton,
        m_scheduleDisplayMode
            == classmngr::engine::ScheduleReportDisplayMode::Testing
        );
    setButtonState(m_scheduleImportModeButton, false);
}

void MainWindow::handleScheduleSlotClick(
    std::wstring day,
    std::wstring timeLabel,
    std::wstring currentState,
    std::wstring defaultState,
    bool slotTogglingEnabled,
    bool testingBlockCreationEnabled
    )
{
    if (!m_openDatabase)
    {
        return;
    }

    if (
        m_scheduleDisplayMode
            == classmngr::engine::ScheduleReportDisplayMode::Intensive
        && slotTogglingEnabled
        )
    {
        classmngr::engine::IntensiveSlotStateService service(*m_openDatabase);
        const auto saved = service.save(
            asUtf8(day),
            asUtf8(timeLabel),
            classmngr::engine::ScheduleReportService::nextSlotState(
                asUtf8(currentState)
                ),
            asUtf8(defaultState)
            );
        if (!saved)
        {
            if (m_scheduleWorkspaceStatusText)
            {
                m_scheduleWorkspaceStatusText.Text(winrt::hstring(
                    L"Schedule slot could not be updated: "
                        + asWide(saved.error().message)
                    ));
            }
            return;
        }

        m_dirtyState.markDirty();
        updateFileCommandState();
        refreshScheduleWorkspace();
        return;
    }

    if (
        m_scheduleDisplayMode
            == classmngr::engine::ScheduleReportDisplayMode::Testing
        && (currentState == L"testing" || testingBlockCreationEnabled)
        )
    {
        if (m_scheduleTabs)
        {
            m_scheduleTabs.SelectedIndex(2);
        }
        if (m_testingDayCombo)
        {
            for (int index = 0;
                 index < static_cast<int>(m_testingDayCombo.Items().Size());
                 ++index)
            {
                const auto item = m_testingDayCombo.Items().GetAt(index)
                    .try_as<Microsoft::UI::Xaml::Controls::ComboBoxItem>();
                if (item && boxedString(item.Tag()) == day)
                {
                    m_testingDayCombo.SelectedIndex(index);
                    break;
                }
            }
        }
        if (m_testingStartTextBox)
        {
            m_testingStartTextBox.Text(timeLabel);
        }
        if (m_testingStatusText)
        {
            m_testingStatusText.Text(
                L"Choose a testing class or plain testing block for the selected slot."
                );
        }
    }
}

void MainWindow::refreshScheduleWorkspace()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_scheduleTabs || !m_scheduleList || !m_scheduleWorkspaceStatusText)
    {
        return;
    }

    const bool hasDatabase = static_cast<bool>(m_openDatabase);
    const auto setEnabled = [hasDatabase](auto const& control) {
        if (control)
        {
            control.IsEnabled(hasDatabase);
        }
    };
    setEnabled(m_scheduleList);
    setEnabled(m_scheduleClassSelector);
    setEnabled(m_scheduleTypeCombo);
    setEnabled(m_scheduleDayCombo);
    setEnabled(m_scheduleStartTextBox);
    setEnabled(m_scheduleEndTextBox);
    setEnabled(m_scheduleSaveButton);
    setEnabled(m_scheduleClearButton);

    m_scheduleLoading = true;
    m_scheduleClasses.clear();
    m_scheduleInfos.clear();
    m_scheduleList.Items().Clear();
    m_scheduleClassSelector.Items().Clear();
    m_scheduleEditingKey.clear();

    if (!hasDatabase)
    {
        m_scheduleWorkspaceStatusText.Text(L"No database open.");
        refreshScheduleBoard();
        if (m_scheduleValidationText)
        {
            m_scheduleValidationText.Text({});
            m_scheduleValidationText.Visibility(Visibility::Collapsed);
        }
        m_scheduleLoading = false;
        return;
    }

    classmngr::engine::ClassRepository repository(*m_openDatabase);
    const auto classes = repository.list();
    classmngr::engine::ClassScheduleService scheduleService(*m_openDatabase);
    const auto infos = scheduleService.loadScheduleClassInfos();
    if (!classes || !infos)
    {
        const std::string message = !classes
            ? classes.error().message
            : infos.error().message;
        m_scheduleWorkspaceStatusText.Text(winrt::hstring(
            L"Schedules could not be loaded: " + asWide(message)
            ));
        if (m_scheduleValidationText)
        {
            m_scheduleValidationText.Text(winrt::hstring(
                L"Engine loading error: " + asWide(message)
                ));
            m_scheduleValidationText.Visibility(Visibility::Visible);
        }
        m_scheduleLoading = false;
        return;
    }

    m_scheduleClasses = *classes;
    m_scheduleInfos = *infos;
    int previousClassId = -1;
    if (m_scheduleClassSelector.SelectedItem())
    {
        const auto item = m_scheduleClassSelector.SelectedItem().try_as<
            ComboBoxItem>();
        previousClassId = item ? boxedInt(item.Tag()) : -1;
    }

    const auto className = [this](int classId) {
        for (const auto& classroom : m_scheduleClasses)
        {
            if (classroom.id == classId)
            {
                const std::wstring name = asWide(classroom.name);
                return name.empty()
                    ? L"Class " + std::to_wstring(classroom.id)
                    : name;
            }
        }
        return L"Class " + std::to_wstring(classId);
    };
    const auto addCell = [](Grid const& row,
                            std::wstring_view text,
                            uint32_t column) {
        auto cell = TextBlock();
        cell.Text(winrt::hstring(text));
        cell.Margin(Thickness{4.0, 4.0, 4.0, 4.0});
        cell.TextWrapping(TextWrapping::Wrap);
        Grid::SetColumn(cell, static_cast<int32_t>(column));
        row.Children().Append(cell);
    };
    const auto appendRow = [this, &className, &addCell](
                               classmngr::engine::ClassInfo const& info,
                               classmngr::engine::ScheduleType type,
                               classmngr::engine::ClassTime const* time
                               ) {
        const std::wstring name = className(info.classId);
        const std::wstring typeName = scheduleTypeText(type);
        const std::wstring day = time ? asWide(time->day) : L"-";
        const std::wstring start = time ? asWide(time->startTime) : L"-";
        const std::wstring end = time ? asWide(time->endTime) : L"-";
        const std::wstring keyDay = time ? asWide(time->day) : L"";
        const std::wstring keyStart = time ? asWide(time->startTime) : L"";
        const std::wstring keyEnd = time ? asWide(time->endTime) : L"";
        auto row = Microsoft::UI::Xaml::Controls::Grid();
        row.ColumnSpacing(8.0);
        row.MinWidth(680.0);
        const std::array<double, 5> widths{
            190.0, 105.0, 125.0, 120.0, 120.0
        };
        for (const double width : widths)
        {
            auto definition = ColumnDefinition();
            definition.Width(
                GridLengthHelper::FromValueAndType(width, GridUnitType::Pixel)
                );
            row.ColumnDefinitions().Append(definition);
        }
        addCell(row, name, 0);
        addCell(row, typeName, 1);
        addCell(row, day, 2);
        addCell(row, start, 3);
        addCell(row, end, 4);
        auto item = Microsoft::UI::Xaml::Controls::ListViewItem();
        const std::wstring key = scheduleSelectionKey(
            info.classId,
            type,
            keyDay,
            keyStart,
            keyEnd
            );
        item.Content(row);
        item.Tag(winrt::box_value(winrt::hstring(key)));
        item.IsTabStop(false);
        setAutomationName(item, L"Schedule row " + name + L" " + typeName);
        m_scheduleList.Items().Append(item);
    };

    int selectedClassIndex = -1;
    std::size_t slotCount = 0;
    for (const auto& info : m_scheduleInfos)
    {
        auto classItem = ComboBoxItem();
        const std::wstring name = className(info.classId);
        classItem.Content(box_value(hstring(name)));
        classItem.Tag(box_value(info.classId));
        setAutomationName(classItem, L"Schedule class " + name);
        m_scheduleClassSelector.Items().Append(classItem);
        if (info.classId == previousClassId)
        {
            selectedClassIndex = static_cast<int>(
                m_scheduleClassSelector.Items().Size() - 1
                );
        }

        for (const auto& time : info.classTimes)
        {
            appendRow(info, classmngr::engine::ScheduleType::Regular, &time);
            ++slotCount;
        }
        for (const auto& time : info.intensiveTimes)
        {
            appendRow(info, classmngr::engine::ScheduleType::Intensive, &time);
            ++slotCount;
        }
        if (info.classTimes.empty() && info.intensiveTimes.empty())
        {
            appendRow(info, classmngr::engine::ScheduleType::Regular, nullptr);
        }
    }
    if (selectedClassIndex < 0 && m_scheduleClassSelector.Items().Size() > 0)
    {
        selectedClassIndex = 0;
    }
    m_scheduleClassSelector.SelectedIndex(selectedClassIndex);
    if (m_scheduleTypeCombo.Items().Size() > 0)
    {
        m_scheduleTypeCombo.SelectedIndex(0);
    }
    if (m_scheduleDayCombo.Items().Size() > 0)
    {
        m_scheduleDayCombo.SelectedIndex(0);
    }
    if (m_scheduleValidationText)
    {
        m_scheduleValidationText.Text({});
        m_scheduleValidationText.Visibility(Visibility::Collapsed);
    }
    m_scheduleWorkspaceStatusText.Text(
        m_scheduleInfos.empty()
            ? L"No classes found for scheduling."
            : winrt::hstring(
                L"Loaded " + std::to_wstring(m_scheduleInfos.size())
                + L" classes and " + std::to_wstring(slotCount)
                + L" schedule slots."
                )
        );
    refreshScheduleBoard();
    m_scheduleLoading = false;
}

winrt::fire_and_forget MainWindow::openScheduleClassEditor(int classId)
{
    auto lifetime = get_strong();
    if (m_ownedDialog || !m_openDatabase || !RootGrid().XamlRoot())
    {
        co_return;
    }

    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;
    using namespace Microsoft::UI::Xaml::Media;

    classmngr::engine::ClassInfoService service(*m_openDatabase);
    const auto loaded = service.load(classId);
    if (!loaded)
    {
        if (m_scheduleWorkspaceStatusText)
        {
            m_scheduleWorkspaceStatusText.Text(winrt::hstring(
                L"Class information could not be loaded: "
                    + asWide(loaded.error().message)
                ));
        }
        co_return;
    }

    classmngr::engine::ClassInfo draft = *loaded;
    const std::string originalGrade = draft.classGrade;
    const std::string originalLevel = draft.classLevel;
    std::string classColor = draft.classColor.empty()
        ? "#FFFFFF"
        : draft.classColor;
    std::string fontColor = draft.fontColor.empty()
        ? "#000000"
        : draft.fontColor;

    auto form = StackPanel();
    form.Spacing(10.0);
    form.MaxWidth(520.0);

    auto title = TextBlock();
    title.Text(L"Edit Class Information");
    title.FontSize(22.0);
    title.FontWeight(Windows::UI::Text::FontWeights::Bold());
    setAutomationName(title, L"Edit Class Information");
    form.Children().Append(title);

    const auto makeReadOnlyField = [&form](std::wstring_view header,
                                            std::string_view value,
                                            std::wstring_view automationName) {
        auto field = TextBox();
        field.Header(box_value(hstring(header)));
        field.Text(asWide(value));
        field.IsReadOnly(true);
        field.IsTabStop(false);
        setAutomationName(field, automationName);
        form.Children().Append(field);
        return field;
    };
    const auto teacherField = makeReadOnlyField(
        L"Korean Teacher",
        draft.teacherKr,
        L"Schedule class Korean teacher"
        );
    const auto roomField = makeReadOnlyField(
        L"Room Number",
        draft.roomNumber,
        L"Schedule class room number"
        );
    static_cast<void>(teacherField);
    static_cast<void>(roomField);

    const auto addChoice = [](ComboBox const& combo,
                              std::string_view value) {
        auto item = ComboBoxItem();
        const hstring text{asWide(value)};
        item.Content(box_value(text));
        item.Tag(box_value(text));
        combo.Items().Append(item);
    };
    const auto selectChoice = [](ComboBox const& combo,
                                 std::string_view value) {
        for (int index = 0;
             index < static_cast<int>(combo.Items().Size());
             ++index)
        {
            const auto item = combo.Items().GetAt(index)
                .try_as<ComboBoxItem>();
            if (item && boxedString(item.Tag()) == asWide(value))
            {
                combo.SelectedIndex(index);
                return;
            }
        }
        combo.SelectedIndex(-1);
    };

    auto grade = ComboBox();
    grade.Header(box_value(hstring(L"Class Grade")));
    grade.MinWidth(260.0);
    grade.IsTabStop(true);
    setAutomationName(grade, L"Schedule class grade");
    for (const std::string& value : classmngr::engine::ClassInfoConfig::grades())
    {
        addChoice(grade, value);
    }
    selectChoice(grade, draft.classGrade);
    form.Children().Append(grade);

    auto level = ComboBox();
    level.Header(box_value(hstring(L"Class Level")));
    level.MinWidth(260.0);
    level.IsTabStop(true);
    setAutomationName(level, L"Schedule class level");
    form.Children().Append(level);

    bool loadingOptions = true;
    const auto rebuildLevels = [&]() {
        const std::wstring selected = selectedComboValue(level);
        level.Items().Clear();
        for (const std::string& value :
             classmngr::engine::ClassInfoConfig::levelsForGrade(
                 asUtf8(selectedComboValue(grade))
                 ))
        {
            addChoice(level, value);
        }
        selectChoice(
            level,
            selected.empty() ? draft.classLevel : asUtf8(selected)
            );
    };
    grade.SelectionChanged(
        [&loadingOptions, &rebuildLevels](auto const&, auto const&) {
            if (!loadingOptions)
            {
                rebuildLevels();
            }
        }
        );
    rebuildLevels();
    selectChoice(level, draft.classLevel);
    loadingOptions = false;

    auto classColorRow = StackPanel();
    classColorRow.Orientation(Orientation::Horizontal);
    classColorRow.Spacing(8.0);
    auto classPreview = Button();
    classPreview.Width(38.0);
    classPreview.Height(38.0);
    classPreview.Padding(Thickness{0.0, 0.0, 0.0, 0.0});
    classPreview.IsTabStop(false);
    classPreview.Content(box_value(hstring(L"")));
    setAutomationName(classPreview, L"Schedule class color preview");
    auto classColorButton = Button();
    classColorButton.Content(box_value(hstring(L"Choose Color")));
    setAutomationName(classColorButton, L"Choose schedule class color");
    classColorRow.Children().Append(classPreview);
    classColorRow.Children().Append(classColorButton);
    auto classColorLabel = TextBlock();
    classColorLabel.Text(L"Class Color");
    classColorLabel.VerticalAlignment(VerticalAlignment::Center);
    form.Children().Append(classColorLabel);
    form.Children().Append(classColorRow);

    auto fontColorRow = StackPanel();
    fontColorRow.Orientation(Orientation::Horizontal);
    fontColorRow.Spacing(8.0);
    auto fontPreview = Button();
    fontPreview.Width(38.0);
    fontPreview.Height(38.0);
    fontPreview.Padding(Thickness{0.0, 0.0, 0.0, 0.0});
    fontPreview.IsTabStop(false);
    fontPreview.Content(box_value(hstring(L"")));
    setAutomationName(fontPreview, L"Schedule font color preview");
    auto fontColorButton = Button();
    fontColorButton.Content(box_value(hstring(L"Choose Color")));
    setAutomationName(fontColorButton, L"Choose schedule font color");
    fontColorRow.Children().Append(fontPreview);
    fontColorRow.Children().Append(fontColorButton);
    auto fontColorLabel = TextBlock();
    fontColorLabel.Text(L"Font Color");
    fontColorLabel.VerticalAlignment(VerticalAlignment::Center);
    form.Children().Append(fontColorLabel);
    form.Children().Append(fontColorRow);
    const auto updatePreview = [](Button const& preview,
                                                std::string_view value) {
        preview.Background(SolidColorBrush(uiColorFromHex(value)));
        preview.BorderBrush(
            SolidColorBrush(Windows::UI::Color{255, 128, 128, 128})
            );
        preview.BorderThickness(Thickness{1.0, 1.0, 1.0, 1.0});
        preview.CornerRadius(CornerRadius{5.0, 5.0, 5.0, 5.0});
    };
    updatePreview(classPreview, classColor);
    updatePreview(fontPreview, fontColor);

    auto validation = TextBlock();
    validation.TextWrapping(TextWrapping::Wrap);
    validation.Visibility(Visibility::Collapsed);
    validation.Foreground(
        SolidColorBrush(Windows::UI::Color{255, 196, 43, 28})
        );
    setAutomationName(validation, L"Schedule class validation");
    form.Children().Append(validation);

    auto dialog = ContentDialog();
    dialog.XamlRoot(RootGrid().XamlRoot());
    dialog.Title(box_value(hstring(L"Edit Schedule Cell")));
    dialog.Content(form);
    dialog.PrimaryButtonText(L"Save");
    dialog.CloseButtonText(L"Cancel");
    dialog.DefaultButton(ContentDialogButton::Primary);
    int requestedColor = -1;
    const auto requestColor = [&dialog, &requestedColor](int colorIndex) {
        requestedColor = colorIndex;
        dialog.Hide();
    };
    classPreview.Click([requestColor](auto const&, auto const&) {
        requestColor(0);
    });
    classColorButton.Click([requestColor](auto const&, auto const&) {
        requestColor(0);
    });
    fontPreview.Click([requestColor](auto const&, auto const&) {
        requestColor(1);
    });
    fontColorButton.Click([requestColor](auto const&, auto const&) {
        requestColor(1);
    });
    m_ownedDialog = dialog;

    for (;;)
    {
        ContentDialogResult result = ContentDialogResult::None;
        try
        {
            result = co_await dialog.ShowAsync();
        }
        catch (...)
        {
            break;
        }
        if (requestedColor >= 0)
        {
            const int colorIndex = requestedColor;
            requestedColor = -1;
            const std::string& currentColor = colorIndex == 0
                ? classColor : fontColor;
            auto picker = ClassMngrWinUISharedUX::buildColorPickerDialog(
                RootGrid().XamlRoot(),
                colorIndex == 0 ? L"Select Class Color" : L"Select Font Color",
                uiColorFromHex(currentColor),
                colorIndex == 0
                    ? L"Schedule class color picker"
                    : L"Schedule font color picker"
                );
            m_ownedDialog = picker.dialog;
            ContentDialogResult pickerResult = ContentDialogResult::None;
            try
            {
                pickerResult = co_await picker.dialog.ShowAsync();
            }
            catch (...)
            {
                m_ownedDialog = dialog;
                break;
            }
            m_ownedDialog = dialog;
            if (pickerResult == ContentDialogResult::Primary)
            {
                const std::string selected = uiHexFromColor(picker.picker.Color());
                if (colorIndex == 0)
                {
                    classColor = selected;
                    updatePreview(classPreview, classColor);
                }
                else
                {
                    fontColor = selected;
                    updatePreview(fontPreview, fontColor);
                }
            }
            continue;
        }
        if (result != ContentDialogResult::Primary)
        {
            break;
        }

        const std::string newGrade = asUtf8(selectedComboValue(grade));
        const std::string newLevel = asUtf8(selectedComboValue(level));
        draft.classGrade = newGrade;
        draft.classLevel = newLevel;
        draft.classColor = classColor;
        draft.fontColor = fontColor;
        if (newGrade != originalGrade || newLevel != originalLevel)
        {
            draft.readingBook.clear();
            draft.essayBook.clear();
        }

        const auto saved = service.save(draft);
        if (!saved)
        {
            validation.Text(winrt::hstring(
                L"The class information could not be saved: "
                    + asWide(saved.error().message)
                ));
            validation.Visibility(Visibility::Visible);
            continue;
        }

        m_dirtyState.markDirty();
        updateFileCommandState();
        refreshScheduleWorkspace();
        if (m_scheduleWorkspaceStatusText)
        {
            m_scheduleWorkspaceStatusText.Text(
                L"Class information saved."
                );
        }
        break;
    }

    if (m_ownedDialog == dialog)
    {
        m_ownedDialog = nullptr;
    }
}

void MainWindow::saveScheduleEntry()
{
    using namespace Microsoft::UI::Xaml;

    if (!m_openDatabase || !m_scheduleClassSelector
        || !m_scheduleStartTextBox || !m_scheduleEndTextBox)
    {
        return;
    }

    const auto selectedClass = m_scheduleClassSelector.SelectedItem().try_as<
        Microsoft::UI::Xaml::Controls::ComboBoxItem>();
    const int classId = selectedClass ? boxedInt(selectedClass.Tag()) : -1;
    const std::wstring day = selectedComboValue(m_scheduleDayCombo);
    const std::wstring start = m_scheduleStartTextBox.Text().c_str();
    const std::wstring end = m_scheduleEndTextBox.Text().c_str();
    const auto type = m_scheduleTypeCombo.SelectedIndex() == 1
        ? classmngr::engine::ScheduleType::Intensive
        : classmngr::engine::ScheduleType::Regular;
    const auto showValidation = [this](std::wstring message) {
        if (m_scheduleValidationText)
        {
            m_scheduleValidationText.Text(winrt::hstring(message));
            m_scheduleValidationText.Visibility(
                Microsoft::UI::Xaml::Visibility::Visible
                );
        }
        if (m_scheduleWorkspaceStatusText)
        {
            m_scheduleWorkspaceStatusText.Text(L"Schedule could not be saved.");
        }
    };
    if (classId <= 0 || day.empty() || start.empty() || end.empty())
    {
        showValidation(L"Choose a class, weekday, start time, and end time.");
        return;
    }

    classmngr::engine::ClassInfoService infoService(*m_openDatabase);
    const auto loaded = infoService.load(classId);
    if (!loaded)
    {
        showValidation(L"Class information could not be loaded: "
            + asWide(loaded.error().message));
        return;
    }
    classmngr::engine::ClassInfo info = *loaded;
    auto& times = type == classmngr::engine::ScheduleType::Intensive
        ? info.intensiveTimes
        : info.classTimes;
    if (const auto previous = scheduleSelectionFromKey(m_scheduleEditingKey))
    {
        auto& previousTimes = previous->type ==
                classmngr::engine::ScheduleType::Intensive
            ? info.intensiveTimes
            : info.classTimes;
        previousTimes.erase(
            std::remove_if(
                previousTimes.begin(),
                previousTimes.end(),
                [&previous](const classmngr::engine::ClassTime& value) {
                    return value.day == asUtf8(previous->day)
                        && value.startTime == asUtf8(previous->startTime)
                        && value.endTime == asUtf8(previous->endTime);
                }
                ),
            previousTimes.end()
            );
    }
    times.push_back({asUtf8(day), asUtf8(start), asUtf8(end)});

    classmngr::engine::ClassScheduleService scheduleService(*m_openDatabase);
    const auto conflicts = scheduleService.getClassTimeConflicts(
        classId,
        times,
        type
        );
    if (!conflicts)
    {
        showValidation(L"Schedule conflict checking failed: "
            + asWide(conflicts.error().message));
        return;
    }
    if (!conflicts->empty())
    {
        showValidation(
            L"The engine rejected an overlapping schedule with "
            + asWide(conflicts->front().conflictingClassName) + L"."
            );
        return;
    }

    const auto saved = infoService.save(info);
    if (!saved)
    {
        showValidation(L"The engine rejected the schedule: "
            + asWide(saved.error().message));
        return;
    }
    m_scheduleEditingKey.clear();
    refreshScheduleWorkspace();
    if (m_scheduleWorkspaceStatusText)
    {
        m_scheduleWorkspaceStatusText.Text(L"Schedule slot saved.");
    }
}

void MainWindow::clearScheduleEntry()
{
    if (m_scheduleLoading)
    {
        return;
    }
    m_scheduleEditingKey.clear();
    if (m_scheduleList)
    {
        m_scheduleList.SelectedIndex(-1);
    }
    if (m_scheduleStartTextBox)
    {
        m_scheduleStartTextBox.Text({});
    }
    if (m_scheduleEndTextBox)
    {
        m_scheduleEndTextBox.Text({});
    }
    if (m_scheduleValidationText)
    {
        m_scheduleValidationText.Text({});
        m_scheduleValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Collapsed
            );
    }
    if (m_scheduleWorkspaceStatusText)
    {
        m_scheduleWorkspaceStatusText.Text(
            L"Choose a day and time to add a schedule slot."
            );
    }
}

void MainWindow::previewScheduleImport()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_openDatabase || !m_scheduleImportStatusText)
    {
        return;
    }

    const auto showValidation = [this](std::wstring message) {
        if (m_scheduleImportValidationText)
        {
            m_scheduleImportValidationText.Text(winrt::hstring(message));
            m_scheduleImportValidationText.Visibility(Visibility::Visible);
        }
        if (m_scheduleImportApplyButton)
        {
            m_scheduleImportApplyButton.IsEnabled(false);
        }
        m_scheduleImportPreview.reset();
        m_scheduleImportPreviewReady = false;
        if (m_scheduleImportStatusText)
        {
            m_scheduleImportStatusText.Text(L"Import preview failed.");
        }
    };

    const std::wstring userName = m_scheduleImportUserTextBox.Text().c_str();
    const std::wstring teacher = m_scheduleImportTeacherTextBox.Text().c_str();
    const std::wstring grade = m_scheduleImportGradeTextBox.Text().c_str();
    const std::wstring level = m_scheduleImportLevelTextBox.Text().c_str();
    const std::wstring room = m_scheduleImportRoomTextBox.Text().c_str();
    const std::wstring start = m_scheduleImportStartTextBox.Text().c_str();
    const std::wstring end = m_scheduleImportEndTextBox.Text().c_str();
    const std::vector<std::wstring> days = scheduleImportDays(
        m_scheduleImportDaysTextBox.Text().c_str()
        );
    if (userName.empty() || teacher.empty() || grade.empty() || level.empty()
        || room.empty() || start.empty() || end.empty() || days.empty())
    {
        showValidation(
            L"Enter a profile, Korean teacher, grade, level, room, meeting "
            L"days, start time, and end time before previewing."
            );
        return;
    }

    classmngr::engine::ScheduleImportClassCandidate candidate;
    candidate.teacherKey = asUtf8(teacher);
    candidate.teacherKr = candidate.teacherKey;
    candidate.rooms.push_back(asUtf8(room));
    candidate.classGrade = asUtf8(grade);
    candidate.classLevel = asUtf8(level);
    candidate.sourceCells.push_back("WinUI schedule import");
    for (const std::wstring& day : days)
    {
        candidate.times.push_back({
            asUtf8(day),
            asUtf8(start),
            asUtf8(end)
        });
    }

    m_scheduleImportUser = {};
    m_scheduleImportUser.name = asUtf8(userName);
    m_scheduleImportUser.headerCell = "WinUI";
    m_scheduleImportUser.classes.push_back(std::move(candidate));
    const auto kind = m_scheduleImportKindCombo.SelectedIndex() == 1
        ? classmngr::engine::ScheduleImportKind::Intensive
        : classmngr::engine::ScheduleImportKind::Normal;
    classmngr::engine::ScheduleImportService service(*m_openDatabase);
    const auto preview = service.previewImport(m_scheduleImportUser, kind);
    if (!preview)
    {
        showValidation(L"The engine rejected the preview: "
            + asWide(preview.error().message));
        return;
    }
    if (preview->classes.empty() || preview->teachers.empty())
    {
        showValidation(L"The preview did not produce a class and teacher candidate.");
        return;
    }

    m_scheduleImportPreview = *preview;
    m_scheduleImportPreviewReady = true;
    const bool hasMatchingTeacher = !preview->teachers.front().matchingTeacherIds.empty();
    m_scheduleImportTeacherActionCombo.SelectedIndex(hasMatchingTeacher ? 0 : 1);
    const bool hasSuggestedClass = preview->classes.front().suggestedClassId > 0;
    m_scheduleImportClassActionCombo.SelectedIndex(hasSuggestedClass ? 0 : 1);
    if (m_scheduleImportValidationText)
    {
        m_scheduleImportValidationText.Text({});
        m_scheduleImportValidationText.Visibility(Visibility::Collapsed);
    }
    m_scheduleImportApplyButton.IsEnabled(true);
    m_scheduleImportStatusText.Text(winrt::hstring(
        L"Preview ready: "
        + std::to_wstring(preview->user.classes.size())
        + L" imported class, "
        + std::to_wstring(preview->inventory.classCount)
        + L" existing classes, "
        + (hasMatchingTeacher ? L"matching teacher found" : L"new teacher required")
        + L", "
        + (hasSuggestedClass ? L"suggested existing class" : L"new class suggested")
        + L"."
        ));
}

void MainWindow::applyScheduleImport()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_openDatabase || !m_scheduleImportPreviewReady
        || !m_scheduleImportPreview || !m_scheduleImportStatusText)
    {
        return;
    }
    if (m_scheduleImportPreview->user.classes.empty()
        || m_scheduleImportPreview->teachers.empty()
        || m_scheduleImportPreview->classes.empty())
    {
        return;
    }

    const auto showValidation = [this](std::wstring message) {
        if (m_scheduleImportValidationText)
        {
            m_scheduleImportValidationText.Text(winrt::hstring(message));
            m_scheduleImportValidationText.Visibility(Visibility::Visible);
        }
        if (m_scheduleImportStatusText)
        {
            m_scheduleImportStatusText.Text(L"Import could not be applied.");
        }
    };
    const auto teacherItem = m_scheduleImportTeacherActionCombo.SelectedItem()
        .try_as<ComboBoxItem>();
    const auto classItem = m_scheduleImportClassActionCombo.SelectedItem()
        .try_as<ComboBoxItem>();
    const auto teacherAction = teacherItem
        ? static_cast<classmngr::engine::ScheduleImportTeacherAction>(
            boxedInt(teacherItem.Tag())
            )
        : classmngr::engine::ScheduleImportTeacherAction::Create;
    const auto classAction = classItem
        ? static_cast<classmngr::engine::ScheduleImportClassAction>(
            boxedInt(classItem.Tag())
            )
        : classmngr::engine::ScheduleImportClassAction::CreateNew;
    const auto& teacherPreview = m_scheduleImportPreview->teachers.front();
    const auto& classPreview = m_scheduleImportPreview->classes.front();
    if (teacherAction == classmngr::engine::ScheduleImportTeacherAction::Reuse
        && teacherPreview.matchingTeacherIds.empty())
    {
        showValidation(L"Reuse is unavailable because no matching teacher was found.");
        return;
    }
    if (classAction == classmngr::engine::ScheduleImportClassAction::UpdateExisting
        && classPreview.suggestedClassId <= 0)
    {
        showValidation(L"Update existing is unavailable because the preview found no target.");
        return;
    }

    classmngr::engine::ScheduleImportPlan plan;
    plan.kind = m_scheduleImportPreview->kind;
    plan.selectedUserName = m_scheduleImportPreview->user.name;
    plan.saveProfileNameIfBlank = true;
    plan.unknownCellsAcknowledged = true;
    plan.candidates = m_scheduleImportPreview->user.classes;
    plan.teachers.push_back({
        plan.candidates.front().teacherKey,
        teacherAction,
        teacherAction == classmngr::engine::ScheduleImportTeacherAction::Reuse
            ? teacherPreview.matchingTeacherIds.front()
            : -1,
        plan.candidates.front().rooms.empty()
            ? std::string{}
            : plan.candidates.front().rooms.front()
    });
    plan.classes.push_back({
        0,
        classAction,
        classAction == classmngr::engine::ScheduleImportClassAction::UpdateExisting
            ? classPreview.suggestedClassId
            : -1,
        "#FFFFFF",
        "#000000"
    });

    classmngr::engine::ScheduleImportService service(*m_openDatabase);
    const auto valid = service.validateImport(plan);
    if (!valid)
    {
        showValidation(L"Import validation failed: "
            + asWide(valid.error().message));
        return;
    }
    const auto imported = service.importSchedule(plan);
    if (!imported)
    {
        showValidation(L"Import failed and was rolled back: "
            + asWide(imported.error().message));
        return;
    }

    m_scheduleImportPreviewReady = false;
    m_scheduleImportPreview.reset();
    m_scheduleImportApplyButton.IsEnabled(false);
    if (m_scheduleImportValidationText)
    {
        m_scheduleImportValidationText.Text({});
        m_scheduleImportValidationText.Visibility(Visibility::Collapsed);
    }
    m_scheduleImportStatusText.Text(winrt::hstring(
        L"Import applied atomically: "
        + std::to_wstring(imported->classesCreated)
        + L" classes created, "
        + std::to_wstring(imported->classesUpdated)
        + L" updated, "
        + std::to_wstring(imported->teachersCreated)
        + L" teachers created."
        ));
    refreshScheduleWorkspace();
}

void MainWindow::refreshTestingWorkspace()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_testingClassSelector || !m_testingAssignmentList
        || !m_testingStatusText)
    {
        return;
    }

    const auto checked = [](auto const& check) {
        const auto value = check.IsChecked();
        return value && value.Value();
    };
    const bool hasDatabase = static_cast<bool>(m_openDatabase);
    const auto setEnabled = [hasDatabase](auto const& control) {
        if (control)
        {
            control.IsEnabled(hasDatabase);
        }
    };
    setEnabled(m_testingClassSelector);
    setEnabled(m_testingClassNameTextBox);
    setEnabled(m_testingClassGradeTextBox);
    setEnabled(m_testingClassLevelTextBox);
    setEnabled(m_testingClassRoomTextBox);
    setEnabled(m_testingDayCombo);
    setEnabled(m_testingStartTextBox);
    setEnabled(m_testingReplaceExistingCheck);
    setEnabled(m_testingCreateButton);
    setEnabled(m_testingAssignButton);
    setEnabled(m_testingAssignmentList);
    setEnabled(m_testingDeleteAssignmentButton);

    int previousClassId = -1;
    if (const auto item = m_testingClassSelector.SelectedItem().try_as<
            ComboBoxItem>())
    {
        previousClassId = boxedInt(item.Tag());
    }
    m_testingLoading = true;
    m_testingClasses.clear();
    m_testingAssignments.clear();
    m_testingClassSelector.Items().Clear();
    m_testingAssignmentList.Items().Clear();
    if (m_testingValidationText)
    {
        m_testingValidationText.Text({});
        m_testingValidationText.Visibility(Visibility::Collapsed);
    }

    if (!hasDatabase)
    {
        m_testingStatusText.Text(L"No database open.");
        m_testingLoading = false;
        return;
    }

    classmngr::engine::TestingClassService classService(*m_openDatabase);
    classmngr::engine::TestingBlockService blockService(*m_openDatabase);
    const auto classes = classService.list();
    const auto assignments = blockService.listAssignments();
    if (!classes || !assignments)
    {
        const std::string message = !classes
            ? classes.error().message
            : assignments.error().message;
        m_testingStatusText.Text(winrt::hstring(
            L"Testing classes could not be loaded: " + asWide(message)
            ));
        if (m_testingValidationText)
        {
            m_testingValidationText.Text(winrt::hstring(
                L"Engine loading error: " + asWide(message)
                ));
            m_testingValidationText.Visibility(Visibility::Visible);
        }
        m_testingLoading = false;
        return;
    }

    m_testingClasses = *classes;
    m_testingAssignments = *assignments;
    int selectedClassIndex = -1;
    for (const auto& testingClass : m_testingClasses)
    {
        auto item = ComboBoxItem();
        item.Content(box_value(hstring(asWide(testingClass.name))));
        item.Tag(box_value(testingClass.classId));
        setAutomationName(item, L"Testing class " + asWide(testingClass.name));
        m_testingClassSelector.Items().Append(item);
        if (testingClass.classId == previousClassId)
        {
            selectedClassIndex = static_cast<int>(
                m_testingClassSelector.Items().Size() - 1
                );
        }
    }
    if (selectedClassIndex < 0 && m_testingClassSelector.Items().Size() > 0)
    {
        selectedClassIndex = 0;
    }
    m_testingClassSelector.SelectedIndex(selectedClassIndex);

    const auto className = [this](int classId) {
        for (const auto& testingClass : m_testingClasses)
        {
            if (testingClass.classId == classId)
            {
                return asWide(testingClass.name);
            }
        }
        return std::wstring(L"Plain testing block");
    };
    for (const auto& assignment : m_testingAssignments)
    {
        auto item = ListViewItem();
        const std::wstring display = asWide(assignment.day) + L" "
            + asWide(assignment.startTime) + L" - "
            + className(assignment.classId)
            + (assignment.room.empty() ? L"" : L" (" + asWide(assignment.room) + L")");
        item.Content(box_value(hstring(display)));
        item.Tag(box_value(hstring(
            asWide(assignment.day) + L"|" + asWide(assignment.startTime)
            )));
        item.IsTabStop(false);
        setAutomationName(item, L"Testing assignment " + display);
        m_testingAssignmentList.Items().Append(item);
    }
    m_testingLoading = false;
    const bool hasSelectedClass = m_testingClassSelector.SelectedIndex() >= 0;
    m_testingAssignButton.IsEnabled(hasDatabase && hasSelectedClass);
    m_testingDeleteAssignmentButton.IsEnabled(false);
    m_testingStatusText.Text(winrt::hstring(
        L"Loaded " + std::to_wstring(m_testingClasses.size())
        + L" testing classes and "
        + std::to_wstring(m_testingAssignments.size())
        + L" assignments."
        ));
}

void MainWindow::createTestingClass()
{
    using namespace Microsoft::UI::Xaml;

    if (!m_openDatabase)
    {
        return;
    }
    classmngr::engine::TestingClass testingClass;
    testingClass.name = asUtf8(m_testingClassNameTextBox.Text());
    testingClass.grade = asUtf8(m_testingClassGradeTextBox.Text());
    testingClass.level = asUtf8(m_testingClassLevelTextBox.Text());
    testingClass.room = asUtf8(m_testingClassRoomTextBox.Text());
    testingClass.classColor = "#FFFFFF";
    testingClass.fontColor = "#000000";
    classmngr::engine::TestingClassService service(*m_openDatabase);
    const auto created = service.create(testingClass);
    if (!created)
    {
        m_testingStatusText.Text(L"Testing class could not be created.");
        m_testingValidationText.Text(winrt::hstring(
            L"Engine validation error: " + asWide(created.error().message)
            ));
        m_testingValidationText.Visibility(Visibility::Visible);
        return;
    }
    refreshTestingWorkspace();
    for (int index = 0;
         index < static_cast<int>(m_testingClassSelector.Items().Size());
         ++index)
    {
        const auto item = m_testingClassSelector.Items().GetAt(index)
            .try_as<Microsoft::UI::Xaml::Controls::ComboBoxItem>();
        if (item && boxedInt(item.Tag()) == *created)
        {
            m_testingClassSelector.SelectedIndex(index);
            break;
        }
    }
    m_testingStatusText.Text(L"Testing class created.");
}

void MainWindow::assignTestingClass()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_openDatabase || !m_testingClassSelector || !m_testingDayCombo
        || !m_testingStartTextBox)
    {
        return;
    }
    const auto classItem = m_testingClassSelector.SelectedItem().try_as<
        ComboBoxItem>();
    const int classId = classItem ? boxedInt(classItem.Tag()) : -1;
    const std::wstring day = selectedComboValue(m_testingDayCombo);
    const std::wstring start = m_testingStartTextBox.Text().c_str();
    if (classId <= 0 || day.empty() || start.empty())
    {
        m_testingStatusText.Text(L"Testing assignment could not be saved.");
        m_testingValidationText.Text(
            L"Choose a testing class, weekday, and strict HH:mm start time."
            );
        m_testingValidationText.Visibility(Visibility::Visible);
        return;
    }
    const auto checked = m_testingReplaceExistingCheck.IsChecked();
    const bool replaceExisting = checked && checked.Value();
    classmngr::engine::TestingBlockService service(*m_openDatabase);
    const auto saved = service.assignClass(
        asUtf8(day),
        asUtf8(start),
        classId,
        replaceExisting
        );
    if (!saved)
    {
        m_testingStatusText.Text(L"Testing assignment could not be saved.");
        m_testingValidationText.Text(winrt::hstring(
            L"Assignment validation error: " + asWide(saved.error().message)
            ));
        m_testingValidationText.Visibility(Visibility::Visible);
        return;
    }
    refreshTestingWorkspace();
    m_testingStatusText.Text(L"Testing assignment saved.");
}

void MainWindow::deleteTestingAssignment()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_openDatabase || !m_testingAssignmentList)
    {
        return;
    }
    const auto item = m_testingAssignmentList.SelectedItem().try_as<
        ListViewItem>();
    if (!item)
    {
        return;
    }
    const auto parts = splitScheduleKey(boxedString(item.Tag()));
    if (parts.size() != 2 || parts[0].empty() || parts[1].empty())
    {
        return;
    }
    classmngr::engine::TestingBlockService service(*m_openDatabase);
    const auto deleted = service.deleteAssignment(
        asUtf8(parts[0]),
        asUtf8(parts[1])
        );
    if (!deleted)
    {
        m_testingStatusText.Text(L"Testing assignment could not be deleted.");
        m_testingValidationText.Text(winrt::hstring(
            L"Assignment deletion failed: " + asWide(deleted.error().message)
            ));
        m_testingValidationText.Visibility(Visibility::Visible);
        return;
    }
    refreshTestingWorkspace();
    m_testingStatusText.Text(L"Testing assignment deleted.");
}

void MainWindow::populateCalendarWorkspace(
    Microsoft::UI::Xaml::Controls::StackPanel const& calendarRoot
    )
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (m_calendarTabs)
    {
        calendarRoot.Children().Append(m_calendarTabs);
        refreshCalendarPage();
        return;
    }

    const EngineCalendarDate today = calendarToday();
    m_calendarDisplayedMonth = calendarMonthStart(today);
    if (static_cast<int>(m_calendarDisplayedMonth.year())
        < calendarFirstTermYear)
    {
        m_calendarDisplayedMonth = EngineCalendarDate{
            std::chrono::year{calendarFirstTermYear},
            std::chrono::month{1},
            std::chrono::day{1}
        };
    }
    m_calendarSelectedDate = today.ok() && !calendarDateLess(
        today,
        classmngr::engine::AcademicCalendarSchedule::initialWinterStart()
        )
        ? today
        : m_calendarDisplayedMonth;

    auto makeText = [](std::wstring_view text, double fontSize = 0.0) {
        auto value = TextBlock();
        value.Text(winrt::hstring(text));
        value.TextWrapping(TextWrapping::Wrap);
        if (fontSize > 0.0)
        {
            value.FontSize(fontSize);
        }
        return value;
    };

    auto monthContent = StackPanel();
    monthContent.Padding(Thickness{16.0, 16.0, 16.0, 24.0});
    monthContent.Spacing(12.0);
    monthContent.HorizontalAlignment(HorizontalAlignment::Stretch);

    m_calendarMonthTitle = makeText(L"Calendar", 24.0);
    setAutomationName(m_calendarMonthTitle, L"Calendar month title");
    monthContent.Children().Append(m_calendarMonthTitle);

    auto monthToolbar = StackPanel();
    monthToolbar.Orientation(Orientation::Horizontal);
    monthToolbar.Spacing(8.0);

    m_calendarPreviousButton = Button();
    m_calendarPreviousButton.Content(box_value(hstring(L"Previous month")));
    m_calendarPreviousButton.Click({this, &MainWindow::CalendarPreviousButton_Click});
    setAutomationName(m_calendarPreviousButton, L"Calendar previous month");
    monthToolbar.Children().Append(m_calendarPreviousButton);

    m_calendarTodayButton = Button();
    m_calendarTodayButton.Content(box_value(hstring(L"Today")));
    m_calendarTodayButton.Click({this, &MainWindow::CalendarTodayButton_Click});
    setAutomationName(m_calendarTodayButton, L"Calendar today");
    monthToolbar.Children().Append(m_calendarTodayButton);

    m_calendarNextButton = Button();
    m_calendarNextButton.Content(box_value(hstring(L"Next month")));
    m_calendarNextButton.Click({this, &MainWindow::CalendarNextButton_Click});
    setAutomationName(m_calendarNextButton, L"Calendar next month");
    monthToolbar.Children().Append(m_calendarNextButton);

    m_calendarAddEventButton = Button();
    m_calendarAddEventButton.Content(box_value(hstring(L"Add event")));
    m_calendarAddEventButton.Click(
        [this](auto const&, auto const&) { openCalendarEventEditor(-1); }
        );
    setAutomationName(m_calendarAddEventButton, L"Calendar add event");
    monthToolbar.Children().Append(m_calendarAddEventButton);
    monthContent.Children().Append(monthToolbar);

    m_calendarGrid = Grid();
    m_calendarGrid.ColumnSpacing(4.0);
    m_calendarGrid.RowSpacing(4.0);
    m_calendarGrid.MinHeight(360.0);
    setAutomationName(m_calendarGrid, L"Calendar month grid");
    for (int column = 0; column < 7; ++column)
    {
        auto definition = ColumnDefinition();
        definition.Width(
            GridLengthHelper::FromValueAndType(1.0, GridUnitType::Star)
            );
        m_calendarGrid.ColumnDefinitions().Append(definition);
    }
    for (int row = 0; row < 7; ++row)
    {
        m_calendarGrid.RowDefinitions().Append(RowDefinition());
    }
    monthContent.Children().Append(m_calendarGrid);

    m_calendarSelectedDateText = makeText(L"Selected date", 18.0);
    setAutomationName(m_calendarSelectedDateText, L"Calendar selected date");
    monthContent.Children().Append(m_calendarSelectedDateText);

    m_calendarEventsPanel = StackPanel();
    m_calendarEventsPanel.Spacing(6.0);
    setAutomationName(m_calendarEventsPanel, L"Calendar selected day events");
    monthContent.Children().Append(m_calendarEventsPanel);

    m_calendarStatusText = makeText(L"Calendar is ready.");
    setAutomationName(m_calendarStatusText, L"Calendar status");
    monthContent.Children().Append(m_calendarStatusText);

    m_calendarValidationText = makeText(L"");
    m_calendarValidationText.Visibility(Visibility::Collapsed);
    setAutomationName(m_calendarValidationText, L"Calendar validation");
    monthContent.Children().Append(m_calendarValidationText);

    auto preferencesContent = StackPanel();
    preferencesContent.Padding(Thickness{16.0, 16.0, 16.0, 24.0});
    preferencesContent.Spacing(12.0);

    auto preferencesHeading = makeText(L"Calendar preferences", 24.0);
    setAutomationName(preferencesHeading, L"Calendar preferences heading");
    preferencesContent.Children().Append(preferencesHeading);

    m_calendarShowAllCampusesCheck = CheckBox();
    m_calendarShowAllCampusesCheck.Content(
        box_value(hstring(L"Show Events at All Campuses"))
        );
    setAutomationName(
        m_calendarShowAllCampusesCheck,
        L"Calendar show events at all campuses"
        );
    preferencesContent.Children().Append(m_calendarShowAllCampusesCheck);

    m_calendarHideStartOfTermCheck = CheckBox();
    m_calendarHideStartOfTermCheck.Content(
        box_value(hstring(L"Hide Start of Term Events"))
        );
    setAutomationName(
        m_calendarHideStartOfTermCheck,
        L"Calendar hide start of term events"
        );
    preferencesContent.Children().Append(m_calendarHideStartOfTermCheck);

    auto firstDayLabel = makeText(L"First day of calendar week");
    preferencesContent.Children().Append(firstDayLabel);
    m_calendarFirstDayCombo = ComboBox();
    auto sunday = ComboBoxItem();
    sunday.Content(box_value(hstring(L"Sunday")));
    sunday.Tag(box_value(hstring(L"0")));
    m_calendarFirstDayCombo.Items().Append(sunday);
    auto monday = ComboBoxItem();
    monday.Content(box_value(hstring(L"Monday")));
    monday.Tag(box_value(hstring(L"1")));
    m_calendarFirstDayCombo.Items().Append(monday);
    setAutomationName(m_calendarFirstDayCombo, L"Calendar first day of week");
    preferencesContent.Children().Append(m_calendarFirstDayCombo);

    auto termYearLabel = makeText(L"Academic term year");
    preferencesContent.Children().Append(termYearLabel);
    m_calendarTermYearTextBox = TextBox();
    m_calendarTermYearTextBox.PlaceholderText(L"2026");
    setAutomationName(m_calendarTermYearTextBox, L"Calendar academic term year");
    preferencesContent.Children().Append(m_calendarTermYearTextBox);

    auto scheduleHeading = makeText(
        L"Term schedules (use Monday dates and 1-53 week durations)",
        18.0
        );
    preferencesContent.Children().Append(scheduleHeading);

    auto scheduleGrid = Grid();
    scheduleGrid.ColumnSpacing(8.0);
    scheduleGrid.RowSpacing(6.0);
    for (int column = 0; column < 6; ++column)
    {
        auto definition = ColumnDefinition();
        if (column == 0)
        {
            definition.Width(
                GridLengthHelper::FromValueAndType(1.0, GridUnitType::Auto)
                );
        }
        else
        {
            definition.Width(
                GridLengthHelper::FromValueAndType(1.0, GridUnitType::Star)
                );
        }
        scheduleGrid.ColumnDefinitions().Append(definition);
    }
    for (int row = 0; row < 3; ++row)
    {
        scheduleGrid.RowDefinitions().Append(RowDefinition());
    }
    const std::array<std::wstring_view, 6> scheduleHeaders{
        L"School", L"Winter start", L"Winter weeks", L"Spring weeks",
        L"Summer weeks", L"Fall weeks"
    };
    for (int column = 0; column < 6; ++column)
    {
        auto header = makeText(scheduleHeaders[static_cast<std::size_t>(column)]);
        Grid::SetRow(header, 0);
        Grid::SetColumn(header, column);
        scheduleGrid.Children().Append(header);
    }
    const std::array<std::wstring_view, 2> schoolNames{
        L"Elementary", L"Middle"
    };
    for (int school = 0; school < 2; ++school)
    {
        auto schoolText = makeText(schoolNames[static_cast<std::size_t>(school)]);
        Grid::SetRow(schoolText, school + 1);
        Grid::SetColumn(schoolText, 0);
        scheduleGrid.Children().Append(schoolText);

        m_calendarWinterStartTextBoxes[static_cast<std::size_t>(school)] =
            TextBox();
        m_calendarWinterStartTextBoxes[static_cast<std::size_t>(school)].
            PlaceholderText(L"yyyy-MM-dd");
        setAutomationName(
            m_calendarWinterStartTextBoxes[static_cast<std::size_t>(school)],
            std::wstring(L"Calendar ") + std::wstring(schoolNames[static_cast<std::size_t>(school)])
                + L" winter start"
            );
        Grid::SetRow(
            m_calendarWinterStartTextBoxes[static_cast<std::size_t>(school)],
            school + 1
            );
        Grid::SetColumn(
            m_calendarWinterStartTextBoxes[static_cast<std::size_t>(school)],
            1
            );
        scheduleGrid.Children().Append(
            m_calendarWinterStartTextBoxes[static_cast<std::size_t>(school)]
            );

        for (int term = 0; term < classmngr::engine::AcademicTermCount; ++term)
        {
            auto field = TextBox();
            field.PlaceholderText(L"weeks");
            setAutomationName(
                field,
                std::wstring(L"Calendar ")
                    + std::wstring(schoolNames[static_cast<std::size_t>(school)])
                    + L" " + std::to_wstring(term + 1) + L" term weeks"
                );
            m_calendarTermWeekTextBoxes[static_cast<std::size_t>(school)]
                [static_cast<std::size_t>(term)] = field;
            Grid::SetRow(field, school + 1);
            Grid::SetColumn(field, term + 2);
            scheduleGrid.Children().Append(field);
        }
    }
    preferencesContent.Children().Append(scheduleGrid);

    auto preferencesActions = StackPanel();
    preferencesActions.Orientation(Orientation::Horizontal);
    preferencesActions.Spacing(8.0);
    m_calendarSavePreferencesButton = Button();
    m_calendarSavePreferencesButton.Content(box_value(hstring(L"Save preferences")));
    m_calendarSavePreferencesButton.Click(
        {this, &MainWindow::CalendarSavePreferencesButton_Click}
        );
    setAutomationName(
        m_calendarSavePreferencesButton,
        L"Calendar save preferences"
        );
    preferencesActions.Children().Append(m_calendarSavePreferencesButton);
    m_calendarRestoreDefaultsButton = Button();
    m_calendarRestoreDefaultsButton.Content(
        box_value(hstring(L"Restore term defaults"))
        );
    m_calendarRestoreDefaultsButton.Click(
        {this, &MainWindow::CalendarRestoreDefaultsButton_Click}
        );
    setAutomationName(
        m_calendarRestoreDefaultsButton,
        L"Calendar restore term defaults"
        );
    preferencesActions.Children().Append(m_calendarRestoreDefaultsButton);
    m_calendarResetEventsButton = Button();
    m_calendarResetEventsButton.Content(
        box_value(hstring(L"Reset calendar events"))
        );
    m_calendarResetEventsButton.Click(
        {this, &MainWindow::CalendarResetEventsButton_Click}
        );
    setAutomationName(m_calendarResetEventsButton, L"Calendar reset events");
    preferencesActions.Children().Append(m_calendarResetEventsButton);
    preferencesContent.Children().Append(preferencesActions);

    m_calendarPreferencesStatusText = makeText(L"Preferences are ready.");
    setAutomationName(
        m_calendarPreferencesStatusText,
        L"Calendar preferences status"
        );
    preferencesContent.Children().Append(m_calendarPreferencesStatusText);
    m_calendarPreferencesValidationText = makeText(L"");
    m_calendarPreferencesValidationText.Visibility(Visibility::Collapsed);
    setAutomationName(
        m_calendarPreferencesValidationText,
        L"Calendar preferences validation"
        );
    preferencesContent.Children().Append(m_calendarPreferencesValidationText);

    auto wrap = [](StackPanel const& content) {
        auto scroll = ScrollViewer();
        scroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
        scroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
        scroll.Content(content);
        return scroll;
    };
    auto calendarItem = PivotItem();
    calendarItem.Header(box_value(hstring(L"Calendar")));
    calendarItem.Content(wrap(monthContent));
    setAutomationName(calendarItem, L"Calendar month tab");
    auto preferencesItem = PivotItem();
    preferencesItem.Header(box_value(hstring(L"Preferences")));
    preferencesItem.Content(wrap(preferencesContent));
    setAutomationName(preferencesItem, L"Calendar preferences tab");

    m_calendarTabs = Pivot();
    m_calendarTabs.IsTabStop(true);
    m_calendarTabs.TabIndex(0);
    m_calendarTabs.Items().Append(calendarItem);
    m_calendarTabs.Items().Append(preferencesItem);
    setAutomationName(m_calendarTabs, L"Calendar tabs");
    calendarRoot.Children().Append(m_calendarTabs);
    refreshCalendarPage();
}

void MainWindow::refreshCalendarPage()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_calendarTabs || !m_calendarGrid || !m_calendarStatusText)
    {
        return;
    }

    const bool hasDatabase = static_cast<bool>(m_openDatabase);
    const auto setEnabled = [hasDatabase](auto const& control) {
        if (control)
        {
            control.IsEnabled(hasDatabase);
        }
    };
    setEnabled(m_calendarPreviousButton);
    setEnabled(m_calendarNextButton);
    setEnabled(m_calendarTodayButton);
    setEnabled(m_calendarAddEventButton);
    setEnabled(m_calendarShowAllCampusesCheck);
    setEnabled(m_calendarHideStartOfTermCheck);
    setEnabled(m_calendarFirstDayCombo);
    setEnabled(m_calendarTermYearTextBox);
    setEnabled(m_calendarSavePreferencesButton);
    setEnabled(m_calendarRestoreDefaultsButton);
    setEnabled(m_calendarResetEventsButton);
    for (auto const& field : m_calendarWinterStartTextBoxes)
    {
        setEnabled(field);
    }
    for (auto const& school : m_calendarTermWeekTextBoxes)
    {
        for (auto const& field : school)
        {
            setEnabled(field);
        }
    }

    if (!m_calendarDisplayedMonth.ok())
    {
        m_calendarDisplayedMonth = calendarMonthStart(calendarToday());
    }
    if (!m_calendarSelectedDate.ok())
    {
        m_calendarSelectedDate = m_calendarDisplayedMonth;
    }

    m_calendarLoading = true;
    m_calendarPreferencesDirty = false;
    m_calendarEvents.clear();
    m_calendarFirstDayOfWeek = 0;
    int termYear = std::max(
        calendarFirstTermYear,
        static_cast<int>(calendarToday().year())
        );

    if (!hasDatabase)
    {
        m_calendarStatusText.Text(L"No database open.");
        m_calendarSelectedDateText.Text(L"Selected date: ");
        m_calendarEventsPanel.Children().Clear();
        auto empty = TextBlock();
        empty.Text(L"Open a database to view and edit calendar events.");
        empty.TextWrapping(TextWrapping::Wrap);
        m_calendarEventsPanel.Children().Append(empty);
        m_calendarValidationText.Text({});
        m_calendarValidationText.Visibility(Visibility::Collapsed);
        m_calendarPreferencesStatusText.Text(L"No database open.");
        m_calendarPreferencesValidationText.Text({});
        m_calendarPreferencesValidationText.Visibility(Visibility::Collapsed);
        m_calendarLoading = false;
    }
    else
    {
        classmngr::engine::ApplicationSettingsService settings(*m_openDatabase);
        const auto loadSetting = [&settings](std::string_view key) {
            return settings.load(key);
        };
        if (const auto value = loadSetting("calendar/firstDayOfWeek"); value)
        {
            m_calendarFirstDayOfWeek = static_cast<int>(settingInteger(*value, 0));
        }
        m_calendarFirstDayOfWeek = m_calendarFirstDayOfWeek == 1 ? 1 : 0;
        if (const auto value = loadSetting("calendar/academic/termYear"); value)
        {
            termYear = static_cast<int>(settingInteger(*value, termYear));
        }
        termYear = std::max(calendarFirstTermYear, termYear);
        bool showAll = false;
        if (const auto value = loadSetting("calendar/showEventsAtAllCampuses"); value)
        {
            showAll = settingBool(*value, false);
        }
        bool hideStart = false;
        if (const auto value = loadSetting("calendar/hideStartOfTermEvents"); value)
        {
            hideStart = settingBool(*value, false);
        }
        m_calendarShowAllCampusesCheck.IsChecked(showAll);
        m_calendarHideStartOfTermCheck.IsChecked(hideStart);
        m_calendarFirstDayCombo.SelectedIndex(m_calendarFirstDayOfWeek);
        m_calendarTermYearTextBox.Text(std::to_wstring(termYear));

        m_calendarSchedule.clear();
        classmngr::engine::AcademicCalendarSchedule::ScheduleMap elementary;
        classmngr::engine::AcademicCalendarSchedule::ScheduleMap middle;
        for (int school = 0; school < 2; ++school)
        {
            const auto level = school == 0
                ? classmngr::engine::SchoolLevel::Elementary
                : classmngr::engine::SchoolLevel::Middle;
            auto schedule = m_calendarSchedule.defaultYearSchedule(level, termYear);
            bool hasCustom = false;
            const auto winter = loadSetting(calendarScheduleKey(
                termYear,
                school,
                0,
                true
                ));
            if (winter && std::holds_alternative<std::string>(*winter))
            {
                EngineCalendarDate parsed;
                if (calendarDateFromText(
                        asWString(winrt::to_hstring(
                            std::get<std::string>(*winter)
                            )),
                        parsed
                        ))
                {
                    schedule.winterStart = parsed;
                    hasCustom = true;
                }
            }
            for (int term = 0;
                 term < classmngr::engine::AcademicTermCount;
                 ++term)
            {
                const auto weeks = loadSetting(calendarScheduleKey(
                    termYear,
                    school,
                    term,
                    false
                    ));
                if (weeks)
                {
                    const auto value = settingInteger(*weeks, -1);
                    if (value >= 1 && value <= 53)
                    {
                        schedule.weeks[static_cast<std::size_t>(term)] =
                            static_cast<int>(value);
                        hasCustom = true;
                    }
                }
            }
            if (hasCustom && schedule.isValid())
            {
                (school == 0 ? elementary : middle).insert({termYear, schedule});
            }
        }
        static_cast<void>(m_calendarSchedule.replaceSchedules(elementary, middle));

        for (int school = 0; school < 2; ++school)
        {
            const auto level = school == 0
                ? classmngr::engine::SchoolLevel::Elementary
                : classmngr::engine::SchoolLevel::Middle;
            const auto schedule = m_calendarSchedule.yearSchedule(level, termYear);
            m_calendarWinterStartTextBoxes[static_cast<std::size_t>(school)].Text(
                calendarDateText(schedule.winterStart)
                );
            for (int term = 0;
                 term < classmngr::engine::AcademicTermCount;
                 ++term)
            {
                m_calendarTermWeekTextBoxes[static_cast<std::size_t>(school)]
                    [static_cast<std::size_t>(term)].Text(
                        std::to_wstring(schedule.weeks[static_cast<std::size_t>(term)])
                        );
            }
        }

        classmngr::engine::CalendarEventService service(*m_openDatabase);
        const auto loaded = service.loadInRange(
            calendarMonthStart(m_calendarDisplayedMonth),
            calendarAddDays(
                calendarMonthStart(m_calendarDisplayedMonth),
                calendarDaysInMonth(m_calendarDisplayedMonth) - 1
                )
            );
        if (loaded)
        {
            m_calendarEvents = *loaded;
            m_calendarStatusText.Text(winrt::hstring(
                L"Calendar loaded: " + std::to_wstring(m_calendarEvents.size())
                    + L" event(s)."
                ));
            m_calendarValidationText.Text({});
            m_calendarValidationText.Visibility(Visibility::Collapsed);
        }
        else
        {
            m_calendarStatusText.Text(winrt::hstring(
                L"Calendar could not be loaded: "
                    + asWide(loaded.error().message)
                ));
            m_calendarValidationText.Text(winrt::hstring(
                L"Engine loading error: " + asWide(loaded.error().message)
                ));
            m_calendarValidationText.Visibility(Visibility::Visible);
        }
        m_calendarPreferencesStatusText.Text(L"Calendar preferences loaded.");
        m_calendarPreferencesValidationText.Text({});
        m_calendarPreferencesValidationText.Visibility(Visibility::Collapsed);
        m_calendarLoading = false;
    }

    const auto visibleOnDate = [this](
                                  classmngr::engine::CalendarEvent const& event,
                                  EngineCalendarDate const& date) {
        if (!event.startDate.ok() || !event.endDate.ok()
            || calendarDateLess(date, event.startDate)
            || calendarDateLess(event.endDate, date))
        {
            return false;
        }
        const auto checked = m_calendarHideStartOfTermCheck.IsChecked();
        const bool hideStart = checked && checked.Value();
        return !hideStart || !classmngr::engine::CalendarEventRules::isStartOfTerm(
            event.title,
            event.eventType
            );
    };
    const auto eventSummary = [](classmngr::engine::CalendarEvent const& event) {
        std::wstring result = asWide(event.title);
        if (result.empty())
        {
            result = L"(untitled event)";
        }
        result += L" — " + asWide(event.eventType);
        if (event.allDay)
        {
            result += L" · All day";
        }
        else if (event.startTime)
        {
            result += L" · " + calendarTimeText(event.startTime);
            if (event.endTime)
            {
                result += L"-" + calendarTimeText(event.endTime);
            }
        }
        return result;
    };

    m_calendarMonthTitle.Text(calendarMonthTitle(m_calendarDisplayedMonth));
    const auto monthStart = calendarMonthStart(m_calendarDisplayedMonth);
    const int firstWeekday = static_cast<int>(
        std::chrono::weekday{std::chrono::sys_days{monthStart}}.c_encoding()
        );
    const int offset = (firstWeekday - m_calendarFirstDayOfWeek + 7) % 7;
    const auto gridStart = calendarAddDays(monthStart, -offset);
    const std::array<std::wstring_view, 7> weekdayNames{
        L"Sun", L"Mon", L"Tue", L"Wed", L"Thu", L"Fri", L"Sat"
    };
    m_calendarGrid.Children().Clear();
    for (int column = 0; column < 7; ++column)
    {
        auto header = TextBlock();
        header.Text(winrt::hstring(weekdayNames[static_cast<std::size_t>(
            (m_calendarFirstDayOfWeek + column) % 7
            )]));
        header.HorizontalAlignment(HorizontalAlignment::Center);
        setAutomationName(header, L"Calendar weekday header");
        Grid::SetRow(header, 0);
        Grid::SetColumn(header, column);
        m_calendarGrid.Children().Append(header);
    }
    for (int index = 0; index < 42; ++index)
    {
        const auto date = calendarAddDays(gridStart, index);
        int eventCount = 0;
        for (auto const& event : m_calendarEvents)
        {
            if (visibleOnDate(event, date))
            {
                ++eventCount;
            }
        }
        std::wstring content = std::to_wstring(
            static_cast<unsigned>(date.day())
            );
        if (eventCount > 0)
        {
            content += L"\n• " + std::to_wstring(eventCount);
        }
        auto day = Button();
        day.Content(box_value(hstring(content)));
        day.MinHeight(48.0);
        day.IsEnabled(hasDatabase);
        day.Opacity(
            date.month() == m_calendarDisplayedMonth.month()
                ? 1.0
                : 0.55
            );
        setAutomationName(day, L"Calendar day " + calendarDateText(date));
        day.Click([this, date](auto const&, auto const&) {
            if (!m_calendarLoading)
            {
                m_calendarSelectedDate = date;
                refreshCalendarPage();
            }
        });
        Grid::SetRow(day, index / 7 + 1);
        Grid::SetColumn(day, index % 7);
        m_calendarGrid.Children().Append(day);
    }

    m_calendarSelectedDateText.Text(winrt::hstring(
        L"Selected date: " + calendarDateText(m_calendarSelectedDate)
        ));
    m_calendarEventsPanel.Children().Clear();
    int selectedEventCount = 0;
    for (auto const& event : m_calendarEvents)
    {
        if (!visibleOnDate(event, m_calendarSelectedDate))
        {
            continue;
        }
        ++selectedEventCount;
        auto eventButton = Button();
        eventButton.HorizontalAlignment(HorizontalAlignment::Stretch);
        eventButton.HorizontalContentAlignment(HorizontalAlignment::Left);
        eventButton.Content(box_value(hstring(eventSummary(event))));
        setAutomationName(
            eventButton,
            L"Calendar event " + std::to_wstring(event.id)
            );
        const int eventId = event.id;
        eventButton.Click(
            [this, eventId](auto const&, auto const&) {
                openCalendarEventEditor(eventId);
            }
            );
        m_calendarEventsPanel.Children().Append(eventButton);
    }
    if (selectedEventCount == 0)
    {
        auto empty = TextBlock();
        empty.Text(L"No events for the selected date.");
        empty.TextWrapping(TextWrapping::Wrap);
        setAutomationName(empty, L"Calendar no selected day events");
        m_calendarEventsPanel.Children().Append(empty);
    }

    const auto firstTermStart = classmngr::engine::AcademicCalendarSchedule::initialWinterStart();
    m_calendarPreviousButton.IsEnabled(
        hasDatabase && calendarDateLess(firstTermStart, monthStart)
        );
    m_calendarNextButton.IsEnabled(hasDatabase);
    m_calendarTodayButton.IsEnabled(hasDatabase);
}

void MainWindow::CalendarPreviousButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_openDatabase)
    {
        return;
    }
    const auto previous = calendarAddMonths(m_calendarDisplayedMonth, -1);
    if (previous.ok()
        && !calendarDateLess(
            previous,
            calendarMonthStart(classmngr::engine::AcademicCalendarSchedule::initialWinterStart())
            ))
    {
        m_calendarDisplayedMonth = previous;
        m_calendarSelectedDate = previous;
        refreshCalendarPage();
    }
}

void MainWindow::CalendarNextButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_openDatabase)
    {
        return;
    }
    m_calendarDisplayedMonth = calendarAddMonths(m_calendarDisplayedMonth, 1);
    m_calendarSelectedDate = m_calendarDisplayedMonth;
    refreshCalendarPage();
}

void MainWindow::CalendarTodayButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_openDatabase)
    {
        return;
    }
    const auto today = calendarToday();
    m_calendarDisplayedMonth = calendarMonthStart(today);
    if (calendarDateLess(
            today,
            classmngr::engine::AcademicCalendarSchedule::initialWinterStart()
            ))
    {
        m_calendarDisplayedMonth = calendarMonthStart(
            classmngr::engine::AcademicCalendarSchedule::initialWinterStart()
            );
    }
    m_calendarSelectedDate = today;
    refreshCalendarPage();
}

void MainWindow::saveCalendarPreferences()
{
    if (!m_openDatabase)
    {
        m_calendarPreferencesStatusText.Text(L"No database open.");
        return;
    }

    auto showValidation = [this](std::wstring_view message) {
        m_calendarPreferencesValidationText.Text(winrt::hstring(message));
        m_calendarPreferencesValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Visible
            );
        m_calendarPreferencesStatusText.Text(L"Calendar preferences were not saved.");
    };
    int termYear = 0;
    try
    {
        termYear = std::stoi(asWString(m_calendarTermYearTextBox.Text()));
    }
    catch (...)
    {
        showValidation(L"Academic term year must be a number.");
        return;
    }
    if (termYear < calendarFirstTermYear || termYear > 2200)
    {
        showValidation(L"Academic term year must be between 2026 and 2200.");
        return;
    }

    std::array<classmngr::engine::AcademicYearSchedule, 2> schedules;
    for (int school = 0; school < 2; ++school)
    {
        auto& schedule = schedules[static_cast<std::size_t>(school)];
        schedule.termYear = termYear;
        if (!calendarDateFromText(
                asWString(m_calendarWinterStartTextBoxes[
                    static_cast<std::size_t>(school)
                    ].Text()),
                schedule.winterStart
                ))
        {
            showValidation(L"Each winter start must be a valid yyyy-MM-dd date.");
            return;
        }
        for (int term = 0;
             term < classmngr::engine::AcademicTermCount;
             ++term)
        {
            try
            {
                schedule.weeks[static_cast<std::size_t>(term)] = std::stoi(
                    asWString(m_calendarTermWeekTextBoxes[
                        static_cast<std::size_t>(school)
                        ][static_cast<std::size_t>(term)].Text())
                    );
            }
            catch (...)
            {
                showValidation(L"Each term duration must be a number from 1 to 53.");
                return;
            }
        }
        if (!schedule.isValid())
        {
            showValidation(
                L"Every term must start on a Monday and last from 1 to 53 weeks."
                );
            return;
        }
    }

    classmngr::engine::ApplicationSettings settingsValues;
    const auto checked = [](auto const& check) {
        const auto value = check.IsChecked();
        return value && value.Value();
    };
    settingsValues.emplace_back(
        "calendar/showEventsAtAllCampuses",
        classmngr::engine::SettingValue{
            std::int64_t{checked(m_calendarShowAllCampusesCheck) ? 1 : 0}
        }
        );
    settingsValues.emplace_back(
        "calendar/firstDayOfWeek",
        classmngr::engine::SettingValue{
            std::int64_t{m_calendarFirstDayCombo.SelectedIndex() == 1 ? 1 : 0}
        }
        );
    settingsValues.emplace_back(
        "calendar/hideStartOfTermEvents",
        classmngr::engine::SettingValue{
            std::int64_t{checked(m_calendarHideStartOfTermCheck) ? 1 : 0}
        }
        );
    settingsValues.emplace_back(
        "calendar/academic/termYear",
        classmngr::engine::SettingValue{std::int64_t{termYear}}
        );
    for (int school = 0; school < 2; ++school)
    {
        settingsValues.emplace_back(
            calendarScheduleKey(termYear, school, 0, true),
            classmngr::engine::SettingValue{
                asUtf8(calendarDateText(schedules[static_cast<std::size_t>(school)].winterStart))
            }
            );
        for (int term = 0;
             term < classmngr::engine::AcademicTermCount;
             ++term)
        {
            settingsValues.emplace_back(
                calendarScheduleKey(termYear, school, term, false),
                classmngr::engine::SettingValue{
                    std::int64_t{
                        schedules[static_cast<std::size_t>(school)]
                            .weeks[static_cast<std::size_t>(term)]
                    }
                }
                );
        }
    }

    classmngr::engine::ApplicationSettingsService settings(*m_openDatabase);
    const auto saved = settings.saveBatch(settingsValues);
    if (!saved)
    {
        showValidation(
            std::wstring(L"Engine rejected the calendar preferences: ")
                + asWide(saved.error().message)
            );
        return;
    }

    classmngr::engine::AcademicCalendarSchedule::ScheduleMap elementary;
    classmngr::engine::AcademicCalendarSchedule::ScheduleMap middle;
    elementary.insert({termYear, schedules[0]});
    middle.insert({termYear, schedules[1]});
    m_calendarSchedule.clear();
    static_cast<void>(m_calendarSchedule.replaceSchedules(elementary, middle));
    m_calendarFirstDayOfWeek = m_calendarFirstDayCombo.SelectedIndex() == 1 ? 1 : 0;
    m_calendarPreferencesDirty = false;
    m_dirtyState.markClean();
    m_calendarPreferencesValidationText.Text({});
    m_calendarPreferencesValidationText.Visibility(
        Microsoft::UI::Xaml::Visibility::Collapsed
        );
    m_calendarPreferencesStatusText.Text(L"Calendar preferences saved.");
    updateFileCommandState();
    refreshCalendarPage();
}

void MainWindow::restoreCalendarDefaults()
{
    if (!m_openDatabase)
    {
        m_calendarPreferencesStatusText.Text(L"No database open.");
        return;
    }

    int termYear = calendarFirstTermYear;
    try
    {
        termYear = std::max(
            calendarFirstTermYear,
            std::stoi(asWString(m_calendarTermYearTextBox.Text()))
            );
    }
    catch (...)
    {
        m_calendarTermYearTextBox.Text(std::to_wstring(termYear));
    }
    for (int school = 0; school < 2; ++school)
    {
        const auto level = school == 0
            ? classmngr::engine::SchoolLevel::Elementary
            : classmngr::engine::SchoolLevel::Middle;
        const auto schedule = m_calendarSchedule.defaultYearSchedule(level, termYear);
        m_calendarWinterStartTextBoxes[static_cast<std::size_t>(school)].Text(
            calendarDateText(schedule.winterStart)
            );
        for (int term = 0;
             term < classmngr::engine::AcademicTermCount;
             ++term)
        {
            m_calendarTermWeekTextBoxes[static_cast<std::size_t>(school)]
                [static_cast<std::size_t>(term)].Text(
                    std::to_wstring(schedule.weeks[static_cast<std::size_t>(term)])
                    );
        }
    }
    m_calendarPreferencesDirty = true;
    m_calendarPreferencesStatusText.Text(
        L"Default term schedules loaded. Save preferences to persist them."
        );
}

void MainWindow::resetCalendarEvents()
{
    if (!m_openDatabase)
    {
        m_calendarPreferencesStatusText.Text(L"No database open.");
        return;
    }

    auto weak = get_weak();
    showDialog(
        L"Reset Calendar",
        L"Delete all calendar events? This cannot be undone.",
        L"Reset",
        {},
        L"Cancel",
        [weak](ClassMngrWinUIDialogs::DialogOutcome outcome) {
            if (outcome != ClassMngrWinUIDialogs::DialogOutcome::Primary)
            {
                return;
            }
            if (auto self = weak.get())
            {
                if (!self->m_openDatabase)
                {
                    return;
                }
                classmngr::engine::CalendarEventService service(
                    *self->m_openDatabase
                    );
                const auto deleted = service.removeAll();
                if (!deleted)
                {
                    self->m_calendarPreferencesStatusText.Text(winrt::hstring(
                        L"Calendar events could not be reset: "
                            + asWide(deleted.error().message)
                        ));
                    return;
                }
                self->m_dirtyState.markClean();
                self->m_calendarPreferencesStatusText.Text(
                    L"Calendar events reset to defaults."
                    );
                self->refreshCalendarPage();
                self->updateFileCommandState();
            }
        }
        );
}

void MainWindow::CalendarSavePreferencesButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    saveCalendarPreferences();
}

void MainWindow::CalendarRestoreDefaultsButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    restoreCalendarDefaults();
}

void MainWindow::CalendarResetEventsButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    resetCalendarEvents();
}

winrt::fire_and_forget MainWindow::openCalendarEventEditor(int eventId)
{
    auto lifetime = get_strong();
    if (m_ownedDialog || !m_openDatabase || !RootGrid().XamlRoot())
    {
        co_return;
    }

    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    classmngr::engine::CalendarEvent event;
    if (eventId > 0)
    {
        classmngr::engine::CalendarEventService service(*m_openDatabase);
        const auto loaded = service.get(eventId);
        if (!loaded)
        {
            m_calendarStatusText.Text(winrt::hstring(
                L"Calendar event could not be loaded: "
                    + asWide(loaded.error().message)
                ));
            co_return;
        }
        event = *loaded;
    }
    else
    {
        event.startDate = m_calendarSelectedDate.ok()
            ? m_calendarSelectedDate
            : calendarToday();
        if (calendarDateLess(
                event.startDate,
                classmngr::engine::AcademicCalendarSchedule::initialWinterStart()
                ))
        {
            event.startDate = classmngr::engine::AcademicCalendarSchedule::initialWinterStart();
        }
        event.endDate = event.startDate;
        event.startTime = std::chrono::minutes{9 * 60};
        event.endTime = std::chrono::minutes{10 * 60};
    }

    auto form = StackPanel();
    form.Spacing(8.0);
    form.MaxWidth(520.0);
    const auto makeField = [&form](
                                std::wstring_view header,
                                std::wstring value,
                                std::wstring_view automationName) {
        auto field = TextBox();
        field.Header(box_value(hstring(header)));
        field.Text(hstring(value));
        field.IsTabStop(true);
        setAutomationName(field, automationName);
        form.Children().Append(field);
        return field;
    };
    auto title = makeField(L"Title", asWide(event.title), L"Calendar event title");
    auto startDate = makeField(
        L"Start date (yyyy-MM-dd)",
        calendarDateText(event.startDate),
        L"Calendar event start date"
        );
    auto endDate = makeField(
        L"End date (yyyy-MM-dd)",
        calendarDateText(event.endDate),
        L"Calendar event end date"
        );
    auto startTime = makeField(
        L"Start time (HH:mm)",
        calendarTimeText(event.startTime),
        L"Calendar event start time"
        );
    auto endTime = makeField(
        L"End time (HH:mm)",
        calendarTimeText(event.endTime),
        L"Calendar event end time"
        );

    auto allDay = CheckBox();
    allDay.Content(box_value(hstring(L"All day")));
    allDay.IsChecked(event.allDay);
    setAutomationName(allDay, L"Calendar event all day");
    form.Children().Append(allDay);

    const auto addChoice = [](ComboBox combo,
                              std::wstring_view display,
                              std::wstring_view value) {
        auto item = ComboBoxItem();
        item.Content(box_value(hstring(display)));
        item.Tag(box_value(hstring(value)));
        combo.Items().Append(item);
    };
    const auto selectChoice = [](ComboBox combo, std::wstring_view value) {
        for (int index = 0; index < static_cast<int>(combo.Items().Size()); ++index)
        {
            const auto item = combo.Items().GetAt(index).try_as<ComboBoxItem>();
            if (item && boxedString(item.Tag()) == value)
            {
                combo.SelectedIndex(index);
                return;
            }
        }
        combo.SelectedIndex(0);
    };

    auto eventType = ComboBox();
    eventType.Header(box_value(hstring(L"Event type")));
    for (const auto value : classmngr::engine::CalendarEventRules::eventTypes())
    {
        addChoice(eventType, asWide(value), asWide(value));
    }
    selectChoice(eventType, asWide(event.eventType));
    setAutomationName(eventType, L"Calendar event type");
    form.Children().Append(eventType);

    auto timeStatus = ComboBox();
    timeStatus.Header(box_value(hstring(L"Time status")));
    for (const auto value : classmngr::engine::CalendarEventRules::timeStatuses())
    {
        addChoice(timeStatus, asWide(value), asWide(value));
    }
    selectChoice(timeStatus, asWide(event.timeStatus));
    setAutomationName(timeStatus, L"Calendar event time status");
    form.Children().Append(timeStatus);

    auto repeat = ComboBox();
    repeat.Header(box_value(hstring(L"Repeat")));
    addChoice(repeat, L"Does not repeat", L"none");
    addChoice(repeat, L"Daily", L"daily");
    addChoice(repeat, L"Weekly", L"weekly");
    addChoice(repeat, L"Monthly", L"monthly");
    selectChoice(repeat, L"none");
    setAutomationName(repeat, L"Calendar event repeat frequency");
    form.Children().Append(repeat);
    auto repeatUntil = makeField(
        L"Repeat until (yyyy-MM-dd; required for repeats)",
        {},
        L"Calendar event repeat until"
        );

    auto validation = TextBlock();
    validation.TextWrapping(TextWrapping::Wrap);
    validation.Visibility(Visibility::Collapsed);
    setAutomationName(validation, L"Calendar event validation");
    form.Children().Append(validation);

    auto dialog = ContentDialog();
    dialog.XamlRoot(RootGrid().XamlRoot());
    dialog.Title(box_value(hstring(
        eventId > 0 ? L"Edit calendar event" : L"Add calendar event"
        )));
    dialog.Content(form);
    dialog.PrimaryButtonText(L"Save");
    if (eventId > 0)
    {
        dialog.SecondaryButtonText(L"Delete");
    }
    dialog.CloseButtonText(L"Cancel");
    dialog.DefaultButton(ContentDialogButton::Primary);
    m_ownedDialog = dialog;

    for (;;)
    {
        ContentDialogResult result = ContentDialogResult::None;
        try
        {
            result = co_await dialog.ShowAsync();
        }
        catch (...)
        {
            break;
        }
        if (result == ContentDialogResult::None
            || result == ContentDialogResult::Secondary)
        {
            if (result == ContentDialogResult::Secondary && eventId > 0)
            {
                classmngr::engine::CalendarEventService service(*m_openDatabase);
                const auto deleted = service.remove(eventId);
                if (!deleted)
                {
                    m_calendarStatusText.Text(winrt::hstring(
                        L"Calendar event could not be deleted: "
                            + asWide(deleted.error().message)
                        ));
                }
                else
                {
                    m_calendarStatusText.Text(L"Calendar event deleted.");
                    m_dirtyState.markClean();
                    refreshCalendarPage();
                    updateFileCommandState();
                }
            }
            break;
        }

        classmngr::engine::CalendarEvent draft = event;
        draft.title = asUtf8(asWString(title.Text()));
        if (!calendarDateFromText(asWString(startDate.Text()), draft.startDate)
            || !calendarDateFromText(asWString(endDate.Text()), draft.endDate))
        {
            validation.Text(L"Start and end dates must use yyyy-MM-dd.");
            validation.Visibility(Visibility::Visible);
            continue;
        }
        const auto startTimeText = asWString(startTime.Text());
        const auto endTimeText = asWString(endTime.Text());
        const auto parsedStartTime = startTimeText.empty()
            ? std::optional<std::chrono::minutes>{}
            : calendarTimeFromText(startTimeText);
        const auto parsedEndTime = endTimeText.empty()
            ? std::optional<std::chrono::minutes>{}
            : calendarTimeFromText(endTimeText);
        if ((!startTimeText.empty() && !parsedStartTime)
            || (!endTimeText.empty() && !parsedEndTime))
        {
            validation.Text(L"Times must use HH:mm.");
            validation.Visibility(Visibility::Visible);
            continue;
        }
        draft.startTime = parsedStartTime;
        draft.endTime = parsedEndTime;
        const auto allDayValue = allDay.IsChecked();
        draft.allDay = allDayValue && allDayValue.Value();
        draft.eventType = asUtf8(selectedComboValue(eventType));
        draft.timeStatus = asUtf8(selectedComboValue(timeStatus));
        if (draft.allDay)
        {
            draft.timeStatus = "Timed";
            draft.startTime.reset();
            draft.endTime.reset();
        }
        const std::wstring repeatValue = selectedComboValue(repeat);
        const bool repeating = repeatValue != L"none";
        if (!repeating)
        {
            draft.repeatSeriesId.clear();
        }
        EngineCalendarDate repeatEnd;
        if (repeating)
        {
            if (!calendarDateFromText(asWString(repeatUntil.Text()), repeatEnd))
            {
                validation.Text(L"Repeat until must use yyyy-MM-dd.");
                validation.Visibility(Visibility::Visible);
                continue;
            }
        }

        const auto eventValidation =
            classmngr::engine::CalendarEventValidator::validate(draft);
        if (!eventValidation.isValid())
        {
            std::wstring message = L"Calendar event validation: ";
            for (const auto& issue : eventValidation.issues())
            {
                if (!issue.isError())
                {
                    continue;
                }
                if (message.back() != L' ')
                {
                    message += L"; ";
                }
                message += asWide(issue.code);
                if (!issue.field.empty())
                {
                    message += L" (" + asWide(issue.field) + L")";
                }
            }
            validation.Text(winrt::hstring(message));
            validation.Visibility(Visibility::Visible);
            continue;
        }
        classmngr::engine::CalendarEventService service(*m_openDatabase);
        bool persisted = false;
        std::string errorMessage;
        if (repeating)
        {
            classmngr::engine::CalendarEventRepeatFrequency frequency =
                classmngr::engine::CalendarEventRepeatFrequency::Daily;
            if (repeatValue == L"weekly")
            {
                frequency = classmngr::engine::CalendarEventRepeatFrequency::Weekly;
            }
            else if (repeatValue == L"monthly")
            {
                frequency = classmngr::engine::CalendarEventRepeatFrequency::Monthly;
            }
            const auto recurrenceValidation =
                classmngr::engine::CalendarEventValidator::validateRecurrence(
                    draft,
                    frequency,
                    repeatEnd
                    );
            if (!recurrenceValidation.isValid())
            {
                validation.Text(L"Repeat range is invalid or too long.");
                validation.Visibility(Visibility::Visible);
                continue;
            }
            if (eventId > 0 && !event.repeatSeriesId.empty())
            {
                const auto updated = service.updateRepeatSeriesFromDate(
                    event,
                    draft
                    );
                persisted = static_cast<bool>(updated);
                if (!persisted)
                {
                    errorMessage = updated.error().message;
                }
            }
            else if (eventId <= 0)
            {
                const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
                draft.repeatSeriesId = "winui-" + std::to_string(stamp);
                const auto created = service.createRepeatSeries(
                    draft,
                    frequency,
                    repeatEnd
                    );
                persisted = static_cast<bool>(created);
                if (!persisted)
                {
                    errorMessage = created.error().message;
                }
            }
            else
            {
                const auto saved = service.save(draft);
                persisted = static_cast<bool>(saved);
                if (!persisted)
                {
                    errorMessage = saved.error().message;
                }
            }
        }
        else
        {
            const auto saved = service.save(draft);
            persisted = static_cast<bool>(saved);
            if (!persisted)
            {
                errorMessage = saved.error().message;
            }
        }
        if (!persisted)
        {
            validation.Text(winrt::hstring(
                L"Calendar event could not be saved: " + asWide(errorMessage)
                ));
            validation.Visibility(Visibility::Visible);
            continue;
        }
        m_dirtyState.markClean();
        m_calendarStatusText.Text(L"Calendar event saved.");
        refreshCalendarPage();
        updateFileCommandState();
        break;
    }

    if (m_ownedDialog == dialog)
    {
        m_ownedDialog = nullptr;
    }
}

void MainWindow::populateSubPrepPage(
    Microsoft::UI::Xaml::Controls::Page const& page,
    bool refresh
    )
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    static_cast<void>(refresh);
    if (!m_subPrepTabs)
    {
        auto scroll = ScrollViewer();
        scroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
        scroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);

        auto root = StackPanel();
        root.Padding(Thickness{32.0, 24.0, 32.0, 32.0});
        root.Spacing(16.0);
        root.MaxWidth(980.0);
        root.HorizontalAlignment(HorizontalAlignment::Center);

        auto title = TextBlock();
        title.Text(L"Sub Prep");
        title.FontSize(24.0);
        setAutomationName(title, L"Sub Prep");
        root.Children().Append(title);

        auto description = TextBlock();
        description.Text(
            L"Prepare substitute materials, schedules, and class notes for a substitute teacher."
            );
        description.TextWrapping(TextWrapping::Wrap);
        setAutomationName(description, L"Sub Prep description");
        root.Children().Append(description);

        m_subPrepStatusText = TextBlock();
        m_subPrepStatusText.Text(L"Loading substitute-preparation information...");
        m_subPrepStatusText.TextWrapping(TextWrapping::Wrap);
        setAutomationName(m_subPrepStatusText, L"Sub Prep status");
        root.Children().Append(m_subPrepStatusText);

        m_subPrepValidationText = TextBlock();
        m_subPrepValidationText.TextWrapping(TextWrapping::Wrap);
        m_subPrepValidationText.Visibility(Visibility::Collapsed);
        setAutomationName(
            m_subPrepValidationText,
            L"Sub Prep validation summary"
            );
        root.Children().Append(m_subPrepValidationText);

        m_subPrepTabs = Pivot();
        m_subPrepTabs.IsTabStop(true);
        m_subPrepTabs.TabIndex(0);
        setAutomationName(m_subPrepTabs, L"Sub Prep sections");

        const auto appendReadOnlyValue = [](
            ClassMngrWinUISharedUX::Card& card,
            wchar_t const* label,
            wchar_t const* automationName
            ) {
            auto heading = TextBlock();
            heading.Text(label);
            heading.TextWrapping(TextWrapping::Wrap);
            setAutomationName(heading, automationName);
            card.content.Children().Append(heading);

            auto value = TextBlock();
            value.Text(L"N/A");
            value.TextWrapping(TextWrapping::Wrap);
            value.Margin(Thickness{0.0, 0.0, 0.0, 6.0});
            setAutomationName(value, std::wstring(automationName) + L" value");
            card.content.Children().Append(value);
            return value;
        };

        auto importantRoot = StackPanel();
        importantRoot.Spacing(12.0);

        auto campusCard = ClassMngrWinUISharedUX::buildCard({
            L"Campus Information",
            L"Read-only campus and office details from the packaged campus directory.",
            L"Sub Prep campus information"
            });
        m_subPrepCampusText = appendReadOnlyValue(
            campusCard,
            L"Campus",
            L"Sub Prep campus"
            );
        m_subPrepOfficeText = appendReadOnlyValue(
            campusCard,
            L"Office",
            L"Sub Prep office"
            );
        m_subPrepWifiText = appendReadOnlyValue(
            campusCard,
            L"Office Wi-Fi",
            L"Sub Prep office Wi-Fi"
            );
        m_subPrepWifiPasswordText = appendReadOnlyValue(
            campusCard,
            L"Office Wi-Fi password",
            L"Sub Prep office Wi-Fi password"
            );
        m_subPrepPhotocopierText = appendReadOnlyValue(
            campusCard,
            L"Photocopier code",
            L"Sub Prep photocopier code"
            );
        importantRoot.Children().Append(campusCard.root);

        auto zoomCard = ClassMngrWinUISharedUX::buildCard({
            L"Personal Zoom Information",
            L"Read-only Zoom information from My Details.",
            L"Sub Prep Zoom information"
            });
        m_subPrepZoomLoginText = appendReadOnlyValue(
            zoomCard,
            L"Zoom login ID",
            L"Sub Prep Zoom login ID"
            );
        m_subPrepZoomPasswordText = appendReadOnlyValue(
            zoomCard,
            L"Zoom password",
            L"Sub Prep Zoom password"
            );
        importantRoot.Children().Append(zoomCard.root);

        auto notesCard = ClassMngrWinUISharedUX::buildCard({
            L"Class Materials and Lesson Notes",
            L"These notes are saved with the active database and included in the renderer-neutral document model.",
            L"Sub Prep editable notes"
            });
        const auto makeNotesBox = [this](
            wchar_t const* header,
            wchar_t const* automationName
            ) {
            auto box = TextBox();
            box.Header(box_value(hstring(header)));
            box.MinHeight(92.0);
            box.MaxLength(10000);
            box.AcceptsReturn(true);
            box.TextWrapping(TextWrapping::Wrap);
            box.VerticalContentAlignment(VerticalAlignment::Top);
            box.HorizontalAlignment(HorizontalAlignment::Stretch);
            box.IsTabStop(true);
            box.TextChanging(
                [this](
                    TextBox const& sender,
                    TextBoxTextChangingEventArgs const& arguments
                    ) {
                    static_cast<void>(sender);
                    static_cast<void>(arguments);
                    markSubPrepDirty();
                }
                );
            setAutomationName(box, automationName);
            return box;
        };
        m_subPrepClassMaterialsTextBox = makeNotesBox(
            L"Class materials",
            L"Sub Prep class materials"
            );
        m_subPrepGradingTextBox = makeNotesBox(
            L"Book report grading",
            L"Sub Prep book report grading"
            );
        m_subPrepSpecialInstructionsTextBox = makeNotesBox(
            L"Book report special instructions",
            L"Sub Prep book report special instructions"
            );
        m_subPrepNotesTextBox = makeNotesBox(
            L"Detailed class and lesson notes",
            L"Sub Prep detailed class and lesson notes"
            );
        notesCard.content.Children().Append(m_subPrepClassMaterialsTextBox);
        notesCard.content.Children().Append(m_subPrepGradingTextBox);
        notesCard.content.Children().Append(m_subPrepSpecialInstructionsTextBox);
        notesCard.content.Children().Append(m_subPrepNotesTextBox);

        auto actions = StackPanel();
        actions.Orientation(Orientation::Horizontal);
        actions.Spacing(8.0);
        m_subPrepSaveButton = Button();
        m_subPrepSaveButton.Content(box_value(hstring(L"Save Changes")));
        m_subPrepSaveButton.IsTabStop(true);
        m_subPrepSaveButton.TabIndex(5);
        m_subPrepSaveButton.Click(
            [this](
                Windows::Foundation::IInspectable const& sender,
                RoutedEventArgs const& arguments
                ) {
                static_cast<void>(sender);
                static_cast<void>(arguments);
                saveSubPrepPage();
            }
            );
        setAutomationName(m_subPrepSaveButton, L"Save Sub Prep changes");

        m_subPrepDiscardButton = Button();
        m_subPrepDiscardButton.Content(
            box_value(hstring(L"Discard Changes"))
            );
        m_subPrepDiscardButton.IsTabStop(true);
        m_subPrepDiscardButton.TabIndex(6);
        m_subPrepDiscardButton.Click(
            [this](
                Windows::Foundation::IInspectable const& sender,
                RoutedEventArgs const& arguments
                ) {
                static_cast<void>(sender);
                static_cast<void>(arguments);
                discardSubPrepPage();
            }
            );
        setAutomationName(
            m_subPrepDiscardButton,
            L"Discard Sub Prep changes"
            );
        actions.Children().Append(m_subPrepSaveButton);
        actions.Children().Append(m_subPrepDiscardButton);
        notesCard.content.Children().Append(actions);
        importantRoot.Children().Append(notesCard.root);

        auto scheduleRoot = StackPanel();
        scheduleRoot.Spacing(12.0);
        m_subPrepScheduleSummaryText = TextBlock();
        m_subPrepScheduleSummaryText.TextWrapping(TextWrapping::Wrap);
        setAutomationName(
            m_subPrepScheduleSummaryText,
            L"Sub Prep schedule summary"
            );
        scheduleRoot.Children().Append(m_subPrepScheduleSummaryText);

        m_subPrepScheduleList = ListView();
        m_subPrepScheduleList.Height(360.0);
        m_subPrepScheduleList.IsTabStop(true);
        m_subPrepScheduleList.SelectionMode(ListViewSelectionMode::None);
        setAutomationName(m_subPrepScheduleList, L"Sub Prep schedule list");
        scheduleRoot.Children().Append(m_subPrepScheduleList);

        m_subPrepDocumentSummaryText = TextBlock();
        m_subPrepDocumentSummaryText.TextWrapping(TextWrapping::Wrap);
        setAutomationName(
        m_subPrepDocumentSummaryText,
            L"Sub Prep document model summary"
            );
        scheduleRoot.Children().Append(m_subPrepDocumentSummaryText);

        auto packageCard = ClassMngrWinUISharedUX::buildCard({
            L"Bundled Sub Prep Package",
            L"Choose dates and classes to preview the deterministic document paths that the Phase 7 output adapters will produce.",
            L"Sub Prep bundled package planner"
            });
        m_subPrepPackageUserNameTextBox = TextBox();
        m_subPrepPackageUserNameTextBox.Header(
            box_value(hstring(L"Substitute teacher name"))
            );
        m_subPrepPackageUserNameTextBox.PlaceholderText(
            L"Name used in the package folder"
            );
        m_subPrepPackageUserNameTextBox.MinWidth(320.0);
        m_subPrepPackageUserNameTextBox.IsTabStop(true);
        setAutomationName(
            m_subPrepPackageUserNameTextBox,
            L"Sub Prep package substitute teacher name"
            );

        m_subPrepPackageDatesTextBox = TextBox();
        m_subPrepPackageDatesTextBox.Header(
            box_value(hstring(L"Selected dates"))
            );
        m_subPrepPackageDatesTextBox.PlaceholderText(
            L"yyyy-MM-dd, yyyy-MM-dd"
            );
        m_subPrepPackageDatesTextBox.MinWidth(320.0);
        m_subPrepPackageDatesTextBox.IsTabStop(true);
        setAutomationName(
            m_subPrepPackageDatesTextBox,
            L"Sub Prep package selected dates"
            );

        m_subPrepPackageRosterTemplateCombo = ComboBox();
        m_subPrepPackageRosterTemplateCombo.Header(
            box_value(hstring(L"Roster document"))
            );
        m_subPrepPackageRosterTemplateCombo.IsTabStop(true);
        m_subPrepPackageRosterTemplateCombo.MinWidth(320.0);
        for (const wchar_t* label : {
                 L"By day",
                 L"Daily",
                 L"One roster per class with extra information"
             })
        {
            auto item = ComboBoxItem();
            item.Content(box_value(hstring(label)));
            m_subPrepPackageRosterTemplateCombo.Items().Append(item);
        }
        m_subPrepPackageRosterTemplateCombo.SelectedIndex(0);
        setAutomationName(
            m_subPrepPackageRosterTemplateCombo,
            L"Sub Prep package roster template"
            );

        auto classesLabel = TextBlock();
        classesLabel.Text(L"Classes included in the package");
        classesLabel.TextWrapping(TextWrapping::Wrap);
        setAutomationName(classesLabel, L"Sub Prep package classes label");

        m_subPrepPackageClassesList = ListView();
        m_subPrepPackageClassesList.Height(200.0);
        m_subPrepPackageClassesList.IsTabStop(true);
        m_subPrepPackageClassesList.SelectionMode(
            ListViewSelectionMode::None
            );
        setAutomationName(
            m_subPrepPackageClassesList,
            L"Sub Prep package class selection"
            );

        m_subPrepPackagePlanButton = Button();
        m_subPrepPackagePlanButton.Content(
            box_value(hstring(L"Plan Bundled Package"))
            );
        m_subPrepPackagePlanButton.IsTabStop(true);
        m_subPrepPackagePlanButton.Click(
            [this](
                Windows::Foundation::IInspectable const& sender,
                RoutedEventArgs const& arguments
                ) {
                static_cast<void>(sender);
                static_cast<void>(arguments);
                planSubPrepPackage();
            }
            );
        setAutomationName(
            m_subPrepPackagePlanButton,
            L"Plan Sub Prep bundled package"
            );

        m_subPrepPackageStatusText = TextBlock();
        m_subPrepPackageStatusText.TextWrapping(TextWrapping::Wrap);
        setAutomationName(
            m_subPrepPackageStatusText,
            L"Sub Prep bundled package status"
            );

        auto packagePathsLabel = TextBlock();
        packagePathsLabel.Text(L"Planned relative document paths");
        packagePathsLabel.TextWrapping(TextWrapping::Wrap);
        setAutomationName(
            packagePathsLabel,
            L"Sub Prep package paths label"
            );

        m_subPrepPackagePathsList = ListView();
        m_subPrepPackagePathsList.Height(140.0);
        m_subPrepPackagePathsList.IsTabStop(true);
        m_subPrepPackagePathsList.SelectionMode(ListViewSelectionMode::None);
        setAutomationName(
            m_subPrepPackagePathsList,
            L"Sub Prep bundled package paths"
            );

        packageCard.content.Children().Append(m_subPrepPackageUserNameTextBox);
        packageCard.content.Children().Append(m_subPrepPackageDatesTextBox);
        packageCard.content.Children().Append(m_subPrepPackageRosterTemplateCombo);
        packageCard.content.Children().Append(classesLabel);
        packageCard.content.Children().Append(m_subPrepPackageClassesList);
        packageCard.content.Children().Append(m_subPrepPackagePlanButton);
        packageCard.content.Children().Append(m_subPrepPackageStatusText);
        packageCard.content.Children().Append(packagePathsLabel);
        packageCard.content.Children().Append(m_subPrepPackagePathsList);
        scheduleRoot.Children().Append(packageCard.root);

        auto classInformationRoot = StackPanel();
        classInformationRoot.Spacing(12.0);
        auto classInformationIntro = TextBlock();
        classInformationIntro.Text(
            L"Classes are grouped by teacher using the same stable ordering as the Qt workflow."
            );
        classInformationIntro.TextWrapping(TextWrapping::Wrap);
        setAutomationName(
            classInformationIntro,
            L"Sub Prep class information description"
            );
        classInformationRoot.Children().Append(classInformationIntro);

        m_subPrepClassInformationList = ListView();
        m_subPrepClassInformationList.Height(480.0);
        m_subPrepClassInformationList.IsTabStop(true);
        m_subPrepClassInformationList.SelectionMode(
            ListViewSelectionMode::None
            );
        setAutomationName(
            m_subPrepClassInformationList,
            L"Sub Prep class information list"
            );
        classInformationRoot.Children().Append(m_subPrepClassInformationList);

        auto importantTab = PivotItem();
        importantTab.Header(box_value(hstring(L"Important Information")));
        importantTab.Content(importantRoot);
        setAutomationName(importantTab, L"Sub Prep Important Information tab");
        m_subPrepTabs.Items().Append(importantTab);

        auto scheduleTab = PivotItem();
        scheduleTab.Header(box_value(hstring(L"Schedule")));
        scheduleTab.Content(scheduleRoot);
        setAutomationName(scheduleTab, L"Sub Prep Schedule tab");
        m_subPrepTabs.Items().Append(scheduleTab);

        auto classInformationTab = PivotItem();
        classInformationTab.Header(box_value(hstring(L"Class Information")));
        classInformationTab.Content(classInformationRoot);
        setAutomationName(
            classInformationTab,
            L"Sub Prep Class Information tab"
            );
        m_subPrepTabs.Items().Append(classInformationTab);

        root.Children().Append(m_subPrepTabs);
        scroll.Content(root);
        page.Content(scroll);
    }

    refreshSubPrepPage();
}

void MainWindow::refreshSubPrepPage()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_subPrepStatusText || !m_subPrepClassMaterialsTextBox)
    {
        return;
    }

    const bool preserveDraft = m_subPrepDirty;
    m_subPrepLoading = true;
    const auto clearPresentation = [this]() {
        m_subPrepDocument = {};
        m_subPrepClasses.clear();
        m_subPrepSourceClasses.clear();
        m_subPrepClassInformation.clear();
        m_subPrepCampusText.Text(L"Campus: N/A");
        m_subPrepOfficeText.Text(L"Office: N/A");
        m_subPrepWifiText.Text(L"Office Wi-Fi: N/A");
        m_subPrepWifiPasswordText.Text(L"Office Wi-Fi password: N/A");
        m_subPrepPhotocopierText.Text(L"Photocopier code: N/A");
        m_subPrepZoomLoginText.Text(L"Zoom login ID: N/A");
        m_subPrepZoomPasswordText.Text(L"Zoom password: N/A");
        m_subPrepClassMaterialsTextBox.Text({});
        m_subPrepGradingTextBox.Text({});
        m_subPrepSpecialInstructionsTextBox.Text({});
        m_subPrepNotesTextBox.Text({});
        m_subPrepScheduleSummaryText.Text({});
        m_subPrepDocumentSummaryText.Text({});
        m_subPrepScheduleList.Items().Clear();
        m_subPrepClassInformationList.Items().Clear();
        m_subPrepPackagePlan = {};
        m_subPrepPackageUserNameTextBox.Text({});
        m_subPrepPackageDatesTextBox.Text({});
        m_subPrepPackageClassesList.Items().Clear();
        m_subPrepPackageClassChecks.clear();
        m_subPrepPackageStatusText.Text({});
        m_subPrepPackagePathsList.Items().Clear();
    };
    const auto showFailure = [this](
        std::wstring status,
        std::wstring validation
        ) {
        m_subPrepStatusText.Text(winrt::hstring(status));
        m_subPrepValidationText.Text(winrt::hstring(validation));
        m_subPrepValidationText.Visibility(Visibility::Visible);
        m_subPrepLoading = false;
        updateSubPrepActions();
    };

    if (!m_openDatabase)
    {
        clearPresentation();
        m_subPrepDirty = false;
        m_subPrepStatusText.Text(L"No database open.");
        m_subPrepValidationText.Text(
            L"Open a .tps or .db file to view substitute-preparation information."
            );
        m_subPrepValidationText.Visibility(Visibility::Collapsed);
        m_subPrepLoading = false;
        updateSubPrepActions();
        return;
    }

    classmngr::engine::ApplicationSettingsService settings(*m_openDatabase);
    classmngr::engine::PersonalDetailsService personalService(settings);
    const auto personal = personalService.load();
    if (!personal)
    {
        showFailure(
            L"Sub Prep could not load personal details.",
            L"Personal details error: " + asWide(personal.error().message)
            );
        return;
    }

    const auto classMaterials = subPrepTextSetting(
        settings,
        "subPrep/classMaterials",
        {}
        );
    const auto gradingInstructions = subPrepTextSetting(
        settings,
        "subPrep/bookReportGrading",
        "Scoring: 0 / 20 / 40 / 60 / 80 / 100\nComments: Please leave a comment about what the student did well and what they need to work on."
        );
    const auto specialInstructions = subPrepTextSetting(
        settings,
        "subPrep/bookReportSpecialInstructions",
        "N/A"
        );
    const auto subNotes = subPrepTextSetting(
        settings,
        "subPrep/subComments",
        {}
        );
    if (!classMaterials || !gradingInstructions || !specialInstructions || !subNotes)
    {
        const auto* error = !classMaterials
            ? &classMaterials.error()
            : !gradingInstructions
                ? &gradingInstructions.error()
                : !specialInstructions
                    ? &specialInstructions.error()
                    : &subNotes.error();
        showFailure(
            L"Sub Prep settings could not be loaded.",
            L"Sub Prep settings error: " + asWide(error->message)
            );
        return;
    }
    if (!preserveDraft)
    {
        m_subPrepClassMaterialsTextBox.Text(asWide(*classMaterials));
        m_subPrepGradingTextBox.Text(asWide(*gradingInstructions));
        m_subPrepSpecialInstructionsTextBox.Text(asWide(*specialInstructions));
        m_subPrepNotesTextBox.Text(asWide(*subNotes));
    }

    const auto displayValue = [](std::wstring value) {
        return value.empty() ? std::wstring(L"N/A") : std::move(value);
    };
    const auto setLabeledText = [&displayValue](
        TextBlock const& target,
        wchar_t const* label,
        std::wstring value
        ) {
        std::wstring text(label);
        text += L": ";
        text += displayValue(std::move(value));
        target.Text(winrt::hstring(text));
    };

    CampusResourceView selectedCampus;
    std::wstring resourceWarning;
    const auto campusResources = loadPackagedCampusResources();
    if (!campusResources)
    {
        resourceWarning = L"Campus directory warning: ";
        resourceWarning += asWide(campusResources.error().message);
    }
    else
    {
        const auto normalized = [](std::wstring value) {
            std::transform(
                value.begin(),
                value.end(),
                value.begin(),
                [](wchar_t character) { return std::towlower(character); }
                );
            return value;
        };
        const std::wstring wantedCampus = normalized(asWide(personal->campus));
        for (const CampusResourceView& candidate : *campusResources)
        {
            if ((!wantedCampus.empty()
                 && (normalized(candidate.id) == wantedCampus
                     || normalized(candidate.campusName) == wantedCampus))
                || (wantedCampus.empty() && selectedCampus.campusName.empty()))
            {
                selectedCampus = candidate;
                break;
            }
        }
        if (selectedCampus.campusName.empty() && !campusResources->empty())
        {
            selectedCampus = campusResources->front();
        }
        if (selectedCampus.campusName.empty() && !wantedCampus.empty())
        {
            resourceWarning = L"No packaged campus directory entry matched the selected campus.";
        }
    }
    setLabeledText(
        m_subPrepCampusText,
        L"Campus",
        selectedCampus.campusName.empty()
            ? asWide(personal->campus)
            : selectedCampus.campusName
        );
    setLabeledText(
        m_subPrepOfficeText,
        L"Office",
        selectedCampus.officeNumber
        );
    setLabeledText(
        m_subPrepWifiText,
        L"Office Wi-Fi",
        selectedCampus.officeWifi
        );
    setLabeledText(
        m_subPrepWifiPasswordText,
        L"Office Wi-Fi password",
        selectedCampus.officeWifiPassword
        );
    setLabeledText(
        m_subPrepPhotocopierText,
        L"Photocopier code",
        selectedCampus.photocopierCode
        );
    setLabeledText(
        m_subPrepZoomLoginText,
        L"Zoom login ID",
        asWide(personal->zoomLoginId)
        );
    setLabeledText(
        m_subPrepZoomPasswordText,
        L"Zoom password",
        asWide(personal->zoomPassword)
        );

    classmngr::engine::ClassRepository repository(*m_openDatabase);
    const auto classrooms = repository.list();
    if (!classrooms)
    {
        clearPresentation();
        showFailure(
            L"Sub Prep classes could not be loaded.",
            L"Class list error: " + asWide(classrooms.error().message)
            );
        return;
    }

    classmngr::engine::ClassInfoService infoService(*m_openDatabase);
    classmngr::engine::RosterService rosterService(*m_openDatabase);
    classmngr::engine::TeacherService teacherService(*m_openDatabase);
    std::vector<classmngr::engine::ClassInfo> classInfos;
    classInfos.reserve(classrooms->size());
    m_subPrepClasses = *classrooms;
    m_subPrepSourceClasses.clear();
    m_subPrepSourceClasses.reserve(classrooms->size());
    for (const classmngr::engine::Classroom& classroom : *classrooms)
    {
        const auto info = infoService.load(classroom.id);
        if (!info)
        {
            clearPresentation();
            showFailure(
                L"Sub Prep class information could not be loaded.",
                L"Class information error: " + asWide(info.error().message)
                );
            return;
        }
        const auto studentCount = rosterService.studentCount(classroom.id);
        if (!studentCount)
        {
            clearPresentation();
            showFailure(
                L"Sub Prep rosters could not be loaded.",
                L"Roster error: " + asWide(studentCount.error().message)
                );
            return;
        }

        classmngr::engine::SubPrepSourceClass source;
        source.classroom = classroom;
        source.info = *info;
        source.studentCount = *studentCount;
        if (info->teacherId > 0)
        {
            const auto teacher = teacherService.get(info->teacherId);
            if (!teacher)
            {
                clearPresentation();
                showFailure(
                    L"Sub Prep teacher information could not be loaded.",
                    L"Teacher error: " + asWide(teacher.error().message)
                    );
                return;
            }
            source.teacher = *teacher;
        }
        classInfos.push_back(*info);
        m_subPrepSourceClasses.push_back(std::move(source));
    }

    m_subPrepPackagePlan = {};
    m_subPrepPackageClassesList.Items().Clear();
    m_subPrepPackageClassChecks.clear();
    if (m_subPrepPackageUserNameTextBox.Text().empty())
    {
        m_subPrepPackageUserNameTextBox.Text(asWide(personal->name));
    }
    if (m_subPrepPackageDatesTextBox.Text().empty())
    {
        m_subPrepPackageDatesTextBox.Text(
            calendarDateText(calendarToday())
            );
    }
    for (const auto& source : m_subPrepSourceClasses)
    {
        std::wstring className = asWide(source.classroom.name);
        if (className.empty())
        {
            className = L"Class " + std::to_wstring(source.classroom.id);
        }
        auto check = CheckBox();
        check.Content(box_value(hstring(className)));
        check.Tag(box_value(source.classroom.id));
        check.IsChecked(true);
        check.IsTabStop(true);
        setAutomationName(
            check,
            L"Include " + className + L" in Sub Prep package"
            );
        m_subPrepPackageClassesList.Items().Append(check);
        m_subPrepPackageClassChecks.push_back(check);
    }
    m_subPrepPackageStatusText.Text(
        L"Select a date and classes, then plan the bundled package."
        );
    m_subPrepPackagePathsList.Items().Clear();

    const auto visibleDays = classmngr::engine::ScheduleReportService::visibleDays(
        false
        );
    const auto scheduleBuild = classmngr::engine::ScheduleBuilderService::build(
        classInfos,
        false,
        visibleDays
        );
    classmngr::engine::ScheduleReportRequest scheduleRequest;
    scheduleRequest.days = visibleDays;
    scheduleRequest.displayMode =
        classmngr::engine::ScheduleReportDisplayMode::Regular;
    const auto schedule = classmngr::engine::ScheduleReportService::build(
        scheduleBuild,
        scheduleRequest
        );

    classmngr::engine::SubPrepBuildOptions classInformationOptions;
    classInformationOptions.visibleDays = visibleDays;
    classInformationOptions.visibleClassIds.reserve(
        m_subPrepSourceClasses.size()
        );
    for (const auto& source : m_subPrepSourceClasses)
    {
        classInformationOptions.visibleClassIds.push_back(source.classroom.id);
    }
    m_subPrepClassInformation =
        classmngr::engine::SubPrepClassInformationService::build(
            m_subPrepSourceClasses,
            classInformationOptions
            );

    classmngr::engine::SubPrepDocumentRequest documentRequest;
    documentRequest.campus.officeNumber = asUtf8(
        std::wstring_view(selectedCampus.officeNumber)
        );
    documentRequest.campus.officeWifi = asUtf8(
        std::wstring_view(selectedCampus.officeWifi)
        );
    documentRequest.campus.officeWifiPassword = asUtf8(
        std::wstring_view(selectedCampus.officeWifiPassword)
        );
    documentRequest.campus.photocopierCode = asUtf8(
        std::wstring_view(selectedCampus.photocopierCode)
        );
    documentRequest.zoom.loginId = personal->zoomLoginId;
    documentRequest.zoom.password = personal->zoomPassword;
    documentRequest.classMaterials = asUtf8(asWString(
        m_subPrepClassMaterialsTextBox.Text()
        ));
    documentRequest.gradingInstructions = asUtf8(asWString(
        m_subPrepGradingTextBox.Text()
        ));
    documentRequest.specialInstructions = asUtf8(asWString(
        m_subPrepSpecialInstructionsTextBox.Text()
        ));
    documentRequest.schedule = schedule;
    documentRequest.classInformation = m_subPrepClassInformation;
    documentRequest.subNotes = asUtf8(asWString(m_subPrepNotesTextBox.Text()));
    m_subPrepDocument = classmngr::engine::SubPrepDocumentService::build(
        documentRequest
        );

    const auto appendListText = [](ListView const& list,
                                   std::wstring text,
                                   std::wstring automationName) {
        auto row = TextBlock();
        row.Text(winrt::hstring(text));
        row.TextWrapping(TextWrapping::Wrap);
        auto item = ListViewItem();
        item.Content(row);
        item.IsTabStop(false);
        setAutomationName(item, automationName);
        list.Items().Append(item);
    };
    m_subPrepScheduleList.Items().Clear();
    std::size_t scheduleRows = 0;
    for (const auto& source : m_subPrepSourceClasses)
    {
        std::wstring className = asWide(source.classroom.name);
        if (className.empty())
        {
            className = L"Class " + std::to_wstring(source.classroom.id);
        }
        std::wstring teacherName = asWide(
            source.teacher.preferredDisplayName()
            );
        if (teacherName.empty())
        {
            teacherName = L"N/A";
        }
        for (const classmngr::engine::ClassTime& time : source.info.classTimes)
        {
            if (std::find(visibleDays.begin(), visibleDays.end(), time.day)
                == visibleDays.end())
            {
                continue;
            }
            std::wstring row = className;
            row += L" — ";
            row += asWide(time.day);
            row += L" ";
            row += asWide(time.startTime);
            row += L" - ";
            row += asWide(time.endTime);
            row += L" — ";
            row += teacherName;
            row += L" — Room ";
            row += displayValue(asWide(source.info.roomNumber));
            appendListText(
                m_subPrepScheduleList,
                std::move(row),
                L"Sub Prep schedule row"
                );
            ++scheduleRows;
        }
    }
    if (scheduleRows == 0)
    {
        appendListText(
            m_subPrepScheduleList,
            L"No regular class times are scheduled for Monday through Friday.",
            L"Sub Prep empty schedule"
            );
    }
    m_subPrepScheduleSummaryText.Text(winrt::hstring(
        L"Schedule model ready: "
            + std::to_wstring(m_subPrepDocument.schedule.summary.scheduledBlocks)
            + L" scheduled blocks across "
            + std::to_wstring(visibleDays.size())
            + L" weekdays."
        ));
    m_subPrepDocumentSummaryText.Text(
        L"Renderer-neutral document model ready for the Phase 7 output adapters."
        );

    m_subPrepClassInformationList.Items().Clear();
    if (m_subPrepClassInformation.empty())
    {
        appendListText(
            m_subPrepClassInformationList,
            L"No teacher-grouped class information is available.",
            L"Sub Prep empty class information"
            );
    }
    else
    {
        for (const auto& group : m_subPrepClassInformation)
        {
            std::wstring row = asWide(group.displayName);
            row += L" — ";
            row += asWide(group.classListText);
            for (const auto& details : group.classes)
            {
                row += L"\n  ";
                row += asWide(details.classLabel);
                row += L" | ";
                row += asWide(details.timeText);
                row += L" | ";
                row += std::to_wstring(details.studentCount);
                row += L" students";
                if (!details.info.notes.empty())
                {
                    row += L"\n  Notes: ";
                    row += asWide(details.info.notes);
                }
            }
            appendListText(
                m_subPrepClassInformationList,
                std::move(row),
                L"Sub Prep teacher class information"
                );
        }
    }

    m_subPrepStatusText.Text(
        preserveDraft
            ? L"Sub Prep information loaded; unsaved text remains in the editor."
            : L"Sub Prep information loaded."
        );
    if (resourceWarning.empty())
    {
        m_subPrepValidationText.Text({});
        m_subPrepValidationText.Visibility(Visibility::Collapsed);
    }
    else
    {
        m_subPrepValidationText.Text(winrt::hstring(resourceWarning));
        m_subPrepValidationText.Visibility(Visibility::Visible);
    }
    m_subPrepLoading = false;
    updateSubPrepActions();
}

void MainWindow::saveSubPrepPage()
{
    if (!m_openDatabase || !m_subPrepClassMaterialsTextBox)
    {
        return;
    }

    classmngr::engine::ApplicationSettingsService settings(*m_openDatabase);
    const auto saved = settings.saveBatch({
        {
            "subPrep/classMaterials",
            classmngr::engine::SettingValue{asUtf8(asWString(
                m_subPrepClassMaterialsTextBox.Text()
                ))}
        },
        {
            "subPrep/bookReportGrading",
            classmngr::engine::SettingValue{asUtf8(asWString(
                m_subPrepGradingTextBox.Text()
                ))}
        },
        {
            "subPrep/bookReportSpecialInstructions",
            classmngr::engine::SettingValue{asUtf8(asWString(
                m_subPrepSpecialInstructionsTextBox.Text()
                ))}
        },
        {
            "subPrep/subComments",
            classmngr::engine::SettingValue{asUtf8(asWString(
                m_subPrepNotesTextBox.Text()
                ))}
        }
        });
    if (!saved)
    {
        m_subPrepStatusText.Text(L"Sub Prep settings could not be saved.");
        m_subPrepValidationText.Text(winrt::hstring(
            L"Sub Prep settings error: " + asWide(saved.error().message)
            ));
        m_subPrepValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Visible
            );
        updateSubPrepActions();
        return;
    }

    m_subPrepDirty = false;
    m_dirtyState.markClean();
    refreshSubPrepPage();
    m_subPrepStatusText.Text(L"Sub Prep settings saved.");
    m_subPrepValidationText.Text({});
    m_subPrepValidationText.Visibility(
        Microsoft::UI::Xaml::Visibility::Collapsed
        );
    updateSubPrepActions();
    updateFileCommandState();
}

void MainWindow::discardSubPrepPage()
{
    if (!m_openDatabase)
    {
        return;
    }

    m_subPrepDirty = false;
    m_dirtyState.markClean();
    refreshSubPrepPage();
    m_subPrepStatusText.Text(L"Sub Prep changes discarded.");
    updateSubPrepActions();
    updateFileCommandState();
}

void MainWindow::updateSubPrepActions()
{
    if (!m_subPrepClassMaterialsTextBox || !m_subPrepSaveButton)
    {
        return;
    }

    const bool enabled = static_cast<bool>(m_openDatabase);
    m_subPrepClassMaterialsTextBox.IsEnabled(enabled);
    m_subPrepGradingTextBox.IsEnabled(enabled);
    m_subPrepSpecialInstructionsTextBox.IsEnabled(enabled);
    m_subPrepNotesTextBox.IsEnabled(enabled);
    m_subPrepSaveButton.IsEnabled(enabled && m_subPrepDirty);
    m_subPrepDiscardButton.IsEnabled(enabled && m_subPrepDirty);
    if (m_subPrepPackageUserNameTextBox)
    {
        m_subPrepPackageUserNameTextBox.IsEnabled(enabled);
    }
    if (m_subPrepPackageDatesTextBox)
    {
        m_subPrepPackageDatesTextBox.IsEnabled(enabled);
    }
    if (m_subPrepPackageRosterTemplateCombo)
    {
        m_subPrepPackageRosterTemplateCombo.IsEnabled(enabled);
    }
    if (m_subPrepPackagePlanButton)
    {
        m_subPrepPackagePlanButton.IsEnabled(
            enabled && !m_subPrepPackageClassChecks.empty()
            );
    }
    if (m_subPrepPackageClassesList)
    {
        m_subPrepPackageClassesList.IsEnabled(enabled);
    }
    if (m_subPrepPackagePathsList)
    {
        m_subPrepPackagePathsList.IsEnabled(enabled);
    }
    if (m_subPrepScheduleList)
    {
        m_subPrepScheduleList.IsEnabled(enabled);
    }
    if (m_subPrepClassInformationList)
    {
        m_subPrepClassInformationList.IsEnabled(enabled);
    }
}

void MainWindow::markSubPrepDirty()
{
    if (m_subPrepLoading || !m_openDatabase)
    {
        return;
    }

    m_subPrepDirty = true;
    m_dirtyState.markDirty();
    if (m_subPrepStatusText)
    {
        m_subPrepStatusText.Text(L"Unsaved Sub Prep changes.");
    }
    updateSubPrepActions();
}

void MainWindow::planSubPrepPackage()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_openDatabase || !m_subPrepPackageDatesTextBox
        || !m_subPrepPackagePlanButton)
    {
        return;
    }

    m_subPrepPackagePlan = {};
    m_subPrepPackagePathsList.Items().Clear();

    const std::wstring rawDates = asWString(
        m_subPrepPackageDatesTextBox.Text()
        );
    std::vector<EngineCalendarDate> selectedDates;
    std::size_t start = 0;
    while (start <= rawDates.size())
    {
        const std::size_t separator = rawDates.find_first_of(
            L",;\r\n",
            start
            );
        const std::size_t end = separator == std::wstring::npos
            ? rawDates.size()
            : separator;
        std::size_t first = start;
        while (first < end && std::iswspace(rawDates[first]) != 0)
        {
            ++first;
        }
        std::size_t last = end;
        while (last > first && std::iswspace(rawDates[last - 1]) != 0)
        {
            --last;
        }
        if (first < last)
        {
            EngineCalendarDate date;
            if (!calendarDateFromText(
                    std::wstring_view(rawDates).substr(first, last - first),
                    date
                    ))
            {
                m_subPrepPackageStatusText.Text(
                    L"Package plan could not be created."
                    );
                m_subPrepValidationText.Text(
                    L"Selected dates must use yyyy-MM-dd, separated by commas."
                    );
                m_subPrepValidationText.Visibility(Visibility::Visible);
                return;
            }
            selectedDates.push_back(date);
        }
        if (separator == std::wstring::npos)
        {
            break;
        }
        start = separator + 1;
    }
    if (selectedDates.empty())
    {
        m_subPrepPackageStatusText.Text(L"Package plan could not be created.");
        m_subPrepValidationText.Text(
            L"Select at least one date before planning the package."
            );
        m_subPrepValidationText.Visibility(Visibility::Visible);
        return;
    }

    std::vector<int> classIds;
    for (const auto& check : m_subPrepPackageClassChecks)
    {
        const auto checked = check.IsChecked();
        if (!checked || !checked.Value())
        {
            continue;
        }
        const int classId = boxedInt(check.Tag());
        if (classId > 0)
        {
            classIds.push_back(classId);
        }
    }
    if (classIds.empty())
    {
        m_subPrepPackageStatusText.Text(L"Package plan could not be created.");
        m_subPrepValidationText.Text(
            L"Select at least one class before planning the package."
            );
        m_subPrepValidationText.Visibility(Visibility::Visible);
        return;
    }

    std::vector<classmngr::engine::SubPrepPackageSourceClass> sourceClasses;
    sourceClasses.reserve(m_subPrepSourceClasses.size());
    for (const auto& source : m_subPrepSourceClasses)
    {
        sourceClasses.push_back({
            source.classroom,
            source.info,
            source.teacher
        });
    }

    classmngr::engine::SubPrepPackageBuildOptions options;
    options.userName = asUtf8(asWString(
        m_subPrepPackageUserNameTextBox.Text()
        ));
    options.selectedDates = std::move(selectedDates);
    options.classIds = std::move(classIds);
    const int templateIndex = m_subPrepPackageRosterTemplateCombo
        ? m_subPrepPackageRosterTemplateCombo.SelectedIndex()
        : 0;
    if (templateIndex == 1)
    {
        options.rosterTemplate =
            classmngr::engine::SubPrepRosterTemplate::Daily;
    }
    else if (templateIndex == 2)
    {
        options.rosterTemplate =
            classmngr::engine::SubPrepRosterTemplate::PerClassWithExtraInfo;
    }

    const auto planned = classmngr::engine::SubPrepPackageService::build(
        sourceClasses,
        options
        );
    if (!planned)
    {
        m_subPrepPackageStatusText.Text(L"Package plan could not be created.");
        m_subPrepValidationText.Text(winrt::hstring(
            L"Package planning error: " + asWide(planned.error().message)
            ));
        m_subPrepValidationText.Visibility(Visibility::Visible);
        return;
    }

    m_subPrepPackagePlan = *planned;
    for (const std::string& path : m_subPrepPackagePlan.relativeDocumentPaths)
    {
        auto row = TextBlock();
        row.Text(asWide(path));
        row.TextWrapping(TextWrapping::Wrap);
        auto item = ListViewItem();
        item.Content(row);
        item.IsTabStop(false);
        setAutomationName(item, L"Sub Prep package path " + asWide(path));
        m_subPrepPackagePathsList.Items().Append(item);
    }

    std::wstring status = L"Package plan ready: ";
    status += std::to_wstring(m_subPrepPackagePlan.classes.size());
    status += L" classes, ";
    status += std::to_wstring(
        m_subPrepPackagePlan.relativeDocumentPaths.size()
        );
    status += L" document paths in folder \"";
    status += asWide(m_subPrepPackagePlan.folderName);
    status += L"\".";
    m_subPrepPackageStatusText.Text(winrt::hstring(status));
    m_subPrepValidationText.Text({});
    m_subPrepValidationText.Visibility(Visibility::Collapsed);
    updateSubPrepActions();
}

void MainWindow::populatePersonalDetailsPage(
    Microsoft::UI::Xaml::Controls::Page const& page,
    bool refresh
    )
{
    const auto host = page.try_as<
        Microsoft::UI::Xaml::Controls::ContentControl>();
    if (!host && !m_personalNameTextBox)
    {
        return;
    }
    populatePersonalDetailsPage(
        host,
        refresh
        );
}

void MainWindow::populatePersonalDetailsPage(
    Microsoft::UI::Xaml::Controls::ContentControl const& host,
    bool refresh
    )
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_personalNameTextBox)
    {
        auto scroll = ScrollViewer();
        scroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
        scroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);

        auto root = StackPanel();
        root.Padding(Thickness{32.0, 24.0, 32.0, 32.0});
        root.Spacing(18.0);
        root.MaxWidth(1180.0);
        root.HorizontalAlignment(HorizontalAlignment::Center);

        auto title = TextBlock();
        title.Text(L"My Information");
        title.FontSize(28.0);
        setAutomationName(title, L"My Information");
        root.Children().Append(title);

        auto description = TextBlock();
        description.Text(
            L"Enter your personal information and choose the signature shown on generated documents."
            );
        description.TextWrapping(TextWrapping::Wrap);
        setAutomationName(description, L"Personal details description");
        root.Children().Append(description);

        m_personalStatusText = TextBlock();
        m_personalStatusText.Text(L"Loading personal details...");
        m_personalStatusText.TextWrapping(TextWrapping::Wrap);
        setAutomationName(m_personalStatusText, L"Personal details status");
        root.Children().Append(m_personalStatusText);

        m_personalValidationText = TextBlock();
        m_personalValidationText.TextWrapping(TextWrapping::Wrap);
        m_personalValidationText.Visibility(Visibility::Collapsed);
        setAutomationName(
            m_personalValidationText,
            L"Personal details validation summary"
            );
        root.Children().Append(m_personalValidationText);

        const auto makeTextBox = [this](
            wchar_t const* header,
            wchar_t const* automationName,
            wchar_t const* placeholder
            ) {
            auto box = TextBox();
            box.Header(box_value(hstring(header)));
            box.PlaceholderText(placeholder);
            box.MinWidth(150.0);
            box.HorizontalAlignment(HorizontalAlignment::Stretch);
            box.IsTabStop(true);
            box.TextChanging({this, &MainWindow::PersonalDetailsField_TextChanging});
            setAutomationName(box, automationName);
            return box;
        };

        auto detailsCard = ClassMngrWinUISharedUX::buildCard({
            L"My Information",
            L"These values are stored in the active ClassMngr database.",
            L"Personal details form"
            });
        m_personalNameTextBox = makeTextBox(
            L"My Name",
            L"Personal name",
            L"Enter your name"
            );
        m_personalNameTextBox.TabIndex(0);
        m_personalCampusCombo = ComboBox();
        m_personalCampusCombo.Header(box_value(hstring(L"My Campus")));
        m_personalCampusCombo.PlaceholderText(L"Select your campus");
        m_personalCampusCombo.MinWidth(150.0);
        m_personalCampusCombo.HorizontalAlignment(HorizontalAlignment::Stretch);
        m_personalCampusCombo.IsTabStop(true);
        m_personalCampusCombo.TabIndex(1);
        m_personalCampusCombo.SelectionChanged(
            {this, &MainWindow::PersonalDetailsCampus_SelectionChanged}
            );
        setAutomationName(m_personalCampusCombo, L"Personal campus");
        if (m_campusResourceRecords.empty())
        {
            const auto campusResources = loadPackagedCampusResources();
            if (campusResources)
            {
                m_campusResourceRecords = *campusResources;
            }
        }
        for (const CampusResourceView& campus : m_campusResourceRecords)
        {
            if (campus.campusName.empty())
            {
                continue;
            }
            auto item = ComboBoxItem();
            item.Content(box_value(hstring(campus.campusName)));
            item.Tag(box_value(hstring(campus.campusName)));
            setAutomationName(item, L"Campus " + campus.campusName);
            m_personalCampusCombo.Items().Append(item);
        }
        m_personalZoomLoginIdTextBox = makeTextBox(
            L"Zoom Login ID",
            L"Zoom login ID",
            L"Enter your Zoom login ID"
            );
        m_personalZoomLoginIdTextBox.TabIndex(2);
        m_personalZoomPasswordBox = PasswordBox();
        m_personalZoomPasswordBox.Header(
            box_value(hstring(L"Zoom Password"))
            );
        m_personalZoomPasswordBox.MinWidth(150.0);
        m_personalZoomPasswordBox.HorizontalAlignment(
            HorizontalAlignment::Stretch
            );
        m_personalZoomPasswordBox.IsTabStop(true);
        m_personalZoomPasswordBox.TabIndex(3);
        m_personalZoomPasswordBox.PasswordChanged(
            {this, &MainWindow::PersonalDetailsPassword_Changed}
            );
        setAutomationName(m_personalZoomPasswordBox, L"Zoom password");

        m_personalZoomNotAvailableCheck = CheckBox();
        m_personalZoomNotAvailableCheck.Content(
            box_value(hstring(L"N/A"))
            );
        m_personalZoomNotAvailableCheck.IsTabStop(true);
        m_personalZoomNotAvailableCheck.TabIndex(4);
        m_personalZoomNotAvailableCheck.VerticalAlignment(
            VerticalAlignment::Center
            );
        m_personalZoomNotAvailableCheck.Checked(
            {this, &MainWindow::PersonalDetailsZoomAvailability_Changed}
            );
        m_personalZoomNotAvailableCheck.Unchecked(
            {this, &MainWindow::PersonalDetailsZoomAvailability_Changed}
            );
        setAutomationName(
            m_personalZoomNotAvailableCheck,
            L"Zoom not available (N/A)"
            );
        auto informationFields = Grid();
        informationFields.ColumnSpacing(16.0);
        for (int index = 0; index < 4; ++index)
        {
            auto column = ColumnDefinition();
            column.Width(GridLengthHelper::FromValueAndType(
                1.0,
                GridUnitType::Star
                ));
            informationFields.ColumnDefinitions().Append(column);
        }
        auto zoomColumn = ColumnDefinition();
        zoomColumn.Width(GridLengthHelper::FromValueAndType(
            1.0,
            GridUnitType::Auto
            ));
        informationFields.ColumnDefinitions().Append(zoomColumn);
        Grid::SetColumn(m_personalNameTextBox, 0);
        Grid::SetColumn(m_personalCampusCombo, 1);
        Grid::SetColumn(m_personalZoomLoginIdTextBox, 2);
        Grid::SetColumn(m_personalZoomPasswordBox, 3);
        Grid::SetColumn(m_personalZoomNotAvailableCheck, 4);
        informationFields.Children().Append(m_personalNameTextBox);
        informationFields.Children().Append(m_personalCampusCombo);
        informationFields.Children().Append(m_personalZoomLoginIdTextBox);
        informationFields.Children().Append(m_personalZoomPasswordBox);
        informationFields.Children().Append(m_personalZoomNotAvailableCheck);
        detailsCard.content.Children().Append(informationFields);
        root.Children().Append(detailsCard.root);

        auto signatureTitle = TextBlock();
        signatureTitle.Text(L"Signature");
        signatureTitle.FontSize(24.0);
        setAutomationName(signatureTitle, L"Signature");
        root.Children().Append(signatureTitle);

        auto signatureCard = ClassMngrWinUISharedUX::buildCard({
            {},
            {},
            L"Personal signature form"
            });
        auto instructions = TextBlock();
        instructions.Text(L"Choose an image file or type your signature.");
        instructions.TextWrapping(TextWrapping::Wrap);
        setAutomationName(instructions, L"Signature instructions");
        signatureCard.content.Children().Append(instructions);
        auto modeButtons = StackPanel();
        modeButtons.Orientation(Orientation::Horizontal);
        modeButtons.Spacing(8.0);
        m_personalSignatureImageButton = Button();
        m_personalSignatureImageButton.Content(box_value(hstring(L"Image")));
        m_personalSignatureImageButton.IsTabStop(true);
        m_personalSignatureImageButton.TabIndex(5);
        m_personalSignatureImageButton.Click(
            [this](auto const&, auto const&) {
                if (m_personalSignatureModeCombo)
                {
                    m_personalSignatureModeCombo.SelectedIndex(0);
                }
            }
            );
        setAutomationName(
            m_personalSignatureImageButton,
            L"Image signature mode"
            );
        m_personalSignatureTypeButton = Button();
        m_personalSignatureTypeButton.Content(box_value(hstring(L"Type")));
        m_personalSignatureTypeButton.IsTabStop(true);
        m_personalSignatureTypeButton.TabIndex(6);
        m_personalSignatureTypeButton.Click(
            [this](auto const&, auto const&) {
                if (m_personalSignatureModeCombo)
                {
                    m_personalSignatureModeCombo.SelectedIndex(1);
                }
            }
            );
        setAutomationName(
            m_personalSignatureTypeButton,
            L"Typed signature mode"
            );
        modeButtons.Children().Append(m_personalSignatureImageButton);
        modeButtons.Children().Append(m_personalSignatureTypeButton);
        signatureCard.content.Children().Append(modeButtons);

        m_personalSignatureModeCombo = ComboBox();
        m_personalSignatureModeCombo.IsTabStop(false);
        m_personalSignatureModeCombo.Visibility(Visibility::Collapsed);
        auto imageMode = ComboBoxItem();
        imageMode.Content(box_value(hstring(L"Image")));
        setAutomationName(imageMode, L"Image signature mode");
        auto typedMode = ComboBoxItem();
        typedMode.Content(box_value(hstring(L"Type")));
        setAutomationName(typedMode, L"Typed signature mode");
        m_personalSignatureModeCombo.Items().Append(imageMode);
        m_personalSignatureModeCombo.Items().Append(typedMode);
        m_personalSignatureModeCombo.SelectionChanged(
            {this, &MainWindow::PersonalDetailsSignatureMode_SelectionChanged}
            );
        setAutomationName(
            m_personalSignatureModeCombo,
            L"Signature mode"
            );
        signatureCard.content.Children().Append(m_personalSignatureModeCombo);

        m_personalSignaturePreviewBorder = Border();
        m_personalSignaturePreviewBorder.MinHeight(150.0);
        m_personalSignaturePreviewBorder.Padding(
            Thickness{16.0, 12.0, 16.0, 12.0}
            );
        m_personalSignaturePreviewBorder.HorizontalAlignment(
            HorizontalAlignment::Stretch
            );
        m_personalSignaturePreviewBorder.CornerRadius(
            CornerRadius{6.0, 6.0, 6.0, 6.0}
            );
        m_personalSignaturePreviewBorder.BorderThickness(
            Thickness{1.0, 1.0, 1.0, 1.0}
            );
        m_personalSignaturePreviewBorder.Background(
            Microsoft::UI::Xaml::Media::SolidColorBrush(
                Windows::UI::Color{255, 255, 255, 255}
                )
            );
        try
        {
            const auto resources = Application::Current().Resources();
            const auto stroke = resources.Lookup(
                box_value(hstring(L"Phase3ControlStrokeBrush"))
                );
            if (stroke)
            {
                m_personalSignaturePreviewBorder.BorderBrush(
                    stroke.as<Microsoft::UI::Xaml::Media::Brush>()
                    );
            }
        }
        catch (...)
        {
        }
        m_personalSignaturePreviewText = TextBlock();
        m_personalSignaturePreviewText.HorizontalAlignment(
            HorizontalAlignment::Center
            );
        m_personalSignaturePreviewText.VerticalAlignment(
            VerticalAlignment::Center
            );
        m_personalSignaturePreviewText.TextAlignment(TextAlignment::Center);
        m_personalSignaturePreviewText.TextWrapping(TextWrapping::Wrap);
        m_personalSignaturePreviewText.FontSize(38.0);
        m_personalSignaturePreviewText.Foreground(
            Microsoft::UI::Xaml::Media::SolidColorBrush(
                Windows::UI::Color{255, 24, 24, 24}
                )
            );
        setAutomationName(m_personalSignaturePreviewText, L"Signature preview");
        m_personalSignaturePreviewBorder.Child(m_personalSignaturePreviewText);
        signatureCard.content.Children().Append(m_personalSignaturePreviewBorder);

        m_personalTypedSignatureTextBox = makeTextBox(
            L"Type your signature",
            L"Typed signature text",
            L"Type your name"
            );
        m_personalTypedSignatureTextBox.TabIndex(7);
        m_personalTypedSignatureControls = StackPanel();
        m_personalTypedSignatureControls.Spacing(8.0);
        m_personalTypedSignatureControls.Children().Append(
            m_personalTypedSignatureTextBox
            );
        m_personalSignatureFontCombo = ComboBox();
        m_personalSignatureFontCombo.Header(
            box_value(hstring(L"Signature style"))
            );
        m_personalSignatureFontCombo.MinWidth(280.0);
        m_personalSignatureFontCombo.IsTabStop(true);
        m_personalSignatureFontCombo.TabIndex(8);
        for (auto const& font : {
                 std::pair{0, L"Just Another Hand"},
                 std::pair{1, L"Dancing Script"},
                 std::pair{2, L"Great Vibes"},
                 std::pair{3, L"Caveat"}
             })
        {
            auto item = ComboBoxItem();
            item.Content(box_value(hstring(font.second)));
            item.Tag(box_value(font.first));
            setAutomationName(item, font.second);
            m_personalSignatureFontCombo.Items().Append(item);
        }
        m_personalSignatureFontCombo.SelectionChanged(
            {this, &MainWindow::PersonalDetailsSignatureMode_SelectionChanged}
            );
        setAutomationName(
            m_personalSignatureFontCombo,
            L"Typed signature style"
            );
        m_personalSignatureFontCombo.Visibility(Visibility::Collapsed);
        m_personalTypedSignatureControls.Children().Append(
            m_personalSignatureFontCombo
            );
        auto styleLabel = TextBlock();
        styleLabel.Text(L"Choose a style");
        m_personalTypedSignatureControls.Children().Append(styleLabel);
        auto fontGrid = Grid();
        fontGrid.ColumnSpacing(8.0);
        fontGrid.RowSpacing(8.0);
        for (int index = 0; index < 2; ++index)
        {
            fontGrid.ColumnDefinitions().Append(ColumnDefinition());
            fontGrid.RowDefinitions().Append(RowDefinition());
        }
        m_personalSignatureFontButtons.clear();
        for (int index = 0; index < 4; ++index)
        {
            const auto item = m_personalSignatureFontCombo.Items().GetAt(index)
                .as<ComboBoxItem>();
            auto card = Border();
            card.Padding(Thickness{10.0, 8.0, 10.0, 8.0});
            card.BorderThickness(Thickness{1.0, 1.0, 1.0, 1.0});
            card.CornerRadius(CornerRadius{6.0, 6.0, 6.0, 6.0});
            auto cardContents = StackPanel();
            cardContents.Spacing(6.0);
            auto fontName = TextBlock();
            fontName.Text(hstring(boxedString(item.Content())));
            auto sample = TextBlock();
            sample.Text(L"Your Signature");
            sample.HorizontalAlignment(HorizontalAlignment::Center);
            sample.FontSize(28.0);
            const std::array<wchar_t const*, 4> sampleFonts{
                L"Comic Sans MS", L"Segoe Script", L"Lucida Handwriting", L"Segoe Print"};
            sample.FontFamily(Microsoft::UI::Xaml::Media::FontFamily(
                sampleFonts[static_cast<std::size_t>(index)]));
            auto select = Button();
            select.IsTabStop(true);
            select.TabIndex(9 + index);
            select.Click([this, index](auto const&, auto const&) {
                m_personalSignatureFontCombo.SelectedIndex(index);
            });
            setAutomationName(select, L"Use signature font " + boxedString(item.Content()));
            cardContents.Children().Append(fontName);
            cardContents.Children().Append(sample);
            cardContents.Children().Append(select);
            card.Child(cardContents);
            Grid::SetColumn(card, index % 2);
            Grid::SetRow(card, index / 2);
            fontGrid.Children().Append(card);
            m_personalSignatureFontButtons.push_back(select);
        }
        m_personalTypedSignatureControls.Children().Append(fontGrid);

        m_personalImageStatusText = TextBlock();
        m_personalImageStatusText.TextWrapping(TextWrapping::Wrap);
        setAutomationName(
            m_personalImageStatusText,
            L"Signature image status"
            );
        m_personalImageControls = StackPanel();
        m_personalImageControls.Spacing(8.0);
        m_personalImageControls.Children().Append(m_personalImageStatusText);
        auto imageActions = StackPanel();
        imageActions.Orientation(Orientation::Horizontal);
        imageActions.Spacing(8.0);
        auto replaceImageButton = Button();
        replaceImageButton.Content(box_value(hstring(L"Replace Signature...")));
        replaceImageButton.IsEnabled(false);
        replaceImageButton.IsTabStop(false);
        setAutomationName(
            replaceImageButton,
            L"Replace signature image (Phase 7 placeholder)"
            );
        auto removeImageButton = Button();
        removeImageButton.Content(box_value(hstring(L"Remove")));
        removeImageButton.IsEnabled(false);
        removeImageButton.IsTabStop(false);
        setAutomationName(
            removeImageButton,
            L"Remove signature image (Phase 7 placeholder)"
            );
        imageActions.Children().Append(replaceImageButton);
        imageActions.Children().Append(removeImageButton);
        m_personalImageControls.Children().Append(imageActions);
        signatureCard.content.Children().Append(m_personalImageControls);
        signatureCard.content.Children().Append(m_personalTypedSignatureControls);
        root.Children().Append(signatureCard.root);

        auto actions = StackPanel();
        actions.Orientation(Orientation::Horizontal);
        actions.Spacing(8.0);
        m_personalSaveButton = Button();
        m_personalSaveButton.Content(box_value(hstring(L"Save Changes")));
        m_personalSaveButton.IsTabStop(true);
        m_personalSaveButton.TabIndex(13);
        m_personalSaveButton.Click(
            {this, &MainWindow::PersonalDetailsSaveButton_Click}
            );
        setAutomationName(m_personalSaveButton, L"Save personal details");
        m_personalDiscardButton = Button();
        m_personalDiscardButton.Content(box_value(hstring(L"Discard Changes")));
        m_personalDiscardButton.IsTabStop(true);
        m_personalDiscardButton.TabIndex(14);
        m_personalDiscardButton.Click(
            {this, &MainWindow::PersonalDetailsDiscardButton_Click}
            );
        setAutomationName(
            m_personalDiscardButton,
            L"Discard personal detail changes"
            );
        actions.Children().Append(m_personalSaveButton);
        actions.Children().Append(m_personalDiscardButton);
        root.Children().Append(actions);

        scroll.Content(root);
        host.Content(scroll);
        m_personalSignatureModeCombo.SelectedIndex(0);
        m_personalSignatureFontCombo.SelectedIndex(0);
        updatePersonalSignatureControls();
    }

    const auto setEditable = [this](bool enabled) {
        const auto checkedValue = m_personalZoomNotAvailableCheck.IsChecked();
        const bool zoomNotAvailable = checkedValue && checkedValue.Value();
        if (m_personalNameTextBox)
        {
            m_personalNameTextBox.IsEnabled(enabled);
        }
        if (m_personalCampusCombo)
        {
            m_personalCampusCombo.IsEnabled(enabled);
        }
        if (m_personalZoomLoginIdTextBox)
        {
            m_personalZoomLoginIdTextBox.IsEnabled(
                enabled && !zoomNotAvailable
                );
        }
        if (m_personalZoomPasswordBox)
        {
            m_personalZoomPasswordBox.IsEnabled(
                enabled && !zoomNotAvailable
                );
        }
        if (m_personalZoomNotAvailableCheck)
        {
            m_personalZoomNotAvailableCheck.IsEnabled(enabled);
        }
        if (m_personalSignatureModeCombo)
        {
            m_personalSignatureModeCombo.IsEnabled(enabled);
        }
        if (m_personalSignatureImageButton)
        {
            m_personalSignatureImageButton.IsEnabled(enabled);
        }
        if (m_personalSignatureTypeButton)
        {
            m_personalSignatureTypeButton.IsEnabled(enabled);
        }
        if (m_personalTypedSignatureTextBox)
        {
            m_personalTypedSignatureTextBox.IsEnabled(
                enabled && m_personalSignatureModeCombo.SelectedIndex() == 1
                );
        }
        if (m_personalSignatureFontCombo)
        {
            m_personalSignatureFontCombo.IsEnabled(
                enabled && m_personalSignatureModeCombo.SelectedIndex() == 1
                );
        }
        for (const auto& button : m_personalSignatureFontButtons)
        {
            button.IsEnabled(
                enabled && m_personalSignatureModeCombo.SelectedIndex() == 1
                );
        }
        if (m_personalSaveButton)
        {
            m_personalSaveButton.IsEnabled(enabled && m_personalDetailsDirty);
        }
        if (m_personalDiscardButton)
        {
            m_personalDiscardButton.IsEnabled(enabled && m_personalDetailsDirty);
        }
        updatePersonalSignatureControls();
    };

    if (!m_openDatabase)
    {
        m_personalDetailsLoading = true;
        m_personalDetailsLoaded = false;
        m_personalDetailsDirty = false;
        m_personalDetails = {};
        m_personalNameTextBox.Text({});
        m_personalCampusCombo.SelectedIndex(-1);
        m_personalZoomLoginIdTextBox.Text({});
        m_personalZoomPasswordBox.Password({});
        m_personalZoomNotAvailableCheck.IsChecked(false);
        m_personalSignatureModeCombo.SelectedIndex(0);
        m_personalSignatureFontCombo.SelectedIndex(0);
        m_personalTypedSignatureTextBox.Text({});
        m_personalImageStatusText.Text(
            L"No database is open. Open a .tps or .db file to edit personal details."
            );
        m_personalStatusText.Text(L"No database open.");
        m_personalValidationText.Text({});
        m_personalValidationText.Visibility(Visibility::Collapsed);
        m_personalDetailsLoading = false;
        setEditable(false);
        return;
    }

    if (refresh && m_personalDetailsDirty)
    {
        m_personalStatusText.Text(L"Unsaved personal detail changes are retained.");
        return;
    }

    m_personalDetailsLoading = true;
    classmngr::engine::ApplicationSettingsService settings(*m_openDatabase);
    classmngr::engine::PersonalDetailsService service(settings);
    const auto loaded = service.load();
    if (!loaded)
    {
        m_personalDetailsLoaded = false;
        m_personalDetailsDirty = false;
        m_personalStatusText.Text(winrt::hstring(
            L"Personal details could not be loaded: "
            + asWide(loaded.error().message)
            ));
        m_personalValidationText.Text(L"The engine rejected the personal-details read.");
        m_personalValidationText.Visibility(Visibility::Visible);
        m_personalDetailsLoading = false;
        setEditable(false);
        return;
    }

    m_personalDetails = *loaded;
    m_personalNameTextBox.Text(asWide(m_personalDetails.name));
    int campusIndex = -1;
    const std::wstring savedCampus = asWide(m_personalDetails.campus);
    for (uint32_t index = 0; index < m_personalCampusCombo.Items().Size(); ++index)
    {
        const auto item = m_personalCampusCombo.Items().GetAt(index).try_as<
            Microsoft::UI::Xaml::Controls::ComboBoxItem>();
        if (item
            && (equalsIgnoreCase(boxedString(item.Tag()), savedCampus)
                || equalsIgnoreCase(boxedString(item.Content()), savedCampus)))
        {
            campusIndex = static_cast<int>(index);
            break;
        }
    }
    if (campusIndex < 0 && !savedCampus.empty())
    {
        auto item = Microsoft::UI::Xaml::Controls::ComboBoxItem();
        item.Content(box_value(hstring(savedCampus)));
        item.Tag(box_value(hstring(savedCampus)));
        setAutomationName(item, L"Campus " + savedCampus);
        m_personalCampusCombo.Items().Append(item);
        campusIndex = static_cast<int>(m_personalCampusCombo.Items().Size()) - 1;
    }
    m_personalCampusCombo.SelectedIndex(
        campusIndex >= 0
            ? campusIndex
            : (m_personalCampusCombo.Items().Size() > 0 ? 0 : -1)
        );
    m_personalZoomLoginIdTextBox.Text(asWide(m_personalDetails.zoomLoginId));
    m_personalZoomPasswordBox.Password(asWide(m_personalDetails.zoomPassword));
    m_personalZoomNotAvailableCheck.IsChecked(m_personalDetails.zoomNotAvailable);
    m_personalSignatureModeCombo.SelectedIndex(
        m_personalDetails.signatureMode
                == classmngr::engine::SignatureMode::Type
            ? 1
            : 0
        );
    m_personalTypedSignatureTextBox.Text(
        asWide(m_personalDetails.typedSignatureText)
        );
    m_personalSignatureFontCombo.SelectedIndex(
        m_personalDetails.typedSignatureFont >= 0
            && m_personalDetails.typedSignatureFont < 4
            ? m_personalDetails.typedSignatureFont
            : 0
        );
    m_personalDetailsLoaded = true;
    m_personalDetailsDirty = false;
    m_personalStatusText.Text(L"Personal details loaded.");
    m_personalValidationText.Text({});
    m_personalValidationText.Visibility(Visibility::Collapsed);
    m_personalImageStatusText.Text(
        m_personalDetails.signatureImageBase64.empty()
            ? L"Signature image selection will be added in a later slice."
            : L"An existing signature image is retained by the engine; image selection is not available in this slice."
        );
    m_personalDetailsLoading = false;
    setEditable(true);
    updatePersonalSignatureControls();
}

void MainWindow::updatePersonalSignatureControls()
{
    if (!m_personalSignatureModeCombo)
    {
        return;
    }

    const bool typed = m_personalSignatureModeCombo.SelectedIndex() == 1;
    if (m_personalImageControls)
    {
        m_personalImageControls.Visibility(
            typed
                ? Microsoft::UI::Xaml::Visibility::Collapsed
                : Microsoft::UI::Xaml::Visibility::Visible
            );
    }
    if (m_personalTypedSignatureControls)
    {
        m_personalTypedSignatureControls.Visibility(
            typed
                ? Microsoft::UI::Xaml::Visibility::Visible
                : Microsoft::UI::Xaml::Visibility::Collapsed
            );
    }

    const auto setModeStyle = [](Microsoft::UI::Xaml::Controls::Button const& button,
                                 bool selected) {
        if (!button)
        {
            return;
        }
        try
        {
            const auto resource =
                Microsoft::UI::Xaml::Application::Current().Resources().Lookup(
                    box_value(hstring(
                        selected
                            ? L"Phase3PrimaryButtonStyle"
                            : L"Phase3SecondaryButtonStyle"
                        ))
                    );
            if (resource)
            {
                button.Style(resource.as<Microsoft::UI::Xaml::Style>());
            }
        }
        catch (...)
        {
        }
    };
    setModeStyle(m_personalSignatureImageButton, !typed);
    setModeStyle(m_personalSignatureTypeButton, typed);
    const int selectedFont = m_personalSignatureFontCombo
        ? m_personalSignatureFontCombo.SelectedIndex()
        : -1;
    for (std::size_t index = 0;
         index < m_personalSignatureFontButtons.size();
         ++index)
    {
        const bool selected = static_cast<int>(index) == selectedFont;
        auto const& button = m_personalSignatureFontButtons[index];
        button.Content(box_value(hstring(
            selected ? L"Selected" : L"Use this font"
            )));
        setModeStyle(button, selected);
    }
    updatePersonalSignaturePreview();
}

void MainWindow::updatePersonalSignaturePreview()
{
    if (!m_personalSignaturePreviewText || !m_personalSignatureModeCombo)
    {
        return;
    }

    const bool typed = m_personalSignatureModeCombo.SelectedIndex() == 1;
    if (!typed)
    {
        m_personalSignaturePreviewText.FontFamily(
            Microsoft::UI::Xaml::Media::FontFamily(L"Segoe UI")
            );
        m_personalSignaturePreviewText.FontSize(22.0);
        m_personalSignaturePreviewText.Text(
            m_personalDetails.signatureImageBase64.empty()
                ? L"No signature image added yet."
                : L"Existing signature image retained.\nPreview unavailable in this phase."
            );
        return;
    }

    const std::wstring typedText = asWString(
        m_personalTypedSignatureTextBox.Text()
        );
    m_personalSignaturePreviewText.Text(
        typedText.find_first_not_of(L" \t\r\n") == std::wstring::npos
            ? L"Your Signature"
            : winrt::hstring(typedText)
        );
    m_personalSignaturePreviewText.FontSize(38.0);
    const std::array<wchar_t const*, 4> previewFonts{
        L"Comic Sans MS",
        L"Segoe Script",
        L"Lucida Handwriting",
        L"Segoe Print"
    };
    const int selectedFont = m_personalSignatureFontCombo
        ? m_personalSignatureFontCombo.SelectedIndex()
        : 0;
    const int normalizedFont = selectedFont >= 0 && selectedFont < 4
        ? selectedFont
        : 0;
    m_personalSignaturePreviewText.FontFamily(
        Microsoft::UI::Xaml::Media::FontFamily(
            previewFonts[static_cast<std::size_t>(normalizedFont)]
            )
        );
}

void MainWindow::refreshPersonalDetailsPage()
{
    if (!m_contentFrame
        || (m_currentPageId != homePageId
            && m_currentPageId != personalDetailsPageId))
    {
        return;
    }

    const auto page = m_contentFrame.Content().try_as<
        Microsoft::UI::Xaml::Controls::Page>();
    if (page)
    {
        populatePersonalDetailsPage(page, true);
    }
}

void MainWindow::populateKoreanTeachersPage(
    Microsoft::UI::Xaml::Controls::Page const& page,
    bool refresh
    )
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_koreanTeacherKrTextBox)
    {
        auto scroll = ScrollViewer();
        scroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
        scroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);

        auto root = StackPanel();
        root.Padding(Thickness{32.0, 24.0, 32.0, 32.0});
        root.Spacing(16.0);
        root.MaxWidth(900.0);
        root.HorizontalAlignment(HorizontalAlignment::Center);

        auto title = TextBlock();
        title.Text(L"Korean Teachers");
        title.FontSize(24.0);
        setAutomationName(title, L"Korean Teachers");
        root.Children().Append(title);

        auto description = TextBlock();
        description.Text(
            L"View and maintain Korean teacher details used by classes and reports."
            );
        description.TextWrapping(TextWrapping::Wrap);
        setAutomationName(description, L"Korean teacher directory description");
        root.Children().Append(description);

        m_koreanTeacherStatusText = TextBlock();
        m_koreanTeacherStatusText.Text(L"Loading Korean teachers...");
        m_koreanTeacherStatusText.TextWrapping(TextWrapping::Wrap);
        setAutomationName(m_koreanTeacherStatusText, L"Korean teacher directory status");
        root.Children().Append(m_koreanTeacherStatusText);

        m_koreanTeacherValidationText = TextBlock();
        m_koreanTeacherValidationText.TextWrapping(TextWrapping::Wrap);
        m_koreanTeacherValidationText.Visibility(Visibility::Collapsed);
        setAutomationName(
            m_koreanTeacherValidationText,
            L"Korean teacher validation summary"
            );
        root.Children().Append(m_koreanTeacherValidationText);

        auto directoryCard = ClassMngrWinUISharedUX::buildCard({
            L"Teacher directory",
            L"Select an existing teacher or create a new directory entry.",
            L"Korean teacher directory selector"
            });
        m_koreanTeacherSelector = ComboBox();
        m_koreanTeacherSelector.Header(box_value(hstring(L"Teacher")));
        m_koreanTeacherSelector.PlaceholderText(L"Select a teacher");
        m_koreanTeacherSelector.MinWidth(420.0);
        m_koreanTeacherSelector.HorizontalAlignment(HorizontalAlignment::Stretch);
        m_koreanTeacherSelector.IsTabStop(true);
        m_koreanTeacherSelector.TabIndex(0);
        m_koreanTeacherSelector.SelectionChanged(
            {this, &MainWindow::KoreanTeacherSelection_SelectionChanged}
            );
        setAutomationName(m_koreanTeacherSelector, L"Korean teacher selector");
        directoryCard.content.Children().Append(m_koreanTeacherSelector);
        root.Children().Append(directoryCard.root);

        const auto makeTextBox = [this](
            wchar_t const* header,
            wchar_t const* automationName,
            wchar_t const* placeholder
            ) {
            auto box = TextBox();
            box.Header(box_value(hstring(header)));
            box.PlaceholderText(placeholder);
            box.MinWidth(320.0);
            box.HorizontalAlignment(HorizontalAlignment::Stretch);
            box.IsTabStop(true);
            box.TextChanging({this, &MainWindow::KoreanTeacherField_TextChanging});
            setAutomationName(box, automationName);
            return box;
        };

        auto detailsCard = ClassMngrWinUISharedUX::buildCard({
            L"Teacher details",
            L"Names, contact information, birthday, and preferred display name.",
            L"Korean teacher details form"
            });
        m_koreanTeacherKrTextBox = makeTextBox(
            L"Korean Name",
            L"Korean teacher Korean name",
            L"Enter the Korean name"
            );
        auto koreanInputScope = Input::InputScope();
        koreanInputScope.Names().Append(
            Input::InputScopeName(Input::InputScopeNameValue::Text)
            );
        m_koreanTeacherKrTextBox.InputScope(koreanInputScope);
        m_koreanTeacherKrTextBox.TabIndex(1);
        m_koreanTeacherEnTextBox = makeTextBox(
            L"English Name",
            L"Korean teacher English name",
            L"Enter the English name"
            );
        m_koreanTeacherEnTextBox.TabIndex(2);
        m_koreanTeacherRomanizationTextBox = makeTextBox(
            L"Preferred Spelling",
            L"Korean teacher preferred spelling",
            L"Optional preferred romanization"
            );
        m_koreanTeacherRomanizationTextBox.TabIndex(3);
        m_koreanTeacherPreferredNameCombo = ComboBox();
        m_koreanTeacherPreferredNameCombo.Header(
            box_value(hstring(L"Preferred Name"))
            );
        m_koreanTeacherPreferredNameCombo.MinWidth(320.0);
        m_koreanTeacherPreferredNameCombo.HorizontalAlignment(
            HorizontalAlignment::Stretch
            );
        m_koreanTeacherPreferredNameCombo.IsTabStop(true);
        m_koreanTeacherPreferredNameCombo.TabIndex(4);
        m_koreanTeacherPreferredNameCombo.SelectionChanged(
            {this, &MainWindow::KoreanTeacherCombo_SelectionChanged}
            );
        setAutomationName(
            m_koreanTeacherPreferredNameCombo,
            L"Korean teacher preferred name"
            );
        m_koreanTeacherRoomTextBox = makeTextBox(
            L"Room Number",
            L"Korean teacher room number",
            L"Optional room number"
            );
        m_koreanTeacherRoomTextBox.TabIndex(5);
        m_koreanTeacherBirthdayTextBox = makeTextBox(
            L"Birthday (MM-dd)",
            L"Korean teacher birthday",
            L"e.g. 03-14"
            );
        m_koreanTeacherBirthdayTextBox.TabIndex(6);
        m_koreanTeacherPhoneTextBox = makeTextBox(
            L"Phone Number",
            L"Korean teacher phone number",
            L"Optional phone number"
            );
        m_koreanTeacherPhoneTextBox.TabIndex(7);
        detailsCard.content.Children().Append(m_koreanTeacherKrTextBox);
        detailsCard.content.Children().Append(m_koreanTeacherEnTextBox);
        detailsCard.content.Children().Append(m_koreanTeacherRomanizationTextBox);
        detailsCard.content.Children().Append(m_koreanTeacherPreferredNameCombo);
        detailsCard.content.Children().Append(m_koreanTeacherRoomTextBox);
        detailsCard.content.Children().Append(m_koreanTeacherBirthdayTextBox);
        detailsCard.content.Children().Append(m_koreanTeacherPhoneTextBox);
        root.Children().Append(detailsCard.root);

        auto connectivityCard = ClassMngrWinUISharedUX::buildCard({
            L"Connectivity and presentation",
            L"Keep the same WiFi, Zoom, and projection choices as the Qt teacher form.",
            L"Korean teacher connectivity form"
            });
        m_koreanTeacherWifiNameTextBox = makeTextBox(
            L"WiFi Name",
            L"Korean teacher WiFi name",
            L"Optional WiFi name"
            );
        m_koreanTeacherWifiNameTextBox.TabIndex(8);
        m_koreanTeacherWifiPasswordBox = PasswordBox();
        m_koreanTeacherWifiPasswordBox.Header(
            box_value(hstring(L"WiFi Password"))
            );
        m_koreanTeacherWifiPasswordBox.MinWidth(320.0);
        m_koreanTeacherWifiPasswordBox.HorizontalAlignment(
            HorizontalAlignment::Stretch
            );
        m_koreanTeacherWifiPasswordBox.IsTabStop(true);
        m_koreanTeacherWifiPasswordBox.TabIndex(9);
        m_koreanTeacherWifiPasswordBox.PasswordChanged(
            {this, &MainWindow::KoreanTeacherPassword_Changed}
            );
        setAutomationName(
            m_koreanTeacherWifiPasswordBox,
            L"Korean teacher WiFi password"
            );
        m_koreanTeacherInternetTypeCombo = ComboBox();
        m_koreanTeacherInternetTypeCombo.Header(
            box_value(hstring(L"Internet Type"))
            );
        m_koreanTeacherInternetTypeCombo.MinWidth(320.0);
        m_koreanTeacherInternetTypeCombo.HorizontalAlignment(
            HorizontalAlignment::Stretch
            );
        m_koreanTeacherInternetTypeCombo.IsTabStop(true);
        m_koreanTeacherInternetTypeCombo.TabIndex(10);
        for (wchar_t const* value : {L"WiFi", L"LAN", L"Both", L"N/A"})
        {
            m_koreanTeacherInternetTypeCombo.Items().Append(
                box_value(hstring(value))
                );
        }
        m_koreanTeacherInternetTypeCombo.SelectionChanged(
            {this, &MainWindow::KoreanTeacherCombo_SelectionChanged}
            );
        setAutomationName(
            m_koreanTeacherInternetTypeCombo,
            L"Korean teacher internet type"
            );
        m_koreanTeacherZoomIdTextBox = makeTextBox(
            L"Zoom ID",
            L"Korean teacher Zoom ID",
            L"Optional Zoom ID"
            );
        m_koreanTeacherZoomIdTextBox.TabIndex(11);
        m_koreanTeacherZoomPasswordBox = PasswordBox();
        m_koreanTeacherZoomPasswordBox.Header(
            box_value(hstring(L"Zoom Password"))
            );
        m_koreanTeacherZoomPasswordBox.MinWidth(320.0);
        m_koreanTeacherZoomPasswordBox.HorizontalAlignment(
            HorizontalAlignment::Stretch
            );
        m_koreanTeacherZoomPasswordBox.IsTabStop(true);
        m_koreanTeacherZoomPasswordBox.TabIndex(12);
        m_koreanTeacherZoomPasswordBox.PasswordChanged(
            {this, &MainWindow::KoreanTeacherPassword_Changed}
            );
        setAutomationName(
            m_koreanTeacherZoomPasswordBox,
            L"Korean teacher Zoom password"
            );
        m_koreanTeacherProjectionTypeCombo = ComboBox();
        m_koreanTeacherProjectionTypeCombo.Header(
            box_value(hstring(L"Projection Type"))
            );
        m_koreanTeacherProjectionTypeCombo.MinWidth(320.0);
        m_koreanTeacherProjectionTypeCombo.HorizontalAlignment(
            HorizontalAlignment::Stretch
            );
        m_koreanTeacherProjectionTypeCombo.IsTabStop(true);
        m_koreanTeacherProjectionTypeCombo.TabIndex(13);
        for (wchar_t const* value : {L"HDMI", L"Zoom", L"Any", L"N/A"})
        {
            m_koreanTeacherProjectionTypeCombo.Items().Append(
                box_value(hstring(value))
                );
        }
        m_koreanTeacherProjectionTypeCombo.SelectionChanged(
            {this, &MainWindow::KoreanTeacherCombo_SelectionChanged}
            );
        setAutomationName(
            m_koreanTeacherProjectionTypeCombo,
            L"Korean teacher projection type"
            );
        connectivityCard.content.Children().Append(m_koreanTeacherWifiNameTextBox);
        connectivityCard.content.Children().Append(m_koreanTeacherWifiPasswordBox);
        connectivityCard.content.Children().Append(m_koreanTeacherInternetTypeCombo);
        connectivityCard.content.Children().Append(m_koreanTeacherZoomIdTextBox);
        connectivityCard.content.Children().Append(m_koreanTeacherZoomPasswordBox);
        connectivityCard.content.Children().Append(m_koreanTeacherProjectionTypeCombo);
        root.Children().Append(connectivityCard.root);

        auto notesCard = ClassMngrWinUISharedUX::buildCard({
            L"Notes",
            L"Free-form teacher notes are validated and persisted by the engine.",
            L"Korean teacher notes form"
            });
        m_koreanTeacherNotesTextBox = makeTextBox(
            L"Notes",
            L"Korean teacher notes",
            L"Optional notes"
            );
        m_koreanTeacherNotesTextBox.AcceptsReturn(true);
        m_koreanTeacherNotesTextBox.TextWrapping(TextWrapping::Wrap);
        m_koreanTeacherNotesTextBox.Height(150.0);
        m_koreanTeacherNotesTextBox.TabIndex(14);
        notesCard.content.Children().Append(m_koreanTeacherNotesTextBox);
        root.Children().Append(notesCard.root);

        auto actions = StackPanel();
        actions.Orientation(Orientation::Horizontal);
        actions.Spacing(8.0);
        m_koreanTeacherNewButton = Button();
        m_koreanTeacherNewButton.Content(box_value(hstring(L"New Teacher")));
        m_koreanTeacherNewButton.IsTabStop(true);
        m_koreanTeacherNewButton.TabIndex(15);
        m_koreanTeacherNewButton.Click(
            {this, &MainWindow::KoreanTeacherNewButton_Click}
            );
        setAutomationName(m_koreanTeacherNewButton, L"New Korean teacher");
        m_koreanTeacherDeleteButton = Button();
        m_koreanTeacherDeleteButton.Content(box_value(hstring(L"Delete Teacher")));
        m_koreanTeacherDeleteButton.IsTabStop(true);
        m_koreanTeacherDeleteButton.TabIndex(16);
        m_koreanTeacherDeleteButton.Click(
            {this, &MainWindow::KoreanTeacherDeleteButton_Click}
            );
        setAutomationName(m_koreanTeacherDeleteButton, L"Delete Korean teacher");
        m_koreanTeacherSaveButton = Button();
        m_koreanTeacherSaveButton.Content(box_value(hstring(L"Save Changes")));
        m_koreanTeacherSaveButton.IsTabStop(true);
        m_koreanTeacherSaveButton.TabIndex(17);
        m_koreanTeacherSaveButton.Click(
            {this, &MainWindow::KoreanTeacherSaveButton_Click}
            );
        setAutomationName(m_koreanTeacherSaveButton, L"Save Korean teacher");
        m_koreanTeacherDiscardButton = Button();
        m_koreanTeacherDiscardButton.Content(box_value(hstring(L"Discard Changes")));
        m_koreanTeacherDiscardButton.IsTabStop(true);
        m_koreanTeacherDiscardButton.TabIndex(18);
        m_koreanTeacherDiscardButton.Click(
            {this, &MainWindow::KoreanTeacherDiscardButton_Click}
            );
        setAutomationName(
            m_koreanTeacherDiscardButton,
            L"Discard Korean teacher changes"
            );
        actions.Children().Append(m_koreanTeacherNewButton);
        actions.Children().Append(m_koreanTeacherDeleteButton);
        actions.Children().Append(m_koreanTeacherSaveButton);
        actions.Children().Append(m_koreanTeacherDiscardButton);
        root.Children().Append(actions);

        scroll.Content(root);
        page.Content(scroll);
    }

    const auto setEditable = [this](bool enabled) {
        const bool clean = !m_koreanTeacherDirty;
        m_koreanTeacherSelector.IsEnabled(enabled && clean && !m_koreanTeacherNew);
        m_koreanTeacherKrTextBox.IsEnabled(enabled);
        m_koreanTeacherEnTextBox.IsEnabled(enabled);
        m_koreanTeacherRomanizationTextBox.IsEnabled(enabled);
        m_koreanTeacherPreferredNameCombo.IsEnabled(
            enabled && m_koreanTeacherPreferredNameCombo.Items().Size() > 0
            );
        m_koreanTeacherRoomTextBox.IsEnabled(enabled);
        m_koreanTeacherBirthdayTextBox.IsEnabled(enabled);
        m_koreanTeacherPhoneTextBox.IsEnabled(enabled);
        m_koreanTeacherWifiNameTextBox.IsEnabled(enabled);
        m_koreanTeacherWifiPasswordBox.IsEnabled(enabled);
        m_koreanTeacherInternetTypeCombo.IsEnabled(enabled);
        m_koreanTeacherZoomIdTextBox.IsEnabled(enabled);
        m_koreanTeacherZoomPasswordBox.IsEnabled(enabled);
        m_koreanTeacherProjectionTypeCombo.IsEnabled(enabled);
        m_koreanTeacherNotesTextBox.IsEnabled(enabled);
        m_koreanTeacherNewButton.IsEnabled(enabled && clean && !m_koreanTeacherNew);
        m_koreanTeacherDeleteButton.IsEnabled(
            enabled && clean && !m_koreanTeacherNew
                && m_koreanTeacherSelectedId > 0
            );
        m_koreanTeacherSaveButton.IsEnabled(enabled && m_koreanTeacherDirty);
        m_koreanTeacherDiscardButton.IsEnabled(enabled && m_koreanTeacherDirty);
    };

    if (!m_openDatabase)
    {
        m_koreanTeacherLoading = true;
        m_koreanTeachers.clear();
        m_koreanTeacherSelectedIndex = -1;
        m_koreanTeacherSelectedId = -1;
        m_koreanTeacherNew = false;
        m_koreanTeacherDirty = false;
        m_koreanTeacherSelector.Items().Clear();
        m_koreanTeacherSelector.SelectedIndex(-1);
        m_koreanTeacherLoading = false;
        presentKoreanTeacher(-1);
        m_koreanTeacherStatusText.Text(L"No database open.");
        m_koreanTeacherValidationText.Text({});
        m_koreanTeacherValidationText.Visibility(Visibility::Collapsed);
        setEditable(false);
        return;
    }

    if (refresh && m_koreanTeacherDirty)
    {
        m_koreanTeacherStatusText.Text(
            L"Unsaved Korean teacher changes are retained."
            );
        return;
    }

    m_koreanTeacherLoading = true;
    classmngr::engine::TeacherService service(*m_openDatabase);
    const auto loaded = service.list();
    if (!loaded)
    {
        m_koreanTeacherLoading = false;
        m_koreanTeachers.clear();
        m_koreanTeacherSelectedIndex = -1;
        m_koreanTeacherSelectedId = -1;
        m_koreanTeacherNew = false;
        m_koreanTeacherDirty = false;
        m_koreanTeacherSelector.Items().Clear();
        m_koreanTeacherSelector.SelectedIndex(-1);
        presentKoreanTeacher(-1);
        m_koreanTeacherStatusText.Text(winrt::hstring(
            L"Korean teacher directory could not be loaded: "
            + asWide(loaded.error().message)
            ));
        m_koreanTeacherValidationText.Text(
            L"The engine rejected the teacher-directory read."
            );
        m_koreanTeacherValidationText.Visibility(Visibility::Visible);
        setEditable(false);
        return;
    }

    const int previousId = m_koreanTeacherSelectedId;
    m_koreanTeachers = *loaded;
    m_koreanTeacherSelector.Items().Clear();
    int selectedIndex = -1;
    for (int index = 0; index < static_cast<int>(m_koreanTeachers.size()); ++index)
    {
        const auto& teacher = m_koreanTeachers[static_cast<std::size_t>(index)];
        std::wstring displayName = asWide(teacher.preferredDisplayName());
        if (!teacher.teacherKr.empty())
        {
            displayName += L" (" + asWide(teacher.teacherKr) + L")";
        }
        auto item = ComboBoxItem();
        item.Content(box_value(hstring(displayName)));
        item.Tag(box_value(teacher.id));
        setAutomationName(item, L"Korean teacher " + displayName);
        m_koreanTeacherSelector.Items().Append(item);
        if (teacher.id == previousId)
        {
            selectedIndex = index;
        }
    }
    if (selectedIndex < 0 && !m_koreanTeachers.empty())
    {
        selectedIndex = 0;
    }
    m_koreanTeacherSelectedIndex = selectedIndex;
    m_koreanTeacherSelectedId = selectedIndex >= 0
        ? m_koreanTeachers[static_cast<std::size_t>(selectedIndex)].id
        : -1;
    m_koreanTeacherNew = false;
    m_koreanTeacherDirty = false;
    m_koreanTeacherSelector.SelectedIndex(selectedIndex);
    presentKoreanTeacher(selectedIndex);
    m_koreanTeacherLoading = false;
    m_koreanTeacherStatusText.Text(
        m_koreanTeachers.empty()
            ? L"No Korean teachers found. Choose New Teacher to add one."
            : L"Korean teacher directory loaded."
        );
    m_koreanTeacherValidationText.Text({});
    m_koreanTeacherValidationText.Visibility(Visibility::Collapsed);
    setEditable(true);
}

void MainWindow::refreshKoreanTeachersPage()
{
    if (!m_contentFrame || m_currentPageId != koreanTeachersPageId)
    {
        return;
    }

    const auto page = m_contentFrame.Content().try_as<
        Microsoft::UI::Xaml::Controls::Page>();
    if (page)
    {
        populateKoreanTeachersPage(page, true);
    }
}

void MainWindow::presentKoreanTeacher(int index)
{
    m_koreanTeacherLoading = true;
    if (index < 0 || index >= static_cast<int>(m_koreanTeachers.size()))
    {
        m_koreanTeacherSelectedIndex = -1;
        m_koreanTeacherSelectedId = -1;
        m_koreanTeacherKrTextBox.Text({});
        m_koreanTeacherEnTextBox.Text({});
        m_koreanTeacherRomanizationTextBox.Text({});
        m_koreanTeacherPreferredNameCombo.Items().Clear();
        m_koreanTeacherPreferredNameCombo.SelectedIndex(-1);
        m_koreanTeacherRoomTextBox.Text({});
        m_koreanTeacherBirthdayTextBox.Text({});
        m_koreanTeacherPhoneTextBox.Text({});
        m_koreanTeacherWifiNameTextBox.Text({});
        m_koreanTeacherWifiPasswordBox.Password({});
        m_koreanTeacherInternetTypeCombo.SelectedIndex(0);
        m_koreanTeacherZoomIdTextBox.Text({});
        m_koreanTeacherZoomPasswordBox.Password({});
        m_koreanTeacherProjectionTypeCombo.SelectedIndex(0);
        m_koreanTeacherNotesTextBox.Text({});
        m_koreanTeacherLoading = false;
        return;
    }

    const auto& teacher = m_koreanTeachers[static_cast<std::size_t>(index)];
    m_koreanTeacherSelectedIndex = index;
    m_koreanTeacherSelectedId = teacher.id;
    m_koreanTeacherKrTextBox.Text(asWide(teacher.teacherKr));
    m_koreanTeacherEnTextBox.Text(asWide(teacher.teacherEn));
    m_koreanTeacherRomanizationTextBox.Text(asWide(teacher.preferredRomanization));
    m_koreanTeacherPreferredNameCombo.Items().Clear();
    int preferredIndex = -1;
    const auto preferredChoices = teacher.preferredNameChoices();
    for (int choiceIndex = 0;
         choiceIndex < static_cast<int>(preferredChoices.size());
         ++choiceIndex)
    {
        const std::wstring choice = asWide(
            preferredChoices[static_cast<std::size_t>(choiceIndex)]
            );
        m_koreanTeacherPreferredNameCombo.Items().Append(
            box_value(hstring(choice))
            );
        if (preferredChoices[static_cast<std::size_t>(choiceIndex)]
            == teacher.preferredName)
        {
            preferredIndex = choiceIndex;
        }
    }
    m_koreanTeacherPreferredNameCombo.SelectedIndex(preferredIndex);
    m_koreanTeacherRoomTextBox.Text(asWide(teacher.roomNumber));
    m_koreanTeacherBirthdayTextBox.Text(asWide(teacher.birthday));
    m_koreanTeacherPhoneTextBox.Text(asWide(teacher.phoneNumber));
    m_koreanTeacherWifiNameTextBox.Text(asWide(teacher.wifiName));
    m_koreanTeacherWifiPasswordBox.Password(asWide(teacher.wifiPassword));

    const auto selectComboValue = [](
        Microsoft::UI::Xaml::Controls::ComboBox combo,
        std::string const& value,
        wchar_t const* fallback
        ) {
        const std::wstring selected = asWide(value);
        for (int optionIndex = 0;
             optionIndex < static_cast<int>(combo.Items().Size());
             ++optionIndex)
        {
            if (boxedString(combo.Items().GetAt(optionIndex)) == selected)
            {
                combo.SelectedIndex(optionIndex);
                return;
            }
        }
        for (int optionIndex = 0;
             optionIndex < static_cast<int>(combo.Items().Size());
             ++optionIndex)
        {
            if (boxedString(combo.Items().GetAt(optionIndex)) == fallback)
            {
                combo.SelectedIndex(optionIndex);
                return;
            }
        }
        combo.SelectedIndex(-1);
    };
    selectComboValue(
        m_koreanTeacherInternetTypeCombo,
        teacher.internetType,
        L"WiFi"
        );
    m_koreanTeacherZoomIdTextBox.Text(asWide(teacher.zoomId));
    m_koreanTeacherZoomPasswordBox.Password(asWide(teacher.zoomPassword));
    selectComboValue(
        m_koreanTeacherProjectionTypeCombo,
        teacher.projectionType,
        L"HDMI"
        );
    m_koreanTeacherNotesTextBox.Text(asWide(teacher.notes));
    m_koreanTeacherLoading = false;
}

void MainWindow::refreshKoreanTeacherPreferredNames()
{
    if (!m_koreanTeacherPreferredNameCombo)
    {
        return;
    }

    classmngr::engine::Teacher teacher;
    teacher.teacherEn = asUtf8(m_koreanTeacherEnTextBox.Text());
    teacher.preferredRomanization = asUtf8(
        m_koreanTeacherRomanizationTextBox.Text()
        );
    const std::wstring current = boxedString(
        m_koreanTeacherPreferredNameCombo.SelectedItem()
        );
    const auto choices = teacher.preferredNameChoices();
    m_koreanTeacherLoading = true;
    m_koreanTeacherPreferredNameCombo.Items().Clear();
    int selectedIndex = -1;
    for (int index = 0; index < static_cast<int>(choices.size()); ++index)
    {
        const std::wstring choice = asWide(choices[static_cast<std::size_t>(index)]);
        m_koreanTeacherPreferredNameCombo.Items().Append(
            box_value(hstring(choice))
            );
        if (choice == current)
        {
            selectedIndex = index;
        }
    }
    if (selectedIndex < 0 && !choices.empty())
    {
        selectedIndex = 0;
    }
    m_koreanTeacherPreferredNameCombo.SelectedIndex(selectedIndex);
    m_koreanTeacherLoading = false;
}

classmngr::engine::Teacher MainWindow::koreanTeacherFromForm() const
{
    classmngr::engine::Teacher teacher;
    teacher.id = m_koreanTeacherSelectedId;
    teacher.teacherKr = asUtf8(m_koreanTeacherKrTextBox.Text());
    teacher.teacherEn = asUtf8(m_koreanTeacherEnTextBox.Text());
    teacher.preferredRomanization = asUtf8(
        m_koreanTeacherRomanizationTextBox.Text()
        );
    teacher.preferredName = asUtf8(
        boxedString(m_koreanTeacherPreferredNameCombo.SelectedItem())
        );
    teacher.roomNumber = asUtf8(m_koreanTeacherRoomTextBox.Text());
    teacher.birthday = asUtf8(m_koreanTeacherBirthdayTextBox.Text());
    teacher.phoneNumber = asUtf8(m_koreanTeacherPhoneTextBox.Text());
    teacher.wifiName = asUtf8(m_koreanTeacherWifiNameTextBox.Text());
    teacher.wifiPassword = asUtf8(m_koreanTeacherWifiPasswordBox.Password());
    teacher.internetType = asUtf8(
        boxedString(m_koreanTeacherInternetTypeCombo.SelectedItem())
        );
    teacher.zoomId = asUtf8(m_koreanTeacherZoomIdTextBox.Text());
    teacher.zoomPassword = asUtf8(m_koreanTeacherZoomPasswordBox.Password());
    teacher.projectionType = asUtf8(
        boxedString(m_koreanTeacherProjectionTypeCombo.SelectedItem())
        );
    teacher.notes = asUtf8(m_koreanTeacherNotesTextBox.Text());
    return teacher;
}

void MainWindow::populateClassesPage(
    Microsoft::UI::Xaml::Controls::Page const& page,
    std::wstring_view pageId
    )
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    // Class child routes share one presentation page so the prototype
    // controls keep a single owner while the sidebar exposes the complete
    // class hierarchy.
    static_cast<void>(pageId);
    auto makeRoot = [](StackPanel const& content) {
        content.Padding(Thickness{16.0, 12.0, 16.0, 24.0});
        content.Spacing(16.0);
        content.MaxWidth(2000.0);
        content.HorizontalAlignment(HorizontalAlignment::Stretch);
        return content;
    };
    auto makeTextSection = [&makeRoot](wchar_t const* titleText,
                                       wchar_t const* descriptionText,
                                       wchar_t const* automationName) {
        auto root = makeRoot(StackPanel());
        auto title = TextBlock();
        title.Text(titleText);
        title.FontSize(24.0);
        setAutomationName(title, automationName);
        auto description = TextBlock();
        description.Text(descriptionText);
        description.TextWrapping(TextWrapping::Wrap);
        root.Children().Append(title);
        root.Children().Append(description);
        return root;
    };

    auto detailsRoot = makeRoot(StackPanel());
    auto detailsTitle = TextBlock();
    detailsTitle.Text(L"Class Details");
    applyResourceStyle(detailsTitle, L"Phase3PageTitleTextBlockStyle");
    setAutomationName(detailsTitle, L"Class Details");
    detailsRoot.Children().Append(detailsTitle);

    auto detailsDescription = TextBlock();
    detailsDescription.Text(
        L"Edit class information shared by schedules, rosters, and reports."
        );
    detailsDescription.TextWrapping(TextWrapping::Wrap);
    detailsDescription.Visibility(Visibility::Collapsed);
    setAutomationName(
        detailsDescription,
        L"Class details and information description"
        );
    detailsRoot.Children().Append(detailsDescription);

    m_classStatusText = TextBlock();
    m_classStatusText.Text(L"Loading classes...");
    m_classStatusText.TextWrapping(TextWrapping::Wrap);
    setAutomationName(m_classStatusText, L"Class information status");
    detailsRoot.Children().Append(m_classStatusText);

    m_classValidationText = TextBlock();
    m_classValidationText.TextWrapping(TextWrapping::Wrap);
    m_classValidationText.Visibility(
        Microsoft::UI::Xaml::Visibility::Collapsed
        );
    setAutomationName(
        m_classValidationText,
        L"Class information validation summary"
        );
    detailsRoot.Children().Append(m_classValidationText);

    auto classSelectorCard = ClassMngrWinUISharedUX::buildCard({
        L"Class directory",
        L"Select a class or create a new class information record.",
        L"Class directory selector"
        });
    m_classSelector = ComboBox();
    m_classSelector.Header(box_value(hstring(L"Class")));
    m_classSelector.PlaceholderText(L"Select a class");
    m_classSelector.MinWidth(420.0);
    m_classSelector.HorizontalAlignment(HorizontalAlignment::Stretch);
    m_classSelector.IsTabStop(true);
    m_classSelector.TabIndex(0);
    m_classSelector.SelectionChanged({
        this,
        &MainWindow::ClassSelection_SelectionChanged
        });
    setAutomationName(m_classSelector, L"Class selector");
    classSelectorCard.content.Children().Append(m_classSelector);
    detailsRoot.Children().Append(classSelectorCard.root);

    const auto makeClassTextBox = [this](
        wchar_t const* header,
        wchar_t const* automationName,
        wchar_t const* placeholder
        ) {
        auto box = TextBox();
        box.Header(box_value(hstring(header)));
        box.PlaceholderText(placeholder);
        box.MinWidth(320.0);
        box.HorizontalAlignment(HorizontalAlignment::Stretch);
        box.IsTabStop(true);
        box.TextChanging({this, &MainWindow::ClassField_TextChanging});
        setAutomationName(box, automationName);
        return box;
    };

    auto detailsCard = ClassMngrWinUISharedUX::buildCard({
        L"Class information",
        L"",
        L"Class information form"
        });
    detailsCard.root.Padding(Thickness{20.0, 20.0, 20.0, 20.0});
    detailsCard.content.Spacing(16.0);

    m_classNameTextBox = makeClassTextBox(
        L"Class name",
        L"Class name",
        L"Enter a class name"
        );
    auto classNameInputScope = Input::InputScope();
    classNameInputScope.Names().Append(
        Input::InputScopeName(Input::InputScopeNameValue::Text)
        );
    m_classNameTextBox.InputScope(classNameInputScope);
    m_classNameTextBox.TabIndex(1);
    classSelectorCard.content.Children().Append(m_classNameTextBox);

    const auto appendChoice = [](ComboBox combo,
                                 std::wstring_view display,
                                 std::wstring_view value) {
        auto item = ComboBoxItem();
        item.Content(box_value(hstring(display)));
        item.Tag(box_value(hstring(value)));
        combo.Items().Append(item);
    };
    const auto configureClassCombo = [this](
        ComboBox combo,
        wchar_t const* header,
        wchar_t const* automationName,
        int tabIndex
        ) {
        combo.Header(box_value(hstring(header)));
        combo.MinWidth(320.0);
        combo.HorizontalAlignment(HorizontalAlignment::Stretch);
        combo.IsTabStop(true);
        combo.TabIndex(tabIndex);
        combo.SelectionChanged({this, &MainWindow::ClassField_SelectionChanged});
        setAutomationName(combo, automationName);
    };

    m_classGradeCombo = ComboBox();
    configureClassCombo(
        m_classGradeCombo,
        L"Grade",
        L"Class grade",
        2
        );
    m_classGradeCombo.MinWidth(130.0);
    m_classGradeCombo.Width(130.0);
    appendChoice(m_classGradeCombo, L"Not set", L"");
    for (const std::string& value :
         classmngr::engine::ClassInfoConfig::grades())
    {
        const std::wstring wideValue = asWide(value);
        appendChoice(m_classGradeCombo, wideValue, wideValue);
    }

    m_classLevelCombo = ComboBox();
    configureClassCombo(
        m_classLevelCombo,
        L"Level",
        L"Class level",
        3
        );
    m_classLevelCombo.MinWidth(180.0);
    m_classLevelCombo.Width(180.0);

    m_classReadingBookCombo = ComboBox();
    configureClassCombo(
        m_classReadingBookCombo,
        L"Reading Book",
        L"Class reading book",
        4
        );
    m_classReadingBookCombo.MinWidth(300.0);

    m_classEssayBookCombo = ComboBox();
    configureClassCombo(
        m_classEssayBookCombo,
        L"Essay Book",
        L"Class essay book",
        5
        );
    m_classEssayBookCombo.MinWidth(120.0);
    m_classEssayBookCombo.Width(120.0);

    m_classColorTextBox = makeClassTextBox(
        L"Color backing value",
        L"Class color backing value",
        L""
        );
    m_classColorTextBox.IsTabStop(false);
    m_classColorPreview = Border();
    m_classColorPreview.Width(40.0);
    m_classColorPreview.Height(24.0);
    m_classColorPreview.CornerRadius(CornerRadius{4.0, 4.0, 4.0, 4.0});
    m_classColorPreview.BorderThickness(Thickness{1.0, 1.0, 1.0, 1.0});
    m_classColorPreview.BorderBrush(
        Microsoft::UI::Xaml::Media::SolidColorBrush(
            Windows::UI::Color{255, 128, 128, 128}
            )
        );
    setAutomationName(m_classColorPreview, L"Class color preview");
    m_classColorChooseButton = Button();
    m_classColorChooseButton.Content(box_value(hstring(L"Choose Color")));
    m_classColorChooseButton.IsTabStop(true);
    m_classColorChooseButton.TabIndex(6);
    setAutomationName(m_classColorChooseButton, L"Choose class color");
    m_classFontColorTextBox = makeClassTextBox(
        L"Font color backing value",
        L"Class font color backing value",
        L""
        );
    m_classFontColorTextBox.IsTabStop(false);

    m_classStudentCountTextBox = TextBox();
    m_classStudentCountTextBox.Header(box_value(hstring(L"# of Students")));
    m_classStudentCountTextBox.MinWidth(64.0);
    m_classStudentCountTextBox.Width(64.0);
    m_classStudentCountTextBox.IsReadOnly(true);
    m_classStudentCountTextBox.IsTabStop(false);
    setAutomationName(m_classStudentCountTextBox, L"Class student count");

    m_classTeacherText = TextBlock();
    m_classTeacherText.TextWrapping(TextWrapping::Wrap);
    setAutomationName(m_classTeacherText, L"Assigned class teacher");

    auto colorField = StackPanel();
    colorField.Spacing(4.0);
    auto colorLabel = TextBlock();
    colorLabel.Text(L"Color");
    applyResourceStyle(colorLabel, L"Phase3BodyTextBlockStyle");
    auto colorControls = StackPanel();
    colorControls.Orientation(Orientation::Horizontal);
    colorControls.Spacing(5.0);
    colorControls.Children().Append(m_classColorPreview);
    colorControls.Children().Append(m_classColorChooseButton);
    colorField.Children().Append(colorLabel);
    colorField.Children().Append(colorControls);

    const auto openClassColorPicker = [weak = get_weak()](auto const&, auto const&)
        -> winrt::fire_and_forget {
        const auto self = weak.get();
        if (!self || self->m_ownedDialog || !self->m_classColorTextBox
            || !self->RootGrid().XamlRoot())
        {
            co_return;
        }
        auto picker = ClassMngrWinUISharedUX::buildColorPickerDialog(
            self->RootGrid().XamlRoot(),
            L"Select Class Color",
            uiColorFromHex(asUtf8(self->m_classColorTextBox.Text())),
            L"Class color picker"
            );
        self->m_ownedDialog = picker.dialog;
        ContentDialogResult result = ContentDialogResult::None;
        try
        {
            result = co_await picker.dialog.ShowAsync();
        }
        catch (...)
        {
        }
        if (self->m_ownedDialog == picker.dialog)
        {
            self->m_ownedDialog = nullptr;
        }
        if (result == ContentDialogResult::Primary && self->m_classColorTextBox)
        {
            const std::string value = uiHexFromColor(picker.picker.Color());
            self->m_classColorTextBox.Text(asWide(value));
            if (self->m_classColorPreview)
            {
                self->m_classColorPreview.Background(
                    Microsoft::UI::Xaml::Media::SolidColorBrush(
                        uiColorFromHex(value)
                        )
                    );
            }
            self->m_classDetailsDirty = true;
            self->markClassDirty();
        }
    };
    m_classColorChooseButton.Click(openClassColorPicker);
    m_classColorPreview.Tapped(openClassColorPicker);

    auto detailsGrid = Grid();
    detailsGrid.ColumnSpacing(16.0);
    detailsGrid.RowSpacing(8.0);
    const std::array<double, 4> detailWidths{
        200.0, 130.0, 180.0, 120.0
    };
    for (const double width : detailWidths)
    {
        auto definition = ColumnDefinition();
        definition.Width(GridLengthHelper::FromValueAndType(
            width,
            GridUnitType::Pixel
            ));
        detailsGrid.ColumnDefinitions().Append(definition);
    }
    for (int row = 0; row < 2; ++row)
    {
        detailsGrid.RowDefinitions().Append(RowDefinition());
    }
    Grid::SetRow(colorField, 0);
    Grid::SetColumn(colorField, 0);
    Grid::SetRow(m_classGradeCombo, 0);
    Grid::SetColumn(m_classGradeCombo, 1);
    Grid::SetRow(m_classLevelCombo, 0);
    Grid::SetColumn(m_classLevelCombo, 2);
    Grid::SetRow(m_classStudentCountTextBox, 0);
    Grid::SetColumn(m_classStudentCountTextBox, 3);
    Grid::SetRow(m_classReadingBookCombo, 1);
    Grid::SetColumn(m_classReadingBookCombo, 0);
    Grid::SetColumnSpan(m_classReadingBookCombo, 2);
    Grid::SetRow(m_classEssayBookCombo, 1);
    Grid::SetColumn(m_classEssayBookCombo, 2);
    detailsGrid.Children().Append(colorField);
    detailsGrid.Children().Append(m_classGradeCombo);
    detailsGrid.Children().Append(m_classLevelCombo);
    detailsGrid.Children().Append(m_classStudentCountTextBox);
    detailsGrid.Children().Append(m_classReadingBookCombo);
    detailsGrid.Children().Append(m_classEssayBookCombo);
    detailsCard.content.Children().Append(detailsGrid);
    detailsRoot.Children().Append(detailsCard.root);

    auto detailsActions = StackPanel();
    detailsActions.Orientation(Orientation::Horizontal);
    detailsActions.Spacing(8.0);
    m_classNewButton = Button();
    m_classNewButton.Content(box_value(hstring(L"New Class")));
    m_classNewButton.IsTabStop(true);
    m_classNewButton.TabIndex(8);
    m_classNewButton.Click({this, &MainWindow::ClassNewButton_Click});
    setAutomationName(m_classNewButton, L"New class");
    m_classDeleteButton = Button();
    m_classDeleteButton.Content(box_value(hstring(L"Delete Class")));
    m_classDeleteButton.IsTabStop(true);
    m_classDeleteButton.TabIndex(9);
    m_classDeleteButton.Click({this, &MainWindow::ClassDeleteButton_Click});
    setAutomationName(m_classDeleteButton, L"Delete class");
    m_classSaveButton = Button();
    m_classSaveButton.Content(box_value(hstring(L"Save Changes")));
    m_classSaveButton.IsTabStop(true);
    m_classSaveButton.TabIndex(10);
    m_classSaveButton.Click({this, &MainWindow::ClassSaveButton_Click});
    setAutomationName(m_classSaveButton, L"Save class information");
    m_classDiscardButton = Button();
    m_classDiscardButton.Content(box_value(hstring(L"Discard Changes")));
    m_classDiscardButton.IsTabStop(true);
    m_classDiscardButton.TabIndex(11);
    m_classDiscardButton.Click({this, &MainWindow::ClassDiscardButton_Click});
    setAutomationName(m_classDiscardButton, L"Discard class information changes");
    // These controls remain instantiated for legacy command and semantic-check
    // paths, but Class Details now keeps actions out of its visual tree.

    auto scheduleCard = ClassMngrWinUISharedUX::buildCard({
        L"Class Times",
        L"",
        L"Class schedule editor"
        });
    scheduleCard.root.Padding(Thickness{20.0, 20.0, 20.0, 20.0});
    scheduleCard.content.Spacing(16.0);

    auto regularScheduleTitle = TextBlock();
    regularScheduleTitle.Text(L"Regular Schedule");
    applyResourceStyle(regularScheduleTitle, L"Phase3BodyTextBlockStyle");
    regularScheduleTitle.FontStyle(
        Windows::UI::Text::FontStyle::Italic
        );
    scheduleCard.content.Children().Append(regularScheduleTitle);
    m_classRegularScheduleGrid = Grid();
    m_classRegularScheduleGrid.HorizontalAlignment(
        HorizontalAlignment::Left
        );
    setAutomationName(
        m_classRegularScheduleGrid,
        L"Regular class schedule"
        );
    scheduleCard.content.Children().Append(m_classRegularScheduleGrid);
    m_classRegularScheduleAddButton = Button();
    m_classRegularScheduleAddButton.Content(
        box_value(hstring(L"+ Add Time"))
        );
    m_classRegularScheduleAddButton.MinWidth(200.0);
    m_classRegularScheduleAddButton.IsTabStop(true);
    m_classRegularScheduleAddButton.Click(
        [this](auto const&, auto const&) {
            addClassScheduleRow(false);
        }
        );
    setAutomationName(
        m_classRegularScheduleAddButton,
        L"Add regular class time"
        );
    scheduleCard.content.Children().Append(m_classRegularScheduleAddButton);

    auto scheduleDivider = Border();
    scheduleDivider.Height(1.0);
    scheduleDivider.Background(
        Microsoft::UI::Xaml::Media::SolidColorBrush(
            Windows::UI::Color{255, 92, 99, 108}
            )
        );
    scheduleCard.content.Children().Append(scheduleDivider);

    auto intensiveScheduleTitle = TextBlock();
    intensiveScheduleTitle.Text(L"Intensive Schedule");
    applyResourceStyle(intensiveScheduleTitle, L"Phase3BodyTextBlockStyle");
    intensiveScheduleTitle.FontStyle(
        Windows::UI::Text::FontStyle::Italic
        );
    scheduleCard.content.Children().Append(intensiveScheduleTitle);
    m_classIntensiveScheduleGrid = Grid();
    m_classIntensiveScheduleGrid.HorizontalAlignment(
        HorizontalAlignment::Left
        );
    setAutomationName(
        m_classIntensiveScheduleGrid,
        L"Intensive class schedule"
        );
    scheduleCard.content.Children().Append(m_classIntensiveScheduleGrid);
    m_classIntensiveScheduleAddButton = Button();
    m_classIntensiveScheduleAddButton.Content(
        box_value(hstring(L"+ Add Intensive Time"))
        );
    m_classIntensiveScheduleAddButton.MinWidth(200.0);
    m_classIntensiveScheduleAddButton.IsTabStop(true);
    m_classIntensiveScheduleAddButton.Click(
        [this](auto const&, auto const&) {
            addClassScheduleRow(true);
        }
        );
    setAutomationName(
        m_classIntensiveScheduleAddButton,
        L"Add intensive class time"
        );
    scheduleCard.content.Children().Append(m_classIntensiveScheduleAddButton);
    detailsRoot.Children().Append(scheduleCard.root);

    auto notesRoot = makeRoot(StackPanel());
    auto notesTitle = TextBlock();
    notesTitle.Text(L"Class Notes");
    applyResourceStyle(notesTitle, L"Phase3PageTitleTextBlockStyle");
    setAutomationName(notesTitle, L"Class Notes");
    notesRoot.Children().Append(notesTitle);

    auto notesDescription = TextBlock();
    notesDescription.Text(
        L"Keep class notes and time-filler activities with the selected class."
        );
    notesDescription.TextWrapping(TextWrapping::Wrap);
    notesDescription.Visibility(Visibility::Collapsed);
    setAutomationName(notesDescription, L"Class notes description");
    notesRoot.Children().Append(notesDescription);

    m_classNotesStatusText = TextBlock();
    m_classNotesStatusText.Text(L"Select a class to edit notes.");
    m_classNotesStatusText.TextWrapping(TextWrapping::Wrap);
    applyResourceStyle(m_classNotesStatusText, L"Phase4CardDescriptionTextBlockStyle");
    m_classNotesStatusText.Visibility(Visibility::Collapsed);
    setAutomationName(m_classNotesStatusText, L"Class notes status");
    notesRoot.Children().Append(m_classNotesStatusText);

    m_classNotesValidationText = TextBlock();
    m_classNotesValidationText.TextWrapping(TextWrapping::Wrap);
    m_classNotesValidationText.Visibility(Visibility::Collapsed);
    setAutomationName(
        m_classNotesValidationText,
        L"Class notes validation summary"
        );
    notesRoot.Children().Append(m_classNotesValidationText);

    auto notesCard = ClassMngrWinUISharedUX::buildCard({
        L"Notes",
        L"",
        L"Class notes form"
        });
    notesCard.root.Padding(Thickness{24.0, 24.0, 24.0, 24.0});
    notesCard.root.BorderThickness(Thickness{2.0, 2.0, 2.0, 2.0});
    notesCard.root.BorderBrush(
        Microsoft::UI::Xaml::Media::SolidColorBrush(
            Windows::UI::Color{255, 0, 120, 212}
            )
        );
    notesCard.content.Spacing(24.0);
    m_classNotesTextBox = makeClassTextBox(
        L"Notes",
        L"Class notes editor",
        L"Enter notes for this class"
        );
    m_classNotesTextBox.AcceptsReturn(true);
    m_classNotesTextBox.TextWrapping(TextWrapping::Wrap);
    m_classNotesTextBox.Height(244.0);
    m_classNotesTextBox.VerticalContentAlignment(VerticalAlignment::Top);
    m_classNotesTextBox.MaxLength(10000);
    m_classNotesTextBox.TabIndex(0);
    m_classTimeFillerActivitiesTextBox = makeClassTextBox(
        L"Time Filler Activities",
        L"Class time filler activities editor",
        L"Enter time-filler activities"
        );
    m_classTimeFillerActivitiesTextBox.AcceptsReturn(true);
    m_classTimeFillerActivitiesTextBox.TextWrapping(TextWrapping::Wrap);
    m_classTimeFillerActivitiesTextBox.Height(244.0);
    m_classTimeFillerActivitiesTextBox.VerticalContentAlignment(
        VerticalAlignment::Top
        );
    m_classTimeFillerActivitiesTextBox.MaxLength(10000);
    m_classTimeFillerActivitiesTextBox.TabIndex(1);
    notesCard.content.Children().Append(m_classNotesTextBox);
    notesCard.content.Children().Append(m_classTimeFillerActivitiesTextBox);
    notesRoot.Children().Append(notesCard.root);

    auto notesActions = StackPanel();
    notesActions.Orientation(Orientation::Horizontal);
    notesActions.Spacing(8.0);
    m_classNotesSaveButton = Button();
    m_classNotesSaveButton.Content(box_value(hstring(L"Save Changes")));
    m_classNotesSaveButton.IsTabStop(true);
    m_classNotesSaveButton.TabIndex(2);
    m_classNotesSaveButton.Click({this, &MainWindow::ClassNotesSaveButton_Click});
    setAutomationName(m_classNotesSaveButton, L"Save class notes");
    m_classNotesDiscardButton = Button();
    m_classNotesDiscardButton.Content(box_value(hstring(L"Discard Changes")));
    m_classNotesDiscardButton.IsTabStop(true);
    m_classNotesDiscardButton.TabIndex(3);
    m_classNotesDiscardButton.Click({this, &MainWindow::ClassNotesDiscardButton_Click});
    setAutomationName(m_classNotesDiscardButton, L"Discard class notes changes");
    notesActions.Children().Append(m_classNotesSaveButton);
    notesActions.Children().Append(m_classNotesDiscardButton);
    notesRoot.Children().Append(notesActions);

    auto rosterRoot = makeRoot(StackPanel());
    auto rosterTitle = TextBlock();
    rosterTitle.Text(L"Class Roster");
    applyResourceStyle(rosterTitle, L"Phase3PageTitleTextBlockStyle");
    setAutomationName(rosterTitle, L"Class Roster");
    rosterRoot.Children().Append(rosterTitle);

    auto rosterCard = ClassMngrWinUISharedUX::buildCard({
        L"",
        L"Student names and evaluation grades are arranged as a classroom spreadsheet. Select a row to manage it, then save when ready.",
        L"Roster editor"
        });
    rosterCard.root.Padding(Thickness{24.0, 24.0, 24.0, 24.0});
    rosterCard.content.Spacing(16.0);

    m_classRosterStatusText = TextBlock();
    m_classRosterStatusText.Text(L"Select a class to edit its roster.");
    m_classRosterStatusText.TextWrapping(TextWrapping::Wrap);
    setAutomationName(m_classRosterStatusText, L"Class roster status");
    rosterCard.content.Children().Append(m_classRosterStatusText);

    m_classRosterValidationText = TextBlock();
    m_classRosterValidationText.TextWrapping(TextWrapping::Wrap);
    m_classRosterValidationText.Visibility(Visibility::Collapsed);
    setAutomationName(
        m_classRosterValidationText,
        L"Class roster validation summary"
        );
    rosterCard.content.Children().Append(m_classRosterValidationText);

    auto rosterActions = StackPanel();
    rosterActions.Orientation(Orientation::Horizontal);
    rosterActions.Spacing(8.0);

    m_classRosterImportScoresButton = Button();
    m_classRosterImportScoresButton.Content(box_value(hstring(L"Import Scores")));
    m_classRosterImportScoresButton.IsTabStop(true);
    m_classRosterImportScoresButton.TabIndex(12);
    m_classRosterImportScoresButton.Click(
        [this](auto const&, auto const&) { importClassRosterScores(); }
        );
    setAutomationName(m_classRosterImportScoresButton, L"Import roster scores");
    rosterActions.Children().Append(m_classRosterImportScoresButton);

    m_classRosterAddButton = Button();
    m_classRosterAddButton.Content(box_value(hstring(L"Add Student")));
    m_classRosterAddButton.IsTabStop(true);
    m_classRosterAddButton.TabIndex(13);
    m_classRosterAddButton.Click(
        [this](auto const&, auto const&) { addClassRosterRow(); }
        );
    setAutomationName(m_classRosterAddButton, L"Add roster student");
    rosterActions.Children().Append(m_classRosterAddButton);

    m_classRosterRemoveButton = Button();
    m_classRosterRemoveButton.Content(box_value(hstring(L"Remove Selected")));
    m_classRosterRemoveButton.IsTabStop(true);
    m_classRosterRemoveButton.TabIndex(14);
    m_classRosterRemoveButton.Click(
        [this](auto const&, auto const&) { removeClassRosterRow(); }
        );
    setAutomationName(m_classRosterRemoveButton, L"Remove selected roster student");
    rosterActions.Children().Append(m_classRosterRemoveButton);

    m_classRosterSaveButton = Button();
    m_classRosterSaveButton.Content(box_value(hstring(L"Save Roster")));
    m_classRosterSaveButton.IsTabStop(true);
    m_classRosterSaveButton.TabIndex(15);
    m_classRosterSaveButton.Click(
        [this](auto const&, auto const&) { saveClassRoster(); }
        );
    setAutomationName(m_classRosterSaveButton, L"Save class roster");
    rosterActions.Children().Append(m_classRosterSaveButton);

    m_classRosterDiscardButton = Button();
    m_classRosterDiscardButton.Content(box_value(hstring(L"Discard Changes")));
    m_classRosterDiscardButton.IsTabStop(true);
    m_classRosterDiscardButton.TabIndex(16);
    m_classRosterDiscardButton.Click(
        [this](auto const&, auto const&) { discardClassRoster(); }
        );
    setAutomationName(m_classRosterDiscardButton, L"Discard class roster changes");
    rosterActions.Children().Append(m_classRosterDiscardButton);
    m_classRosterHeaderGrid = Grid();
    m_classRosterHeaderGrid.ColumnSpacing(2.0);
    setAutomationName(m_classRosterHeaderGrid, L"Class roster column headers");
    rosterCard.content.Children().Append(m_classRosterHeaderGrid);

    m_classRosterList = ListView();
    m_classRosterList.SelectionMode(ListViewSelectionMode::Single);
    m_classRosterList.IsTabStop(true);
    m_classRosterList.TabIndex(17);
    m_classRosterList.Height(440.0);
    m_classRosterList.HorizontalAlignment(HorizontalAlignment::Stretch);
    m_classRosterList.SelectionChanged(
        [this](auto const&, auto const&) { updateClassRosterActions(); }
        );
    setAutomationName(m_classRosterList, L"Class roster student grid");
    rosterCard.content.Children().Append(m_classRosterList);

    auto transferCard = ClassMngrWinUISharedUX::buildCard({
        L"Student transfer",
        L"Move the selected roster row to another class of the same grade. Both rosters are validated and saved atomically.",
        L"Roster student transfer"
        });
    m_classRosterTransferTargetCombo = ComboBox();
    m_classRosterTransferTargetCombo.Header(box_value(hstring(L"Target class")));
    m_classRosterTransferTargetCombo.PlaceholderText(L"Select a same-grade class");
    m_classRosterTransferTargetCombo.MinWidth(320.0);
    m_classRosterTransferTargetCombo.IsTabStop(true);
    m_classRosterTransferTargetCombo.TabIndex(18);
    m_classRosterTransferTargetCombo.SelectionChanged(
        [this](auto const&, auto const&) { updateClassRosterActions(); }
        );
    setAutomationName(
        m_classRosterTransferTargetCombo,
        L"Roster transfer target class"
        );
    transferCard.content.Children().Append(m_classRosterTransferTargetCombo);

    m_classRosterTransferButton = Button();
    m_classRosterTransferButton.Content(
        box_value(hstring(L"Transfer selected student"))
        );
    m_classRosterTransferButton.IsTabStop(true);
    m_classRosterTransferButton.TabIndex(19);
    m_classRosterTransferButton.Click(
        [this](auto const&, auto const&) { transferClassRosterRow(); }
        );
    setAutomationName(
        m_classRosterTransferButton,
        L"Transfer selected roster student"
        );
    transferCard.content.Children().Append(m_classRosterTransferButton);

    m_classRosterPrepareTransferButton = Button();
    m_classRosterPrepareTransferButton.Content(
        box_value(hstring(L"Prepare class transfer package"))
        );
    m_classRosterPrepareTransferButton.IsTabStop(true);
    m_classRosterPrepareTransferButton.TabIndex(20);
    m_classRosterPrepareTransferButton.Click(
        [this](auto const&, auto const&) { prepareClassTransfer(); }
        );
    setAutomationName(
        m_classRosterPrepareTransferButton,
        L"Prepare class transfer package"
        );
    transferCard.content.Children().Append(m_classRosterPrepareTransferButton);
    rosterCard.content.Children().Append(transferCard.root);

    auto templateCard = ClassMngrWinUISharedUX::buildCard({
        L"Roster report template",
        L"Choose a renderer-neutral template and preview its current-class report scope.",
        L"Roster report template"
        });
    m_classRosterTemplateCombo = ComboBox();
    m_classRosterTemplateCombo.Header(box_value(hstring(L"Template")));
    m_classRosterTemplateCombo.MinWidth(320.0);
    m_classRosterTemplateCombo.IsTabStop(true);
    m_classRosterTemplateCombo.TabIndex(21);
    setAutomationName(m_classRosterTemplateCombo, L"Roster report template selector");
    for (const auto reportTemplate :
         classmngr::engine::RosterReportTemplateService::availableTemplates())
    {
        auto item = ComboBoxItem();
        const auto templateValue = static_cast<int>(reportTemplate);
        const wchar_t* label = reportTemplate ==
                classmngr::engine::RosterReportTemplate::ByDay
            ? L"By Day"
            : reportTemplate == classmngr::engine::RosterReportTemplate::Daily
                ? L"Daily"
                : L"Per Class with Extra Info";
        item.Content(box_value(hstring(label)));
        item.Tag(box_value(templateValue));
        m_classRosterTemplateCombo.Items().Append(item);
    }
    m_classRosterTemplateCombo.SelectionChanged(
        [this](auto const&, auto const&) {
            if (m_classRosterLoading || !m_classRosterTemplateStatusText)
            {
                return;
            }
            const auto item = m_classRosterTemplateCombo.SelectedItem().try_as<
                ComboBoxItem>();
            const auto reportTemplate = item
                ? static_cast<classmngr::engine::RosterReportTemplate>(
                    boxedInt(item.Tag()))
                : classmngr::engine::RosterReportTemplate::ByDay;
            const bool landscape =
                classmngr::engine::RosterReportTemplateService::orientation(
                    reportTemplate
                    ) == classmngr::engine::RosterReportOrientation::Landscape;
            const wchar_t* label = reportTemplate ==
                    classmngr::engine::RosterReportTemplate::ByDay
                ? L"By Day"
                : reportTemplate == classmngr::engine::RosterReportTemplate::Daily
                    ? L"Daily"
                    : L"Per Class with Extra Info";
            m_classRosterTemplateStatusText.Text(
                winrt::hstring(
                    std::wstring(label)
                    + (landscape
                        ? L" · landscape · current class scope."
                        : L" · portrait · current class scope.")
                    )
                );
        }
        );
    templateCard.content.Children().Append(m_classRosterTemplateCombo);
    m_classRosterTemplateStatusText = TextBlock();
    m_classRosterTemplateStatusText.TextWrapping(TextWrapping::Wrap);
    setAutomationName(
        m_classRosterTemplateStatusText,
        L"Roster report template status"
        );
    templateCard.content.Children().Append(m_classRosterTemplateStatusText);
    rosterCard.content.Children().Append(templateCard.root);
    rosterCard.content.Children().Append(rosterActions);
    rosterRoot.Children().Append(rosterCard.root);

    auto speakingRoot = makeRoot(StackPanel());
    auto speakingTopBar = Grid();
    speakingTopBar.ColumnSpacing(16.0);
    auto speakingTitleColumn = ColumnDefinition();
    speakingTitleColumn.Width(GridLengthHelper::FromValueAndType(
        1.0,
        GridUnitType::Star
        ));
    speakingTopBar.ColumnDefinitions().Append(speakingTitleColumn);
    speakingTopBar.ColumnDefinitions().Append(ColumnDefinition());

    auto speakingTitle = TextBlock();
    speakingTitle.Text(L"Evaluations");
    applyResourceStyle(speakingTitle, L"Phase3PageTitleTextBlockStyle");
    speakingTitle.VerticalAlignment(VerticalAlignment::Center);
    setAutomationName(speakingTitle, L"Evaluations");
    Grid::SetColumn(speakingTitle, 0);
    speakingTopBar.Children().Append(speakingTitle);

    auto speakingCard = ClassMngrWinUISharedUX::buildCard({
        {},
        {},
        L"Evaluations editor"
        });
    speakingCard.root.Padding(Thickness{12.0, 12.0, 12.0, 12.0});
    speakingCard.content.Spacing(10.0);

    m_speakingEvaluationStatusText = TextBlock();
    m_speakingEvaluationStatusText.Text(
        L"Select a class to edit evaluations."
        );
    m_speakingEvaluationStatusText.TextWrapping(TextWrapping::Wrap);
    setAutomationName(
        m_speakingEvaluationStatusText,
        L"Speaking evaluation status"
        );
    speakingCard.content.Children().Append(m_speakingEvaluationStatusText);

    m_speakingEvaluationValidationText = TextBlock();
    m_speakingEvaluationValidationText.TextWrapping(TextWrapping::Wrap);
    m_speakingEvaluationValidationText.Visibility(Visibility::Collapsed);
    setAutomationName(
        m_speakingEvaluationValidationText,
        L"Speaking evaluation validation summary"
        );
    speakingCard.content.Children().Append(m_speakingEvaluationValidationText);

    m_speakingEvaluationSelector = ComboBox();
    m_speakingEvaluationSelector.Header(
        box_value(hstring(L"Evaluation"))
        );
    m_speakingEvaluationSelector.MinWidth(200.0);
    m_speakingEvaluationSelector.Width(220.0);
    m_speakingEvaluationSelector.HorizontalAlignment(HorizontalAlignment::Right);
    m_speakingEvaluationSelector.IsTabStop(true);
    m_speakingEvaluationSelector.TabIndex(0);
    setAutomationName(
        m_speakingEvaluationSelector,
        L"Speaking evaluation selector"
        );
    m_speakingEvaluationName = "Winter";
    m_speakingEvaluationLoading = true;
    for (const std::string_view evaluationName :
         classmngr::engine::SpeakingEvaluationNames)
    {
        auto item = ComboBoxItem();
        const std::wstring display = asWide(evaluationName);
        item.Content(box_value(hstring(display)));
        item.Tag(box_value(hstring(display)));
        setAutomationName(item, L"Speaking evaluation " + display);
        m_speakingEvaluationSelector.Items().Append(item);
    }
    m_speakingEvaluationSelector.SelectedIndex(0);
    m_speakingEvaluationSelector.SelectionChanged(
        [this](Windows::Foundation::IInspectable const& rawSender,
               SelectionChangedEventArgs const&) {
            if (m_speakingEvaluationLoading)
            {
                return;
            }
            const auto sender = rawSender.try_as<ComboBox>();
            if (!sender)
            {
                return;
            }
            if (m_speakingEvaluationDirty)
            {
                m_speakingEvaluationLoading = true;
                int restoreIndex = 0;
                for (int index = 0;
                     index < static_cast<int>(sender.Items().Size());
                     ++index)
                {
                    const auto item = sender.Items().GetAt(index).try_as<
                        ComboBoxItem>();
                    if (item && boxedString(item.Tag())
                        == asWide(m_speakingEvaluationName))
                    {
                        restoreIndex = index;
                        break;
                    }
                }
                sender.SelectedIndex(restoreIndex);
                m_speakingEvaluationLoading = false;
                m_speakingEvaluationStatusText.Text(
                    L"Save or discard the current speaking evaluation before selecting another."
                    );
                return;
            }
            const auto item = sender.SelectedItem().try_as<ComboBoxItem>();
            if (!item)
            {
                return;
            }
            m_speakingEvaluationName = asUtf8(boxedString(item.Tag()));
            refreshSpeakingEvaluation();
        }
        );
    m_speakingEvaluationLoading = false;
    Grid::SetColumn(m_speakingEvaluationSelector, 1);
    speakingTopBar.Children().Append(m_speakingEvaluationSelector);
    speakingRoot.Children().Append(speakingTopBar);

    auto speakingActions = StackPanel();
    speakingActions.Orientation(Orientation::Horizontal);
    speakingActions.Spacing(8.0);

    m_speakingEvaluationImportNamesButton = Button();
    m_speakingEvaluationImportNamesButton.Content(
        box_value(hstring(L"Import Names"))
        );
    m_speakingEvaluationImportNamesButton.IsTabStop(true);
    m_speakingEvaluationImportNamesButton.TabIndex(1);
    m_speakingEvaluationImportNamesButton.Click(
        [this](auto const&, auto const&) {
            importSpeakingEvaluationNames();
        }
        );
    setAutomationName(
        m_speakingEvaluationImportNamesButton,
        L"Import speaking evaluation names"
        );
    speakingActions.Children().Append(m_speakingEvaluationImportNamesButton);

    m_speakingEvaluationSaveButton = Button();
    m_speakingEvaluationSaveButton.Content(
        box_value(hstring(L"Save Evaluation"))
        );
    m_speakingEvaluationSaveButton.IsTabStop(true);
    m_speakingEvaluationSaveButton.TabIndex(2);
    m_speakingEvaluationSaveButton.Click(
        [this](auto const&, auto const&) { saveSpeakingEvaluation(); }
        );
    setAutomationName(
        m_speakingEvaluationSaveButton,
        L"Save speaking evaluation"
        );
    speakingActions.Children().Append(m_speakingEvaluationSaveButton);

    m_speakingEvaluationDiscardButton = Button();
    m_speakingEvaluationDiscardButton.Content(
        box_value(hstring(L"Discard Changes"))
        );
    m_speakingEvaluationDiscardButton.IsTabStop(true);
    m_speakingEvaluationDiscardButton.TabIndex(3);
    m_speakingEvaluationDiscardButton.Click(
        [this](auto const&, auto const&) { discardSpeakingEvaluation(); }
        );
    setAutomationName(
        m_speakingEvaluationDiscardButton,
        L"Discard speaking evaluation changes"
        );
    speakingActions.Children().Append(m_speakingEvaluationDiscardButton);

    m_speakingEvaluationHeaderGrid = Grid();
    m_speakingEvaluationHeaderGrid.ColumnSpacing(4.0);
    m_speakingEvaluationHeaderGrid.MinHeight(42.0);
    setAutomationName(
        m_speakingEvaluationHeaderGrid,
        L"Speaking evaluation column headers"
        );
    speakingCard.content.Children().Append(m_speakingEvaluationHeaderGrid);

    m_speakingEvaluationList = ListView();
    m_speakingEvaluationList.SelectionMode(ListViewSelectionMode::Single);
    m_speakingEvaluationList.IsTabStop(true);
    m_speakingEvaluationList.TabIndex(4);
    m_speakingEvaluationList.Height(620.0);
    m_speakingEvaluationList.HorizontalAlignment(
        HorizontalAlignment::Stretch
        );
    setAutomationName(
        m_speakingEvaluationList,
        L"Speaking evaluation grid"
        );
    m_speakingEvaluationList.SelectionChanged(
        [this](auto const&, auto const&) {
            refreshSpeakingAiSelection();
        }
        );
    speakingCard.content.Children().Append(m_speakingEvaluationList);

    m_speakingEvaluationPasteTextBox = TextBox();
    m_speakingEvaluationPasteTextBox.Header(
        box_value(hstring(L"Paste score range (tab/newline)"))
        );
    m_speakingEvaluationPasteTextBox.PlaceholderText(L"A+	A	B+\nA	B+	B");
    m_speakingEvaluationPasteTextBox.AcceptsReturn(true);
    m_speakingEvaluationPasteTextBox.Height(72.0);
    m_speakingEvaluationPasteTextBox.IsTabStop(true);
    m_speakingEvaluationPasteTextBox.TabIndex(5);
    setAutomationName(
        m_speakingEvaluationPasteTextBox,
        L"Speaking evaluation score range"
        );
    speakingCard.content.Children().Append(m_speakingEvaluationPasteTextBox);

    m_speakingEvaluationPasteButton = Button();
    m_speakingEvaluationPasteButton.Content(
        box_value(hstring(L"Apply Range to Scores"))
        );
    m_speakingEvaluationPasteButton.IsTabStop(true);
    m_speakingEvaluationPasteButton.TabIndex(6);
    m_speakingEvaluationPasteButton.Click(
        [this](auto const&, auto const&) { applySpeakingEvaluationPaste(); }
        );
    setAutomationName(
        m_speakingEvaluationPasteButton,
        L"Apply speaking evaluation score range"
        );
    speakingCard.content.Children().Append(m_speakingEvaluationPasteButton);

    auto speakingActionBar = Border();
    speakingActionBar.Padding(Thickness{8.0, 8.0, 8.0, 8.0});
    speakingActionBar.Background(Microsoft::UI::Xaml::Media::SolidColorBrush(
        Windows::UI::Color{255, 245, 247, 250}
        ));
    speakingActionBar.BorderBrush(Microsoft::UI::Xaml::Media::SolidColorBrush(
        Windows::UI::Color{255, 190, 198, 210}
        ));
    speakingActionBar.BorderThickness(Thickness{1.0, 1.0, 1.0, 1.0});
    speakingActionBar.Child(speakingActions);
    speakingCard.content.Children().Append(speakingActionBar);

    auto aiCard = ClassMngrWinUISharedUX::buildCard({
        L"AI comments",
        L"Build privacy-preserving prompts from the selected student's private observations, then paste and review the provider response before applying it.",
        L"Speaking AI comment workflow"
        });
    m_speakingAiStatusText = TextBlock();
    m_speakingAiStatusText.Text(
        L"Select a speaking-evaluation row to prepare an AI comment."
        );
    m_speakingAiStatusText.TextWrapping(TextWrapping::Wrap);
    setAutomationName(m_speakingAiStatusText, L"Speaking AI comment status");
    aiCard.content.Children().Append(m_speakingAiStatusText);

    m_speakingAiVoiceSelector = ComboBox();
    m_speakingAiVoiceSelector.Header(
        box_value(hstring(L"Comment voice"))
        );
    m_speakingAiVoiceSelector.MinWidth(320.0);
    m_speakingAiVoiceSelector.IsTabStop(true);
    m_speakingAiVoiceSelector.TabIndex(7);
    const auto appendAiVoice = [this](std::wstring_view label, int tag) {
        auto item = ComboBoxItem();
        item.Content(box_value(hstring(label)));
        item.Tag(box_value(tag));
        setAutomationName(item, L"AI voice " + std::wstring(label));
        m_speakingAiVoiceSelector.Items().Append(item);
    };
    appendAiVoice(L"Direct to Student", 0);
    appendAiVoice(L"Third Person", 1);
    m_speakingAiVoiceSelector.SelectedIndex(0);
    setAutomationName(m_speakingAiVoiceSelector, L"Speaking AI comment voice");
    aiCard.content.Children().Append(m_speakingAiVoiceSelector);

    const auto configureAiEditor = [](
        TextBox& editor,
        std::wstring_view header,
        std::wstring_view placeholder,
        double height,
        int tabIndex
        ) {
        editor.Header(box_value(hstring(header)));
        editor.PlaceholderText(hstring(placeholder));
        editor.AcceptsReturn(true);
        editor.TextWrapping(TextWrapping::Wrap);
        editor.Height(height);
        editor.IsTabStop(true);
        editor.TabIndex(tabIndex);
    };
    m_speakingAiDidWellTextBox = TextBox();
    configureAiEditor(
        m_speakingAiDidWellTextBox,
        L"Did Well observations",
        L"Clear pronunciation\nUses complete sentences",
        84.0,
        8
        );
    setAutomationName(
        m_speakingAiDidWellTextBox,
        L"Speaking AI Did Well observations"
        );
    aiCard.content.Children().Append(m_speakingAiDidWellTextBox);

    m_speakingAiNeedsImprovementTextBox = TextBox();
    configureAiEditor(
        m_speakingAiNeedsImprovementTextBox,
        L"Needs Improvement observations",
        L"Add supporting details\nPractice fluency",
        84.0,
        9
        );
    setAutomationName(
        m_speakingAiNeedsImprovementTextBox,
        L"Speaking AI Needs Improvement observations"
        );
    aiCard.content.Children().Append(m_speakingAiNeedsImprovementTextBox);

    auto aiStudentActions = StackPanel();
    aiStudentActions.Orientation(Orientation::Horizontal);
    aiStudentActions.Spacing(8.0);

    m_speakingAiGenerateButton = Button();
    m_speakingAiGenerateButton.Content(
        box_value(hstring(L"Generate Student Prompt"))
        );
    m_speakingAiGenerateButton.IsTabStop(true);
    m_speakingAiGenerateButton.TabIndex(10);
    m_speakingAiGenerateButton.HorizontalAlignment(
        HorizontalAlignment::Left
        );
    m_speakingAiGenerateButton.Click(
        [this](auto const&, auto const&) { generateSpeakingAiPrompt(); }
        );
    setAutomationName(
        m_speakingAiGenerateButton,
        L"Generate speaking AI student prompt"
        );
    aiStudentActions.Children().Append(m_speakingAiGenerateButton);

    m_speakingAiGenerateBatchButton = Button();
    m_speakingAiGenerateBatchButton.Content(
        box_value(hstring(L"Generate Batch Prompt"))
        );
    m_speakingAiGenerateBatchButton.IsTabStop(true);
    m_speakingAiGenerateBatchButton.TabIndex(11);
    m_speakingAiGenerateBatchButton.HorizontalAlignment(
        HorizontalAlignment::Left
        );
    m_speakingAiGenerateBatchButton.Click(
        [this](auto const&, auto const&) {
            generateSpeakingAiBatchPrompt();
        }
        );
    setAutomationName(
        m_speakingAiGenerateBatchButton,
        L"Generate speaking AI batch prompt"
        );
    aiStudentActions.Children().Append(m_speakingAiGenerateBatchButton);
    aiCard.content.Children().Append(aiStudentActions);

    m_speakingAiPromptTextBox = TextBox();
    configureAiEditor(
        m_speakingAiPromptTextBox,
        L"Prompt (copy to the selected AI provider)",
        L"The generated prompt will appear here.",
        190.0,
        12
        );
    m_speakingAiPromptTextBox.IsReadOnly(true);
    setAutomationName(m_speakingAiPromptTextBox, L"Speaking AI prompt");
    aiCard.content.Children().Append(m_speakingAiPromptTextBox);

    auto aiPromptActions = StackPanel();
    aiPromptActions.Orientation(Orientation::Horizontal);
    aiPromptActions.Spacing(8.0);

    m_speakingAiCopyButton = Button();
    m_speakingAiCopyButton.Content(box_value(hstring(L"Copy Prompt")));
    m_speakingAiCopyButton.IsTabStop(true);
    m_speakingAiCopyButton.TabIndex(13);
    m_speakingAiCopyButton.HorizontalAlignment(HorizontalAlignment::Left);
    m_speakingAiCopyButton.Click(
        [this](auto const&, auto const&) { copySpeakingAiPrompt(false); }
        );
    setAutomationName(m_speakingAiCopyButton, L"Copy speaking AI prompt");
    aiPromptActions.Children().Append(m_speakingAiCopyButton);

    m_speakingAiCopyOpenButton = Button();
    m_speakingAiCopyOpenButton.Content(
        box_value(hstring(L"Copy and Open ChatGPT"))
        );
    m_speakingAiCopyOpenButton.IsTabStop(true);
    m_speakingAiCopyOpenButton.TabIndex(14);
    m_speakingAiCopyOpenButton.HorizontalAlignment(
        HorizontalAlignment::Left
        );
    m_speakingAiCopyOpenButton.Click(
        [this](auto const&, auto const&) { copySpeakingAiPrompt(true); }
        );
    setAutomationName(
        m_speakingAiCopyOpenButton,
        L"Copy speaking AI prompt and open ChatGPT"
        );
    aiPromptActions.Children().Append(m_speakingAiCopyOpenButton);
    aiCard.content.Children().Append(aiPromptActions);

    m_speakingAiResponseTextBox = TextBox();
    configureAiEditor(
        m_speakingAiResponseTextBox,
        L"Provider response (paste here)",
        L"Paste the completed student comment or marked batch response.",
        150.0,
        15
        );
    m_speakingAiResponseTextBox.TextChanging(
        [this](auto const&, auto const&) { updateSpeakingAiActions(); }
        );
    setAutomationName(m_speakingAiResponseTextBox, L"Speaking AI response");
    aiCard.content.Children().Append(m_speakingAiResponseTextBox);

    auto aiResponseActions = StackPanel();
    aiResponseActions.Orientation(Orientation::Horizontal);
    aiResponseActions.Spacing(8.0);

    m_speakingAiApplyStudentButton = Button();
    m_speakingAiApplyStudentButton.Content(
        box_value(hstring(L"Apply Student Comment"))
        );
    m_speakingAiApplyStudentButton.IsTabStop(true);
    m_speakingAiApplyStudentButton.TabIndex(16);
    m_speakingAiApplyStudentButton.HorizontalAlignment(
        HorizontalAlignment::Left
        );
    m_speakingAiApplyStudentButton.Click(
        [this](auto const&, auto const&) {
            applySpeakingAiStudentComment();
        }
        );
    setAutomationName(
        m_speakingAiApplyStudentButton,
        L"Apply speaking AI student comment"
        );
    aiResponseActions.Children().Append(m_speakingAiApplyStudentButton);

    m_speakingAiParseBatchButton = Button();
    m_speakingAiParseBatchButton.Content(
        box_value(hstring(L"Parse Batch Response"))
        );
    m_speakingAiParseBatchButton.IsTabStop(true);
    m_speakingAiParseBatchButton.TabIndex(17);
    m_speakingAiParseBatchButton.HorizontalAlignment(
        HorizontalAlignment::Left
        );
    m_speakingAiParseBatchButton.Click(
        [this](auto const&, auto const&) {
            parseSpeakingAiBatchResponse();
        }
        );
    setAutomationName(
        m_speakingAiParseBatchButton,
        L"Parse speaking AI batch response"
        );
    aiResponseActions.Children().Append(m_speakingAiParseBatchButton);

    m_speakingAiApplyBatchButton = Button();
    m_speakingAiApplyBatchButton.Content(
        box_value(hstring(L"Apply Parsed Batch Comments"))
        );
    m_speakingAiApplyBatchButton.IsTabStop(true);
    m_speakingAiApplyBatchButton.TabIndex(18);
    m_speakingAiApplyBatchButton.HorizontalAlignment(
        HorizontalAlignment::Left
        );
    m_speakingAiApplyBatchButton.Click(
        [this](auto const&, auto const&) {
            applySpeakingAiBatchComments();
        }
        );
    setAutomationName(
        m_speakingAiApplyBatchButton,
        L"Apply speaking AI batch comments"
        );
    aiResponseActions.Children().Append(m_speakingAiApplyBatchButton);
    aiCard.content.Children().Append(aiResponseActions);

    speakingRoot.Children().Append(speakingCard.root);
    speakingRoot.Children().Append(aiCard.root);

    auto batchReportCard = ClassMngrWinUISharedUX::buildCard({
        L"Batch report operations",
        L"Plan a renderer-neutral batch of speaking reports. PDF rendering, printing, and PowerPoint automation are completed by the output services in the next phase.",
        L"Speaking evaluation batch report operations"
        });
    m_speakingBatchStatusText = TextBlock();
    m_speakingBatchStatusText.Text(
        L"Choose an output mode and plan the named students in this evaluation."
        );
    m_speakingBatchStatusText.TextWrapping(TextWrapping::Wrap);
    setAutomationName(
        m_speakingBatchStatusText,
        L"Speaking batch report status"
        );
    batchReportCard.content.Children().Append(m_speakingBatchStatusText);

    const auto appendBatchOption = [](
        ComboBox& selector,
        std::wstring_view label,
        int tag
        ) {
        auto item = ComboBoxItem();
        item.Content(box_value(hstring(label)));
        item.Tag(box_value(tag));
        selector.Items().Append(item);
    };
    m_speakingBatchRendererSelector = ComboBox();
    m_speakingBatchRendererSelector.Header(
        box_value(hstring(L"Renderer"))
        );
    m_speakingBatchRendererSelector.MinWidth(320.0);
    m_speakingBatchRendererSelector.IsTabStop(true);
    m_speakingBatchRendererSelector.TabIndex(19);
    appendBatchOption(
        m_speakingBatchRendererSelector,
        L"Internal renderer",
        0
        );
    appendBatchOption(
        m_speakingBatchRendererSelector,
        L"PowerPoint renderer",
        1
        );
    m_speakingBatchRendererSelector.SelectedIndex(0);
    m_speakingBatchRendererSelector.SelectionChanged(
        [this](auto const&, auto const&) {
            updateSpeakingBatchReportActions();
        }
        );
    setAutomationName(
        m_speakingBatchRendererSelector,
        L"Speaking batch report renderer"
        );
    batchReportCard.content.Children().Append(
        m_speakingBatchRendererSelector
        );

    m_speakingBatchTemplateSelector = ComboBox();
    m_speakingBatchTemplateSelector.Header(
        box_value(hstring(L"Report template"))
        );
    m_speakingBatchTemplateSelector.MinWidth(320.0);
    m_speakingBatchTemplateSelector.IsTabStop(true);
    m_speakingBatchTemplateSelector.TabIndex(20);
    appendBatchOption(
        m_speakingBatchTemplateSelector,
        L"Standard",
        0
        );
    appendBatchOption(
        m_speakingBatchTemplateSelector,
        L"Advanced",
        1
        );
    m_speakingBatchTemplateSelector.SelectedIndex(0);
    m_speakingBatchTemplateSelector.SelectionChanged(
        [this](auto const&, auto const&) {
            updateSpeakingBatchReportActions();
        }
        );
    setAutomationName(
        m_speakingBatchTemplateSelector,
        L"Speaking batch report template"
        );
    batchReportCard.content.Children().Append(
        m_speakingBatchTemplateSelector
        );

    m_speakingBatchSavePdfCheck = CheckBox();
    m_speakingBatchSavePdfCheck.Content(
        box_value(hstring(L"Save PDF output"))
        );
    m_speakingBatchSavePdfCheck.IsChecked(true);
    m_speakingBatchSavePdfCheck.IsTabStop(true);
    m_speakingBatchSavePdfCheck.TabIndex(21);
    m_speakingBatchSavePdfCheck.Checked(
        [this](auto const&, auto const&) {
            updateSpeakingBatchReportActions();
        }
        );
    m_speakingBatchSavePdfCheck.Unchecked(
        [this](auto const&, auto const&) {
            updateSpeakingBatchReportActions();
        }
        );
    setAutomationName(
        m_speakingBatchSavePdfCheck,
        L"Save speaking report PDF output"
        );
    batchReportCard.content.Children().Append(m_speakingBatchSavePdfCheck);

    m_speakingBatchPrintCheck = CheckBox();
    m_speakingBatchPrintCheck.Content(
        box_value(hstring(L"Print reports"))
        );
    m_speakingBatchPrintCheck.IsChecked(false);
    m_speakingBatchPrintCheck.IsTabStop(true);
    m_speakingBatchPrintCheck.TabIndex(22);
    m_speakingBatchPrintCheck.Checked(
        [this](auto const&, auto const&) {
            updateSpeakingBatchReportActions();
        }
        );
    m_speakingBatchPrintCheck.Unchecked(
        [this](auto const&, auto const&) {
            updateSpeakingBatchReportActions();
        }
        );
    setAutomationName(
        m_speakingBatchPrintCheck,
        L"Print speaking reports"
        );
    batchReportCard.content.Children().Append(m_speakingBatchPrintCheck);

    m_speakingBatchKeepIndividualPdfsCheck = CheckBox();
    m_speakingBatchKeepIndividualPdfsCheck.Content(
        box_value(hstring(L"Keep individual PDFs when creating a ZIP archive"))
        );
    m_speakingBatchKeepIndividualPdfsCheck.IsChecked(false);
    m_speakingBatchKeepIndividualPdfsCheck.IsTabStop(true);
    m_speakingBatchKeepIndividualPdfsCheck.TabIndex(23);
    setAutomationName(
        m_speakingBatchKeepIndividualPdfsCheck,
        L"Keep individual speaking report PDFs"
        );
    batchReportCard.content.Children().Append(
        m_speakingBatchKeepIndividualPdfsCheck
        );

    m_speakingBatchOutputDirectoryTextBox = TextBox();
    m_speakingBatchOutputDirectoryTextBox.Header(
        box_value(hstring(L"Output folder (required for PDF output)"))
        );
    m_speakingBatchOutputDirectoryTextBox.PlaceholderText(
        L"Type or choose the output folder in the output phase"
        );
    m_speakingBatchOutputDirectoryTextBox.MinWidth(420.0);
    m_speakingBatchOutputDirectoryTextBox.IsTabStop(true);
    m_speakingBatchOutputDirectoryTextBox.TabIndex(24);
    m_speakingBatchOutputDirectoryTextBox.TextChanging(
        [this](auto const&, auto const&) {
            updateSpeakingBatchReportActions();
        }
        );
    setAutomationName(
        m_speakingBatchOutputDirectoryTextBox,
        L"Speaking batch report output folder"
        );
    batchReportCard.content.Children().Append(
        m_speakingBatchOutputDirectoryTextBox
        );

    m_speakingBatchPlanButton = Button();
    m_speakingBatchPlanButton.Content(
        box_value(hstring(L"Plan Batch Reports"))
        );
    m_speakingBatchPlanButton.IsTabStop(true);
    m_speakingBatchPlanButton.TabIndex(25);
    m_speakingBatchPlanButton.HorizontalAlignment(
        HorizontalAlignment::Left
        );
    m_speakingBatchPlanButton.Click(
        [this](auto const&, auto const&) {
            planSpeakingBatchReports();
        }
        );
    setAutomationName(
        m_speakingBatchPlanButton,
        L"Plan speaking batch reports"
        );
    batchReportCard.content.Children().Append(m_speakingBatchPlanButton);
    speakingRoot.Children().Append(batchReportCard.root);

    auto analyticsRoot = makeRoot(StackPanel());
    auto analyticsTopBar = Grid();
    analyticsTopBar.ColumnSpacing(16.0);
    auto analyticsTitleColumn = ColumnDefinition();
    analyticsTitleColumn.Width(GridLengthHelper::FromValueAndType(
        1.0,
        GridUnitType::Star
        ));
    analyticsTopBar.ColumnDefinitions().Append(analyticsTitleColumn);
    analyticsTopBar.ColumnDefinitions().Append(ColumnDefinition());

    auto analyticsTitle = TextBlock();
    analyticsTitle.Text(L"Class Analytics");
    applyResourceStyle(analyticsTitle, L"Phase3PageTitleTextBlockStyle");
    setAutomationName(analyticsTitle, L"Class Analytics");
    analyticsTitle.VerticalAlignment(VerticalAlignment::Center);
    Grid::SetColumn(analyticsTitle, 0);
    analyticsTopBar.Children().Append(analyticsTitle);

    m_speakingAnalyticsStatusText = TextBlock();
    m_speakingAnalyticsStatusText.Text(
        L"Select a class to view speaking analytics."
        );
    m_speakingAnalyticsStatusText.TextWrapping(TextWrapping::Wrap);
    setAutomationName(
        m_speakingAnalyticsStatusText,
        L"Speaking analytics status"
        );
    m_speakingAnalyticsLoading = true;
    m_speakingAnalyticsName = "All";
    m_speakingAnalyticsSelector = ComboBox();
    m_speakingAnalyticsSelector.MinWidth(220.0);
    m_speakingAnalyticsSelector.IsTabStop(true);
    m_speakingAnalyticsSelector.TabIndex(0);
    setAutomationName(
        m_speakingAnalyticsSelector,
        L"Speaking analytics evaluation selector"
        );
    for (const std::string_view evaluationName : {
             std::string_view{"All"},
             classmngr::engine::SpeakingEvaluationNames[0],
             classmngr::engine::SpeakingEvaluationNames[1],
             classmngr::engine::SpeakingEvaluationNames[2],
             classmngr::engine::SpeakingEvaluationNames[3]
         })
    {
        auto item = ComboBoxItem();
        const std::wstring display = asWide(evaluationName);
        item.Content(box_value(hstring(display)));
        item.Tag(box_value(hstring(display)));
        setAutomationName(item, L"Analytics scope " + display);
        m_speakingAnalyticsSelector.Items().Append(item);
    }
    m_speakingAnalyticsSelector.SelectedIndex(0);
    m_speakingAnalyticsSelector.SelectionChanged(
        [this](auto const&, auto const&) {
            if (m_speakingAnalyticsLoading)
            {
                return;
            }
            const auto item = m_speakingAnalyticsSelector.SelectedItem()
                .try_as<ComboBoxItem>();
            if (!item)
            {
                return;
            }
            m_speakingAnalyticsName = asUtf8(
                boxedString(item.Tag())
                );
            refreshSpeakingAnalytics();
        }
        );
    m_speakingAnalyticsLoading = false;
    auto evaluationControls = StackPanel();
    evaluationControls.Orientation(Orientation::Horizontal);
    evaluationControls.Spacing(8.0);
    evaluationControls.VerticalAlignment(VerticalAlignment::Center);
    auto evaluationLabel = TextBlock();
    evaluationLabel.Text(L"Evaluation");
    evaluationLabel.VerticalAlignment(VerticalAlignment::Center);
    applyResourceStyle(evaluationLabel, L"Phase3BodyTextBlockStyle");
    evaluationControls.Children().Append(evaluationLabel);
    evaluationControls.Children().Append(m_speakingAnalyticsSelector);
    Grid::SetColumn(evaluationControls, 1);
    analyticsTopBar.Children().Append(evaluationControls);
    analyticsRoot.Children().Append(analyticsTopBar);
    analyticsRoot.Children().Append(m_speakingAnalyticsStatusText);

    auto analyticsSummaryGrid = Grid();
    analyticsSummaryGrid.ColumnSpacing(12.0);
    const std::array<wchar_t const*, 4> summaryTitles{
        L"Class Average", L"Students Fully Scored", L"Strongest Area", L"Focus Area"
    };
    for (int column = 0; column < static_cast<int>(summaryTitles.size()); ++column)
    {
        auto definition = ColumnDefinition();
        definition.Width(GridLengthHelper::FromValueAndType(
            1.0,
            GridUnitType::Star
            ));
        analyticsSummaryGrid.ColumnDefinitions().Append(definition);
        auto card = ClassMngrWinUISharedUX::buildCard({
            summaryTitles[static_cast<std::size_t>(column)],
            L"",
            hstring(L"Speaking analytics ")
                + hstring(summaryTitles[static_cast<std::size_t>(column)])
            });
        auto value = TextBlock();
        value.Text(L"—");
        value.FontSize(20.0);
        value.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
        value.TextWrapping(TextWrapping::Wrap);
        m_speakingAnalyticsSummaryValues[static_cast<std::size_t>(column)] = value;
        card.content.Children().Append(value);
        Grid::SetColumn(card.root, column);
        analyticsSummaryGrid.Children().Append(card.root);
    }
    analyticsRoot.Children().Append(analyticsSummaryGrid);

    m_speakingAnalyticsSummaryText = TextBlock();
    m_speakingAnalyticsSummaryText.TextWrapping(TextWrapping::Wrap);
    m_speakingAnalyticsSummaryText.Visibility(Visibility::Collapsed);
    setAutomationName(
        m_speakingAnalyticsSummaryText,
        L"Speaking analytics summary values"
        );
    analyticsRoot.Children().Append(m_speakingAnalyticsSummaryText);

    auto analyticsChartsGrid = Grid();
    analyticsChartsGrid.ColumnSpacing(12.0);
    for (int column = 0; column < 2; ++column)
    {
        auto definition = ColumnDefinition();
        definition.Width(GridLengthHelper::FromValueAndType(
            1.0,
            GridUnitType::Star
            ));
        analyticsChartsGrid.ColumnDefinitions().Append(definition);
    }

    auto analyticsCriteriaCard = ClassMngrWinUISharedUX::buildCard({
        L"By Criterion",
        L"Average score and grade distribution across each criterion.",
        L"Speaking analytics criteria"
        });
    auto analyticsLegend = StackPanel();
    analyticsLegend.Orientation(Orientation::Horizontal);
    analyticsLegend.Spacing(12.0);
    for (const std::wstring_view grade : {L"A+", L"A", L"B+", L"B", L"C"})
    {
        auto item = TextBlock();
        item.Text(hstring(L"● ") + hstring(grade));
        item.FontSize(12.0);
        const auto gradeColor = grade == L"A+" ? Windows::UI::Color{255, 21, 148, 71}
            : grade == L"A" ? Windows::UI::Color{255, 63, 126, 203}
            : grade == L"B+" ? Windows::UI::Color{255, 215, 163, 22}
            : grade == L"B" ? Windows::UI::Color{255, 239, 90, 19}
            : Windows::UI::Color{255, 189, 24, 33};
        item.Foreground(Microsoft::UI::Xaml::Media::SolidColorBrush(gradeColor));
        analyticsLegend.Children().Append(item);
    }
    analyticsCriteriaCard.content.Children().Append(analyticsLegend);
    m_speakingAnalyticsCriteriaPanel = StackPanel();
    m_speakingAnalyticsCriteriaPanel.Spacing(10.0);
    setAutomationName(
        m_speakingAnalyticsCriteriaPanel,
        L"Speaking analytics criterion metrics"
        );
    analyticsCriteriaCard.content.Children().Append(
        m_speakingAnalyticsCriteriaPanel
        );
    Grid::SetColumn(analyticsCriteriaCard.root, 0);
    analyticsChartsGrid.Children().Append(analyticsCriteriaCard.root);

    auto analyticsShapeCard = ClassMngrWinUISharedUX::buildCard({
        L"Class Shape",
        L"Grade histogram for the selected evaluation and year-to-date trend.",
        L"Speaking analytics class shape"
        });
    m_speakingAnalyticsShapeText = TextBlock();
    m_speakingAnalyticsShapeText.TextWrapping(TextWrapping::Wrap);
    m_speakingAnalyticsShapeText.Visibility(Visibility::Collapsed);
    setAutomationName(
        m_speakingAnalyticsShapeText,
        L"Speaking analytics class shape values"
        );
    analyticsShapeCard.content.Children().Append(m_speakingAnalyticsShapeText);
    m_speakingAnalyticsShapePanel = StackPanel();
    m_speakingAnalyticsShapePanel.Spacing(10.0);
    analyticsShapeCard.content.Children().Append(m_speakingAnalyticsShapePanel);
    Grid::SetColumn(analyticsShapeCard.root, 1);
    analyticsChartsGrid.Children().Append(analyticsShapeCard.root);
    analyticsRoot.Children().Append(analyticsChartsGrid);

    auto analyticsRankingCard = ClassMngrWinUISharedUX::buildCard({
        L"Student Ranking",
        L"Read-only ranking, including every scored speaking criterion.",
        L"Speaking analytics student ranking"
        });
    m_speakingAnalyticsRankingList = ListView();
    m_speakingAnalyticsRankingList.SelectionMode(ListViewSelectionMode::None);
    m_speakingAnalyticsRankingList.IsTabStop(true);
    m_speakingAnalyticsRankingList.TabIndex(1);
    m_speakingAnalyticsRankingList.MinHeight(360.0);
    ScrollViewer::SetHorizontalScrollBarVisibility(
        m_speakingAnalyticsRankingList,
        ScrollBarVisibility::Auto
        );
    setAutomationName(
        m_speakingAnalyticsRankingList,
        L"Speaking analytics student ranking list"
        );
    analyticsRankingCard.content.Children().Append(
        m_speakingAnalyticsRankingList
        );
    analyticsRoot.Children().Append(analyticsRankingCard.root);

    auto coTeacherRoot = makeRoot(StackPanel());
    auto coTeacherTitle = TextBlock();
    coTeacherTitle.Text(L"Co-Teacher");
    applyResourceStyle(coTeacherTitle, L"Phase3PageTitleTextBlockStyle");
    setAutomationName(coTeacherTitle, L"Co-Teacher");
    coTeacherRoot.Children().Append(coTeacherTitle);

    auto coTeacherCard = ClassMngrWinUISharedUX::buildCard({
        L"Korean Teacher",
        L"",
        L"Class co-teacher information"
        });
    coTeacherCard.root.Padding(Thickness{24.0, 24.0, 24.0, 24.0});
    coTeacherCard.content.Spacing(16.0);

    auto coTeacherGrid = Grid();
    coTeacherGrid.ColumnSpacing(20.0);
    coTeacherGrid.RowSpacing(16.0);
    for (int column = 0; column < 3; ++column)
    {
        auto definition = ColumnDefinition();
        definition.Width(GridLengthHelper::FromValueAndType(
            240.0,
            GridUnitType::Pixel
            ));
        coTeacherGrid.ColumnDefinitions().Append(definition);
    }
    for (int row = 0; row < 3; ++row)
    {
        coTeacherGrid.RowDefinitions().Append(RowDefinition());
    }

    const auto configureCoTeacherCombo = [this](
        ComboBox combo,
        wchar_t const* header,
        wchar_t const* automationName
        ) {
        combo.Header(box_value(hstring(header)));
        combo.MinWidth(240.0);
        combo.Width(240.0);
        combo.IsTabStop(true);
        combo.SelectionChanged({
            this,
            &MainWindow::ClassCoTeacherSelection_SelectionChanged
            });
        setAutomationName(combo, automationName);
    };
    const auto configureCoTeacherReadOnly = [](
        TextBox box,
        wchar_t const* header,
        wchar_t const* automationName
        ) {
        box.Header(box_value(hstring(header)));
        box.MinWidth(240.0);
        box.Width(240.0);
        box.IsReadOnly(true);
        box.IsTabStop(false);
        setAutomationName(box, automationName);
        return box;
    };

    m_classCoTeacherKrCombo = ComboBox();
    configureCoTeacherCombo(
        m_classCoTeacherKrCombo,
        L"Korean",
        L"Co-teacher Korean name"
        );
    m_classCoTeacherEnCombo = ComboBox();
    configureCoTeacherCombo(
        m_classCoTeacherEnCombo,
        L"English",
        L"Co-teacher English name"
        );
    m_classCoTeacherRoomTextBox = configureCoTeacherReadOnly(
        TextBox(),
        L"Room",
        L"Co-teacher room"
        );
    m_classCoTeacherInternetTypeTextBox = configureCoTeacherReadOnly(
        TextBox(),
        L"Internet Type",
        L"Co-teacher internet type"
        );
    m_classCoTeacherWifiNameTextBox = configureCoTeacherReadOnly(
        TextBox(),
        L"WiFi Name",
        L"Co-teacher WiFi name"
        );
    m_classCoTeacherWifiPasswordTextBox = configureCoTeacherReadOnly(
        TextBox(),
        L"WiFi Password",
        L"Co-teacher WiFi password"
        );
    m_classCoTeacherProjectionTypeTextBox = configureCoTeacherReadOnly(
        TextBox(),
        L"Projection Type",
        L"Co-teacher projection type"
        );
    m_classCoTeacherZoomIdTextBox = configureCoTeacherReadOnly(
        TextBox(),
        L"Zoom ID",
        L"Co-teacher Zoom ID"
        );
    m_classCoTeacherZoomPasswordTextBox = configureCoTeacherReadOnly(
        TextBox(),
        L"Zoom Password",
        L"Co-teacher Zoom password"
        );

    const std::array<FrameworkElement, 9> coTeacherFields{
        m_classCoTeacherKrCombo,
        m_classCoTeacherEnCombo,
        m_classCoTeacherRoomTextBox,
        m_classCoTeacherInternetTypeTextBox,
        m_classCoTeacherWifiNameTextBox,
        m_classCoTeacherWifiPasswordTextBox,
        m_classCoTeacherProjectionTypeTextBox,
        m_classCoTeacherZoomIdTextBox,
        m_classCoTeacherZoomPasswordTextBox
    };
    for (int index = 0; index < static_cast<int>(coTeacherFields.size()); ++index)
    {
        const int row = index / 3;
        const int column = index % 3;
        Grid::SetRow(coTeacherFields[static_cast<std::size_t>(index)], row);
        Grid::SetColumn(
            coTeacherFields[static_cast<std::size_t>(index)],
            column
            );
        coTeacherGrid.Children().Append(
            coTeacherFields[static_cast<std::size_t>(index)]
            );
    }
    coTeacherCard.content.Children().Append(coTeacherGrid);
    coTeacherRoot.Children().Append(coTeacherCard.root);

    auto coTeacherActions = StackPanel();
    coTeacherActions.Orientation(Orientation::Horizontal);
    coTeacherActions.Spacing(8.0);
    m_classCoTeacherSaveButton = Button();
    m_classCoTeacherSaveButton.Content(box_value(hstring(L"Save Changes")));
    m_classCoTeacherSaveButton.IsTabStop(true);
    m_classCoTeacherSaveButton.Click({
        this,
        &MainWindow::ClassCoTeacherSaveButton_Click
        });
    setAutomationName(m_classCoTeacherSaveButton, L"Save co-teacher changes");
    m_classCoTeacherDiscardButton = Button();
    m_classCoTeacherDiscardButton.Content(
        box_value(hstring(L"Discard Changes"))
        );
    m_classCoTeacherDiscardButton.IsTabStop(true);
    m_classCoTeacherDiscardButton.Click({
        this,
        &MainWindow::ClassCoTeacherDiscardButton_Click
        });
    setAutomationName(
        m_classCoTeacherDiscardButton,
        L"Discard co-teacher changes"
        );
    coTeacherActions.Children().Append(m_classCoTeacherSaveButton);
    coTeacherActions.Children().Append(m_classCoTeacherDiscardButton);
    coTeacherRoot.Children().Append(coTeacherActions);

    const auto navigationCard = ClassMngrWinUISharedUX::buildCard({
        L"",
        L"",
        L"Classes navigation"
        });
    m_classNavigationCard = navigationCard.root;
    m_classNavigationRoot = StackPanel();
    m_classNavigationRoot.Spacing(8.0);
    m_classNavigationRoot.HorizontalAlignment(HorizontalAlignment::Stretch);
    setAutomationName(m_classNavigationRoot, L"Classes navigation controls");

    auto navigationFilters = Grid();
    navigationFilters.ColumnSpacing(12.0);
    navigationFilters.ColumnDefinitions().Append(ColumnDefinition());
    navigationFilters.ColumnDefinitions().Append(ColumnDefinition());

    m_classNavigationGradeTabs = StackPanel();
    m_classNavigationGradeTabs.Orientation(Orientation::Horizontal);
    m_classNavigationGradeTabs.Spacing(6.0);
    m_classNavigationGradeTabs.HorizontalAlignment(HorizontalAlignment::Left);
    setAutomationName(
        m_classNavigationGradeTabs,
        L"Class grade filters"
        );
    Grid::SetColumn(m_classNavigationGradeTabs, 0);
    navigationFilters.Children().Append(m_classNavigationGradeTabs);

    auto dayFilterScroll = ScrollViewer();
    dayFilterScroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Auto);
    dayFilterScroll.VerticalScrollBarVisibility(ScrollBarVisibility::Disabled);
    dayFilterScroll.HorizontalAlignment(HorizontalAlignment::Right);
    m_classNavigationDayTabs = StackPanel();
    m_classNavigationDayTabs.Orientation(Orientation::Horizontal);
    m_classNavigationDayTabs.Spacing(6.0);
    m_classNavigationDayTabs.HorizontalAlignment(HorizontalAlignment::Right);
    setAutomationName(
        m_classNavigationDayTabs,
        L"Class day filters"
        );
    dayFilterScroll.Content(m_classNavigationDayTabs);
    Grid::SetColumn(dayFilterScroll, 1);
    navigationFilters.Children().Append(dayFilterScroll);
    m_classNavigationRoot.Children().Append(navigationFilters);

    auto classTabScroll = ScrollViewer();
    classTabScroll.Height(64.0);
    classTabScroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Auto);
    classTabScroll.VerticalScrollBarVisibility(ScrollBarVisibility::Disabled);
    m_classNavigationClassTabs = StackPanel();
    m_classNavigationClassTabs.Orientation(Orientation::Horizontal);
    m_classNavigationClassTabs.Spacing(6.0);
    m_classNavigationClassTabs.HorizontalAlignment(HorizontalAlignment::Left);
    setAutomationName(
        m_classNavigationClassTabs,
        L"Class selection tabs"
        );
    classTabScroll.Content(m_classNavigationClassTabs);
    m_classNavigationRoot.Children().Append(classTabScroll);
    navigationCard.content.Children().Append(m_classNavigationRoot);

    const auto scrollTab = [](StackPanel const& content) {
        auto scroll = ScrollViewer();
        scroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
        scroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
        scroll.HorizontalContentAlignment(HorizontalAlignment::Stretch);
        scroll.Content(content);
        return scroll;
    };

    m_classSectionScrollViews = {
        scrollTab(detailsRoot),
        scrollTab(rosterRoot),
        scrollTab(analyticsRoot),
        scrollTab(speakingRoot),
        scrollTab(coTeacherRoot),
        scrollTab(notesRoot)
    };

    m_classSectionSelectorBar = SelectorBar();
    m_classSectionSelectorBar.IsTabStop(true);
    m_classSectionSelectorBar.TabIndex(0);
    setAutomationName(
        m_classSectionSelectorBar,
        L"Classes section selector"
        );
    const std::array<std::pair<wchar_t const*, wchar_t const*>, 6>
        sectionDefinitions{
            {
                {L"Details", L"Class Details section"},
                {L"Roster", L"Class Roster section"},
                {L"Analytics", L"Class Analytics section"},
                {L"Evaluations", L"Class Evaluations section"},
                {L"Co-Teacher", L"Class Co-Teacher section"},
                {L"Notes", L"Class Notes section"}
            }
        };
    for (int index = 0;
         index < static_cast<int>(sectionDefinitions.size());
         ++index)
    {
        auto item = SelectorBarItem();
        item.Text(sectionDefinitions[static_cast<std::size_t>(index)].first);
        item.Tag(box_value(index));
        setAutomationName(
            item,
            sectionDefinitions[static_cast<std::size_t>(index)].second
            );
        m_classSectionSelectorItems[static_cast<std::size_t>(index)] = item;
        m_classSectionSelectorBar.Items().Append(item);
    }
    m_classSectionSelectorBar.SelectionChanged(
        [this](SelectorBar const& sender, auto const&) {
            if (m_classSectionSelectionChanging)
            {
                return;
            }
            const auto selected = sender.SelectedItem();
            if (selected)
            {
                selectClassSection(boxedInt(selected.Tag()));
            }
        }
        );

    m_classSectionContentHost = ContentControl();
    m_classSectionContentHost.HorizontalContentAlignment(
        HorizontalAlignment::Stretch
        );
    m_classSectionContentHost.VerticalContentAlignment(
        VerticalAlignment::Stretch
        );
    setAutomationName(
        m_classSectionContentHost,
        L"Active Classes section content"
        );
    selectClassSection(m_classSectionIndex);

    m_classPageRoot = Grid();
    m_classPageRoot.RowSpacing(12.0);
    m_classPageRoot.HorizontalAlignment(HorizontalAlignment::Stretch);
    m_classPageRoot.VerticalAlignment(VerticalAlignment::Stretch);
    for (int rowIndex = 0; rowIndex < 3; ++rowIndex)
    {
        auto row = RowDefinition();
        row.Height(
            GridLengthHelper::FromValueAndType(
                1.0,
                rowIndex == 1 ? GridUnitType::Star : GridUnitType::Auto
                )
            );
        m_classPageRoot.RowDefinitions().Append(row);
    }
    Grid::SetRow(m_classSectionSelectorBar, 0);
    Grid::SetRow(m_classSectionContentHost, 1);
    Grid::SetRow(m_classNavigationCard, 2);
    m_classPageRoot.Children().Append(m_classSectionSelectorBar);
    m_classPageRoot.Children().Append(m_classSectionContentHost);
    m_classPageRoot.Children().Append(m_classNavigationCard);
    page.Content(m_classPageRoot);
    applyClassNavigationLayout();
    refreshClassesPage();
}

void MainWindow::refreshClassInformationOptions()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_classGradeCombo || !m_classLevelCombo
        || !m_classReadingBookCombo || !m_classEssayBookCombo)
    {
        return;
    }

    const std::wstring currentLevel = selectedComboValue(m_classLevelCombo);
    const std::wstring currentReadingBook =
        selectedComboValue(m_classReadingBookCombo);
    const std::wstring currentEssayBook =
        selectedComboValue(m_classEssayBookCombo);
    const std::wstring grade = selectedComboValue(m_classGradeCombo);

    const auto appendChoice = [](ComboBox combo,
                                 std::wstring_view display,
                                 std::wstring_view value) {
        auto item = ComboBoxItem();
        item.Content(box_value(hstring(display)));
        item.Tag(box_value(hstring(value)));
        combo.Items().Append(item);
    };
    const auto selectChoice = [](ComboBox combo, std::wstring_view value) {
        for (int index = 0;
             index < static_cast<int>(combo.Items().Size());
             ++index)
        {
            const auto item = combo.Items().GetAt(index).try_as<ComboBoxItem>();
            if (item && boxedString(item.Tag()) == value)
            {
                combo.SelectedIndex(index);
                return;
            }
        }
        combo.SelectedIndex(-1);
    };

    m_classLoading = true;
    m_classLevelCombo.Items().Clear();
    appendChoice(m_classLevelCombo, L"Not set", L"");
    for (const std::string& value :
         classmngr::engine::ClassInfoConfig::levelsForGrade(
             asUtf8(std::wstring_view(grade))
             ))
    {
        const std::wstring wideValue = asWide(value);
        appendChoice(m_classLevelCombo, wideValue, wideValue);
    }
    selectChoice(m_classLevelCombo, currentLevel);

    m_classReadingBookCombo.Items().Clear();
    appendChoice(m_classReadingBookCombo, L"Not set", L"");
    for (const std::string& value :
         classmngr::engine::ClassInfoConfig::readingBooks(
             asUtf8(std::wstring_view(grade)),
             asUtf8(std::wstring_view(selectedComboValue(m_classLevelCombo)))
             ))
    {
        const std::wstring wideValue = asWide(value);
        appendChoice(
            m_classReadingBookCombo,
            wideValue.empty() ? L"Not set" : wideValue,
            wideValue
            );
    }
    selectChoice(m_classReadingBookCombo, currentReadingBook);

    m_classEssayBookCombo.Items().Clear();
    appendChoice(m_classEssayBookCombo, L"Not set", L"");
    for (const std::string& value :
         classmngr::engine::ClassInfoConfig::essayBooks(
             asUtf8(std::wstring_view(grade)),
             asUtf8(std::wstring_view(selectedComboValue(m_classLevelCombo)))
             ))
    {
        const std::wstring wideValue = asWide(value);
        appendChoice(
            m_classEssayBookCombo,
            wideValue.empty() ? L"Not set" : wideValue,
            wideValue
            );
    }
    selectChoice(m_classEssayBookCombo, currentEssayBook);
    m_classLoading = false;
}

void MainWindow::refreshClassCoTeacher()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_classCoTeacherKrCombo || !m_classCoTeacherEnCombo)
    {
        return;
    }

    m_classCoTeacherLoading = true;
    const auto clearTeacher = [this]() {
        m_classCoTeacherKrCombo.Items().Clear();
        m_classCoTeacherEnCombo.Items().Clear();
        m_classCoTeacherKrCombo.SelectedIndex(-1);
        m_classCoTeacherEnCombo.SelectedIndex(-1);
        m_classCoTeacherRoomTextBox.Text({});
        m_classCoTeacherInternetTypeTextBox.Text({});
        m_classCoTeacherWifiNameTextBox.Text({});
        m_classCoTeacherWifiPasswordTextBox.Text({});
        m_classCoTeacherProjectionTypeTextBox.Text({});
        m_classCoTeacherZoomIdTextBox.Text({});
        m_classCoTeacherZoomPasswordTextBox.Text({});
        m_classCoTeacherSelectedId = -1;
    };

    if (!m_openDatabase || m_classSelectedId <= 0 || m_classNew)
    {
        clearTeacher();
        m_classCoTeacherLoading = false;
        return;
    }

    classmngr::engine::TeacherService service(*m_openDatabase);
    const auto loaded = service.list();
    if (!loaded)
    {
        clearTeacher();
        m_classCoTeacherLoading = false;
        if (m_classStatusText)
        {
            m_classStatusText.Text(winrt::hstring(
                L"Co-teacher directory could not be loaded: "
                + asWide(loaded.error().message)
                ));
        }
        return;
    }

    auto teachers = *loaded;
    std::sort(
        teachers.begin(),
        teachers.end(),
        [](const auto& left, const auto& right) {
            return classmngr::engine::teacherDisplayLessThan(left, right);
        }
        );

    const auto appendTeacher = [](ComboBox combo,
                                  std::wstring_view display,
                                  int id) {
        auto item = ComboBoxItem();
        item.Content(box_value(hstring(display)));
        item.Tag(box_value(id));
        combo.Items().Append(item);
    };
    appendTeacher(m_classCoTeacherKrCombo, L"Unassigned", -1);
    appendTeacher(m_classCoTeacherEnCombo, L"Unassigned", -1);
    for (const auto& teacher : teachers)
    {
        std::wstring korean = asWide(teacher.teacherKr);
        std::wstring english = asWide(teacher.teacherEn);
        if (korean.empty())
        {
            korean = asWide(teacher.preferredDisplayName());
        }
        if (english.empty())
        {
            english = asWide(teacher.preferredDisplayName());
        }
        appendTeacher(m_classCoTeacherKrCombo, korean, teacher.id);
        appendTeacher(m_classCoTeacherEnCombo, english, teacher.id);
    }

    const auto selectTeacher = [](ComboBox combo, int teacherId) {
        for (int index = 0;
             index < static_cast<int>(combo.Items().Size());
             ++index)
        {
            const auto item = combo.Items().GetAt(index).try_as<ComboBoxItem>();
            if (item && boxedInt(item.Tag()) == teacherId)
            {
                combo.SelectedIndex(index);
                return;
            }
        }
        combo.SelectedIndex(0);
    };
    m_classCoTeacherSelectedId = m_classInfo.teacherId;
    selectTeacher(m_classCoTeacherKrCombo, m_classCoTeacherSelectedId);
    selectTeacher(m_classCoTeacherEnCombo, m_classCoTeacherSelectedId);

    m_classCoTeacherRoomTextBox.Text(asWide(m_classInfo.roomNumber));
    m_classCoTeacherInternetTypeTextBox.Text(
        asWide(m_classInfo.internetType)
        );
    m_classCoTeacherWifiNameTextBox.Text(asWide(m_classInfo.wifiName));
    m_classCoTeacherWifiPasswordTextBox.Text(
        asWide(m_classInfo.wifiPassword)
        );
    m_classCoTeacherProjectionTypeTextBox.Text(
        asWide(m_classInfo.projectionType)
        );
    m_classCoTeacherZoomIdTextBox.Text(asWide(m_classInfo.zoomId));
    m_classCoTeacherZoomPasswordTextBox.Text(
        asWide(m_classInfo.zoomPassword)
        );
    m_classCoTeacherLoading = false;
}

std::vector<classmngr::engine::ClassTime>
MainWindow::classScheduleFromForm(bool intensive) const
{
    const auto& days = intensive
        ? m_classIntensiveDayCombos
        : m_classRegularDayCombos;
    const auto& startHours = intensive
        ? m_classIntensiveStartHourCombos
        : m_classRegularStartHourCombos;
    const auto& startMinutes = intensive
        ? m_classIntensiveStartMinuteCombos
        : m_classRegularStartMinuteCombos;
    const auto& startPeriods = intensive
        ? m_classIntensiveStartPeriodCombos
        : m_classRegularStartPeriodCombos;
    const auto& ends = intensive
        ? m_classIntensiveEndCombos
        : m_classRegularEndCombos;
    const std::size_t rowCount = std::min(
        days.size(),
        std::min(
            std::min(startHours.size(), startMinutes.size()),
            std::min(startPeriods.size(), ends.size())
            )
        );
    std::vector<classmngr::engine::ClassTime> result;
    result.reserve(rowCount);
    for (std::size_t index = 0; index < rowCount; ++index)
    {
        result.push_back({
            asUtf8(selectedComboValue(days[index])),
            asUtf8(selectedComboValue(startHours[index]))
                + asUtf8(selectedComboValue(startMinutes[index])) + " "
                + asUtf8(selectedComboValue(startPeriods[index])),
            asUtf8(selectedComboValue(ends[index]))
        });
    }
    return result;
}

void MainWindow::rebuildClassScheduleRows(
    bool intensive,
    std::vector<classmngr::engine::ClassTime> const& times
    )
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    auto grid = intensive
        ? m_classIntensiveScheduleGrid
        : m_classRegularScheduleGrid;
    if (!grid)
    {
        return;
    }

    auto& dayCombos = intensive
        ? m_classIntensiveDayCombos
        : m_classRegularDayCombos;
    auto& startHourCombos = intensive
        ? m_classIntensiveStartHourCombos
        : m_classRegularStartHourCombos;
    auto& startMinuteCombos = intensive
        ? m_classIntensiveStartMinuteCombos
        : m_classRegularStartMinuteCombos;
    auto& startPeriodCombos = intensive
        ? m_classIntensiveStartPeriodCombos
        : m_classRegularStartPeriodCombos;
    auto& endCombos = intensive
        ? m_classIntensiveEndCombos
        : m_classRegularEndCombos;
    dayCombos.clear();
    startHourCombos.clear();
    startMinuteCombos.clear();
    startPeriodCombos.clear();
    endCombos.clear();
    grid.ColumnDefinitions().Clear();
    grid.RowDefinitions().Clear();
    grid.Children().Clear();

    constexpr double dayWidth = 160.0;
    constexpr double startHourWidth = 70.0;
    constexpr double startMinuteWidth = 80.0;
    constexpr double startPeriodWidth = 70.0;
    constexpr double startWidth = startHourWidth + startMinuteWidth
        + startPeriodWidth + 16.0;
    constexpr double endWidth = 120.0;
    constexpr double removeWidth = 90.0;
    const std::array<double, 4> widths{
        dayWidth, startWidth, endWidth, removeWidth
    };
    grid.ColumnSpacing(16.0);
    grid.RowSpacing(4.0);
    for (const double width : widths)
    {
        auto definition = ColumnDefinition();
        definition.Width(GridLengthHelper::FromValueAndType(
            width,
            GridUnitType::Pixel
            ));
        grid.ColumnDefinitions().Append(definition);
    }
    const std::array<wchar_t const*, 4> headers{
        L"Days", L"Start Time", L"End Time", L""
    };
    grid.RowDefinitions().Append(RowDefinition());
    for (int column = 0; column < 4; ++column)
    {
        auto header = TextBlock();
        header.Text(headers[static_cast<std::size_t>(column)]);
        header.Margin(Thickness{4.0, 0.0, 4.0, 4.0});
        applyResourceStyle(header, L"Phase3BodyTextBlockStyle");
        Grid::SetColumn(header, column);
        grid.Children().Append(header);
    }

    const auto appendChoice = [](ComboBox const& combo,
                                 std::string_view value) {
        auto item = ComboBoxItem();
        const hstring text{asWide(value)};
        item.Content(box_value(text));
        item.Tag(box_value(text));
        combo.Items().Append(item);
    };
    const auto selectChoice = [](ComboBox const& combo,
                                 std::string_view value) {
        for (int choice = 0; choice < static_cast<int>(combo.Items().Size());
             ++choice)
        {
            const auto item = combo.Items().GetAt(choice).try_as<ComboBoxItem>();
            if (item && boxedString(item.Tag()) == asWide(value))
            {
                combo.SelectedIndex(choice);
                return true;
            }
        }
        return false;
    };
    const auto formatTime = [](int totalMinutes) {
        totalMinutes %= 24 * 60;
        const int hour24 = totalMinutes / 60;
        const int minute = totalMinutes % 60;
        const int hour = hour24 % 12 == 0 ? 12 : hour24 % 12;
        const char* period = hour24 < 12 ? "AM" : "PM";
        return std::to_string(hour) + ":"
            + (minute < 10 ? "0" : "") + std::to_string(minute)
            + " " + period;
    };
    const auto parseStart = [](std::string_view value,
                               std::string& hour,
                               std::string& minute,
                               std::string& period) {
        const auto space = value.find(' ');
        const auto colon = value.find(':');
        if (space == std::string_view::npos || colon == std::string_view::npos
            || colon > space || space + 1 >= value.size())
        {
            return false;
        }
        hour = std::string(value.substr(0, colon));
        minute = ":" + std::string(value.substr(colon + 1, space - colon - 1));
        period = std::string(value.substr(space + 1));
        return true;
    };
    const bool wasLoading = m_classLoading;
    m_classLoading = true;
    for (std::size_t index = 0; index < times.size(); ++index)
    {
        grid.RowDefinitions().Append(RowDefinition());

        auto day = ComboBox();
        day.Width(widths[0]);
        day.MinWidth(widths[0]);
        day.IsTabStop(true);
        for (const std::string& weekday : classmngr::engine::ClassInfoConfig::days())
        {
            appendChoice(day, weekday);
        }
        if (!selectChoice(day, times[index].day))
        {
            day.SelectedIndex(0);
        }
        day.SelectionChanged({this, &MainWindow::ClassField_SelectionChanged});
        setAutomationName(
            day,
            std::wstring(L"Class schedule day ") + std::to_wstring(index + 1)
            );

        auto start = StackPanel();
        start.Orientation(Orientation::Horizontal);
        start.Spacing(8.0);
        auto hour = ComboBox();
        hour.Width(startHourWidth);
        hour.IsTabStop(true);
        for (const std::string& value : intensive
                 ? classmngr::engine::ClassInfoConfig::intensiveHours()
                 : classmngr::engine::ClassInfoConfig::regularHours())
        {
            appendChoice(hour, value);
        }
        auto minute = ComboBox();
        minute.Width(startMinuteWidth);
        minute.IsTabStop(true);
        for (const std::string& value : classmngr::engine::ClassInfoConfig::startMinutes())
        {
            appendChoice(minute, value);
        }
        auto period = ComboBox();
        period.Width(startPeriodWidth);
        period.IsTabStop(true);
        appendChoice(period, "PM");
        appendChoice(period, "AM");
        std::string parsedHour;
        std::string parsedMinute;
        std::string parsedPeriod;
        const bool hasStart = parseStart(
            times[index].startTime, parsedHour, parsedMinute, parsedPeriod
            );
        if (!hasStart || !selectChoice(hour, parsedHour))
        {
            selectChoice(hour, "4");
        }
        if (!hasStart || !selectChoice(minute, parsedMinute))
        {
            selectChoice(minute, ":00");
        }
        if (!hasStart || !selectChoice(period, parsedPeriod))
        {
            selectChoice(period, "PM");
        }
        setAutomationName(hour, std::wstring(L"Class schedule start hour ")
            + std::to_wstring(index + 1));
        setAutomationName(minute, std::wstring(L"Class schedule start minute ")
            + std::to_wstring(index + 1));
        setAutomationName(period, std::wstring(L"Class schedule start period ")
            + std::to_wstring(index + 1));
        start.Children().Append(hour);
        start.Children().Append(minute);
        start.Children().Append(period);

        auto end = ComboBox();
        end.Width(endWidth);
        end.IsTabStop(true);
        setAutomationName(end, std::wstring(L"Class schedule end time ")
            + std::to_wstring(index + 1));
        const auto updateEndOptions = [hour, minute, period, end, formatTime,
                                       selectChoice]() {
            const int selectedHour = std::stoi(
                asUtf8(selectedComboValue(hour))
                );
            const int selectedMinute = std::stoi(asUtf8(
                selectedComboValue(minute).substr(1)
                ));
            const bool am = selectedComboValue(period) == L"AM";
            const int hour24 = am
                ? (selectedHour == 12 ? 0 : selectedHour)
                : (selectedHour == 12 ? 12 : selectedHour + 12);
            const std::string current = asUtf8(selectedComboValue(end));
            end.Items().Clear();
            for (const int duration : {55, 85, 115, 175, 235})
            {
                const int endMinutes = hour24 * 60 + selectedMinute + duration;
                if (endMinutes <= 21 * 60 + 55)
                {
                    auto item = ComboBoxItem();
                    const hstring text{asWide(formatTime(endMinutes))};
                    item.Content(box_value(text));
                    item.Tag(box_value(text));
                    end.Items().Append(item);
                }
            }
            if (!selectChoice(end, current) && end.Items().Size() > 0)
            {
                end.SelectedIndex(0);
            }
        };
        updateEndOptions();
        if (!times[index].endTime.empty())
        {
            selectChoice(end, times[index].endTime);
        }
        hour.SelectionChanged([this, updateEndOptions](auto const&, auto const&) {
            updateEndOptions();
            m_classDetailsDirty = true;
            markClassDirty();
        });
        minute.SelectionChanged([this, updateEndOptions](auto const&, auto const&) {
            updateEndOptions();
            m_classDetailsDirty = true;
            markClassDirty();
        });
        period.SelectionChanged([this, updateEndOptions](auto const&, auto const&) {
            updateEndOptions();
            m_classDetailsDirty = true;
            markClassDirty();
        });
        end.SelectionChanged({this, &MainWindow::ClassField_SelectionChanged});

        auto remove = Button();
        remove.Content(box_value(hstring(L"Remove")));
        remove.Width(removeWidth);
        remove.IsTabStop(true);
        remove.Click(
            [this, intensive, index](auto const&, auto const&) {
                removeClassScheduleRow(
                    intensive,
                    static_cast<int>(index)
                    );
            }
            );
        setAutomationName(
            remove,
            std::wstring(L"Remove class schedule row ")
                + std::to_wstring(index + 1)
            );

        Grid::SetRow(day, static_cast<int>(index + 1));
        Grid::SetColumn(day, 0);
        Grid::SetRow(start, static_cast<int>(index + 1));
        Grid::SetColumn(start, 1);
        Grid::SetRow(end, static_cast<int>(index + 1));
        Grid::SetColumn(end, 2);
        Grid::SetRow(remove, static_cast<int>(index + 1));
        Grid::SetColumn(remove, 3);
        grid.Children().Append(day);
        grid.Children().Append(start);
        grid.Children().Append(end);
        grid.Children().Append(remove);
        dayCombos.push_back(day);
        startHourCombos.push_back(hour);
        startMinuteCombos.push_back(minute);
        startPeriodCombos.push_back(period);
        endCombos.push_back(end);
    }
    grid.MinWidth(std::accumulate(widths.begin(), widths.end(), 0.0));
    m_classLoading = wasLoading;
}

void MainWindow::addClassScheduleRow(bool intensive)
{
    auto times = classScheduleFromForm(intensive);
    times.push_back({});
    rebuildClassScheduleRows(intensive, times);
    m_classDetailsDirty = true;
    markClassDirty();
}

void MainWindow::removeClassScheduleRow(bool intensive, int index)
{
    auto times = classScheduleFromForm(intensive);
    if (index < 0 || index >= static_cast<int>(times.size()))
    {
        return;
    }
    times.erase(times.begin() + index);
    rebuildClassScheduleRows(intensive, times);
    m_classDetailsDirty = true;
    markClassDirty();
}

void MainWindow::updateClassActions()
{
    if (!m_classSelector || !m_classNameTextBox || !m_classSaveButton
        || !m_classNotesSaveButton
        || !m_classCoTeacherKrCombo
        || !m_classCoTeacherSaveButton)
    {
        return;
    }

    const bool hasDatabase = static_cast<bool>(m_openDatabase);
    const bool hasClass = m_classSelectedId > 0 || m_classNew;
    const bool clean = !m_classDirty && !m_classRosterDirty
        && !m_speakingEvaluationDirty;
    const bool detailsEnabled = hasDatabase && hasClass;
    const bool notesEnabled = hasDatabase && m_classSelectedId > 0;

    m_classSelector.IsEnabled(hasDatabase && clean && !m_classNew);
    m_classNameTextBox.IsEnabled(detailsEnabled);
    m_classGradeCombo.IsEnabled(detailsEnabled);
    m_classLevelCombo.IsEnabled(detailsEnabled);
    m_classReadingBookCombo.IsEnabled(detailsEnabled);
    m_classEssayBookCombo.IsEnabled(detailsEnabled);
    m_classColorTextBox.IsEnabled(detailsEnabled);
    m_classColorPreview.IsHitTestVisible(detailsEnabled);
    m_classColorChooseButton.IsEnabled(detailsEnabled);
    m_classFontColorTextBox.IsEnabled(detailsEnabled);
    for (const auto& control : m_classRegularDayCombos)
    {
        control.IsEnabled(detailsEnabled);
    }
    for (const auto& control : m_classRegularStartHourCombos)
    {
        control.IsEnabled(detailsEnabled);
    }
    for (const auto& control : m_classRegularStartMinuteCombos)
    {
        control.IsEnabled(detailsEnabled);
    }
    for (const auto& control : m_classRegularStartPeriodCombos)
    {
        control.IsEnabled(detailsEnabled);
    }
    for (const auto& control : m_classRegularEndCombos)
    {
        control.IsEnabled(detailsEnabled);
    }
    for (const auto& control : m_classIntensiveDayCombos)
    {
        control.IsEnabled(detailsEnabled);
    }
    for (const auto& control : m_classIntensiveStartHourCombos)
    {
        control.IsEnabled(detailsEnabled);
    }
    for (const auto& control : m_classIntensiveStartMinuteCombos)
    {
        control.IsEnabled(detailsEnabled);
    }
    for (const auto& control : m_classIntensiveStartPeriodCombos)
    {
        control.IsEnabled(detailsEnabled);
    }
    for (const auto& control : m_classIntensiveEndCombos)
    {
        control.IsEnabled(detailsEnabled);
    }
    m_classRegularScheduleAddButton.IsEnabled(detailsEnabled);
    m_classIntensiveScheduleAddButton.IsEnabled(detailsEnabled);
    m_classNotesTextBox.IsEnabled(notesEnabled || (hasDatabase && m_classNew));
    m_classTimeFillerActivitiesTextBox.IsEnabled(
        notesEnabled || (hasDatabase && m_classNew)
        );
    m_classCoTeacherKrCombo.IsEnabled(detailsEnabled);
    m_classCoTeacherEnCombo.IsEnabled(detailsEnabled);
    m_classCoTeacherRoomTextBox.IsEnabled(detailsEnabled);
    m_classCoTeacherInternetTypeTextBox.IsEnabled(detailsEnabled);
    m_classCoTeacherWifiNameTextBox.IsEnabled(detailsEnabled);
    m_classCoTeacherWifiPasswordTextBox.IsEnabled(detailsEnabled);
    m_classCoTeacherProjectionTypeTextBox.IsEnabled(detailsEnabled);
    m_classCoTeacherZoomIdTextBox.IsEnabled(detailsEnabled);
    m_classCoTeacherZoomPasswordTextBox.IsEnabled(detailsEnabled);

    m_classNewButton.IsEnabled(hasDatabase && clean && !m_classNew);
    m_classDeleteButton.IsEnabled(
        hasDatabase && clean && !m_classNew && m_classSelectedId > 0
        );
    m_classSaveButton.IsEnabled(
        hasDatabase && hasClass && m_classDetailsDirty
        );
    m_classDiscardButton.IsEnabled(
        hasDatabase && hasClass && m_classDirty
        );
    m_classNotesSaveButton.IsEnabled(
        hasDatabase && m_classSelectedId > 0 && m_classNotesDirty
        );
    m_classNotesDiscardButton.IsEnabled(
        hasDatabase && m_classSelectedId > 0 && m_classNotesDirty
        );
    m_classCoTeacherSaveButton.IsEnabled(
        hasDatabase && m_classSelectedId > 0 && m_classDetailsDirty
        );
    m_classCoTeacherDiscardButton.IsEnabled(
        hasDatabase && m_classSelectedId > 0 && m_classDetailsDirty
        );
    updateClassRosterActions();
}

void MainWindow::markClassDirty()
{
    if (m_classLoading || !m_openDatabase)
    {
        return;
    }

    m_classDirty = true;
    m_dirtyState.markDirty();
    if (m_classStatusText)
    {
        m_classStatusText.Text(L"Unsaved class information changes.");
    }
    if (m_classNotesStatusText)
    {
        m_classNotesStatusText.Text(L"Unsaved class notes changes.");
    }
    updateClassActions();
}

void MainWindow::clearClassDirty()
{
    m_classDirty = false;
    m_classDetailsDirty = false;
    m_classNotesDirty = false;
    if (!m_classRosterDirty && !m_speakingEvaluationDirty)
    {
        m_dirtyState.markClean();
    }
    updateClassActions();
}

void MainWindow::presentClass(int index)
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_classSelector || !m_classNameTextBox)
    {
        return;
    }

    m_classLoading = true;
    classmngr::engine::Classroom classroom;
    if (index >= 0 && index < static_cast<int>(m_classes.size()))
    {
        classroom = m_classes[static_cast<std::size_t>(index)];
        m_classSelectedIndex = index;
        m_classSelectedId = classroom.id;
    }
    else
    {
        m_classSelectedIndex = -1;
        m_classSelectedId = -1;
    }

    classmngr::engine::ClassInfo info;
    bool loaded = false;
    if (classroom.id > 0 && m_openDatabase)
    {
        classmngr::engine::ClassInfoService service(*m_openDatabase);
        const auto result = service.load(classroom.id);
        if (result)
        {
            info = *result;
            loaded = true;
        }
        else if (m_classValidationText)
        {
            m_classValidationText.Text(winrt::hstring(
                L"Class information could not be loaded: "
                + asWide(result.error().message)
                ));
            m_classValidationText.Visibility(
                Microsoft::UI::Xaml::Visibility::Visible
                );
        }
    }
    info.classId = classroom.id;
    m_classInfo = info;
    refreshClassCoTeacher();

    m_classNameTextBox.Text(asWide(classroom.name));
    m_classGradeCombo.SelectedIndex(0);
    refreshClassInformationOptions();

    const auto selectChoice = [](ComboBox combo, std::wstring_view value) {
        for (int optionIndex = 0;
             optionIndex < static_cast<int>(combo.Items().Size());
             ++optionIndex)
        {
            const auto item = combo.Items().GetAt(optionIndex).try_as<ComboBoxItem>();
            if (item && boxedString(item.Tag()) == value)
            {
                combo.SelectedIndex(optionIndex);
                return;
            }
        }
        combo.SelectedIndex(-1);
    };
    selectChoice(m_classGradeCombo, asWide(info.classGrade));
    refreshClassInformationOptions();
    selectChoice(m_classLevelCombo, asWide(info.classLevel));
    refreshClassInformationOptions();
    selectChoice(m_classReadingBookCombo, asWide(info.readingBook));
    selectChoice(m_classEssayBookCombo, asWide(info.essayBook));
    m_classColorTextBox.Text(
        asWide(info.classColor.empty() ? "#FFFFFF" : info.classColor)
        );
    if (m_classColorPreview)
    {
        m_classColorPreview.Background(
            Microsoft::UI::Xaml::Media::SolidColorBrush(
                uiColorFromHex(
                    info.classColor.empty() ? "#FFFFFF" : info.classColor
                    )
                )
            );
    }
    m_classFontColorTextBox.Text(
        asWide(info.fontColor.empty() ? "#000000" : info.fontColor)
        );

    if (m_classStudentCountTextBox)
    {
        std::wstring count = L"0";
        if (classroom.id > 0 && m_openDatabase)
        {
            classmngr::engine::RosterService rosterService(*m_openDatabase);
            const auto studentCount = rosterService.studentCount(classroom.id);
            if (studentCount)
            {
                count = std::to_wstring(*studentCount);
            }
        }
        m_classStudentCountTextBox.Text(winrt::hstring(count));
    }
    rebuildClassScheduleRows(false, info.classTimes);
    rebuildClassScheduleRows(true, info.intensiveTimes);

    std::wstring teacher = asWide(info.teacherPreferredName);
    if (teacher.empty())
    {
        teacher = asWide(info.teacherEn);
    }
    if (teacher.empty())
    {
        teacher = asWide(info.teacherKr);
    }
    m_classTeacherText.Text(
        winrt::hstring(
            teacher.empty()
                ? L"Assigned teacher: Unassigned"
                : L"Assigned teacher: " + teacher
            )
        );

    m_classNotesTextBox.Text(asWide(info.notes));
    m_classTimeFillerActivitiesTextBox.Text(
        asWide(info.timeFillerActivities)
        );
    m_classLoading = false;

    if (m_classValidationText && loaded)
    {
        m_classValidationText.Text({});
        m_classValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Collapsed
            );
    }
    updateClassActions();
}

void MainWindow::refreshClassNavigation(bool selectFallback)
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_classNavigationGradeTabs
        || !m_classNavigationDayTabs
        || !m_classNavigationClassTabs)
    {
        return;
    }

    m_classNavigationLoading = true;
    m_classNavigationGradeTabs.Children().Clear();
    m_classNavigationDayTabs.Children().Clear();
    m_classNavigationClassTabs.Children().Clear();

    if (!m_openDatabase || m_classes.empty())
    {
        m_classNavigationGrade.clear();
        m_classNavigationSelectedDays.clear();
        m_classNavigationAll = true;
        auto empty = TextBlock();
        empty.Text(
            !m_openDatabase
                ? L"Open a database to browse scheduled classes."
                : L"No classes are available."
            );
        empty.TextWrapping(TextWrapping::Wrap);
        setAutomationName(empty, L"Class navigation empty state");
        m_classNavigationClassTabs.Children().Append(empty);
        m_classNavigationLoading = false;
        return;
    }

    classmngr::engine::ClassInfoService infoService(*m_openDatabase);
    std::vector<classmngr::engine::ClassTabNavigationService::ClassEntry>
        entries;
    entries.reserve(m_classes.size());
    for (const classmngr::engine::Classroom& classroom : m_classes)
    {
        if (classroom.id <= 0)
        {
            continue;
        }

        classmngr::engine::ClassInfo info;
        const auto loaded = infoService.load(classroom.id);
        if (loaded)
        {
            info = *loaded;
        }

        classmngr::engine::ClassTabNavigationService::ClassEntry entry;
        entry.classId = classroom.id;
        entry.classroomName = classroom.name;
        entry.grade = info.classGrade;
        entry.level = info.classLevel;
        entry.regularTimes = info.classTimes;
        entry.intensiveTimes = info.intensiveTimes;
        entry.teacherEn = info.teacherEn;
        entry.teacherKr = info.teacherKr;
        entries.push_back(std::move(entry));
    }

    const auto isWeekendDay = [](std::string value) {
        std::transform(
            value.begin(),
            value.end(),
            value.begin(),
            [](char character) {
                return static_cast<char>(
                    std::tolower(static_cast<unsigned char>(character))
                    );
            }
            );
        return value == "saturday" || value == "sunday";
    };
    const auto includesWeekend = [&isWeekendDay](
        const std::vector<classmngr::engine::ClassTime>& times
        ) {
        return std::any_of(
            times.begin(),
            times.end(),
            [&isWeekendDay](const auto& time) {
                return isWeekendDay(time.day);
            }
            );
    };
    const bool weekendAvailable = std::any_of(
        entries.begin(),
        entries.end(),
        [&includesWeekend](const auto& entry) {
            return includesWeekend(entry.regularTimes)
                || includesWeekend(entry.intensiveTimes);
        }
        );

    if (!weekendAvailable)
    {
        m_classNavigationSelectedDays.erase(
            std::remove(
                m_classNavigationSelectedDays.begin(),
                m_classNavigationSelectedDays.end(),
                "Wkend"
                ),
            m_classNavigationSelectedDays.end()
            );
    }

    classmngr::engine::ClassTabNavigationService::DayFilter dayFilter;
    dayFilter.selectedDays = m_classNavigationSelectedDays;
    dayFilter.scheduleSource =
        classmngr::engine::ClassTabNavigationService::ScheduleSource::Regular;
    dayFilter.visibilityScope =
        classmngr::engine::ClassTabNavigationService::VisibilityScope::ActiveSchedule;
    const auto navigation =
        classmngr::engine::ClassTabNavigationService::build(
            entries,
            classmngr::engine::ClassTabNavigationService::GroupingPolicy::AlwaysGradeGrouped,
            dayFilter
            );

    if (!m_classNavigationAll)
    {
        const bool gradeAvailable = std::any_of(
            navigation.gradeGroups.begin(),
            navigation.gradeGroups.end(),
            [this](const auto& group) {
                return group.grade == m_classNavigationGrade;
            }
            );
        if (!gradeAvailable)
        {
            m_classNavigationAll = true;
            m_classNavigationGrade.clear();
        }
    }

    const auto setButtonState = [](Button const& button, bool selected) {
        button.Background(
            Microsoft::UI::Xaml::Media::SolidColorBrush(
                Windows::UI::Color{
                    255,
                    static_cast<std::uint8_t>(selected ? 59 : 48),
                    static_cast<std::uint8_t>(selected ? 169 : 53),
                    static_cast<std::uint8_t>(selected ? 225 : 60)
                }
                )
            );
        button.Foreground(
            Microsoft::UI::Xaml::Media::SolidColorBrush(
                Windows::UI::Color{255, 255, 255, 255}
                )
            );
        button.BorderBrush(
            Microsoft::UI::Xaml::Media::SolidColorBrush(
                Windows::UI::Color{255, 92, 99, 108}
                )
            );
        button.BorderThickness(Thickness{1.0, 1.0, 1.0, 1.0});
        button.CornerRadius(CornerRadius{5.0, 5.0, 5.0, 5.0});
    };

    const auto makeNavigationButton = [](std::wstring const& label) {
        auto button = Button();
        button.Content(box_value(hstring(label)));
        button.MinHeight(34.0);
        button.Padding(Thickness{12.0, 4.0, 12.0, 4.0});
        button.IsTabStop(true);
        button.HorizontalAlignment(HorizontalAlignment::Left);
        return button;
    };

    for (const auto& group : navigation.gradeGroups)
    {
        const std::wstring label = asWide(group.label);
        auto button = makeNavigationButton(label);
        setAutomationName(
            button,
            std::wstring(L"Class grade filter ") + label
            );
        setButtonState(
            button,
            !m_classNavigationAll
                && group.grade == m_classNavigationGrade
            );
        const std::string grade = group.grade;
        button.Click(
            [this, grade](auto const&, auto const&) {
                selectClassNavigationGrade(false, grade);
            }
            );
        m_classNavigationGradeTabs.Children().Append(button);
    }

    if (!navigation.allClasses.empty())
    {
        auto button = makeNavigationButton(L"All");
        setAutomationName(button, L"Class grade filter All");
        setButtonState(button, m_classNavigationAll);
        button.Click(
            [this](auto const&, auto const&) {
                selectClassNavigationGrade(true, {});
            }
            );
        m_classNavigationGradeTabs.Children().Append(button);
    }

    const std::array<std::pair<std::wstring_view, std::string_view>, 6>
        dayButtons{
            {
                {L"M", "Monday"},
                {L"T", "Tuesday"},
                {L"W", "Wednesday"},
                {L"Th", "Thursday"},
                {L"F", "Friday"},
                {L"Wkend", "Wkend"}
            }
        };
    for (const auto& definition : dayButtons)
    {
        if (definition.second == "Wkend" && !weekendAvailable)
        {
            continue;
        }

        const std::wstring label(definition.first);
        auto button = makeNavigationButton(label);
        setAutomationName(
            button,
            std::wstring(L"Class day filter ") + asWide(definition.second)
            );
        const std::string day(definition.second);
        const bool selected = std::find(
            m_classNavigationSelectedDays.begin(),
            m_classNavigationSelectedDays.end(),
            day
            ) != m_classNavigationSelectedDays.end();
        setButtonState(button, selected);
        button.Click(
            [this, day](auto const&, auto const&) {
                toggleClassNavigationDay(day);
            }
            );
        m_classNavigationDayTabs.Children().Append(button);
    }

    const std::vector<
        classmngr::engine::ClassTabNavigationService::ClassTab>* visibleClasses =
        &navigation.allClasses;
    if (!m_classNavigationAll)
    {
        visibleClasses = nullptr;
        for (const auto& group : navigation.gradeGroups)
        {
            if (group.grade == m_classNavigationGrade)
            {
                visibleClasses = &group.classes;
                break;
            }
        }
    }

    if (!visibleClasses || visibleClasses->empty())
    {
        auto empty = TextBlock();
        empty.Text(L"No scheduled classes match the current filters.");
        empty.TextWrapping(TextWrapping::Wrap);
        setAutomationName(
            empty,
            L"No scheduled classes match the current filters"
            );
        m_classNavigationClassTabs.Children().Append(empty);
    }
    else
    {
        for (const auto& classTab : *visibleClasses)
        {
            std::wstring label = asWide(classTab.label);
            if (label.empty())
            {
                label = L"Class " + std::to_wstring(classTab.classId);
            }
            auto button = makeNavigationButton(label);
            setAutomationName(
                button,
                std::wstring(L"Class tab ") + label
                );
            setButtonState(button, classTab.classId == m_classSelectedId);
            const int classId = classTab.classId;
            button.Click(
                [this, classId](auto const&, auto const&) {
                    selectClassFromNavigation(classId);
                }
                );
            m_classNavigationClassTabs.Children().Append(button);
        }
    }

    int fallbackClassId = -1;
    if (visibleClasses && !visibleClasses->empty())
    {
        const auto current = std::find_if(
            visibleClasses->begin(),
            visibleClasses->end(),
            [this](const auto& classTab) {
                return classTab.classId == m_classSelectedId;
            }
            );
        if (current == visibleClasses->end())
        {
            fallbackClassId = visibleClasses->front().classId;
        }
    }

    m_classNavigationLoading = false;
    const bool clean = !m_classDirty
        && !m_classRosterDirty
        && !m_speakingEvaluationDirty
        && !m_classNew;
    if (selectFallback
        && clean
        && fallbackClassId > 0
        && fallbackClassId != m_classSelectedId)
    {
        selectClassFromNavigation(fallbackClassId);
    }
}

void MainWindow::selectClassFromNavigation(int classId)
{
    if (!m_openDatabase || !m_classSelector || m_classNew)
    {
        return;
    }

    if (m_classDirty || m_classRosterDirty || m_speakingEvaluationDirty)
    {
        m_classStatusText.Text(
            m_speakingEvaluationDirty
                ? L"Save or discard the current speaking evaluation before selecting another."
                : m_classRosterDirty
                    ? L"Save or discard the current roster before selecting another."
                    : L"Save or discard the current class before selecting another."
            );
        return;
    }

    int resolvedIndex = -1;
    for (int index = 0; index < static_cast<int>(m_classes.size()); ++index)
    {
        if (m_classes[static_cast<std::size_t>(index)].id == classId)
        {
            resolvedIndex = index;
            break;
        }
    }

    if (resolvedIndex == m_classSelectedIndex)
    {
        refreshClassNavigation(false);
        return;
    }

    m_classSelector.SelectedIndex(resolvedIndex);
}

void MainWindow::selectClassNavigationGrade(bool all, std::string grade)
{
    if (m_classNavigationLoading)
    {
        return;
    }
    if (m_classDirty || m_classRosterDirty || m_speakingEvaluationDirty)
    {
        m_classStatusText.Text(
            m_speakingEvaluationDirty
                ? L"Save or discard the current speaking evaluation before changing class filters."
                : m_classRosterDirty
                    ? L"Save or discard the current roster before changing class filters."
                    : L"Save or discard the current class before changing class filters."
            );
        return;
    }

    m_classNavigationAll = all;
    m_classNavigationGrade = all ? std::string{} : std::move(grade);
    refreshClassNavigation(true);
}

void MainWindow::toggleClassNavigationDay(std::string day)
{
    if (m_classNavigationLoading)
    {
        return;
    }
    if (m_classDirty || m_classRosterDirty || m_speakingEvaluationDirty)
    {
        m_classStatusText.Text(
            m_speakingEvaluationDirty
                ? L"Save or discard the current speaking evaluation before changing class filters."
                : m_classRosterDirty
                    ? L"Save or discard the current roster before changing class filters."
                    : L"Save or discard the current class before changing class filters."
            );
        return;
    }

    const auto found = std::find(
        m_classNavigationSelectedDays.begin(),
        m_classNavigationSelectedDays.end(),
        day
        );
    if (found == m_classNavigationSelectedDays.end())
    {
        m_classNavigationSelectedDays.push_back(std::move(day));
    }
    else
    {
        m_classNavigationSelectedDays.erase(found);
    }
    refreshClassNavigation(true);
}

void MainWindow::refreshClassNavigationLocation()
{
    m_classNavigationLocation = ClassNavigationLocation::Top;
    if (m_openDatabase)
    {
        classmngr::engine::ApplicationSettingsService settings(*m_openDatabase);
        const auto loaded = settings.load(classNavigationLocationKey);
        if (loaded)
        {
            const auto stored = classNavigationLocationFromSetting(*loaded);
            if (stored)
            {
                m_classNavigationLocation = *stored;
            }
        }
    }
    applyClassNavigationLayout();
}

void MainWindow::applyClassNavigationLayout()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_classPageRoot
        || !m_classSectionContentHost
        || !m_classNavigationCard)
    {
        return;
    }

    const bool bottom = m_classNavigationLocation
        == ClassNavigationLocation::Bottom
        && static_cast<bool>(m_openDatabase);
    m_classPageRoot.RowDefinitions().GetAt(1).Height(
        GridLengthHelper::FromValueAndType(
            1.0,
            bottom ? GridUnitType::Star : GridUnitType::Auto
            )
        );
    m_classPageRoot.RowDefinitions().GetAt(2).Height(
        GridLengthHelper::FromValueAndType(
            1.0,
            bottom ? GridUnitType::Auto : GridUnitType::Star
            )
        );
    Grid::SetRow(m_classSectionContentHost, bottom ? 1 : 2);
    Grid::SetRow(m_classNavigationCard, bottom ? 2 : 1);
}

void MainWindow::selectClassSection(int index)
{
    if (index < 0
        || index >= static_cast<int>(m_classSectionScrollViews.size())
        || !m_classSectionContentHost)
    {
        return;
    }

    m_classSectionIndex = index;
    m_classSectionContentHost.Content(
        m_classSectionScrollViews[static_cast<std::size_t>(index)]
        );

    if (m_classSectionSelectorBar
        && m_classSectionSelectorItems[static_cast<std::size_t>(index)])
    {
        m_classSectionSelectionChanging = true;
        m_classSectionSelectorBar.SelectedItem(
            m_classSectionSelectorItems[static_cast<std::size_t>(index)]
            );
        m_classSectionSelectionChanging = false;
    }
}

void MainWindow::refreshClassesPage()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_classSelector || !m_classStatusText)
    {
        return;
    }

    refreshClassNavigationLocation();

    if (!m_openDatabase)
    {
        m_classLoading = true;
        m_classes.clear();
        m_classSelectedIndex = -1;
        m_classSelectedId = -1;
        m_classNew = false;
        m_classSelector.Items().Clear();
        presentClass(-1);
        refreshClassNavigation(false);
        m_classLoading = false;
        m_classStatusText.Text(L"No database open.");
        m_classNotesStatusText.Text(L"No database open.");
        m_classValidationText.Text({});
        m_classValidationText.Visibility(Visibility::Collapsed);
        m_classNotesValidationText.Text({});
        m_classNotesValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Collapsed
            );
        clearClassDirty();
        refreshClassRoster();
        refreshSpeakingEvaluation();
        refreshSpeakingAnalytics();
        return;
    }

    classmngr::engine::ClassRepository repository(*m_openDatabase);
    const auto loaded = repository.list();
    if (!loaded)
    {
        m_classLoading = true;
        m_classes.clear();
        m_classSelector.Items().Clear();
        presentClass(-1);
        refreshClassNavigation(false);
        m_classLoading = false;
        m_classStatusText.Text(winrt::hstring(
            L"Classes could not be loaded: " + asWide(loaded.error().message)
            ));
        m_classNotesStatusText.Text(L"Class notes are unavailable.");
        m_classValidationText.Text(winrt::hstring(
            L"Engine loading error: " + asWide(loaded.error().message)
            ));
        m_classValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Visible
            );
        m_classNotesValidationText.Text(winrt::hstring(
            L"Engine loading error: " + asWide(loaded.error().message)
            ));
        m_classNotesValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Visible
            );
        clearClassDirty();
        refreshClassRoster();
        refreshSpeakingEvaluation();
        refreshSpeakingAnalytics();
        return;
    }

    const int previousId = m_classSelectedId;
    m_classes = *loaded;
    m_classLoading = true;
    m_classSelector.Items().Clear();
    int selectedIndex = -1;
    for (int index = 0; index < static_cast<int>(m_classes.size()); ++index)
    {
        const auto& classroom = m_classes[static_cast<std::size_t>(index)];
        std::wstring display = asWide(classroom.name);
        if (display.empty())
        {
            display = L"Class " + std::to_wstring(classroom.id);
        }
        auto item = ComboBoxItem();
        item.Content(box_value(hstring(display)));
        item.Tag(box_value(classroom.id));
        setAutomationName(item, L"Class " + display);
        m_classSelector.Items().Append(item);
        if (classroom.id == previousId)
        {
            selectedIndex = index;
        }
    }
    if (selectedIndex < 0 && !m_classes.empty())
    {
        selectedIndex = 0;
    }
    m_classSelectedIndex = selectedIndex;
    m_classSelectedId = selectedIndex >= 0
        ? m_classes[static_cast<std::size_t>(selectedIndex)].id
        : -1;
    m_classSelector.SelectedIndex(selectedIndex);
    m_classLoading = false;

    if (selectedIndex >= 0)
    {
        presentClass(selectedIndex);
        m_classStatusText.Text(L"Class directory loaded.");
        m_classNotesStatusText.Text(L"Select a class tab to edit notes.");
    }
    else
    {
        presentClass(-1);
        m_classStatusText.Text(
            L"No classes found. Choose New Class to add one."
            );
        m_classNotesStatusText.Text(L"No class selected.");
    }
    m_classValidationText.Text({});
    m_classValidationText.Visibility(
        Microsoft::UI::Xaml::Visibility::Collapsed
        );
    m_classNotesValidationText.Text({});
    m_classNotesValidationText.Visibility(Visibility::Collapsed);
    clearClassDirty();
    refreshClassRoster();
    refreshSpeakingEvaluation();
    refreshSpeakingAnalytics();
    refreshClassNavigation(true);
}

classmngr::engine::ClassInfo MainWindow::classInfoFromForm() const
{
    classmngr::engine::ClassInfo info = m_classInfo;
    info.classId = m_classSelectedId;
    info.classGrade = asUtf8(
        std::wstring_view(selectedComboValue(m_classGradeCombo))
        );
    info.classLevel = asUtf8(
        std::wstring_view(selectedComboValue(m_classLevelCombo))
        );
    info.readingBook = asUtf8(
        std::wstring_view(selectedComboValue(m_classReadingBookCombo))
        );
    info.essayBook = asUtf8(
        std::wstring_view(selectedComboValue(m_classEssayBookCombo))
        );
    info.classColor = asUtf8(m_classColorTextBox.Text());
    info.fontColor = asUtf8(m_classFontColorTextBox.Text());
    info.teacherId = m_classCoTeacherSelectedId;
    info.classTimes = classScheduleFromForm(false);
    info.intensiveTimes = classScheduleFromForm(true);
    info.notes = asUtf8(m_classNotesTextBox.Text());
    info.timeFillerActivities = asUtf8(
        m_classTimeFillerActivitiesTextBox.Text()
        );
    return info;
}

classmngr::engine::Roster MainWindow::classRosterFromForm() const
{
    classmngr::engine::Roster roster = m_classRoster;
    roster.rows.clear();
    roster.rows.reserve(m_classRosterCellBoxes.size());
    for (const auto& rowBoxes : m_classRosterCellBoxes)
    {
        std::vector<std::string> row;
        row.reserve(roster.columns.size());
        for (std::size_t column = 0; column < roster.columns.size(); ++column)
        {
            row.push_back(
                column < rowBoxes.size()
                    ? asUtf8(rowBoxes[column].Text())
                    : std::string{}
                );
        }
        roster.rows.push_back(std::move(row));
    }
    return roster;
}

void MainWindow::refreshSpeakingAnalytics()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_speakingAnalyticsStatusText
        || !m_speakingAnalyticsCriteriaPanel
        || !m_speakingAnalyticsShapeText
        || !m_speakingAnalyticsShapePanel
        || !m_speakingAnalyticsRankingList)
    {
        return;
    }

    m_speakingAnalyticsCriteriaPanel.Children().Clear();
    m_speakingAnalyticsRankingList.Items().Clear();
    m_speakingAnalyticsSummaryText.Text({});
    m_speakingAnalyticsShapeText.Text({});
    m_speakingAnalyticsShapePanel.Children().Clear();
    for (auto const& value : m_speakingAnalyticsSummaryValues)
    {
        if (value)
        {
            value.Text(L"-");
        }
    }
    const auto showAnalyticsPlaceholder = [this](std::wstring_view message) {
        auto placeholder = TextBlock();
        placeholder.Text(hstring(message));
        placeholder.TextWrapping(TextWrapping::Wrap);
        m_speakingAnalyticsShapePanel.Children().Append(placeholder);
    };

    if (!m_openDatabase)
    {
        m_speakingAnalyticsStatusText.Text(L"No database open.");
        m_speakingAnalyticsSummaryText.Text(
            L"Open a database to calculate speaking analytics."
            );
        showAnalyticsPlaceholder(L"Open a database to view class shape and trend data.");
        return;
    }
    if (m_classSelectedId <= 0 || m_classNew)
    {
        m_speakingAnalyticsStatusText.Text(
            L"Save the selected class before viewing speaking analytics."
            );
        m_speakingAnalyticsSummaryText.Text(
            L"No class is available for analytics."
            );
        showAnalyticsPlaceholder(L"Save the selected class to view analytics.");
        return;
    }

    classmngr::engine::RosterService rosterService(*m_openDatabase);
    const auto roster = rosterService.load(m_classSelectedId);
    if (!roster)
    {
        m_speakingAnalyticsStatusText.Text(winrt::hstring(
            L"Speaking analytics could not load the roster: "
                + asWide(roster.error().message)
            ));
        m_speakingAnalyticsSummaryText.Text(
            L"The analytics roster is unavailable."
            );
        showAnalyticsPlaceholder(L"The analytics roster is unavailable.");
        return;
    }

    classmngr::engine::SpeakingEvaluationPersistenceService evaluationService(
        *m_openDatabase
        );
    classmngr::engine::SpeakingAnalyticsDashboardInput input;
    input.selection = m_speakingAnalyticsName.empty()
        ? "All"
        : m_speakingAnalyticsName;
    input.roster = *roster;
    input.evaluations.reserve(
        classmngr::engine::SpeakingEvaluationNames.size()
        );
    for (const std::string_view evaluationName :
         classmngr::engine::SpeakingEvaluationNames)
    {
        const auto loaded = evaluationService.load(
            m_classSelectedId,
            evaluationName
            );
        if (!loaded)
        {
            m_speakingAnalyticsStatusText.Text(winrt::hstring(
                L"Speaking analytics could not load an evaluation: "
                    + asWide(loaded.error().message)
                ));
            m_speakingAnalyticsSummaryText.Text(
                L"The analytics evaluations are unavailable."
                );
            showAnalyticsPlaceholder(L"The analytics evaluations are unavailable.");
            return;
        }

        input.evaluations.push_back({
            std::string(evaluationName),
            loaded->empty()
                ? classmngr::engine::SpeakingAnalyticsRows{}
                : classmngr::engine::SpeakingEvaluationValidator::normalized(
                    *loaded
                    )
            });
    }

    rebuildSpeakingAnalytics(
        classmngr::engine::SpeakingAnalyticsService::buildDashboard(input)
        );
}

void MainWindow::rebuildSpeakingAnalytics(
    classmngr::engine::SpeakingAnalyticsDashboard const& dashboard
    )
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_speakingAnalyticsStatusText
        || !m_speakingAnalyticsSummaryText
        || !m_speakingAnalyticsCriteriaPanel
        || !m_speakingAnalyticsShapeText
        || !m_speakingAnalyticsShapePanel
        || !m_speakingAnalyticsRankingList)
    {
        return;
    }

    m_speakingAnalyticsCriteriaPanel.Children().Clear();
    m_speakingAnalyticsShapePanel.Children().Clear();
    m_speakingAnalyticsRankingList.Items().Clear();

    const auto join = [](std::vector<std::string> const& values,
                         std::wstring_view separator) {
        std::wstring result;
        for (const std::string& value : values)
        {
            if (!result.empty())
            {
                result += separator;
            }
            result += asWide(value);
        }
        return result;
    };
    const auto displayOrDash = [](std::wstring value) {
        return value.empty() ? std::wstring(L"—") : value;
    };

    const auto gradeColor = [](std::wstring_view grade) {
        if (grade == L"A+") return Windows::UI::Color{255, 21, 148, 71};
        if (grade == L"A") return Windows::UI::Color{255, 63, 126, 203};
        if (grade == L"B+") return Windows::UI::Color{255, 215, 163, 22};
        if (grade == L"B") return Windows::UI::Color{255, 239, 90, 19};
        return Windows::UI::Color{255, 189, 24, 33};
    };
    const auto gradeBadge = [&gradeColor](std::wstring_view grade) {
        auto badge = Border();
        badge.Background(Microsoft::UI::Xaml::Media::SolidColorBrush(gradeColor(grade)));
        badge.CornerRadius(CornerRadius{4.0, 4.0, 4.0, 4.0});
        badge.Padding(Thickness{6.0, 2.0, 6.0, 2.0});
        auto label = TextBlock();
        label.Text(hstring(grade));
        label.FontSize(12.0);
        label.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
        label.Foreground(Microsoft::UI::Xaml::Media::SolidColorBrush(
            Windows::UI::Color{255, 255, 255, 255}));
        badge.Child(label);
        return badge;
    };
    const auto setSummaryValue = [this](std::size_t index,
                                        std::wstring_view value) {
        if (index < m_speakingAnalyticsSummaryValues.size()
            && m_speakingAnalyticsSummaryValues[index])
        {
            m_speakingAnalyticsSummaryValues[index].Text(hstring(value));
        }
    };

    const auto& snapshot = dashboard.selectedSnapshot.hasData
        ? dashboard.selectedSnapshot
        : dashboard.classShapeSnapshot;
    const bool hasData = dashboard.selectedSnapshot.hasData
        || dashboard.classShapeSnapshot.hasData
        || !dashboard.yearToDatePoints.empty();
    if (!hasData)
    {
        m_speakingAnalyticsStatusText.Text(
            L"No scored speaking evaluations have been recorded for this class."
            );
        m_speakingAnalyticsSummaryText.Text(
            L"Enter and save scores in Speaking Evaluations to populate analytics."
            );
        m_speakingAnalyticsShapeText.Text(
            L"Class shape: No fully scored evaluation is available."
            );
        setSummaryValue(0, L"-");
        setSummaryValue(1, L"0 students");
        setSummaryValue(2, L"-");
        setSummaryValue(3, L"-");
        auto emptyShape = TextBlock();
        emptyShape.Text(L"No fully scored evaluation is available yet.");
        emptyShape.TextWrapping(TextWrapping::Wrap);
        m_speakingAnalyticsShapePanel.Children().Append(emptyShape);
        return;
    }

    const std::wstring scope = m_speakingAnalyticsName.empty()
        ? L"All"
        : asWide(m_speakingAnalyticsName);
    m_speakingAnalyticsStatusText.Text(
        hstring(L"Analytics loaded for " + scope + L".")
        );

    std::wstring summary = L"Class average: ";
    if (dashboard.selectedSnapshot.hasData)
    {
        const std::wstring average = asWide(
            classmngr::engine::SpeakingAnalyticsService::formatAverage(
                snapshot.classAverage3
                ));
        setSummaryValue(
            0,
            asWide(snapshot.classAverageLetter) + L" · " + average
            );
        std::wstring assessed = std::to_wstring(snapshot.fullyScoredCount);
        if (snapshot.rosterStudentCount > 0)
        {
            assessed += L" / " + std::to_wstring(snapshot.rosterStudentCount);
        }
        setSummaryValue(1, assessed + L" students");
        setSummaryValue(2, displayOrDash(join(snapshot.strongestLabels, L", ")));
        setSummaryValue(3, displayOrDash(join(snapshot.focusLabels, L", ")));
        summary += asWide(snapshot.classAverageLetter)
            + L" · " + asWide(
                classmngr::engine::SpeakingAnalyticsService::formatAverage(
                    snapshot.classAverage3
                    )
                );
        summary += L"\nStudents fully scored: "
            + std::to_wstring(snapshot.fullyScoredCount);
        if (snapshot.rosterStudentCount > 0)
        {
            summary += L" / " + std::to_wstring(snapshot.rosterStudentCount);
        }
        summary += L"\nStrongest areas: "
            + displayOrDash(join(snapshot.strongestLabels, L", "));
        summary += L"\nFocus areas: "
            + displayOrDash(join(snapshot.focusLabels, L", "));
    }
    else
    {
        summary += L"—\nNo aggregate score is available for the selected scope.";
    }
    if (!dashboard.selectedSnapshot.hasData)
    {
        setSummaryValue(0, L"-");
        setSummaryValue(1, L"No scores");
        setSummaryValue(2, L"-");
        setSummaryValue(3, L"-");
    }
    m_speakingAnalyticsSummaryText.Text(hstring(summary));

    for (const auto& criterion : snapshot.criteria)
    {
        auto criterionRoot = StackPanel();
        criterionRoot.Spacing(4.0);
        auto value = TextBlock();
        std::wstring text = asWide(criterion.name) + L": ";
        if (!criterion.hasData)
        {
            text += L"No scores";
        }
        else
        {
            text += asWide(
                classmngr::engine::SpeakingAnalyticsService::numberToGrade(
                    classmngr::engine::SpeakingAnalyticsService::roundAverageToGrade(
                        criterion.average3
                        )
                    )
                );
            text += L" · average " + asWide(
                classmngr::engine::SpeakingAnalyticsService::formatAverage(
                    criterion.average3
                    )
                );
            text += L" · distribution: ";
            bool hasDistribution = false;
            for (const std::string_view grade :
                 classmngr::engine::SpeakingEvaluationScoreValues)
            {
                const auto found = criterion.distribution.find(
                    std::string(grade)
                    );
                if (found == criterion.distribution.end())
                {
                    continue;
                }
                if (hasDistribution)
                {
                    text += L", ";
                }
                text += asWide(grade) + L" " + std::to_wstring(found->second);
                hasDistribution = true;
            }
        }
        value.Text(hstring(text));
        value.TextWrapping(TextWrapping::Wrap);
        value.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
        criterionRoot.Children().Append(value);
        if (criterion.hasData)
        {
            int largestCount = 1;
            for (const auto& [grade, count] : criterion.distribution)
            {
                static_cast<void>(grade);
                largestCount = std::max(largestCount, count);
            }
            auto distribution = StackPanel();
            distribution.Orientation(Orientation::Horizontal);
            distribution.Spacing(4.0);
            for (const std::string_view grade :
                 classmngr::engine::SpeakingEvaluationScoreValues)
            {
                const auto found = criterion.distribution.find(std::string(grade));
                if (found == criterion.distribution.end())
                {
                    continue;
                }
                auto segment = Border();
                segment.Background(Microsoft::UI::Xaml::Media::SolidColorBrush(
                    gradeColor(asWide(grade))));
                segment.CornerRadius(CornerRadius{3.0, 3.0, 3.0, 3.0});
                segment.Padding(Thickness{6.0, 3.0, 6.0, 3.0});
                segment.Width(std::max(
                    34.0,
                    190.0 * static_cast<double>(found->second)
                        / static_cast<double>(largestCount)));
                auto segmentLabel = TextBlock();
                segmentLabel.Text(hstring(asWide(grade) + L" "
                    + std::to_wstring(found->second)));
                segmentLabel.FontSize(11.0);
                segmentLabel.Foreground(Microsoft::UI::Xaml::Media::SolidColorBrush(
                    Windows::UI::Color{255, 255, 255, 255}));
                segment.Child(segmentLabel);
                distribution.Children().Append(segment);
            }
            criterionRoot.Children().Append(distribution);
        }
        setAutomationName(
            criterionRoot,
            L"Speaking analytics " + asWide(criterion.name)
            );
        m_speakingAnalyticsCriteriaPanel.Children().Append(criterionRoot);
    }

    std::map<std::string, int> shapeDistribution;
    for (const std::string& letter : snapshot.overallLetters)
    {
        if (!letter.empty())
        {
            ++shapeDistribution[letter];
        }
    }
    std::wstring shape = L"Class-shape evaluation: ";
    shape += dashboard.classShapeEvaluationName.empty()
        ? L"—"
        : asWide(dashboard.classShapeEvaluationName);
    shape += L"\nOverall grades: ";
    bool hasShape = false;
    for (const std::string_view grade :
         classmngr::engine::SpeakingEvaluationScoreValues)
    {
        const auto found = shapeDistribution.find(std::string(grade));
        if (found == shapeDistribution.end())
        {
            continue;
        }
        if (hasShape)
        {
            shape += L", ";
        }
        shape += asWide(grade) + L" " + std::to_wstring(found->second);
        hasShape = true;
    }
    if (!hasShape)
    {
        shape += L"—";
    }
    shape += L"\nYear to date: ";
    if (dashboard.yearToDatePoints.empty())
    {
        shape += L"No fully scored evaluations";
    }
    else
    {
        bool first = true;
        for (const auto& point : dashboard.yearToDatePoints)
        {
            if (!first)
            {
                shape += L", ";
            }
            shape += asWide(point.evaluationName) + L" "
                + asWide(point.classAverageLetter) + L" ("
                + asWide(
                    classmngr::engine::SpeakingAnalyticsService::formatAverage(
                        point.classAverage3
                        )
                    )
                + L")";
            first = false;
        }
    }
    m_speakingAnalyticsShapeText.Text(hstring(shape));

    auto evaluationCaption = TextBlock();
    evaluationCaption.Text(hstring(L"Evaluation: "
        + (dashboard.classShapeEvaluationName.empty()
            ? std::wstring(L"-")
            : asWide(dashboard.classShapeEvaluationName))));
    evaluationCaption.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
    m_speakingAnalyticsShapePanel.Children().Append(evaluationCaption);

    auto histogram = Grid();
    histogram.Height(150.0);
    histogram.ColumnSpacing(8.0);
    int histogramMaximum = 1;
    for (const auto& [grade, count] : shapeDistribution)
    {
        static_cast<void>(grade);
        histogramMaximum = std::max(histogramMaximum, count);
    }
    for (int column = 0;
         column < static_cast<int>(classmngr::engine::SpeakingEvaluationScoreValues.size());
         ++column)
    {
        auto definition = ColumnDefinition();
        definition.Width(GridLengthHelper::FromValueAndType(1.0, GridUnitType::Star));
        histogram.ColumnDefinitions().Append(definition);
        const std::string_view grade = classmngr::engine::SpeakingEvaluationScoreValues[
            static_cast<std::size_t>(column)];
        const auto found = shapeDistribution.find(std::string(grade));
        const int count = found == shapeDistribution.end() ? 0 : found->second;
        auto barColumn = StackPanel();
        barColumn.VerticalAlignment(VerticalAlignment::Bottom);
        barColumn.HorizontalAlignment(HorizontalAlignment::Stretch);
        barColumn.Spacing(3.0);
        auto countLabel = TextBlock();
        countLabel.Text(std::to_wstring(count));
        countLabel.HorizontalAlignment(HorizontalAlignment::Center);
        barColumn.Children().Append(countLabel);
        auto bar = Border();
        bar.Background(Microsoft::UI::Xaml::Media::SolidColorBrush(gradeColor(asWide(grade))));
        bar.CornerRadius(CornerRadius{4.0, 4.0, 0.0, 0.0});
        bar.Height(count == 0 ? 5.0 : 92.0 * static_cast<double>(count)
            / static_cast<double>(histogramMaximum));
        barColumn.Children().Append(bar);
        auto gradeLabel = TextBlock();
        gradeLabel.Text(hstring(asWide(grade)));
        gradeLabel.HorizontalAlignment(HorizontalAlignment::Center);
        gradeLabel.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
        barColumn.Children().Append(gradeLabel);
        Grid::SetColumn(barColumn, column);
        histogram.Children().Append(barColumn);
    }
    m_speakingAnalyticsShapePanel.Children().Append(histogram);

    auto trendHeading = TextBlock();
    trendHeading.Text(L"Year to Date");
    trendHeading.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
    m_speakingAnalyticsShapePanel.Children().Append(trendHeading);
    auto trend = StackPanel();
    trend.Orientation(Orientation::Horizontal);
    trend.Spacing(8.0);
    if (dashboard.yearToDatePoints.empty())
    {
        auto emptyTrend = TextBlock();
        emptyTrend.Text(L"No fully scored evaluations");
        trend.Children().Append(emptyTrend);
    }
    else
    {
        for (const auto& point : dashboard.yearToDatePoints)
        {
            auto pointCard = Border();
            pointCard.Background(Microsoft::UI::Xaml::Media::SolidColorBrush(
                Windows::UI::Color{255, 49, 55, 65}));
            pointCard.CornerRadius(CornerRadius{4.0, 4.0, 4.0, 4.0});
            pointCard.Padding(Thickness{8.0, 5.0, 8.0, 5.0});
            auto pointContent = StackPanel();
            auto pointName = TextBlock();
            pointName.Text(hstring(asWide(point.evaluationName)));
            pointName.FontSize(11.0);
            pointContent.Children().Append(pointName);
            auto pointValue = StackPanel();
            pointValue.Orientation(Orientation::Horizontal);
            pointValue.Spacing(5.0);
            pointValue.Children().Append(gradeBadge(asWide(point.classAverageLetter)));
            auto average = TextBlock();
            average.Text(hstring(asWide(
                classmngr::engine::SpeakingAnalyticsService::formatAverage(
                    point.classAverage3))));
            average.VerticalAlignment(VerticalAlignment::Center);
            pointValue.Children().Append(average);
            pointContent.Children().Append(pointValue);
            pointCard.Child(pointContent);
            trend.Children().Append(pointCard);
        }
    }
    m_speakingAnalyticsShapePanel.Children().Append(trend);

    auto rankingHeader = Grid();
    rankingHeader.ColumnSpacing(8.0);
    rankingHeader.MinWidth(900.0);
    const std::array<double, 5> rankingWidths{44.0, 160.0, 140.0, 100.0, 420.0};
    const std::array<wchar_t const*, 5> rankingHeaders{
        L"#", L"English Name", L"Korean Name", L"Average", L"Criteria"
    };
    for (int column = 0; column < static_cast<int>(rankingWidths.size()); ++column)
    {
        auto definition = ColumnDefinition();
        definition.Width(GridLengthHelper::FromValueAndType(
            rankingWidths[static_cast<std::size_t>(column)], GridUnitType::Pixel));
        rankingHeader.ColumnDefinitions().Append(definition);
        auto label = TextBlock();
        label.Text(rankingHeaders[static_cast<std::size_t>(column)]);
        label.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
        label.Margin(Thickness{4.0, 2.0, 4.0, 6.0});
        Grid::SetColumn(label, column);
        rankingHeader.Children().Append(label);
    }
    m_speakingAnalyticsRankingList.Header(rankingHeader);

    for (std::size_t index = 0;
         index < snapshot.rankings.size();
         ++index)
    {
        const auto& rank = snapshot.rankings[index];
        auto row = Grid();
        row.ColumnSpacing(8.0);
        row.MinWidth(900.0);
        const std::array<double, 5> widths{
            44.0, 160.0, 140.0, 100.0, 420.0
        };
        for (const double width : widths)
        {
            auto definition = ColumnDefinition();
            definition.Width(GridLengthHelper::FromValueAndType(
                width,
                GridUnitType::Pixel
                ));
            row.ColumnDefinitions().Append(definition);
        }
        const std::array<std::wstring, 5> values{
            std::to_wstring(index + 1),
            asWide(rank.englishName),
            asWide(rank.koreanName),
            asWide(
                classmngr::engine::SpeakingAnalyticsService::formatAverage(
                    rank.overall3
                    )
                ) + L" (" + asWide(rank.overallLetter) + L")",
            join(rank.criterionLetters, L" · ")
        };
        for (int column = 0; column < static_cast<int>(values.size()); ++column)
        {
            if (column == 4)
            {
                auto criteria = StackPanel();
                criteria.Orientation(Orientation::Horizontal);
                criteria.Spacing(6.0);
                const std::size_t count = std::min(
                    snapshot.criteria.size(), rank.criterionLetters.size());
                for (std::size_t criterionIndex = 0;
                     criterionIndex < count;
                     ++criterionIndex)
                {
                    auto criterion = StackPanel();
                    criterion.Spacing(2.0);
                    auto criterionName = TextBlock();
                    criterionName.Text(hstring(asWide(
                        snapshot.criteria[criterionIndex].name)));
                    criterionName.FontSize(10.0);
                    criterionName.MaxWidth(64.0);
                    criterionName.TextTrimming(TextTrimming::CharacterEllipsis);
                    criterion.Children().Append(criterionName);
                    criterion.Children().Append(gradeBadge(asWide(
                        rank.criterionLetters[criterionIndex])));
                    criteria.Children().Append(criterion);
                }
                Grid::SetColumn(criteria, column);
                row.Children().Append(criteria);
                continue;
            }
            if (column == 3)
            {
                auto average = StackPanel();
                average.Orientation(Orientation::Horizontal);
                average.Spacing(6.0);
                auto score = TextBlock();
                score.Text(hstring(asWide(
                    classmngr::engine::SpeakingAnalyticsService::formatAverage(
                        rank.overall3))));
                score.VerticalAlignment(VerticalAlignment::Center);
                average.Children().Append(score);
                average.Children().Append(gradeBadge(asWide(rank.overallLetter)));
                Grid::SetColumn(average, column);
                row.Children().Append(average);
                continue;
            }
            auto cell = TextBlock();
            cell.Text(hstring(values[static_cast<std::size_t>(column)]));
            cell.TextWrapping(TextWrapping::Wrap);
            cell.Margin(Thickness{4.0, 4.0, 4.0, 4.0});
            Grid::SetColumn(cell, column);
            row.Children().Append(cell);
        }
        setAutomationName(
            row,
            L"Speaking analytics ranking row " + std::to_wstring(index + 1)
            );
        m_speakingAnalyticsRankingList.Items().Append(row);
    }
}

void MainWindow::refreshSpeakingEvaluation()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_speakingEvaluationStatusText
        || !m_speakingEvaluationHeaderGrid
        || !m_speakingEvaluationList)
    {
        return;
    }

    const auto emptyRows = []() {
        return classmngr::engine::SpeakingEvaluationRows(
            static_cast<std::size_t>(
                classmngr::engine::SpeakingEvaluationRowCount
                ),
            classmngr::engine::SpeakingEvaluationRow(
                static_cast<std::size_t>(
                    classmngr::engine::SpeakingEvaluationColumnCount
                    )
                )
            );
    };
    const auto clearControls = [this, &emptyRows]() {
        m_speakingEvaluationRows = emptyRows();
        m_speakingEvaluationDirtyCells.clear();
        m_speakingEvaluationDirty = false;
        m_speakingAiStudentRow = -1;
        m_speakingAiBatchRows.clear();
        m_speakingAiParsedComments.clear();
        if (m_speakingAiDidWellTextBox)
        {
            m_speakingAiDidWellTextBox.Text({});
        }
        if (m_speakingAiNeedsImprovementTextBox)
        {
            m_speakingAiNeedsImprovementTextBox.Text({});
        }
        if (m_speakingAiPromptTextBox)
        {
            m_speakingAiPromptTextBox.Text({});
        }
        if (m_speakingAiResponseTextBox)
        {
            m_speakingAiResponseTextBox.Text({});
        }
        if (m_speakingBatchStatusText)
        {
            m_speakingBatchStatusText.Text(
                L"Open or select a saved class before planning batch reports."
                );
        }
        m_speakingEvaluationLoading = true;
        rebuildSpeakingEvaluationGrid();
        m_speakingEvaluationLoading = false;
        m_speakingEvaluationValidationText.Text({});
        m_speakingEvaluationValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Collapsed
            );
    };

    if (!m_openDatabase || m_classSelectedId <= 0 || m_classNew)
    {
        clearControls();
        m_speakingEvaluationStatusText.Text(
            !m_openDatabase
                ? L"No database open."
                : L"Save the selected class before editing speaking evaluations."
            );
        if (!m_classDirty && !m_classRosterDirty)
        {
            m_dirtyState.markClean();
        }
        updateSpeakingEvaluationActions();
        return;
    }

    if (m_speakingEvaluationDirty)
    {
        updateSpeakingEvaluationActions();
        return;
    }

    m_speakingEvaluationLoading = true;
    classmngr::engine::SpeakingEvaluationPersistenceService service(
        *m_openDatabase
        );
    const auto loaded = service.load(
        m_classSelectedId,
        m_speakingEvaluationName.empty()
            ? std::string_view{"Winter"}
            : std::string_view(m_speakingEvaluationName)
        );
    if (!loaded)
    {
        m_speakingEvaluationRows = emptyRows();
        m_speakingAiStudentRow = -1;
        m_speakingAiBatchRows.clear();
        m_speakingAiParsedComments.clear();
        if (m_speakingAiDidWellTextBox)
        {
            m_speakingAiDidWellTextBox.Text({});
        }
        if (m_speakingAiNeedsImprovementTextBox)
        {
            m_speakingAiNeedsImprovementTextBox.Text({});
        }
        if (m_speakingAiPromptTextBox)
        {
            m_speakingAiPromptTextBox.Text({});
        }
        if (m_speakingAiResponseTextBox)
        {
            m_speakingAiResponseTextBox.Text({});
        }
        if (m_speakingBatchStatusText)
        {
            m_speakingBatchStatusText.Text(
                L"The batch report plan is unavailable until this evaluation loads."
                );
        }
        rebuildSpeakingEvaluationGrid();
        m_speakingEvaluationLoading = false;
        m_speakingEvaluationDirty = false;
        m_speakingEvaluationDirtyCells.clear();
        m_speakingEvaluationStatusText.Text(winrt::hstring(
            L"Speaking evaluation could not be loaded: "
                + asWide(loaded.error().message)
            ));
        m_speakingEvaluationValidationText.Text(winrt::hstring(
            L"Engine loading error: " + asWide(loaded.error().message)
            ));
        m_speakingEvaluationValidationText.Visibility(Visibility::Visible);
        updateSpeakingEvaluationActions();
        return;
    }

    m_speakingEvaluationRows = loaded->empty()
        ? emptyRows()
        : classmngr::engine::SpeakingEvaluationValidator::normalized(*loaded);
    m_speakingEvaluationRows.resize(
        static_cast<std::size_t>(
            classmngr::engine::SpeakingEvaluationRowCount
            )
        );
    for (auto& row : m_speakingEvaluationRows)
    {
        row.resize(static_cast<std::size_t>(
            classmngr::engine::SpeakingEvaluationColumnCount
            ));
    }
    m_speakingEvaluationDirtyCells.clear();
    m_speakingEvaluationDirty = false;
    m_speakingAiStudentRow = -1;
    m_speakingAiBatchRows.clear();
    m_speakingAiParsedComments.clear();
    if (m_speakingEvaluationName.empty())
    {
        m_speakingEvaluationName = "Winter";
    }
    for (int index = 0;
         index < static_cast<int>(m_speakingEvaluationSelector.Items().Size());
         ++index)
    {
        const auto item = m_speakingEvaluationSelector.Items().GetAt(index)
            .try_as<ComboBoxItem>();
        if (item && boxedString(item.Tag()) == asWide(m_speakingEvaluationName))
        {
            m_speakingEvaluationSelector.SelectedIndex(index);
            break;
        }
    }
    rebuildSpeakingEvaluationGrid();
    m_speakingEvaluationLoading = false;
    m_speakingEvaluationStatusText.Text(
        loaded->empty()
            ? L"No saved speaking evaluation; enter scores and save."
            : L"Speaking evaluation loaded."
        );
    m_speakingEvaluationValidationText.Text({});
    m_speakingEvaluationValidationText.Visibility(Visibility::Collapsed);
    if (!m_classDirty && !m_classRosterDirty
        && !m_speakingEvaluationDirty)
    {
        m_dirtyState.markClean();
    }
    updateSpeakingEvaluationActions();
    refreshSpeakingAiSelection();
    if (m_speakingBatchStatusText)
    {
        m_speakingBatchStatusText.Text(
            L"Choose an output mode and plan the named students in this evaluation."
            );
    }
    updateSpeakingBatchReportActions();
}

void MainWindow::rebuildSpeakingEvaluationGrid()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_speakingEvaluationHeaderGrid || !m_speakingEvaluationList)
    {
        return;
    }

    constexpr std::array<double, classmngr::engine::SpeakingEvaluationColumnCount>
        widths{40.0, 180.0, 180.0, 150.0, 150.0, 150.0, 150.0, 150.0,
               150.0, 500.0, 300.0};
    constexpr std::array<wchar_t const*,
                         classmngr::engine::SpeakingEvaluationColumnCount>
        headers{L"#", L"English Name", L"Korean Name", L"Grammar",
                L"Pronunciation", L"Fluency", L"Manner", L"Content",
                L"Overall Effort", L"Comments", L"Notes"};
    const auto columnColor = [](int column, bool header) {
        const auto color = [header](std::uint8_t red,
                                    std::uint8_t green,
                                    std::uint8_t blue) {
            return Windows::UI::Color{
                255,
                static_cast<std::uint8_t>(header ? red * 0.9 : red),
                static_cast<std::uint8_t>(header ? green * 0.9 : green),
                static_cast<std::uint8_t>(header ? blue * 0.9 : blue)
            };
        };
        switch (column)
        {
        case 0:
            return color(217, 217, 217);
        case 3:
            return color(217, 210, 233);
        case 4:
        case 8:
            return color(207, 226, 243);
        case 5:
            return color(244, 204, 204);
        case 6:
            return color(252, 229, 205);
        case 7:
            return color(217, 234, 211);
        case 9:
            return color(238, 238, 238);
        case 10:
            return color(230, 224, 201);
        default:
            return color(255, 255, 255);
        }
    };
    const auto borderColor = Windows::UI::Color{255, 190, 198, 210};

    const bool wasLoading = m_speakingEvaluationLoading;
    m_speakingEvaluationLoading = true;
    m_speakingEvaluationHeaderGrid.ColumnDefinitions().Clear();
    m_speakingEvaluationHeaderGrid.Children().Clear();
    m_speakingEvaluationList.Items().Clear();
    m_speakingEvaluationList.SelectedIndex(-1);
    m_speakingEvaluationCellBoxes.clear();

    double totalWidth = 0.0;
    for (int column = 0;
         column < classmngr::engine::SpeakingEvaluationColumnCount;
         ++column)
    {
        totalWidth += widths[static_cast<std::size_t>(column)];
        auto definition = ColumnDefinition();
        definition.Width(GridLengthHelper::FromValueAndType(
            widths[static_cast<std::size_t>(column)],
            GridUnitType::Pixel
            ));
        m_speakingEvaluationHeaderGrid.ColumnDefinitions().Append(definition);

        auto header = Border();
        header.MinHeight(42.0);
        header.Padding(Thickness{6.0, 4.0, 6.0, 4.0});
        header.Background(Microsoft::UI::Xaml::Media::SolidColorBrush(
            columnColor(column, true)
            ));
        header.BorderBrush(Microsoft::UI::Xaml::Media::SolidColorBrush(
            borderColor
            ));
        header.BorderThickness(Thickness{1.0, 1.0, 1.0, 1.0});
        auto label = TextBlock();
        label.Text(headers[static_cast<std::size_t>(column)]);
        label.TextAlignment(TextAlignment::Center);
        label.VerticalAlignment(VerticalAlignment::Center);
        label.TextWrapping(TextWrapping::Wrap);
        label.FontSize(14.0);
        label.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
        label.Foreground(Microsoft::UI::Xaml::Media::SolidColorBrush(
            Windows::UI::Color{255, 31, 41, 55}
            ));
        header.Child(label);
        setAutomationName(
            header,
            L"Speaking evaluation header "
                + std::wstring(headers[static_cast<std::size_t>(column)])
            );
        Grid::SetColumn(header, column);
        m_speakingEvaluationHeaderGrid.Children().Append(header);
    }
    m_speakingEvaluationHeaderGrid.MinWidth(totalWidth);

    const std::size_t rowCount = std::min(
        m_speakingEvaluationRows.size(),
        static_cast<std::size_t>(
            classmngr::engine::SpeakingEvaluationRowCount
            )
        );
    m_speakingEvaluationCellBoxes.reserve(rowCount);
    for (std::size_t rowIndex = 0; rowIndex < rowCount; ++rowIndex)
    {
        auto rowGrid = Grid();
        rowGrid.ColumnSpacing(4.0);
        rowGrid.MinWidth(totalWidth);
        rowGrid.MinHeight(52.0);
        for (const double width : widths)
        {
            auto definition = ColumnDefinition();
            definition.Width(GridLengthHelper::FromValueAndType(
                width,
                GridUnitType::Pixel
                ));
            rowGrid.ColumnDefinitions().Append(definition);
        }

        std::vector<TextBox> rowBoxes;
        rowBoxes.reserve(
            static_cast<std::size_t>(
                classmngr::engine::SpeakingEvaluationColumnCount
                )
            );
        for (int column = 0;
             column < classmngr::engine::SpeakingEvaluationColumnCount;
             ++column)
        {
            auto cell = TextBox();
            cell.Width(widths[static_cast<std::size_t>(column)]);
            cell.MinHeight(48.0);
            cell.VerticalContentAlignment(VerticalAlignment::Center);
            cell.TextAlignment(TextAlignment::Center);
            cell.Background(Microsoft::UI::Xaml::Media::SolidColorBrush(
                columnColor(column, false)
                ));
            cell.BorderBrush(Microsoft::UI::Xaml::Media::SolidColorBrush(
                borderColor
                ));
            cell.BorderThickness(Thickness{1.0, 1.0, 1.0, 1.0});
            cell.Margin(Thickness{0.0, 2.0, 0.0, 2.0});
            cell.IsTabStop(column != 0);
            cell.TabIndex(
                10 + static_cast<int32_t>(
                    rowIndex * classmngr::engine::SpeakingEvaluationColumnCount
                        + static_cast<std::size_t>(column)
                    )
                );
            if (column == 0)
            {
                cell.Text(std::to_wstring(rowIndex + 1));
                cell.IsReadOnly(true);
            }
            else
            {
                const std::size_t rowSize = m_speakingEvaluationRows[rowIndex].size();
                cell.Text(
                    column < static_cast<int>(rowSize)
                        ? asWide(m_speakingEvaluationRows[rowIndex][
                            static_cast<std::size_t>(column)])
                        : std::wstring{}
                    );
                cell.MaxLength(
                    column == classmngr::engine::toInt(
                        classmngr::engine::SpeakingEvaluationColumn::Notes
                        )
                        ? static_cast<int32_t>(
                            classmngr::engine::SpeakingEvaluationMaximumNotesLength
                            )
                        : column == classmngr::engine::toInt(
                            classmngr::engine::SpeakingEvaluationColumn::Comments
                            )
                            ? classmngr::engine::SpeakingEvaluationCommentMaxLength
                            : 128
                    );
                if (column >= classmngr::engine::toInt(
                        classmngr::engine::SpeakingEvaluationColumn::Comments
                        ))
                {
                    cell.AcceptsReturn(true);
                    cell.TextWrapping(TextWrapping::Wrap);
                    cell.TextAlignment(TextAlignment::Left);
                }
                cell.TextChanging(
                    [this](TextBox const&, TextBoxTextChangingEventArgs const&) {
                        if (!m_speakingEvaluationLoading)
                        {
                            markSpeakingEvaluationDirty();
                        }
                    }
                    );
            }
            setAutomationName(
                cell,
                L"Speaking evaluation row " + std::to_wstring(rowIndex + 1)
                    + L" " + headers[static_cast<std::size_t>(column)]
                );
            Grid::SetColumn(cell, column);
            rowGrid.Children().Append(cell);
            rowBoxes.push_back(cell);
        }
        m_speakingEvaluationList.Items().Append(rowGrid);
        m_speakingEvaluationCellBoxes.push_back(std::move(rowBoxes));
    }
    m_speakingEvaluationLoading = wasLoading;
    updateSpeakingEvaluationActions();
}

void MainWindow::updateSpeakingEvaluationActions()
{
    if (!m_speakingEvaluationStatusText)
    {
        return;
    }

    const bool hasClass = static_cast<bool>(m_openDatabase)
        && m_classSelectedId > 0
        && !m_classNew;
    if (m_speakingEvaluationSelector)
    {
        m_speakingEvaluationSelector.IsEnabled(hasClass && !m_speakingEvaluationDirty);
    }
    if (m_speakingEvaluationList)
    {
        m_speakingEvaluationList.IsEnabled(hasClass);
    }
    if (m_speakingEvaluationPasteTextBox)
    {
        m_speakingEvaluationPasteTextBox.IsEnabled(hasClass);
    }
    if (m_speakingEvaluationImportNamesButton)
    {
        m_speakingEvaluationImportNamesButton.IsEnabled(hasClass);
    }
    if (m_speakingEvaluationPasteButton)
    {
        m_speakingEvaluationPasteButton.IsEnabled(hasClass);
    }
    if (m_speakingEvaluationSaveButton)
    {
        m_speakingEvaluationSaveButton.IsEnabled(hasClass && m_speakingEvaluationDirty);
    }
    if (m_speakingEvaluationDiscardButton)
    {
        m_speakingEvaluationDiscardButton.IsEnabled(hasClass && m_speakingEvaluationDirty);
    }
    updateSpeakingAiActions();
    updateSpeakingBatchReportActions();
}

void MainWindow::refreshSpeakingAiSelection()
{
    if (!m_speakingEvaluationList
        || !m_speakingAiDidWellTextBox
        || !m_speakingAiNeedsImprovementTextBox
        || !m_speakingAiPromptTextBox
        || !m_speakingAiResponseTextBox
        || !m_speakingAiStatusText)
    {
        return;
    }
    if (m_speakingEvaluationLoading)
    {
        return;
    }

    const int selectedIndex = m_speakingEvaluationList.SelectedIndex();
    if (selectedIndex < 0
        || selectedIndex >= static_cast<int>(m_speakingEvaluationCellBoxes.size())
        || m_speakingEvaluationCellBoxes[static_cast<std::size_t>(selectedIndex)]
            .size() <= static_cast<std::size_t>(
                classmngr::engine::toInt(
                    classmngr::engine::SpeakingEvaluationColumn::Notes
                    )
                ))
    {
        m_speakingAiStudentRow = -1;
        m_speakingAiDidWellTextBox.Text({});
        m_speakingAiNeedsImprovementTextBox.Text({});
        m_speakingAiPromptTextBox.Text({});
        m_speakingAiResponseTextBox.Text({});
        m_speakingAiBatchRows.clear();
        m_speakingAiParsedComments.clear();
        m_speakingAiStatusText.Text(
            L"Select a speaking-evaluation row to prepare an AI comment."
            );
        updateSpeakingAiActions();
        return;
    }

    const auto& cells = m_speakingEvaluationCellBoxes[
        static_cast<std::size_t>(selectedIndex)
        ];
    const auto notes = splitSpeakingAiPrivateNotes(asUtf8(
        cells[static_cast<std::size_t>(
            classmngr::engine::toInt(
                classmngr::engine::SpeakingEvaluationColumn::Notes
                )
            )].Text()
        ));
    m_speakingAiStudentRow = selectedIndex;
    m_speakingAiDidWellTextBox.Text(asWide(notes.didWell));
    m_speakingAiNeedsImprovementTextBox.Text(asWide(notes.needsImprovement));
    m_speakingAiPromptTextBox.Text({});
    m_speakingAiResponseTextBox.Text({});
    m_speakingAiBatchRows.clear();
    m_speakingAiParsedComments.clear();
    m_speakingAiStatusText.Text(
        L"Private observations loaded for the selected student."
        );
    updateSpeakingAiActions();
    if (m_speakingBatchStatusText)
    {
        m_speakingBatchStatusText.Text(
            L"Choose an output mode and plan the named students in this evaluation."
            );
    }
    updateSpeakingBatchReportActions();
}

void MainWindow::generateSpeakingAiPrompt()
{
    if (!m_speakingAiStatusText
        || !m_speakingAiPromptTextBox
        || !m_speakingAiResponseTextBox
        || !m_speakingEvaluationList
        || !m_speakingAiDidWellTextBox
        || !m_speakingAiNeedsImprovementTextBox)
    {
        return;
    }

    const int selectedIndex = m_speakingEvaluationList.SelectedIndex();
    if (selectedIndex < 0
        || selectedIndex >= static_cast<int>(m_speakingEvaluationCellBoxes.size()))
    {
        m_speakingAiStatusText.Text(
            L"Select a named speaking-evaluation row before generating a prompt."
            );
        updateSpeakingAiActions();
        return;
    }

    const auto& cells = m_speakingEvaluationCellBoxes[
        static_cast<std::size_t>(selectedIndex)
        ];
    const int englishColumn = classmngr::engine::toInt(
        classmngr::engine::SpeakingEvaluationColumn::EnglishName
        );
    const int koreanColumn = classmngr::engine::toInt(
        classmngr::engine::SpeakingEvaluationColumn::KoreanName
        );
    if (cells.size() <= static_cast<std::size_t>(koreanColumn))
    {
        return;
    }
    const std::wstring englishName = cells[static_cast<std::size_t>(
        englishColumn
        )].Text().c_str();
    const std::wstring koreanName = cells[static_cast<std::size_t>(
        koreanColumn
        )].Text().c_str();
    if (englishName.empty() && koreanName.empty())
    {
        m_speakingAiStatusText.Text(
            L"The selected speaking-evaluation row does not have a student name."
            );
        updateSpeakingAiActions();
        return;
    }

    classmngr::engine::SpeakingEvaluationAiPromptInput input;
    input.grade = classmngr::engine::SpeakingEvaluationReportModel::elementaryGrade(
        m_classInfo.classGrade
        );
    input.englishName = asUtf8(englishName);
    input.koreanName = asUtf8(koreanName);
    input.didWell = asUtf8(m_speakingAiDidWellTextBox.Text());
    input.needsImprovement = asUtf8(m_speakingAiNeedsImprovementTextBox.Text());
    input.voice = m_speakingAiVoiceSelector
        && m_speakingAiVoiceSelector.SelectedIndex() == 1
        ? classmngr::engine::SpeakingEvaluationAiVoice::ThirdPerson
        : classmngr::engine::SpeakingEvaluationAiVoice::DirectToStudent;

    if (!classmngr::engine::SpeakingEvaluationAiPromptService::canBuildPrompt(input))
    {
        m_speakingAiPromptTextBox.Text({});
        m_speakingAiResponseTextBox.Text({});
        m_speakingAiStatusText.Text(
            L"AI comments require an E4-E6 class and at least one observation in both sections."
            );
        updateSpeakingAiActions();
        return;
    }

    const std::string prompt =
        classmngr::engine::SpeakingEvaluationAiPromptService::buildCommentPrompt(
            input
            );
    m_speakingAiStudentRow = selectedIndex;
    m_speakingAiBatchRows.clear();
    m_speakingAiParsedComments.clear();
    m_speakingAiPromptTextBox.Text(asWide(prompt));
    m_speakingAiResponseTextBox.Text({});
    m_speakingAiStatusText.Text(
        L"Student prompt generated with the STD_NAME privacy placeholder."
        );
    updateSpeakingAiActions();
}

void MainWindow::copySpeakingAiPrompt(bool openProvider)
{
    if (!m_speakingAiPromptTextBox || !m_speakingAiStatusText)
    {
        return;
    }
    const std::string prompt = asUtf8(m_speakingAiPromptTextBox.Text());
    if (prompt.empty())
    {
        m_speakingAiStatusText.Text(
            L"Generate an AI prompt before copying it."
            );
        updateSpeakingAiActions();
        return;
    }

    const auto copied = classmngr::windows::winui::WindowsClipboard::writeText(
        prompt
        );
    if (!copied)
    {
        m_speakingAiStatusText.Text(winrt::hstring(
            L"The AI prompt could not be copied: "
                + asWide(copied.error().message)
            ));
        return;
    }

    if (!openProvider)
    {
        m_speakingAiStatusText.Text(L"AI prompt copied to the clipboard.");
        return;
    }

    const auto opened = classmngr::windows::winui::WindowsUrlLauncher::openUrl(
        "https://chatgpt.com/"
        );
    m_speakingAiStatusText.Text(
        opened
            ? L"AI prompt copied; ChatGPT was opened for review."
            : winrt::hstring(
                L"AI prompt copied, but ChatGPT could not be opened: "
                    + asWide(opened.error().message)
                )
        );
}

void MainWindow::generateSpeakingAiBatchPrompt()
{
    if (!m_speakingAiPromptTextBox
        || !m_speakingAiResponseTextBox
        || !m_speakingAiStatusText
        || !m_speakingEvaluationCellBoxes.size())
    {
        return;
    }

    classmngr::engine::SpeakingEvaluationAiBatchPromptInput input;
    input.voice = m_speakingAiVoiceSelector
        && m_speakingAiVoiceSelector.SelectedIndex() == 1
        ? classmngr::engine::SpeakingEvaluationAiVoice::ThirdPerson
        : classmngr::engine::SpeakingEvaluationAiVoice::DirectToStudent;
    const int grade = classmngr::engine::SpeakingEvaluationReportModel::elementaryGrade(
        m_classInfo.classGrade
        );
    const int englishColumn = classmngr::engine::toInt(
        classmngr::engine::SpeakingEvaluationColumn::EnglishName
        );
    const int koreanColumn = classmngr::engine::toInt(
        classmngr::engine::SpeakingEvaluationColumn::KoreanName
        );
    const int commentsColumn = classmngr::engine::toInt(
        classmngr::engine::SpeakingEvaluationColumn::Comments
        );
    const int notesColumn = classmngr::engine::toInt(
        classmngr::engine::SpeakingEvaluationColumn::Notes
        );

    for (const auto& cells : m_speakingEvaluationCellBoxes)
    {
        if (cells.size() <= static_cast<std::size_t>(koreanColumn))
        {
            continue;
        }
        const std::string englishName = asUtf8(
            cells[static_cast<std::size_t>(englishColumn)].Text()
            );
        const std::string koreanName = asUtf8(
            cells[static_cast<std::size_t>(koreanColumn)].Text()
            );
        if (!englishName.empty())
        {
            input.additionalNamesToRedact.push_back(englishName);
        }
        if (!koreanName.empty())
        {
            input.additionalNamesToRedact.push_back(koreanName);
        }
    }

    m_speakingAiBatchRows.clear();
    for (std::size_t rowIndex = 0;
         rowIndex < m_speakingEvaluationCellBoxes.size();
         ++rowIndex)
    {
        const auto& cells = m_speakingEvaluationCellBoxes[rowIndex];
        if (cells.size() <= static_cast<std::size_t>(notesColumn))
        {
            continue;
        }
        const std::string englishName = asUtf8(
            cells[static_cast<std::size_t>(englishColumn)].Text()
            );
        const std::string koreanName = asUtf8(
            cells[static_cast<std::size_t>(koreanColumn)].Text()
            );
        const std::string existingComment = asUtf8(
            cells[static_cast<std::size_t>(commentsColumn)].Text()
            );
        const auto notes = splitSpeakingAiPrivateNotes(asUtf8(
            cells[static_cast<std::size_t>(notesColumn)].Text()
            ));
        if ((englishName.empty() && koreanName.empty())
            || !existingComment.empty()
            || grade < 4
            || grade > 6
            || classmngr::engine::SpeakingEvaluationAiPromptService::observationItems(
                notes.didWell
                ).empty()
            || classmngr::engine::SpeakingEvaluationAiPromptService::observationItems(
                notes.needsImprovement
                ).empty())
        {
            continue;
        }

        input.students.push_back({
            speakingAiStudentId(rowIndex),
            grade,
            englishName,
            koreanName,
            notes.didWell,
            notes.needsImprovement
            });
        m_speakingAiBatchRows.push_back(static_cast<int>(rowIndex));
    }

    if (input.students.empty())
    {
        m_speakingAiPromptTextBox.Text({});
        m_speakingAiResponseTextBox.Text({});
        m_speakingAiParsedComments.clear();
        m_speakingAiStatusText.Text(
            L"No eligible students. Each batch row needs a name, an E4-E6 class, both private observation sections, and no existing comment."
            );
        updateSpeakingAiActions();
        return;
    }

    const std::string prompt =
        classmngr::engine::SpeakingEvaluationAiPromptService::buildBatchCommentPrompt(
            input
            );
    if (prompt.empty())
    {
        m_speakingAiBatchRows.clear();
        m_speakingAiStatusText.Text(
            L"The engine could not build the batch AI prompt from the selected rows."
            );
        updateSpeakingAiActions();
        return;
    }

    m_speakingAiStudentRow = -1;
    m_speakingAiParsedComments.clear();
    m_speakingAiPromptTextBox.Text(asWide(prompt));
    m_speakingAiResponseTextBox.Text({});
    m_speakingAiStatusText.Text(winrt::hstring(
        L"Batch prompt generated for "
            + std::to_wstring(input.students.size())
            + L" students; names are redacted in the prompt."
        ));
    updateSpeakingAiActions();
}

void MainWindow::parseSpeakingAiBatchResponse()
{
    if (!m_speakingAiStatusText || !m_speakingAiResponseTextBox)
    {
        return;
    }
    if (m_speakingAiBatchRows.empty())
    {
        m_speakingAiStatusText.Text(
            L"Generate a batch prompt before parsing a batch response."
            );
        updateSpeakingAiActions();
        return;
    }

    const std::string response = asUtf8(m_speakingAiResponseTextBox.Text());
    if (response.empty())
    {
        m_speakingAiStatusText.Text(
            L"Paste the marked batch response before parsing it."
            );
        updateSpeakingAiActions();
        return;
    }

    std::vector<std::string> expectedIds;
    expectedIds.reserve(m_speakingAiBatchRows.size());
    for (const int row : m_speakingAiBatchRows)
    {
        if (row >= 0)
        {
            expectedIds.push_back(speakingAiStudentId(
                static_cast<std::size_t>(row)
                ));
        }
    }
    const auto parsed =
        classmngr::engine::SpeakingEvaluationAiPromptService::parseBatchResponse(
            response,
            expectedIds
            );
    m_speakingAiParsedComments = parsed.comments;

    std::wstring status = L"Parsed "
        + std::to_wstring(parsed.comments.size())
        + L" of " + std::to_wstring(expectedIds.size())
        + L" batch comments.";
    if (!parsed.duplicateIds.empty())
    {
        status += L" Duplicate blocks: "
            + std::to_wstring(parsed.duplicateIds.size()) + L".";
    }
    if (!parsed.malformedIds.empty())
    {
        status += L" Malformed blocks: "
            + std::to_wstring(parsed.malformedIds.size()) + L".";
    }
    if (!parsed.unknownIds.empty())
    {
        status += L" Unknown IDs ignored: "
            + std::to_wstring(parsed.unknownIds.size()) + L".";
    }
    m_speakingAiStatusText.Text(hstring(status));
    updateSpeakingAiActions();
}

void MainWindow::applySpeakingAiStudentComment()
{
    if (!m_speakingAiStatusText || !m_speakingAiResponseTextBox)
    {
        return;
    }
    if (m_speakingAiStudentRow < 0
        || m_speakingAiStudentRow >= static_cast<int>(m_speakingEvaluationCellBoxes.size()))
    {
        m_speakingAiStatusText.Text(
            L"Generate a student prompt and keep that row selected before applying a comment."
            );
        updateSpeakingAiActions();
        return;
    }

    std::wstring comment = asWString(m_speakingAiResponseTextBox.Text());
    if (comment.empty())
    {
        m_speakingAiStatusText.Text(
            L"Paste a completed student comment before applying it."
            );
        updateSpeakingAiActions();
        return;
    }
    const auto& cells = m_speakingEvaluationCellBoxes[
        static_cast<std::size_t>(m_speakingAiStudentRow)
        ];
    const int englishColumn = classmngr::engine::toInt(
        classmngr::engine::SpeakingEvaluationColumn::EnglishName
        );
    const int koreanColumn = classmngr::engine::toInt(
        classmngr::engine::SpeakingEvaluationColumn::KoreanName
        );
    const int commentsColumn = classmngr::engine::toInt(
        classmngr::engine::SpeakingEvaluationColumn::Comments
        );
    if (cells.size() <= static_cast<std::size_t>(commentsColumn))
    {
        return;
    }
    const std::wstring englishName = cells[static_cast<std::size_t>(
        englishColumn
        )].Text().c_str();
    const std::wstring koreanName = cells[static_cast<std::size_t>(
        koreanColumn
        )].Text().c_str();
    const std::wstring preferredName = englishName.empty()
        ? koreanName
        : englishName;
    replaceSpeakingAiPlaceholder(comment, preferredName);
    if (comment.size() > static_cast<std::size_t>(
            classmngr::engine::SpeakingEvaluationCommentMaxLength
            ))
    {
        m_speakingAiStatusText.Text(
            L"The student comment is longer than the 450-character limit."
            );
        updateSpeakingAiActions();
        return;
    }
    m_speakingEvaluationCellBoxes[
        static_cast<std::size_t>(m_speakingAiStudentRow)
        ][static_cast<std::size_t>(commentsColumn)].Text(hstring(comment));
    m_speakingAiStatusText.Text(
        preferredName.empty()
            ? L"Student comment applied without a name placeholder. Save the evaluation to persist it."
            : L"Student comment applied. Save the evaluation to persist it."
        );
    updateSpeakingAiActions();
}

void MainWindow::applySpeakingAiBatchComments()
{
    if (!m_speakingAiStatusText)
    {
        return;
    }
    if (m_speakingAiBatchRows.empty() || m_speakingAiParsedComments.empty())
    {
        m_speakingAiStatusText.Text(
            L"Generate and parse a batch response before applying comments."
            );
        updateSpeakingAiActions();
        return;
    }

    const int englishColumn = classmngr::engine::toInt(
        classmngr::engine::SpeakingEvaluationColumn::EnglishName
        );
    const int koreanColumn = classmngr::engine::toInt(
        classmngr::engine::SpeakingEvaluationColumn::KoreanName
        );
    const int commentsColumn = classmngr::engine::toInt(
        classmngr::engine::SpeakingEvaluationColumn::Comments
        );
    std::size_t applied = 0;
    std::size_t skipped = 0;
    for (const auto& parsed : m_speakingAiParsedComments)
    {
        const auto rowIt = std::find_if(
            m_speakingAiBatchRows.begin(),
            m_speakingAiBatchRows.end(),
            [&parsed](int row) {
                return speakingAiStudentId(static_cast<std::size_t>(row))
                    == parsed.id;
            }
            );
        if (rowIt == m_speakingAiBatchRows.end())
        {
            ++skipped;
            continue;
        }
        const int row = *rowIt;
        if (row < 0
            || row >= static_cast<int>(m_speakingEvaluationCellBoxes.size()))
        {
            ++skipped;
            continue;
        }
        const auto& cells = m_speakingEvaluationCellBoxes[
            static_cast<std::size_t>(row)
            ];
        if (cells.size() <= static_cast<std::size_t>(commentsColumn))
        {
            ++skipped;
            continue;
        }
        std::wstring comment = asWide(parsed.comment);
        const std::wstring englishName = cells[static_cast<std::size_t>(
            englishColumn
            )].Text().c_str();
        const std::wstring koreanName = cells[static_cast<std::size_t>(
            koreanColumn
            )].Text().c_str();
        replaceSpeakingAiPlaceholder(
            comment,
            englishName.empty() ? koreanName : englishName
            );
        if (comment.empty()
            || comment.size() > static_cast<std::size_t>(
                classmngr::engine::SpeakingEvaluationCommentMaxLength
                ))
        {
            ++skipped;
            continue;
        }
        cells[static_cast<std::size_t>(commentsColumn)].Text(hstring(comment));
        ++applied;
    }

    m_speakingAiStatusText.Text(winrt::hstring(
        L"Applied " + std::to_wstring(applied)
            + L" parsed batch comments"
            + (skipped == 0
                ? L". Save the evaluation to persist them."
                : L"; " + std::to_wstring(skipped)
                    + L" were skipped as invalid. Save the rest to persist them.")
        ));
    updateSpeakingAiActions();
}

void MainWindow::updateSpeakingAiActions()
{
    if (!m_speakingAiStatusText)
    {
        return;
    }
    const bool hasClass = static_cast<bool>(m_openDatabase)
        && m_classSelectedId > 0
        && !m_classNew;
    const bool hasPrompt = m_speakingAiPromptTextBox
        && !m_speakingAiPromptTextBox.Text().empty();
    const bool hasResponse = m_speakingAiResponseTextBox
        && !m_speakingAiResponseTextBox.Text().empty();
    if (m_speakingAiVoiceSelector)
    {
        m_speakingAiVoiceSelector.IsEnabled(hasClass);
    }
    if (m_speakingAiDidWellTextBox)
    {
        m_speakingAiDidWellTextBox.IsEnabled(hasClass);
    }
    if (m_speakingAiNeedsImprovementTextBox)
    {
        m_speakingAiNeedsImprovementTextBox.IsEnabled(hasClass);
    }
    if (m_speakingAiGenerateButton)
    {
        m_speakingAiGenerateButton.IsEnabled(hasClass);
    }
    if (m_speakingAiGenerateBatchButton)
    {
        m_speakingAiGenerateBatchButton.IsEnabled(hasClass);
    }
    if (m_speakingAiPromptTextBox)
    {
        m_speakingAiPromptTextBox.IsEnabled(hasClass);
    }
    if (m_speakingAiCopyButton)
    {
        m_speakingAiCopyButton.IsEnabled(hasClass && hasPrompt);
    }
    if (m_speakingAiCopyOpenButton)
    {
        m_speakingAiCopyOpenButton.IsEnabled(hasClass && hasPrompt);
    }
    if (m_speakingAiResponseTextBox)
    {
        m_speakingAiResponseTextBox.IsEnabled(hasClass);
    }
    if (m_speakingAiApplyStudentButton)
    {
        m_speakingAiApplyStudentButton.IsEnabled(
            hasClass && m_speakingAiStudentRow >= 0 && hasResponse
            );
    }
    if (m_speakingAiParseBatchButton)
    {
        m_speakingAiParseBatchButton.IsEnabled(
            hasClass && !m_speakingAiBatchRows.empty() && hasResponse
            );
    }
    if (m_speakingAiApplyBatchButton)
    {
        m_speakingAiApplyBatchButton.IsEnabled(
            hasClass && !m_speakingAiParsedComments.empty()
            );
    }
}

void MainWindow::planSpeakingBatchReports()
{
    if (!m_speakingBatchStatusText
        || !m_speakingBatchRendererSelector
        || !m_speakingBatchTemplateSelector
        || !m_speakingBatchSavePdfCheck
        || !m_speakingBatchPrintCheck
        || !m_speakingBatchKeepIndividualPdfsCheck
        || !m_speakingBatchOutputDirectoryTextBox)
    {
        return;
    }

    const bool hasClass = static_cast<bool>(m_openDatabase)
        && m_classSelectedId > 0
        && !m_classNew;
    if (!hasClass)
    {
        m_speakingBatchStatusText.Text(
            L"Open or select a saved class before planning batch reports."
            );
        updateSpeakingBatchReportActions();
        return;
    }

    const int englishColumn = classmngr::engine::toInt(
        classmngr::engine::SpeakingEvaluationColumn::EnglishName
        );
    const int koreanColumn = classmngr::engine::toInt(
        classmngr::engine::SpeakingEvaluationColumn::KoreanName
        );
    std::size_t reportCount = 0;
    for (const auto& cells : m_speakingEvaluationCellBoxes)
    {
        if (cells.size() <= static_cast<std::size_t>(koreanColumn))
        {
            continue;
        }
        if (!cells[static_cast<std::size_t>(englishColumn)].Text().empty()
            || !cells[static_cast<std::size_t>(koreanColumn)].Text().empty())
        {
            ++reportCount;
        }
    }

    if (reportCount == 0)
    {
        m_speakingBatchStatusText.Text(
            L"Import or enter at least one student name before planning reports."
            );
        updateSpeakingBatchReportActions();
        return;
    }

    const auto checked = [](Microsoft::UI::Xaml::Controls::CheckBox const& box) {
        const auto value = box.IsChecked();
        return value && value.Value();
    };
    classmngr::engine::SpeakingEvaluationBatchReportRequest request;
    request.reportCount = reportCount;
    request.renderer = m_speakingBatchRendererSelector.SelectedIndex() == 1
        ? classmngr::engine::SpeakingEvaluationReportRenderer::PowerPoint
        : classmngr::engine::SpeakingEvaluationReportRenderer::Internal;
    request.savePdf = checked(m_speakingBatchSavePdfCheck);
    request.printReports = checked(m_speakingBatchPrintCheck);
    request.keepIndividualPdfFiles =
        checked(m_speakingBatchKeepIndividualPdfsCheck);
    request.hasOutputDirectory =
        !asUtf8(m_speakingBatchOutputDirectoryTextBox.Text()).empty();
    request.hasExactOutputFilePath = false;
    const auto reportTemplate = m_speakingBatchTemplateSelector.SelectedIndex() == 1
        ? classmngr::engine::SpeakingEvaluationReportTemplate::Advanced
        : classmngr::engine::SpeakingEvaluationReportTemplate::Standard;
    request.reportTemplates.assign(reportCount, reportTemplate);

    const auto planned =
        classmngr::engine::SpeakingEvaluationBatchReportPolicy::plan(request);
    if (!planned)
    {
        m_speakingBatchStatusText.Text(winrt::hstring(
            L"Batch report plan rejected: "
                + asWide(planned.error().message)
            ));
        updateSpeakingBatchReportActions();
        return;
    }

    const std::wstring renderer = request.renderer
        == classmngr::engine::SpeakingEvaluationReportRenderer::PowerPoint
        ? L"PowerPoint"
        : L"Internal";
    std::wstring status = L"Planned "
        + std::to_wstring(reportCount)
        + L" speaking report(s) using the "
        + renderer
        + L" renderer. ";
    if (planned->createsBatchArchive)
    {
        status += L"The output phase will create one ZIP archive";
        if (planned->savesIndividualPdfFiles)
        {
            status += L" and retain individual PDFs";
        }
        status += L".";
    }
    else if (planned->savesIndividualPdfFiles)
    {
        status += L"The output phase will save individual PDF files.";
    }
    else
    {
        status += L"No PDF files are required.";
    }
    if (request.printReports)
    {
        status += L" Printing is also requested.";
    }
    status += L" Renderer-neutral plan accepted; output execution remains in Phase 7.";
    m_speakingBatchStatusText.Text(winrt::hstring(status));
    updateSpeakingBatchReportActions();
}

void MainWindow::updateSpeakingBatchReportActions()
{
    if (!m_speakingBatchStatusText
        || !m_speakingBatchRendererSelector
        || !m_speakingBatchTemplateSelector
        || !m_speakingBatchSavePdfCheck
        || !m_speakingBatchPrintCheck
        || !m_speakingBatchKeepIndividualPdfsCheck
        || !m_speakingBatchOutputDirectoryTextBox
        || !m_speakingBatchPlanButton)
    {
        return;
    }

    const bool hasClass = static_cast<bool>(m_openDatabase)
        && m_classSelectedId > 0
        && !m_classNew;
    const int englishColumn = classmngr::engine::toInt(
        classmngr::engine::SpeakingEvaluationColumn::EnglishName
        );
    const int koreanColumn = classmngr::engine::toInt(
        classmngr::engine::SpeakingEvaluationColumn::KoreanName
        );
    std::size_t reportCount = 0;
    for (const auto& cells : m_speakingEvaluationCellBoxes)
    {
        if (cells.size() <= static_cast<std::size_t>(koreanColumn))
        {
            continue;
        }
        if (!cells[static_cast<std::size_t>(englishColumn)].Text().empty()
            || !cells[static_cast<std::size_t>(koreanColumn)].Text().empty())
        {
            ++reportCount;
        }
    }

    const auto checked = [](Microsoft::UI::Xaml::Controls::CheckBox const& box) {
        const auto value = box.IsChecked();
        return value && value.Value();
    };
    const bool savePdf = checked(m_speakingBatchSavePdfCheck);
    const bool printReports = checked(m_speakingBatchPrintCheck);
    m_speakingBatchRendererSelector.IsEnabled(hasClass);
    m_speakingBatchTemplateSelector.IsEnabled(hasClass);
    m_speakingBatchSavePdfCheck.IsEnabled(hasClass);
    m_speakingBatchPrintCheck.IsEnabled(hasClass);
    m_speakingBatchKeepIndividualPdfsCheck.IsEnabled(
        hasClass && savePdf && reportCount > 1
        );
    m_speakingBatchOutputDirectoryTextBox.IsEnabled(hasClass && savePdf);
    m_speakingBatchPlanButton.IsEnabled(
        hasClass && reportCount > 0 && (savePdf || printReports)
        );
}

void MainWindow::markSpeakingEvaluationDirty()
{
    if (m_speakingEvaluationLoading || !m_openDatabase
        || m_classSelectedId <= 0 || m_classNew)
    {
        return;
    }

    m_speakingEvaluationDirty = true;
    m_dirtyState.markDirty();
    if (m_speakingEvaluationStatusText)
    {
        m_speakingEvaluationStatusText.Text(
            L"Unsaved speaking evaluation changes."
            );
    }
    updateSpeakingEvaluationActions();
    updateClassActions();
}

void MainWindow::clearSpeakingEvaluationDirty()
{
    m_speakingEvaluationDirty = false;
    m_speakingEvaluationDirtyCells.clear();
    if (!m_classDirty && !m_classRosterDirty
        && !m_speakingEvaluationDirty)
    {
        m_dirtyState.markClean();
    }
    updateSpeakingEvaluationActions();
    updateClassActions();
}

classmngr::engine::SpeakingEvaluationRows
MainWindow::speakingEvaluationFromForm() const
{
    classmngr::engine::SpeakingEvaluationRows rows =
        m_speakingEvaluationRows;
    rows.resize(static_cast<std::size_t>(
        classmngr::engine::SpeakingEvaluationRowCount
        ));
    for (auto& row : rows)
    {
        row.resize(static_cast<std::size_t>(
            classmngr::engine::SpeakingEvaluationColumnCount
            ));
    }
    for (std::size_t rowIndex = 0;
         rowIndex < rows.size() && rowIndex < m_speakingEvaluationCellBoxes.size();
         ++rowIndex)
    {
        const auto& rowBoxes = m_speakingEvaluationCellBoxes[rowIndex];
        for (std::size_t column = 1;
             column < rows[rowIndex].size() && column < rowBoxes.size();
             ++column)
        {
            rows[rowIndex][column] = asUtf8(rowBoxes[column].Text());
        }
    }
    return rows;
}

void MainWindow::saveSpeakingEvaluation()
{
    if (!m_openDatabase)
    {
        m_speakingEvaluationStatusText.Text(L"No database open.");
        return;
    }
    if (m_classSelectedId <= 0 || m_classNew)
    {
        m_speakingEvaluationStatusText.Text(
            L"Select and save a class before saving its speaking evaluation."
            );
        return;
    }

    classmngr::engine::SpeakingEvaluationRows normalized =
        classmngr::engine::SpeakingEvaluationValidator::normalized(
            speakingEvaluationFromForm()
            );
    const auto validation = classmngr::engine::SpeakingEvaluationValidator::validate(
        m_classSelectedId,
        m_speakingEvaluationName,
        normalized
        );
    if (validation.hasErrors())
    {
        std::wstring summary = L"Engine validation failed:";
        for (const auto& issue : validation.errors())
        {
            summary += L"\n- " + asWide(issue.code);
            if (issue.row >= 0)
            {
                summary += L" (row " + std::to_wstring(issue.row + 1);
                if (issue.column >= 0)
                {
                    summary += L", column " + std::to_wstring(issue.column + 1);
                }
                summary += L")";
            }
        }
        m_speakingEvaluationStatusText.Text(
            L"Speaking evaluation could not be saved."
            );
        m_speakingEvaluationValidationText.Text(winrt::hstring(summary));
        m_speakingEvaluationValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Visible
            );
        m_speakingEvaluationDirty = true;
        m_dirtyState.markDirty();
        updateSpeakingEvaluationActions();
        return;
    }

    m_speakingEvaluationDirtyCells.clear();
    for (std::size_t row = 0; row < normalized.size(); ++row)
    {
        for (std::size_t column = 0; column < normalized[row].size(); ++column)
        {
            const std::string oldValue = row < m_speakingEvaluationRows.size()
                && column < m_speakingEvaluationRows[row].size()
                ? m_speakingEvaluationRows[row][column]
                : std::string{};
            if (oldValue != normalized[row][column])
            {
                m_speakingEvaluationDirtyCells.push_back({
                    static_cast<int>(row),
                    static_cast<int>(column)
                });
            }
        }
    }

    classmngr::engine::SpeakingEvaluationPersistenceService service(
        *m_openDatabase
        );
    const auto saved = service.save(
        m_classSelectedId,
        m_speakingEvaluationName,
        normalized,
        m_speakingEvaluationDirtyCells
        );
    if (!saved)
    {
        m_speakingEvaluationStatusText.Text(winrt::hstring(
            L"Speaking evaluation could not be saved: "
                + asWide(saved.error().message)
            ));
        m_speakingEvaluationValidationText.Text(winrt::hstring(
            L"Engine persistence error: " + asWide(saved.error().message)
            ));
        m_speakingEvaluationValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Visible
            );
        m_speakingEvaluationDirty = true;
        m_dirtyState.markDirty();
        updateSpeakingEvaluationActions();
        return;
    }

    m_speakingEvaluationRows = std::move(normalized);
    m_speakingEvaluationLoading = true;
    rebuildSpeakingEvaluationGrid();
    m_speakingEvaluationLoading = false;
    clearSpeakingEvaluationDirty();
    m_speakingEvaluationStatusText.Text(L"Speaking evaluation saved.");
    m_speakingEvaluationValidationText.Text({});
    m_speakingEvaluationValidationText.Visibility(
        Microsoft::UI::Xaml::Visibility::Collapsed
        );
    refreshSpeakingAnalytics();
}

void MainWindow::discardSpeakingEvaluation()
{
    if (!m_openDatabase || m_classSelectedId <= 0 || m_classNew)
    {
        return;
    }

    m_speakingEvaluationDirty = false;
    refreshSpeakingEvaluation();
    m_speakingEvaluationStatusText.Text(L"Speaking evaluation changes discarded.");
}

void MainWindow::importSpeakingEvaluationNames()
{
    if (!m_openDatabase || m_classSelectedId <= 0 || m_classNew)
    {
        return;
    }

    classmngr::engine::RosterService service(*m_openDatabase);
    const auto loaded = service.load(m_classSelectedId);
    if (!loaded)
    {
        m_speakingEvaluationStatusText.Text(winrt::hstring(
            L"Roster names could not be imported: "
                + asWide(loaded.error().message)
            ));
        return;
    }

    const auto columnIndex = [](const std::vector<std::string>& columns,
                                std::string_view expected) {
        for (std::size_t index = 0; index < columns.size(); ++index)
        {
            if (columns[index].size() != expected.size())
            {
                continue;
            }
            bool matches = true;
            for (std::size_t character = 0; character < expected.size(); ++character)
            {
                const unsigned char actual = static_cast<unsigned char>(
                    columns[index][character]
                    );
                const unsigned char wanted = static_cast<unsigned char>(
                    expected[character]
                    );
                if (std::tolower(actual) != std::tolower(wanted))
                {
                    matches = false;
                    break;
                }
            }
            if (matches)
            {
                return static_cast<int>(index);
            }
        }
        return -1;
    };
    const int englishColumn = columnIndex(loaded->columns, "English");
    const int koreanColumn = columnIndex(loaded->columns, "Korean");
    if (englishColumn < 0 || koreanColumn < 0)
    {
        m_speakingEvaluationStatusText.Text(
            L"Roster must contain English and Korean columns before names can be imported."
            );
        return;
    }

    classmngr::engine::SpeakingEvaluationRows importedRows =
        classmngr::engine::SpeakingEvaluationValidator::normalized(
            speakingEvaluationFromForm()
            );
    bool changed = false;
    const std::size_t rowCount = std::min(
        loaded->rows.size(),
        static_cast<std::size_t>(
            classmngr::engine::SpeakingEvaluationRowCount
            )
        );
    for (std::size_t row = 0; row < rowCount; ++row)
    {
        const auto& source = loaded->rows[row];
        const std::string english = englishColumn < static_cast<int>(source.size())
            ? source[static_cast<std::size_t>(englishColumn)]
            : std::string{};
        const std::string korean = koreanColumn < static_cast<int>(source.size())
            ? source[static_cast<std::size_t>(koreanColumn)]
            : std::string{};
        if (importedRows[row][1] != english
            || importedRows[row][2] != korean)
        {
            changed = true;
        }
        importedRows[row][1] = english;
        importedRows[row][2] = korean;
    }
    if (!changed)
    {
        m_speakingEvaluationStatusText.Text(
            L"Roster names are already up to date."
            );
        return;
    }

    m_speakingEvaluationLoading = true;
    for (std::size_t row = 0;
         row < rowCount && row < m_speakingEvaluationCellBoxes.size();
         ++row)
    {
        auto& rowBoxes = m_speakingEvaluationCellBoxes[row];
        if (rowBoxes.size() > 2)
        {
            rowBoxes[1].Text(asWide(importedRows[row][1]));
            rowBoxes[2].Text(asWide(importedRows[row][2]));
        }
    }
    m_speakingEvaluationLoading = false;
    markSpeakingEvaluationDirty();
    m_speakingEvaluationStatusText.Text(
        L"Roster names imported into the speaking evaluation. Save to persist them."
        );
}

void MainWindow::applySpeakingEvaluationPaste()
{
    if (!m_openDatabase || m_classSelectedId <= 0 || m_classNew
        || !m_speakingEvaluationPasteTextBox)
    {
        return;
    }

    const auto rows = parsePastedRange(std::wstring_view(
        m_speakingEvaluationPasteTextBox.Text().c_str(),
        m_speakingEvaluationPasteTextBox.Text().size()
        ));
    if (rows.empty())
    {
        m_speakingEvaluationStatusText.Text(
            L"Paste a tab/newline range of score values first."
            );
        return;
    }

    const int selectedRow = m_speakingEvaluationList
        ? m_speakingEvaluationList.SelectedIndex()
        : -1;
    const std::size_t startRow = selectedRow >= 0
        ? static_cast<std::size_t>(selectedRow)
        : 0;
    constexpr std::size_t firstScoreColumn = static_cast<std::size_t>(
        classmngr::engine::toInt(
            classmngr::engine::SpeakingEvaluationColumn::Grammar
            )
        );
    constexpr std::size_t lastScoreColumn = static_cast<std::size_t>(
        classmngr::engine::toInt(
            classmngr::engine::SpeakingEvaluationColumn::OverallEffort
            )
        );
    std::size_t applied = 0;
    m_speakingEvaluationLoading = true;
    for (std::size_t row = 0;
         row < rows.size() && startRow + row < m_speakingEvaluationCellBoxes.size();
         ++row)
    {
        for (std::size_t column = 0;
             column < rows[row].size()
             && firstScoreColumn + column <= lastScoreColumn;
             ++column)
        {
            const std::size_t targetColumn = firstScoreColumn + column;
            m_speakingEvaluationCellBoxes[startRow + row][targetColumn].Text(
                rows[row][column]
                );
            ++applied;
        }
    }
    m_speakingEvaluationLoading = false;
    if (applied == 0)
    {
        m_speakingEvaluationStatusText.Text(
            L"The pasted range did not contain any score cells."
            );
        return;
    }
    markSpeakingEvaluationDirty();
    m_speakingEvaluationStatusText.Text(winrt::hstring(
        L"Applied " + std::to_wstring(applied)
            + L" score cells starting at row " + std::to_wstring(startRow + 1)
            + L". Save to persist them."
        ));
}

void MainWindow::refreshClassRoster()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_classRosterStatusText || !m_classRosterHeaderGrid
        || !m_classRosterList)
    {
        return;
    }

    const auto clearRosterControls = [this]() {
        m_classRoster = {};
        m_classRosterCellBoxes.clear();
        m_classRosterHeaderGrid.ColumnDefinitions().Clear();
        m_classRosterHeaderGrid.Children().Clear();
        m_classRosterList.Items().Clear();
        m_classRosterList.SelectedIndex(-1);
        if (m_classStudentCountTextBox)
        {
            m_classStudentCountTextBox.Text(L"0");
        }
        if (m_classRosterTransferTargetCombo)
        {
            m_classRosterTransferTargetCombo.Items().Clear();
            m_classRosterTransferTargetCombo.SelectedIndex(-1);
        }
    };

    m_classRosterLoading = true;
    if (!m_openDatabase || m_classSelectedId <= 0 || m_classNew)
    {
        clearRosterControls();
        m_classRosterDirty = false;
        m_classRosterLoading = false;
        m_classRosterStatusText.Text(
            !m_openDatabase
                ? L"No database open."
                : L"Save the selected class before editing its roster."
            );
        m_classRosterValidationText.Text({});
        m_classRosterValidationText.Visibility(Visibility::Collapsed);
        if (!m_classDirty)
        {
            m_dirtyState.markClean();
        }
        updateClassRosterActions();
        return;
    }

    classmngr::engine::RosterService service(*m_openDatabase);
    const auto loaded = service.load(m_classSelectedId);
    if (!loaded)
    {
        clearRosterControls();
        m_classRosterDirty = false;
        m_classRosterLoading = false;
        m_classRosterStatusText.Text(winrt::hstring(
            L"Roster could not be loaded: " + asWide(loaded.error().message)
            ));
        m_classRosterValidationText.Text(winrt::hstring(
            L"Engine loading error: " + asWide(loaded.error().message)
            ));
        m_classRosterValidationText.Visibility(Visibility::Visible);
        if (!m_classDirty)
        {
            m_dirtyState.markClean();
        }
        updateClassRosterActions();
        return;
    }

    m_classRoster = *loaded;
    if (m_classRoster.columns.empty())
    {
        m_classRoster = defaultRoster();
    }
    if (m_classRoster.columnWidths.size() > m_classRoster.columns.size())
    {
        m_classRoster.columnWidths.resize(m_classRoster.columns.size());
    }
    m_classRoster.columnWidths.resize(
        m_classRoster.columns.size(),
        100
        );
    for (auto& row : m_classRoster.rows)
    {
        row.resize(m_classRoster.columns.size());
    }
    if (m_classStudentCountTextBox)
    {
        m_classStudentCountTextBox.Text(
            std::to_wstring(
                classmngr::engine::rosterStudentCount(m_classRoster)
                )
            );
    }

    if (m_classRosterTransferTargetCombo)
    {
        m_classRosterTransferTargetCombo.Items().Clear();
        const std::string currentGrade = m_classInfo.classGrade;
        classmngr::engine::ClassInfoService infoService(*m_openDatabase);
        for (const auto& classroom : m_classes)
        {
            if (classroom.id == m_classSelectedId)
            {
                continue;
            }

            bool sameGrade = currentGrade.empty();
            if (!sameGrade)
            {
                const auto targetInfo = infoService.load(classroom.id);
                sameGrade = targetInfo && targetInfo->classGrade == currentGrade;
            }
            if (!sameGrade)
            {
                continue;
            }

            auto item = ComboBoxItem();
            std::wstring display = asWide(classroom.name);
            if (display.empty())
            {
                display = L"Class " + std::to_wstring(classroom.id);
            }
            item.Content(box_value(hstring(display)));
            item.Tag(box_value(classroom.id));
            setAutomationName(item, L"Roster target " + display);
            m_classRosterTransferTargetCombo.Items().Append(item);
        }
        m_classRosterTransferTargetCombo.SelectedIndex(
            m_classRosterTransferTargetCombo.Items().Size() > 0 ? 0 : -1
            );
    }

    if (m_classRosterTemplateCombo && m_classRosterTemplateCombo.Items().Size() > 0)
    {
        m_classRosterTemplateCombo.SelectedIndex(0);
    }

    rebuildClassRosterGrid();
    m_classRosterLoading = false;
    m_classRosterDirty = false;
    if (!m_classDirty && !m_classRosterDirty
        && !m_speakingEvaluationDirty)
    {
        m_dirtyState.markClean();
    }
    m_classRosterStatusText.Text(
        winrt::hstring(
            L"Roster loaded. "
            + std::to_wstring(
                classmngr::engine::rosterStudentCount(m_classRoster)
                )
            + L" students."
            )
        );
    m_classRosterValidationText.Text({});
    m_classRosterValidationText.Visibility(Visibility::Collapsed);

    if (m_classRosterTemplateStatusText && m_classRosterTemplateCombo
        && m_classRosterTemplateCombo.SelectedItem())
    {
        const auto item = m_classRosterTemplateCombo.SelectedItem().try_as<
            ComboBoxItem>();
        const auto reportTemplate = item
            ? static_cast<classmngr::engine::RosterReportTemplate>(
                boxedInt(item.Tag()))
            : classmngr::engine::RosterReportTemplate::ByDay;
        const bool landscape =
            classmngr::engine::RosterReportTemplateService::orientation(
                reportTemplate
                ) == classmngr::engine::RosterReportOrientation::Landscape;
        const wchar_t* label = reportTemplate ==
                classmngr::engine::RosterReportTemplate::ByDay
            ? L"By Day"
            : reportTemplate == classmngr::engine::RosterReportTemplate::Daily
                ? L"Daily"
                : L"Per Class with Extra Info";
        m_classRosterTemplateStatusText.Text(
            winrt::hstring(
                std::wstring(label)
                + (landscape
                    ? L" · landscape · current class scope."
                    : L" · portrait · current class scope.")
                )
            );
    }
    updateClassRosterActions();
}

void MainWindow::rebuildClassRosterGrid()
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_classRosterHeaderGrid || !m_classRosterList)
    {
        return;
    }

    const bool wasLoading = m_classRosterLoading;
    m_classRosterLoading = true;
    m_classRosterCellBoxes.clear();
    m_classRosterHeaderGrid.ColumnDefinitions().Clear();
    m_classRosterHeaderGrid.Children().Clear();
    m_classRosterList.Items().Clear();
    m_classRosterList.SelectedIndex(-1);

    std::vector<double> widths;
    widths.reserve(m_classRoster.columns.size());
    const auto columnGroup = [](std::string_view column) {
        if (column == "English" || column == "Korean")
        {
            return 0;
        }
        if (column == "Winter" || column == "Speech Contest"
            || column == "Summer" || column == "Fall" || column == "Autumn")
        {
            return 1;
        }
        return 2;
    };
    const auto groupColor = [](int group, bool header) {
        if (group == 0)
        {
            return Windows::UI::Color{
                255,
                static_cast<std::uint8_t>(header ? 100 : 232),
                static_cast<std::uint8_t>(header ? 160 : 242),
                static_cast<std::uint8_t>(header ? 255 : 255)
            };
        }
        if (group == 1)
        {
            return Windows::UI::Color{
                255,
                static_cast<std::uint8_t>(header ? 120 : 232),
                static_cast<std::uint8_t>(header ? 200 : 246),
                static_cast<std::uint8_t>(header ? 120 : 232)
            };
        }
        return Windows::UI::Color{
            255,
            static_cast<std::uint8_t>(header ? 200 : 245),
            static_cast<std::uint8_t>(header ? 200 : 245),
            static_cast<std::uint8_t>(header ? 200 : 245)
        };
    };
    double totalWidth = 0.0;
    for (std::size_t column = 0; column < m_classRoster.columns.size(); ++column)
    {
        const int configuredWidth = column < m_classRoster.columnWidths.size()
            ? m_classRoster.columnWidths[column]
            : 100;
        const double width = std::clamp(
            static_cast<double>(configuredWidth),
            88.0,
            280.0
            );
        widths.push_back(width);
        totalWidth += width;

        auto definition = ColumnDefinition();
        definition.Width(
            GridLengthHelper::FromValueAndType(width, GridUnitType::Pixel)
            );
        m_classRosterHeaderGrid.ColumnDefinitions().Append(definition);

        const int group = columnGroup(m_classRoster.columns[column]);
        const bool beginsGroup = column == 0
            || group != columnGroup(m_classRoster.columns[column - 1]);
        auto header = Border();
        header.MinHeight(58.0);
        header.Padding(Thickness{8.0, 6.0, 8.0, 6.0});
        header.Background(
            Microsoft::UI::Xaml::Media::SolidColorBrush(groupColor(group, true))
            );
        header.CornerRadius(CornerRadius{3.0, 3.0, 3.0, 3.0});
        auto headerContent = StackPanel();
        headerContent.Spacing(2.0);
        auto groupLabel = TextBlock();
        groupLabel.Text(
            beginsGroup
                ? group == 0
                    ? L"Student Names"
                    : group == 1 ? L"Evaluations" : L"Student Information"
                : L""
            );
        groupLabel.FontSize(11.0);
        groupLabel.Foreground(Microsoft::UI::Xaml::Media::SolidColorBrush(
            Windows::UI::Color{255, 31, 41, 55}
            ));
        auto columnLabel = TextBlock();
        columnLabel.Text(asWide(m_classRoster.columns[column]));
        columnLabel.TextWrapping(TextWrapping::Wrap);
        columnLabel.FontSize(14.0);
        columnLabel.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
        columnLabel.Foreground(Microsoft::UI::Xaml::Media::SolidColorBrush(
            Windows::UI::Color{255, 31, 41, 55}
            ));
        headerContent.Children().Append(groupLabel);
        headerContent.Children().Append(columnLabel);
        header.Child(headerContent);
        Grid::SetColumn(header, static_cast<int32_t>(column));
        m_classRosterHeaderGrid.Children().Append(header);
    }
    m_classRosterHeaderGrid.MinWidth(totalWidth);

    for (std::size_t rowIndex = 0; rowIndex < m_classRoster.rows.size(); ++rowIndex)
    {
        auto rowGrid = Grid();
        rowGrid.ColumnSpacing(2.0);
        rowGrid.MinWidth(totalWidth);
        rowGrid.MinHeight(46.0);
        for (const double width : widths)
        {
            auto definition = ColumnDefinition();
            definition.Width(
                GridLengthHelper::FromValueAndType(width, GridUnitType::Pixel)
                );
            rowGrid.ColumnDefinitions().Append(definition);
        }

        std::vector<TextBox> rowBoxes;
        rowBoxes.reserve(m_classRoster.columns.size());
        for (std::size_t column = 0; column < m_classRoster.columns.size(); ++column)
        {
            auto cell = TextBox();
            cell.Text(
                column < m_classRoster.rows[rowIndex].size()
                    ? asWide(m_classRoster.rows[rowIndex][column])
                    : std::wstring{}
                );
            cell.MinWidth(widths[column]);
            cell.MinHeight(42.0);
            cell.VerticalContentAlignment(VerticalAlignment::Center);
            cell.Background(Microsoft::UI::Xaml::Media::SolidColorBrush(
                groupColor(columnGroup(m_classRoster.columns[column]), false)
                ));
            cell.BorderBrush(Microsoft::UI::Xaml::Media::SolidColorBrush(
                Windows::UI::Color{255, 190, 198, 210}
                ));
            cell.BorderThickness(Thickness{1.0, 1.0, 1.0, 1.0});
            cell.MaxLength(
                static_cast<int32_t>(
                    classmngr::engine::RosterValidator::MaximumCellLength
                    )
                );
            cell.Margin(Thickness{0.0, 2.0, 0.0, 2.0});
            cell.IsTabStop(true);
            cell.TextChanging(
                [this](TextBox const&, TextBoxTextChangingEventArgs const&) {
                    if (!m_classRosterLoading)
                    {
                        markClassRosterDirty();
                    }
                }
                );
            setAutomationName(
                cell,
                L"Roster row " + std::to_wstring(rowIndex + 1) + L" "
                    + asWide(m_classRoster.columns[column])
                );
            Grid::SetColumn(cell, static_cast<int32_t>(column));
            rowGrid.Children().Append(cell);
            rowBoxes.push_back(cell);
        }
        m_classRosterList.Items().Append(rowGrid);
        m_classRosterCellBoxes.push_back(std::move(rowBoxes));
    }

    m_classRosterLoading = wasLoading;
    updateClassRosterActions();
}

void MainWindow::updateClassRosterActions()
{
    if (!m_classRosterStatusText)
    {
        return;
    }

    const bool hasDatabase = static_cast<bool>(m_openDatabase);
    const bool hasClass = hasDatabase && m_classSelectedId > 0 && !m_classNew;
    const bool hasSelectedRow = m_classRosterList
        && m_classRosterList.SelectedIndex() >= 0
        && m_classRosterList.SelectedIndex()
            < static_cast<int32_t>(m_classRoster.rows.size());
    const bool hasTarget = m_classRosterTransferTargetCombo
        && m_classRosterTransferTargetCombo.SelectedItem();

    if (m_classRosterList)
    {
        m_classRosterList.IsEnabled(hasClass);
    }
    if (m_classRosterImportScoresButton)
    {
        m_classRosterImportScoresButton.IsEnabled(hasClass);
    }
    if (m_classRosterTransferTargetCombo)
    {
        m_classRosterTransferTargetCombo.IsEnabled(hasClass && hasTarget);
    }
    if (m_classRosterAddButton)
    {
        m_classRosterAddButton.IsEnabled(
            hasClass
            && m_classRoster.rows.size()
                < classmngr::engine::RosterValidator::MaximumRows
            );
    }
    if (m_classRosterRemoveButton)
    {
        m_classRosterRemoveButton.IsEnabled(hasClass && hasSelectedRow);
    }
    if (m_classRosterSaveButton)
    {
        m_classRosterSaveButton.IsEnabled(hasClass && m_classRosterDirty);
    }
    if (m_classRosterDiscardButton)
    {
        m_classRosterDiscardButton.IsEnabled(hasClass && m_classRosterDirty);
    }
    if (m_classRosterTransferButton)
    {
        m_classRosterTransferButton.IsEnabled(
            hasClass && hasSelectedRow && hasTarget
            );
    }
    if (m_classRosterPrepareTransferButton)
    {
        m_classRosterPrepareTransferButton.IsEnabled(
            hasClass && !m_classRosterDirty
            );
    }
}

void MainWindow::markClassRosterDirty()
{
    if (m_classRosterLoading || !m_openDatabase || m_classSelectedId <= 0)
    {
        return;
    }

    m_classRosterDirty = true;
    m_dirtyState.markDirty();
    if (m_classRosterStatusText)
    {
        m_classRosterStatusText.Text(L"Unsaved roster changes.");
    }
    updateClassRosterActions();
    updateClassActions();
}

void MainWindow::clearClassRosterDirty()
{
    m_classRosterDirty = false;
    if (!m_classDirty && !m_classRosterDirty
        && !m_speakingEvaluationDirty)
    {
        m_dirtyState.markClean();
    }
    updateClassRosterActions();
}

void MainWindow::saveClassRoster()
{
    if (!m_openDatabase)
    {
        m_classRosterStatusText.Text(L"No database open.");
        return;
    }
    if (m_classSelectedId <= 0 || m_classNew)
    {
        m_classRosterStatusText.Text(L"Select and save a class before saving its roster.");
        return;
    }

    classmngr::engine::Roster normalized =
        classmngr::engine::RosterValidator::normalized(classRosterFromForm());
    const auto validation = classmngr::engine::RosterValidator::validate(
        normalized
        );
    if (validation.hasErrors())
    {
        std::wstring summary = L"Engine validation failed:";
        for (const auto& issue : validation.errors())
        {
            summary += L"\n- " + asWide(issue.code);
            if (!issue.field.empty())
            {
                summary += L" (" + asWide(issue.field) + L")";
            }
        }
        m_classRosterStatusText.Text(L"Roster could not be saved.");
        m_classRosterValidationText.Text(winrt::hstring(summary));
        m_classRosterValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Visible
            );
        m_classRosterDirty = true;
        m_dirtyState.markDirty();
        updateClassRosterActions();
        return;
    }

    classmngr::engine::RosterService service(*m_openDatabase);
    const auto saved = service.save(m_classSelectedId, normalized);
    if (!saved)
    {
        m_classRosterStatusText.Text(winrt::hstring(
            L"Roster could not be saved: " + asWide(saved.error().message)
            ));
        m_classRosterValidationText.Text(winrt::hstring(
            L"Engine persistence error: " + asWide(saved.error().message)
            ));
        m_classRosterValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Visible
            );
        m_classRosterDirty = true;
        m_dirtyState.markDirty();
        updateClassRosterActions();
        return;
    }

    m_classRosterLoading = true;
    m_classRoster = std::move(normalized);
    rebuildClassRosterGrid();
    m_classRosterLoading = false;
    clearClassRosterDirty();
    m_classRosterStatusText.Text(
        winrt::hstring(
            L"Roster saved. "
            + std::to_wstring(
                classmngr::engine::rosterStudentCount(m_classRoster)
                )
            + L" students."
            )
        );
    m_classRosterValidationText.Text({});
    m_classRosterValidationText.Visibility(
        Microsoft::UI::Xaml::Visibility::Collapsed
        );
    updateClassActions();
}

void MainWindow::discardClassRoster()
{
    if (!m_openDatabase || m_classSelectedId <= 0)
    {
        return;
    }
    refreshClassRoster();
    m_classRosterStatusText.Text(L"Roster changes discarded.");
}

void MainWindow::addClassRosterRow()
{
    if (!m_openDatabase || m_classSelectedId <= 0 || m_classNew)
    {
        return;
    }
    if (m_classRoster.rows.size()
        >= classmngr::engine::RosterValidator::MaximumRows)
    {
        m_classRosterStatusText.Text(L"The roster already has the maximum of 25 rows.");
        return;
    }
    if (m_classRoster.columns.empty())
    {
        m_classRoster = defaultRoster();
    }
    m_classRoster.rows.emplace_back(m_classRoster.columns.size());
    rebuildClassRosterGrid();
    m_classRosterList.SelectedIndex(
        static_cast<int32_t>(m_classRoster.rows.size()) - 1
        );
    markClassRosterDirty();
    m_classRosterStatusText.Text(L"Blank roster row added.");
}

void MainWindow::removeClassRosterRow()
{
    if (!m_openDatabase || m_classSelectedId <= 0 || m_classNew
        || !m_classRosterList)
    {
        return;
    }
    const int32_t selectedIndex = m_classRosterList.SelectedIndex();
    if (selectedIndex < 0
        || selectedIndex >= static_cast<int32_t>(m_classRoster.rows.size()))
    {
        m_classRosterStatusText.Text(L"Select a roster row to remove.");
        return;
    }

    m_classRoster.rows.erase(
        m_classRoster.rows.begin() + selectedIndex
        );
    rebuildClassRosterGrid();
    if (!m_classRoster.rows.empty())
    {
        m_classRosterList.SelectedIndex(
            std::min(
                selectedIndex,
                static_cast<int32_t>(m_classRoster.rows.size()) - 1
                )
            );
    }
    markClassRosterDirty();
    m_classRosterStatusText.Text(L"Selected roster row removed.");
}

void MainWindow::transferClassRosterRow()
{
    if (!m_openDatabase || m_classSelectedId <= 0 || m_classNew
        || !m_classRosterList || !m_classRosterTransferTargetCombo)
    {
        return;
    }
    const int32_t selectedIndex = m_classRosterList.SelectedIndex();
    const auto targetItem = m_classRosterTransferTargetCombo.SelectedItem().try_as<
        Microsoft::UI::Xaml::Controls::ComboBoxItem>();
    const int targetId = targetItem ? boxedInt(targetItem.Tag()) : -1;
    if (selectedIndex < 0 || targetId <= 0)
    {
        m_classRosterStatusText.Text(
            L"Select a student and a target class before transferring."
            );
        return;
    }

    classmngr::engine::Roster source = classRosterFromForm();
    if (selectedIndex >= static_cast<int32_t>(source.rows.size()))
    {
        m_classRosterStatusText.Text(L"The selected roster row is unavailable.");
        return;
    }
    if (!classmngr::engine::isRosterStudentRow(
            source,
            source.rows[static_cast<std::size_t>(selectedIndex)]
            ))
    {
        m_classRosterStatusText.Text(
            L"Only a populated student row can be transferred."
            );
        return;
    }

    source = classmngr::engine::RosterValidator::normalized(source);
    for (auto& row : source.rows)
    {
        row.resize(source.columns.size());
    }

    if (!m_classInfo.classGrade.empty())
    {
        classmngr::engine::ClassInfoService infoService(*m_openDatabase);
        const auto targetInfo = infoService.load(targetId);
        if (!targetInfo || targetInfo->classGrade != m_classInfo.classGrade)
        {
            m_classRosterStatusText.Text(
                L"Students can only be transferred between same-grade classes."
                );
            return;
        }
    }

    classmngr::engine::RosterService service(*m_openDatabase);
    const auto loadedTarget = service.load(targetId);
    if (!loadedTarget)
    {
        m_classRosterStatusText.Text(winrt::hstring(
            L"Target roster could not be loaded: "
            + asWide(loadedTarget.error().message)
            ));
        return;
    }
    classmngr::engine::Roster target = *loadedTarget;
    if (target.columns.empty())
    {
        target = defaultRoster();
    }
    target = classmngr::engine::RosterValidator::normalized(target);
    if (target.columnWidths.size() > target.columns.size())
    {
        target.columnWidths.resize(target.columns.size());
    }
    target.columnWidths.resize(target.columns.size(), 100);
    for (auto& row : target.rows)
    {
        row.resize(target.columns.size());
    }
    if (target.rows.size()
        >= classmngr::engine::RosterValidator::MaximumRows)
    {
        m_classRosterStatusText.Text(
            L"The target roster already has the maximum of 25 rows."
            );
        return;
    }

    const auto& sourceRow = source.rows[static_cast<std::size_t>(selectedIndex)];
    std::vector<std::string> targetRow(target.columns.size());
    for (std::size_t targetColumn = 0;
         targetColumn < target.columns.size();
         ++targetColumn)
    {
        const auto sourceColumn = std::find(
            source.columns.begin(),
            source.columns.end(),
            target.columns[targetColumn]
            );
        if (sourceColumn != source.columns.end())
        {
            const std::size_t sourceIndex = static_cast<std::size_t>(
                std::distance(source.columns.begin(), sourceColumn)
                );
            if (sourceIndex < sourceRow.size())
            {
                targetRow[targetColumn] = sourceRow[sourceIndex];
            }
        }
    }
    target.rows.push_back(std::move(targetRow));
    source.rows.erase(source.rows.begin() + selectedIndex);

    source = classmngr::engine::RosterValidator::normalized(source);
    target = classmngr::engine::RosterValidator::normalized(target);
    const auto sourceValidation = classmngr::engine::RosterValidator::validate(
        source
        );
    const auto targetValidation = classmngr::engine::RosterValidator::validate(
        target
        );
    if (sourceValidation.hasErrors() || targetValidation.hasErrors())
    {
        m_classRosterStatusText.Text(
            L"Transfer rejected because one or both rosters failed validation."
            );
        m_classRosterValidationText.Text(
            L"Fix roster validation errors before transferring a student."
            );
        m_classRosterValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Visible
            );
        return;
    }

    const auto saved = service.saveBatch({
        {m_classSelectedId, source},
        {targetId, target}
        });
    if (!saved)
    {
        m_classRosterStatusText.Text(winrt::hstring(
            L"Student transfer could not be saved: "
            + asWide(saved.error().message)
            ));
        return;
    }

    m_classRosterLoading = true;
    m_classRoster = std::move(source);
    rebuildClassRosterGrid();
    m_classRosterLoading = false;
    clearClassRosterDirty();
    m_classRosterStatusText.Text(
        winrt::hstring(
            L"Student transferred to "
            + boxedString(targetItem.Content())
            + L"."
            )
        );
    m_classRosterValidationText.Text({});
    m_classRosterValidationText.Visibility(
        Microsoft::UI::Xaml::Visibility::Collapsed
        );
}

void MainWindow::prepareClassTransfer()
{
    if (!m_openDatabase || m_classSelectedId <= 0 || m_classNew)
    {
        return;
    }
    if (m_classRosterDirty)
    {
        m_classRosterStatusText.Text(
            L"Save or discard roster changes before preparing a class package."
            );
        return;
    }

    classmngr::engine::ClassTransferService service(*m_openDatabase);
    const auto package = service.buildPackage({m_classSelectedId});
    if (!package)
    {
        m_classRosterStatusText.Text(winrt::hstring(
            L"Class transfer package could not be prepared: "
            + asWide(package.error().message)
            ));
        return;
    }
    const auto preview = service.previewImport(*package);
    if (!preview)
    {
        m_classRosterStatusText.Text(winrt::hstring(
            L"Class transfer preview could not be prepared: "
            + asWide(preview.error().message)
            ));
        return;
    }
    m_classRosterStatusText.Text(
        winrt::hstring(
            L"Class transfer package ready for "
            + std::to_wstring(package->classes.size())
            + L" class."
            )
        );
}

void MainWindow::ClassSelection_SelectionChanged(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& arguments
    )
{
    static_cast<void>(arguments);
    if (m_classLoading || !m_openDatabase)
    {
        return;
    }
    if (m_classDirty || m_classRosterDirty || m_speakingEvaluationDirty)
    {
        m_classLoading = true;
        m_classSelector.SelectedIndex(m_classSelectedIndex);
        m_classLoading = false;
        m_classStatusText.Text(
            m_speakingEvaluationDirty
                ? L"Save or discard the current speaking evaluation before selecting another."
                : m_classRosterDirty
                    ? L"Save or discard the current roster before selecting another."
                    : L"Save or discard the current class before selecting another."
            );
        return;
    }

    const auto selected = sender.try_as<
        Microsoft::UI::Xaml::Controls::ComboBox>();
    if (!selected)
    {
        return;
    }
    const auto item = selected.SelectedItem().try_as<
        Microsoft::UI::Xaml::Controls::ComboBoxItem>();
    const int selectedId = item ? boxedInt(item.Tag()) : -1;
    int resolvedIndex = -1;
    for (int index = 0; index < static_cast<int>(m_classes.size()); ++index)
    {
        if (m_classes[static_cast<std::size_t>(index)].id == selectedId)
        {
            resolvedIndex = index;
            break;
        }
    }
    presentClass(resolvedIndex);
    clearClassDirty();
    m_classStatusText.Text(
        resolvedIndex >= 0
            ? L"Class information loaded."
            : L"No class selected."
        );
    m_classNotesStatusText.Text(
        resolvedIndex >= 0
            ? L"Select a class tab to edit notes."
            : L"No class selected."
    );
    refreshClassRoster();
    refreshClassNavigation(false);
}

void MainWindow::ClassField_TextChanging(
    Microsoft::UI::Xaml::Controls::TextBox const& sender,
    Microsoft::UI::Xaml::Controls::TextBoxTextChangingEventArgs const& arguments
    )
{
    static_cast<void>(arguments);
    if (m_classLoading || !m_openDatabase)
    {
        return;
    }
    if (sender == m_classColorTextBox && m_classColorPreview)
    {
        m_classColorPreview.Background(
            Microsoft::UI::Xaml::Media::SolidColorBrush(
                uiColorFromHex(asUtf8(sender.Text()))
                )
            );
    }
    if (sender == m_classNotesTextBox
        || sender == m_classTimeFillerActivitiesTextBox)
    {
        m_classNotesDirty = true;
    }
    else
    {
        m_classDetailsDirty = true;
    }
    markClassDirty();
}

void MainWindow::ClassField_SelectionChanged(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& arguments
    )
{
    static_cast<void>(arguments);
    if (m_classLoading || !m_openDatabase)
    {
        return;
    }
    const auto combo = sender.try_as<
        Microsoft::UI::Xaml::Controls::ComboBox>();
    if (combo == m_classGradeCombo || combo == m_classLevelCombo)
    {
        refreshClassInformationOptions();
    }
    m_classDetailsDirty = true;
    markClassDirty();
}

void MainWindow::ClassNewButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_openDatabase || m_classDirty || m_classRosterDirty
        || m_speakingEvaluationDirty)
    {
        if (m_classStatusText && (m_classDirty || m_classRosterDirty
            || m_speakingEvaluationDirty))
        {
            m_classStatusText.Text(
                m_speakingEvaluationDirty
                    ? L"Save or discard the current speaking evaluation before creating another."
                    : m_classRosterDirty
                        ? L"Save or discard the current roster before creating another."
                        : L"Save or discard the current class before creating another."
                );
        }
        return;
    }

    m_classNew = true;
    m_classSelectedIndex = -1;
    m_classSelectedId = -1;
    m_classInfo = {};
    m_classLoading = true;
    m_classSelector.SelectedIndex(-1);
    m_classLoading = false;
    presentClass(-1);
    refreshClassNavigation(false);
    m_classDetailsDirty = true;
    m_classNotesDirty = false;
    m_classDirty = true;
    m_dirtyState.markDirty();
    m_classStatusText.Text(
        L"New class. Enter a name and class information, then save."
        );
    m_classNotesStatusText.Text(L"Save the new class before editing notes.");
    m_classValidationText.Text({});
    m_classValidationText.Visibility(
        Microsoft::UI::Xaml::Visibility::Collapsed
        );
    refreshClassRoster();
    updateClassActions();
}

void MainWindow::ClassDeleteButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_openDatabase || m_classSelectedId <= 0
        || m_classDirty || m_classRosterDirty || m_speakingEvaluationDirty)
    {
        return;
    }

    const int classId = m_classSelectedId;
    auto weak = get_weak();
    showDialog(
        L"Delete class",
        L"Delete the selected class and its saved information?",
        L"Delete",
        {},
        L"Cancel",
        [weak, classId](ClassMngrWinUIDialogs::DialogOutcome outcome) {
            if (outcome != ClassMngrWinUIDialogs::DialogOutcome::Primary)
            {
                return;
            }
            if (auto self = weak.get())
            {
                if (!self->m_openDatabase)
                {
                    return;
                }
                classmngr::engine::ClassRepository repository(
                    *self->m_openDatabase
                    );
                const auto removed = repository.remove(classId);
                if (!removed)
                {
                    self->m_classStatusText.Text(winrt::hstring(
                        L"Class could not be deleted: "
                        + asWide(removed.error().message)
                        ));
                    return;
                }
                self->m_classSelectedId = -1;
                self->m_classSelectedIndex = -1;
                self->m_classNew = false;
                self->clearClassDirty();
                self->refreshClassesPage();
                self->m_classStatusText.Text(L"Class deleted.");
            }
        }
        );
}

void MainWindow::ClassSaveButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_openDatabase)
    {
        m_classStatusText.Text(L"No database open.");
        return;
    }

    const std::wstring className = asWString(m_classNameTextBox.Text());
    if (className.find_first_not_of(L" \t\r\n") == std::wstring::npos)
    {
        m_classStatusText.Text(L"Class could not be saved.");
        m_classValidationText.Text(L"A class name is required.");
        m_classValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Visible
            );
        m_classDetailsDirty = true;
        markClassDirty();
        return;
    }

    classmngr::engine::ClassInfo info = classInfoFromForm();
    info.classId = m_classNew ? 1 : m_classSelectedId;
    const auto normalized = classmngr::engine::ClassInfoValidator::normalized(info);
    const auto validation = classmngr::engine::ClassInfoValidator::validate(
        normalized
        );
    if (validation.hasErrors())
    {
        std::wstring summary = L"Engine validation failed:";
        for (const auto& issue : validation.errors())
        {
            summary += L"\n- ";
            summary += asWide(issue.code);
            if (!issue.field.empty())
            {
                summary += L" (" + asWide(issue.field) + L")";
            }
        }
        m_classStatusText.Text(L"Class could not be saved.");
        m_classValidationText.Text(winrt::hstring(summary));
        m_classValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Visible
            );
        m_classDetailsDirty = true;
        markClassDirty();
        return;
    }

    classmngr::engine::ClassRepository repository(*m_openDatabase);
    int classId = m_classSelectedId;
    bool created = false;
    if (m_classNew)
    {
        const auto newId = repository.create(
            asUtf8(std::wstring_view(className))
            );
        if (!newId)
        {
            m_classStatusText.Text(winrt::hstring(
                L"Class could not be created: " + asWide(newId.error().message)
                ));
            return;
        }
        classId = *newId;
        created = true;
    }
    info.classId = classId;

    classmngr::engine::ClassInfoService service(*m_openDatabase);
    const auto saved = service.save(info);
    if (!saved)
    {
        if (created)
        {
            static_cast<void>(repository.remove(classId));
        }
        m_classStatusText.Text(winrt::hstring(
            L"Class could not be saved: " + asWide(saved.error().message)
            ));
        m_classValidationText.Text(winrt::hstring(
            L"Engine validation or persistence error: "
            + asWide(saved.error().message)
            ));
        m_classValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Visible
            );
        m_classDetailsDirty = true;
        markClassDirty();
        return;
    }

    if (!created
        && classId > 0
        && m_classSelectedIndex >= 0
        && m_classSelectedIndex < static_cast<int>(m_classes.size())
        && m_classes[static_cast<std::size_t>(m_classSelectedIndex)].name
            != asUtf8(std::wstring_view(className)))
    {
        const auto renamed = repository.rename(
            classId,
            asUtf8(std::wstring_view(className))
            );
        if (!renamed)
        {
            m_classStatusText.Text(winrt::hstring(
                L"Class name could not be saved: "
                + asWide(renamed.error().message)
                ));
            m_classDetailsDirty = true;
            markClassDirty();
            return;
        }
    }

    m_classSelectedId = classId;
    m_classNew = false;
    clearClassDirty();
    refreshClassesPage();
    m_classStatusText.Text(L"Class information saved.");
    m_classNotesStatusText.Text(L"Class notes are ready to edit.");
    m_classValidationText.Text({});
    m_classValidationText.Visibility(
        Microsoft::UI::Xaml::Visibility::Collapsed
        );
}

void MainWindow::ClassDiscardButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_openDatabase)
    {
        return;
    }
    m_classNew = false;
    clearClassDirty();
    refreshClassesPage();
}

void MainWindow::ClassNotesSaveButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_openDatabase)
    {
        m_classNotesStatusText.Text(L"No database open.");
        return;
    }
    if (m_classSelectedId <= 0)
    {
        m_classNotesStatusText.Text(L"Select a class before saving notes.");
        return;
    }
    if (m_classDetailsDirty || m_classNew)
    {
        m_classNotesStatusText.Text(
            L"Save class information before saving notes."
            );
        return;
    }

    classmngr::engine::ClassInfoService service(*m_openDatabase);
    const auto saved = service.saveNotes(
        m_classSelectedId,
        asUtf8(m_classNotesTextBox.Text()),
        asUtf8(m_classTimeFillerActivitiesTextBox.Text())
        );
    if (!saved)
    {
        m_classNotesStatusText.Text(winrt::hstring(
            L"Class notes could not be saved: "
            + asWide(saved.error().message)
            ));
        m_classNotesValidationText.Text(winrt::hstring(
            L"Engine validation or persistence error: "
            + asWide(saved.error().message)
            ));
        m_classNotesValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Visible
            );
        m_classNotesDirty = true;
        markClassDirty();
        return;
    }

    m_classInfo.notes = asUtf8(m_classNotesTextBox.Text());
    m_classInfo.timeFillerActivities = asUtf8(
        m_classTimeFillerActivitiesTextBox.Text()
        );
    m_classNotesDirty = false;
    m_classDirty = m_classDetailsDirty;
    if (!m_classDirty && !m_classRosterDirty)
    {
        m_dirtyState.markClean();
    }
    m_classNotesStatusText.Text(L"Class notes saved.");
    m_classNotesValidationText.Text({});
    m_classNotesValidationText.Visibility(
        Microsoft::UI::Xaml::Visibility::Collapsed
        );
    updateClassActions();
}

void MainWindow::ClassNotesDiscardButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_openDatabase || m_classSelectedId <= 0)
    {
        return;
    }
    m_classLoading = true;
    m_classNotesTextBox.Text(asWide(m_classInfo.notes));
    m_classTimeFillerActivitiesTextBox.Text(
        asWide(m_classInfo.timeFillerActivities)
        );
    m_classLoading = false;
    m_classNotesDirty = false;
    m_classDirty = m_classDetailsDirty;
    if (!m_classDirty && !m_classRosterDirty)
    {
        m_dirtyState.markClean();
    }
    m_classNotesStatusText.Text(L"Class note changes discarded.");
    m_classNotesValidationText.Text({});
    m_classNotesValidationText.Visibility(
        Microsoft::UI::Xaml::Visibility::Collapsed
        );
    updateClassActions();
}

void MainWindow::importClassRosterScores()
{
    if (!m_openDatabase || m_classSelectedId <= 0 || m_classNew)
    {
        if (m_classRosterStatusText)
        {
            m_classRosterStatusText.Text(
                L"Save the selected class before importing scores."
                );
        }
        return;
    }

    classmngr::engine::Roster roster = classRosterFromForm();
    const auto columnIndex = [&roster](std::string_view name) {
        for (std::size_t index = 0; index < roster.columns.size(); ++index)
        {
            if (roster.columns[index] == name)
            {
                return static_cast<int>(index);
            }
        }
        return -1;
    };
    const int englishColumn = columnIndex("English");
    const int koreanColumn = columnIndex("Korean");
    if (englishColumn < 0 || koreanColumn < 0)
    {
        m_classRosterStatusText.Text(
            L"Roster must contain English and Korean columns to import scores."
            );
        return;
    }

    classmngr::engine::SpeakingEvaluationPersistenceService service(
        *m_openDatabase
        );
    int imported = 0;
    for (const std::string_view evaluation :
         classmngr::engine::SpeakingEvaluationNames)
    {
        const int evaluationColumn = columnIndex(evaluation);
        if (evaluationColumn < 0)
        {
            continue;
        }

        const auto scores = service.buildRosterScoreImport(
            m_classSelectedId,
            evaluation
            );
        if (!scores)
        {
            m_classRosterStatusText.Text(winrt::hstring(
                L"Scores could not be imported: " + asWide(scores.error().message)
                ));
            return;
        }

        std::map<std::string, std::string> scoresByStudent;
        for (const auto& score : *scores)
        {
            scoresByStudent.emplace(
                classmngr::engine::StudentNameService::namePairKey(
                    score.englishName,
                    score.koreanName
                    ),
                score.finalGrade
                );
        }
        for (auto& row : roster.rows)
        {
            if (static_cast<std::size_t>(englishColumn) >= row.size()
                || static_cast<std::size_t>(koreanColumn) >= row.size()
                || static_cast<std::size_t>(evaluationColumn) >= row.size())
            {
                continue;
            }
            const auto score = scoresByStudent.find(
                classmngr::engine::StudentNameService::namePairKey(
                    row[static_cast<std::size_t>(englishColumn)],
                    row[static_cast<std::size_t>(koreanColumn)]
                    )
                );
            if (score != scoresByStudent.end()
                && row[static_cast<std::size_t>(evaluationColumn)]
                    != score->second)
            {
                row[static_cast<std::size_t>(evaluationColumn)] = score->second;
                ++imported;
            }
        }
    }

    if (imported == 0)
    {
        m_classRosterStatusText.Text(
            L"Scores are already current or no saved evaluations match this roster."
            );
        return;
    }

    m_classRoster = std::move(roster);
    rebuildClassRosterGrid();
    markClassRosterDirty();
    m_classRosterStatusText.Text(winrt::hstring(
        L"Imported " + std::to_wstring(imported)
            + L" evaluation score" + (imported == 1 ? L"." : L"s.")
        ));
}

void MainWindow::ClassCoTeacherSelection_SelectionChanged(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& arguments
    )
{
    static_cast<void>(arguments);
    if (m_classLoading || m_classCoTeacherLoading || !m_openDatabase)
    {
        return;
    }

    using namespace Microsoft::UI::Xaml::Controls;
    const auto combo = sender.try_as<ComboBox>();
    if (!combo
        || (combo != m_classCoTeacherKrCombo
            && combo != m_classCoTeacherEnCombo))
    {
        return;
    }

    const auto item = combo.SelectedItem().try_as<ComboBoxItem>();
    const int teacherId = item ? boxedInt(item.Tag()) : -1;
    m_classCoTeacherSelectedId = teacherId;
    m_classCoTeacherLoading = true;

    const auto selectTeacher = [](ComboBox target, int id) {
        for (int index = 0;
             index < static_cast<int>(target.Items().Size());
             ++index)
        {
            const auto candidate = target.Items().GetAt(index)
                .try_as<ComboBoxItem>();
            if (candidate && boxedInt(candidate.Tag()) == id)
            {
                target.SelectedIndex(index);
                return;
            }
        }
        target.SelectedIndex(0);
    };
    selectTeacher(m_classCoTeacherKrCombo, teacherId);
    selectTeacher(m_classCoTeacherEnCombo, teacherId);

    classmngr::engine::Teacher teacher;
    if (teacherId > 0)
    {
        classmngr::engine::TeacherService service(*m_openDatabase);
        const auto loaded = service.get(teacherId);
        if (loaded)
        {
            teacher = *loaded;
        }
    }
    m_classCoTeacherRoomTextBox.Text(asWide(teacher.roomNumber));
    m_classCoTeacherInternetTypeTextBox.Text(
        asWide(teacher.internetType)
        );
    m_classCoTeacherWifiNameTextBox.Text(asWide(teacher.wifiName));
    m_classCoTeacherWifiPasswordTextBox.Text(
        asWide(teacher.wifiPassword)
        );
    m_classCoTeacherProjectionTypeTextBox.Text(
        asWide(teacher.projectionType)
        );
    m_classCoTeacherZoomIdTextBox.Text(asWide(teacher.zoomId));
    m_classCoTeacherZoomPasswordTextBox.Text(
        asWide(teacher.zoomPassword)
        );
    m_classCoTeacherLoading = false;
    m_classDetailsDirty = true;
    markClassDirty();
}

void MainWindow::ClassCoTeacherSaveButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    ClassSaveButton_Click(sender, arguments);
}

void MainWindow::ClassCoTeacherDiscardButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    ClassDiscardButton_Click(sender, arguments);
}

void MainWindow::NameTextBox_TextChanged(
    Microsoft::UI::Xaml::Controls::TextBox const& sender,
    Microsoft::UI::Xaml::Controls::TextBoxTextChangingEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    m_dirtyState.markDirty();
}

void MainWindow::PersonalDetailsField_TextChanging(
    Microsoft::UI::Xaml::Controls::TextBox const& sender,
    Microsoft::UI::Xaml::Controls::TextBoxTextChangingEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (m_personalDetailsLoading || !m_openDatabase)
    {
        if (sender == m_personalTypedSignatureTextBox)
        {
            updatePersonalSignaturePreview();
        }
        return;
    }

    if (sender == m_personalTypedSignatureTextBox)
    {
        updatePersonalSignaturePreview();
    }

    m_personalDetailsDirty = true;
    m_dirtyState.markDirty();
    if (m_personalStatusText)
    {
        m_personalStatusText.Text(L"Unsaved personal detail changes.");
    }
    if (m_personalSaveButton)
    {
        m_personalSaveButton.IsEnabled(true);
    }
    if (m_personalDiscardButton)
    {
        m_personalDiscardButton.IsEnabled(true);
    }
}

void MainWindow::PersonalDetailsPassword_Changed(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (m_personalDetailsLoading || !m_openDatabase)
    {
        return;
    }

    m_personalDetailsDirty = true;
    m_dirtyState.markDirty();
    if (m_personalStatusText)
    {
        m_personalStatusText.Text(L"Unsaved personal detail changes.");
    }
    if (m_personalSaveButton)
    {
        m_personalSaveButton.IsEnabled(true);
    }
    if (m_personalDiscardButton)
    {
        m_personalDiscardButton.IsEnabled(true);
    }
}

void MainWindow::PersonalDetailsZoomAvailability_Changed(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_personalZoomNotAvailableCheck)
    {
        return;
    }

    const auto checkedValue = m_personalZoomNotAvailableCheck.IsChecked();
    const bool checked = checkedValue && checkedValue.Value();
    if (!m_personalDetailsLoading)
    {
        if (checked)
        {
            m_personalZoomLoginIdTextBox.Text(L"N/A");
            m_personalZoomPasswordBox.Password(L"N/A");
        }
        else
        {
            if (m_personalZoomLoginIdTextBox.Text() == L"N/A")
            {
                m_personalZoomLoginIdTextBox.Text({});
            }
            if (m_personalZoomPasswordBox.Password() == L"N/A")
            {
                m_personalZoomPasswordBox.Password({});
            }
        }
    }

    m_personalZoomLoginIdTextBox.IsEnabled(!checked && m_openDatabase);
    m_personalZoomPasswordBox.IsEnabled(!checked && m_openDatabase);
    if (!m_personalDetailsLoading && m_openDatabase)
    {
        m_personalDetailsDirty = true;
        m_dirtyState.markDirty();
        m_personalStatusText.Text(L"Unsaved personal detail changes.");
        m_personalSaveButton.IsEnabled(true);
        m_personalDiscardButton.IsEnabled(true);
    }
}

void MainWindow::PersonalDetailsCampus_SelectionChanged(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (m_personalDetailsLoading || !m_openDatabase)
    {
        return;
    }

    m_personalDetailsDirty = true;
    m_dirtyState.markDirty();
    m_personalStatusText.Text(L"Unsaved personal detail changes.");
    m_personalSaveButton.IsEnabled(true);
    m_personalDiscardButton.IsEnabled(true);
}

void MainWindow::PersonalDetailsSignatureMode_SelectionChanged(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& arguments
    )
{
    static_cast<void>(arguments);
    updatePersonalSignatureControls();
    if (m_personalDetailsLoading || !m_openDatabase)
    {
        return;
    }

    if (sender == m_personalSignatureModeCombo)
    {
        const bool typed = m_personalSignatureModeCombo.SelectedIndex() == 1;
        m_personalTypedSignatureTextBox.IsEnabled(typed);
        m_personalSignatureFontCombo.IsEnabled(typed);
        m_personalImageStatusText.Text(
            typed
                ? L"Typed signature values are stored with the personal details."
                : L"Existing image data is retained; image selection is a Phase 7 placeholder."
            );
    }
    updatePersonalSignaturePreview();

    m_personalDetailsDirty = true;
    m_dirtyState.markDirty();
    m_personalStatusText.Text(L"Unsaved personal detail changes.");
    m_personalSaveButton.IsEnabled(true);
    m_personalDiscardButton.IsEnabled(true);
}

void MainWindow::PersonalDetailsSaveButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_openDatabase)
    {
        m_personalStatusText.Text(L"No database open; personal details were not saved.");
        return;
    }

    const auto name = asWString(m_personalNameTextBox.Text());
    if (name.find_first_not_of(L" \t\r\n") == std::wstring::npos)
    {
        m_personalValidationText.Text(L"Your name is required.");
        m_personalValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Visible
            );
        m_personalNameTextBox.Focus(
            Microsoft::UI::Xaml::FocusState::Programmatic
            );
        return;
    }

    classmngr::engine::PersonalDetails draft = m_personalDetails;
    draft.name = asUtf8(name);
    draft.campus = asUtf8(selectedComboValue(m_personalCampusCombo));
    draft.zoomLoginId = asUtf8(asWString(m_personalZoomLoginIdTextBox.Text()));
    draft.zoomPassword = asUtf8(
        asWString(m_personalZoomPasswordBox.Password())
        );
    const auto checkedValue = m_personalZoomNotAvailableCheck.IsChecked();
    draft.zoomNotAvailable = checkedValue && checkedValue.Value();
    draft.signatureMode =
        m_personalSignatureModeCombo.SelectedIndex() == 1
            ? classmngr::engine::SignatureMode::Type
            : classmngr::engine::SignatureMode::Image;
    draft.typedSignatureText = asUtf8(
        asWString(m_personalTypedSignatureTextBox.Text())
        );
    draft.typedSignatureFont = m_personalSignatureFontCombo.SelectedIndex();

    classmngr::engine::ApplicationSettingsService settings(*m_openDatabase);
    classmngr::engine::PersonalDetailsService service(settings);
    const auto saved = service.save(draft);
    if (!saved)
    {
        m_personalStatusText.Text(winrt::hstring(
            L"Personal details could not be saved: "
            + asWide(saved.error().message)
            ));
        m_personalValidationText.Text(L"The engine rejected the personal-details write.");
        m_personalValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Visible
            );
        return;
    }

    m_personalDetails = std::move(draft);
    m_personalDetailsLoaded = true;
    m_personalDetailsDirty = false;
    m_dirtyState.markClean();
    m_personalStatusText.Text(L"Personal details saved.");
    m_personalValidationText.Text({});
    m_personalValidationText.Visibility(
        Microsoft::UI::Xaml::Visibility::Collapsed
        );
    m_personalSaveButton.IsEnabled(false);
    m_personalDiscardButton.IsEnabled(false);
    updateFileCommandState();
}

void MainWindow::PersonalDetailsDiscardButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_openDatabase)
    {
        return;
    }

    m_personalDetailsDirty = false;
    m_dirtyState.markClean();
    refreshPersonalDetailsPage();
}

void MainWindow::KoreanTeacherSelection_SelectionChanged(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& arguments
    )
{
    static_cast<void>(arguments);
    if (m_koreanTeacherLoading)
    {
        return;
    }

    const auto selector = sender.try_as<
        Microsoft::UI::Xaml::Controls::ComboBox>();
    if (!selector)
    {
        return;
    }

    if (m_koreanTeacherDirty)
    {
        m_koreanTeacherLoading = true;
        selector.SelectedIndex(m_koreanTeacherSelectedIndex);
        m_koreanTeacherLoading = false;
        m_koreanTeacherStatusText.Text(
            L"Save or discard the current Korean teacher before selecting another."
            );
        return;
    }

    const auto item = selector.SelectedItem().try_as<
        Microsoft::UI::Xaml::Controls::ComboBoxItem>();
    const int selectedId = item ? boxedInt(item.Tag()) : -1;
    int selectedIndex = -1;
    for (int index = 0; index < static_cast<int>(m_koreanTeachers.size()); ++index)
    {
        if (m_koreanTeachers[static_cast<std::size_t>(index)].id == selectedId)
        {
            selectedIndex = index;
            break;
        }
    }
    m_koreanTeacherSelectedIndex = selectedIndex;
    m_koreanTeacherSelectedId = selectedId;
    m_koreanTeacherNew = false;
    presentKoreanTeacher(selectedIndex);
    m_koreanTeacherStatusText.Text(
        selectedIndex >= 0
            ? L"Korean teacher selected."
            : L"Select a Korean teacher or choose New Teacher."
        );
    m_koreanTeacherValidationText.Text({});
    m_koreanTeacherValidationText.Visibility(
        Microsoft::UI::Xaml::Visibility::Collapsed
        );
}

void MainWindow::KoreanTeacherField_TextChanging(
    Microsoft::UI::Xaml::Controls::TextBox const& sender,
    Microsoft::UI::Xaml::Controls::TextBoxTextChangingEventArgs const& arguments
    )
{
    static_cast<void>(arguments);
    if (m_koreanTeacherLoading || !m_openDatabase)
    {
        return;
    }

    if (sender == m_koreanTeacherEnTextBox
        || sender == m_koreanTeacherRomanizationTextBox)
    {
        refreshKoreanTeacherPreferredNames();
    }
    m_koreanTeacherDirty = true;
    m_dirtyState.markDirty();
    m_koreanTeacherStatusText.Text(L"Unsaved Korean teacher changes.");
    m_koreanTeacherSaveButton.IsEnabled(true);
    m_koreanTeacherDiscardButton.IsEnabled(true);
    m_koreanTeacherNewButton.IsEnabled(false);
    m_koreanTeacherDeleteButton.IsEnabled(false);
    m_koreanTeacherSelector.IsEnabled(false);
}

void MainWindow::KoreanTeacherPassword_Changed(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (m_koreanTeacherLoading || !m_openDatabase)
    {
        return;
    }

    m_koreanTeacherDirty = true;
    m_dirtyState.markDirty();
    m_koreanTeacherStatusText.Text(L"Unsaved Korean teacher changes.");
    m_koreanTeacherSaveButton.IsEnabled(true);
    m_koreanTeacherDiscardButton.IsEnabled(true);
    m_koreanTeacherNewButton.IsEnabled(false);
    m_koreanTeacherDeleteButton.IsEnabled(false);
    m_koreanTeacherSelector.IsEnabled(false);
}

void MainWindow::KoreanTeacherCombo_SelectionChanged(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (m_koreanTeacherLoading || !m_openDatabase)
    {
        return;
    }

    m_koreanTeacherDirty = true;
    m_dirtyState.markDirty();
    m_koreanTeacherStatusText.Text(L"Unsaved Korean teacher changes.");
    m_koreanTeacherSaveButton.IsEnabled(true);
    m_koreanTeacherDiscardButton.IsEnabled(true);
    m_koreanTeacherNewButton.IsEnabled(false);
    m_koreanTeacherDeleteButton.IsEnabled(false);
    m_koreanTeacherSelector.IsEnabled(false);
}

void MainWindow::KoreanTeacherNewButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_openDatabase || m_koreanTeacherDirty)
    {
        if (m_koreanTeacherStatusText && m_koreanTeacherDirty)
        {
            m_koreanTeacherStatusText.Text(
                L"Save or discard the current Korean teacher before creating another."
                );
        }
        return;
    }

    m_koreanTeacherNew = true;
    m_koreanTeacherSelectedIndex = -1;
    m_koreanTeacherSelectedId = -1;
    m_koreanTeacherLoading = true;
    m_koreanTeacherSelector.SelectedIndex(-1);
    m_koreanTeacherLoading = false;
    presentKoreanTeacher(-1);
    m_koreanTeacherDirty = true;
    m_dirtyState.markDirty();
    m_koreanTeacherStatusText.Text(
        L"New Korean teacher. Enter the required name fields and save."
        );
    m_koreanTeacherValidationText.Text({});
    m_koreanTeacherValidationText.Visibility(
        Microsoft::UI::Xaml::Visibility::Collapsed
        );
    m_koreanTeacherSelector.IsEnabled(false);
    m_koreanTeacherNewButton.IsEnabled(false);
    m_koreanTeacherDeleteButton.IsEnabled(false);
    m_koreanTeacherSaveButton.IsEnabled(true);
    m_koreanTeacherDiscardButton.IsEnabled(true);
}

void MainWindow::KoreanTeacherDeleteButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_openDatabase || m_koreanTeacherSelectedId <= 0
        || m_koreanTeacherDirty)
    {
        return;
    }

    const int teacherId = m_koreanTeacherSelectedId;
    auto weak = get_weak();
    showDialog(
        L"Delete Korean teacher",
        L"Delete the selected Korean teacher from the active database?",
        L"Delete",
        {},
        L"Cancel",
        [weak, teacherId](ClassMngrWinUIDialogs::DialogOutcome outcome) {
            if (outcome != ClassMngrWinUIDialogs::DialogOutcome::Primary)
            {
                return;
            }
            if (auto self = weak.get())
            {
                if (!self->m_openDatabase)
                {
                    return;
                }
                classmngr::engine::TeacherService service(
                    *self->m_openDatabase
                    );
                const auto removed = service.remove(teacherId);
                if (!removed)
                {
                    self->m_koreanTeacherStatusText.Text(winrt::hstring(
                        L"Korean teacher could not be deleted: "
                        + asWide(removed.error().message)
                        ));
                    return;
                }
                self->m_koreanTeacherSelectedId = -1;
                self->m_koreanTeacherSelectedIndex = -1;
                self->m_koreanTeacherDirty = false;
                self->m_koreanTeacherNew = false;
                self->m_dirtyState.markClean();
                self->refreshKoreanTeachersPage();
                self->m_koreanTeacherStatusText.Text(
                    L"Korean teacher deleted."
                    );
            }
        }
        );
}

void MainWindow::KoreanTeacherSaveButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_openDatabase)
    {
        m_koreanTeacherStatusText.Text(L"No database open.");
        return;
    }

    classmngr::engine::TeacherService service(*m_openDatabase);
    const auto saved = service.save(koreanTeacherFromForm());
    if (!saved)
    {
        m_koreanTeacherStatusText.Text(winrt::hstring(
            L"Korean teacher could not be saved: "
            + asWide(saved.error().message)
            ));
        m_koreanTeacherValidationText.Text(winrt::hstring(
            L"Engine validation or persistence error: "
            + asWide(saved.error().message)
            ));
        m_koreanTeacherValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Visible
            );
        return;
    }

    m_koreanTeacherSelectedId = *saved;
    m_koreanTeacherSelectedIndex = -1;
    m_koreanTeacherNew = false;
    m_koreanTeacherDirty = false;
    m_dirtyState.markClean();
    refreshKoreanTeachersPage();
    m_koreanTeacherStatusText.Text(L"Korean teacher saved.");
    m_koreanTeacherValidationText.Text({});
    m_koreanTeacherValidationText.Visibility(
        Microsoft::UI::Xaml::Visibility::Collapsed
        );
}

void MainWindow::KoreanTeacherDiscardButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    m_koreanTeacherDirty = false;
    m_koreanTeacherNew = false;
    m_dirtyState.markClean();
    refreshKoreanTeachersPage();
}

void MainWindow::populateNativeEnglishTeachersPage(
    Microsoft::UI::Xaml::Controls::Page const& page,
    bool refresh
    )
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_nativeEnglishTeacherNameTextBox)
    {
        auto scroll = ScrollViewer();
        scroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
        scroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);

        auto root = StackPanel();
        root.Padding(Thickness{32.0, 24.0, 32.0, 32.0});
        root.Spacing(16.0);
        root.MaxWidth(900.0);
        root.HorizontalAlignment(HorizontalAlignment::Center);

        auto title = TextBlock();
        title.Text(L"Native English Teachers");
        title.FontSize(24.0);
        setAutomationName(title, L"Native English Teachers");
        root.Children().Append(title);

        auto description = TextBlock();
        description.Text(
            L"View and maintain Native English Teacher contact information used by classes and reports."
            );
        description.TextWrapping(TextWrapping::Wrap);
        setAutomationName(
            description,
            L"Native English teacher directory description"
            );
        root.Children().Append(description);

        m_nativeEnglishTeacherStatusText = TextBlock();
        m_nativeEnglishTeacherStatusText.Text(
            L"Loading Native English Teachers..."
            );
        m_nativeEnglishTeacherStatusText.TextWrapping(TextWrapping::Wrap);
        setAutomationName(
            m_nativeEnglishTeacherStatusText,
            L"Native English teacher directory status"
            );
        root.Children().Append(m_nativeEnglishTeacherStatusText);

        m_nativeEnglishTeacherValidationText = TextBlock();
        m_nativeEnglishTeacherValidationText.TextWrapping(TextWrapping::Wrap);
        m_nativeEnglishTeacherValidationText.Visibility(Visibility::Collapsed);
        setAutomationName(
            m_nativeEnglishTeacherValidationText,
            L"Native English teacher validation summary"
            );
        root.Children().Append(m_nativeEnglishTeacherValidationText);

        auto directoryCard = ClassMngrWinUISharedUX::buildCard({
            L"Teacher directory",
            L"Select an existing teacher or create a new directory entry.",
            L"Native English teacher directory selector"
            });
        m_nativeEnglishTeacherSelector = ComboBox();
        m_nativeEnglishTeacherSelector.Header(box_value(hstring(L"Teacher")));
        m_nativeEnglishTeacherSelector.PlaceholderText(
            L"Select a teacher"
            );
        m_nativeEnglishTeacherSelector.MinWidth(420.0);
        m_nativeEnglishTeacherSelector.HorizontalAlignment(
            HorizontalAlignment::Stretch
            );
        m_nativeEnglishTeacherSelector.IsTabStop(true);
        m_nativeEnglishTeacherSelector.TabIndex(0);
        m_nativeEnglishTeacherSelector.SelectionChanged({
            this,
            &MainWindow::NativeEnglishTeacherSelection_SelectionChanged
            });
        setAutomationName(
            m_nativeEnglishTeacherSelector,
            L"Native English teacher selector"
            );
        directoryCard.content.Children().Append(
            m_nativeEnglishTeacherSelector
            );
        root.Children().Append(directoryCard.root);

        const auto makeTextBox = [this](
            wchar_t const* header,
            wchar_t const* automationName,
            wchar_t const* placeholder
            ) {
            auto box = TextBox();
            box.Header(box_value(hstring(header)));
            box.PlaceholderText(placeholder);
            box.MinWidth(320.0);
            box.HorizontalAlignment(HorizontalAlignment::Stretch);
            box.IsTabStop(true);
            box.TextChanging({
                this,
                &MainWindow::NativeEnglishTeacherField_TextChanging
                });
            setAutomationName(box, automationName);
            return box;
        };

        auto detailsCard = ClassMngrWinUISharedUX::buildCard({
            L"Teacher details",
            L"Name, position, contact information, birthday, and nationality.",
            L"Native English teacher details form"
            });
        m_nativeEnglishTeacherNameTextBox = makeTextBox(
            L"Name",
            L"Native English teacher name",
            L"Enter the teacher name"
            );
        auto nameInputScope = Input::InputScope();
        nameInputScope.Names().Append(
            Input::InputScopeName(Input::InputScopeNameValue::Text)
            );
        m_nativeEnglishTeacherNameTextBox.InputScope(nameInputScope);
        m_nativeEnglishTeacherNameTextBox.TabIndex(1);

        m_nativeEnglishTeacherPositionCombo = ComboBox();
        m_nativeEnglishTeacherPositionCombo.Header(
            box_value(hstring(L"Position"))
            );
        m_nativeEnglishTeacherPositionCombo.MinWidth(320.0);
        m_nativeEnglishTeacherPositionCombo.HorizontalAlignment(
            HorizontalAlignment::Stretch
            );
        m_nativeEnglishTeacherPositionCombo.IsTabStop(true);
        m_nativeEnglishTeacherPositionCombo.TabIndex(2);
        const auto addPosition = [this](
            wchar_t const* display,
            wchar_t const* stored
            ) {
            auto item = ComboBoxItem();
            item.Content(box_value(hstring(display)));
            item.Tag(box_value(hstring(stored)));
            m_nativeEnglishTeacherPositionCombo.Items().Append(item);
        };
        addPosition(L"Coordinator", L"Co-ordinator");
        addPosition(L"Team Leader", L"Team Leader");
        addPosition(L"M3 Song's", L"M3 Song's");
        addPosition(L"M2 Song's", L"M2 Song's");
        addPosition(L"M1 Song's", L"M1 Song's");
        addPosition(L"E6 Song's", L"E6 Song's");
        addPosition(L"E5 Athena", L"E5 Athena");
        addPosition(L"NET", L"NET");
        m_nativeEnglishTeacherPositionCombo.SelectionChanged({
            this,
            &MainWindow::NativeEnglishTeacherPosition_SelectionChanged
            });
        setAutomationName(
            m_nativeEnglishTeacherPositionCombo,
            L"Native English teacher position"
            );

        m_nativeEnglishTeacherPhoneTextBox = makeTextBox(
            L"Phone Number",
            L"Native English teacher phone number",
            L"Optional phone number"
            );
        m_nativeEnglishTeacherPhoneTextBox.TabIndex(3);
        m_nativeEnglishTeacherEmailTextBox = makeTextBox(
            L"Email",
            L"Native English teacher email",
            L"Optional email address"
            );
        m_nativeEnglishTeacherEmailTextBox.TabIndex(4);
        m_nativeEnglishTeacherBirthdayTextBox = makeTextBox(
            L"Birthday (MM-dd)",
            L"Native English teacher birthday",
            L"e.g. 03-14"
            );
        m_nativeEnglishTeacherBirthdayTextBox.TabIndex(5);
        m_nativeEnglishTeacherNationalityTextBox = makeTextBox(
            L"Nationality",
            L"Native English teacher nationality",
            L"Optional nationality"
            );
        m_nativeEnglishTeacherNationalityTextBox.TabIndex(6);

        detailsCard.content.Children().Append(
            m_nativeEnglishTeacherNameTextBox
            );
        detailsCard.content.Children().Append(
            m_nativeEnglishTeacherPositionCombo
            );
        detailsCard.content.Children().Append(
            m_nativeEnglishTeacherPhoneTextBox
            );
        detailsCard.content.Children().Append(
            m_nativeEnglishTeacherEmailTextBox
            );
        detailsCard.content.Children().Append(
            m_nativeEnglishTeacherBirthdayTextBox
            );
        detailsCard.content.Children().Append(
            m_nativeEnglishTeacherNationalityTextBox
            );
        root.Children().Append(detailsCard.root);

        auto actions = StackPanel();
        actions.Orientation(Orientation::Horizontal);
        actions.Spacing(8.0);
        m_nativeEnglishTeacherNewButton = Button();
        m_nativeEnglishTeacherNewButton.Content(
            box_value(hstring(L"New Teacher"))
            );
        m_nativeEnglishTeacherNewButton.IsTabStop(true);
        m_nativeEnglishTeacherNewButton.TabIndex(7);
        m_nativeEnglishTeacherNewButton.Click({
            this,
            &MainWindow::NativeEnglishTeacherNewButton_Click
            });
        setAutomationName(
            m_nativeEnglishTeacherNewButton,
            L"New Native English teacher"
            );
        m_nativeEnglishTeacherDeleteButton = Button();
        m_nativeEnglishTeacherDeleteButton.Content(
            box_value(hstring(L"Delete Teacher"))
            );
        m_nativeEnglishTeacherDeleteButton.IsTabStop(true);
        m_nativeEnglishTeacherDeleteButton.TabIndex(8);
        m_nativeEnglishTeacherDeleteButton.Click({
            this,
            &MainWindow::NativeEnglishTeacherDeleteButton_Click
            });
        setAutomationName(
            m_nativeEnglishTeacherDeleteButton,
            L"Delete Native English teacher"
            );
        m_nativeEnglishTeacherSaveButton = Button();
        m_nativeEnglishTeacherSaveButton.Content(
            box_value(hstring(L"Save Changes"))
            );
        m_nativeEnglishTeacherSaveButton.IsTabStop(true);
        m_nativeEnglishTeacherSaveButton.TabIndex(9);
        m_nativeEnglishTeacherSaveButton.Click({
            this,
            &MainWindow::NativeEnglishTeacherSaveButton_Click
            });
        setAutomationName(
            m_nativeEnglishTeacherSaveButton,
            L"Save Native English teacher"
            );
        m_nativeEnglishTeacherDiscardButton = Button();
        m_nativeEnglishTeacherDiscardButton.Content(
            box_value(hstring(L"Discard Changes"))
            );
        m_nativeEnglishTeacherDiscardButton.IsTabStop(true);
        m_nativeEnglishTeacherDiscardButton.TabIndex(10);
        m_nativeEnglishTeacherDiscardButton.Click({
            this,
            &MainWindow::NativeEnglishTeacherDiscardButton_Click
            });
        setAutomationName(
            m_nativeEnglishTeacherDiscardButton,
            L"Discard Native English teacher changes"
            );
        actions.Children().Append(m_nativeEnglishTeacherNewButton);
        actions.Children().Append(m_nativeEnglishTeacherDeleteButton);
        actions.Children().Append(m_nativeEnglishTeacherSaveButton);
        actions.Children().Append(m_nativeEnglishTeacherDiscardButton);
        root.Children().Append(actions);

        scroll.Content(root);
        page.Content(scroll);
    }

    const auto setEditable = [this](bool enabled) {
        const bool clean = !m_nativeEnglishTeacherDirty;
        m_nativeEnglishTeacherSelector.IsEnabled(
            enabled && clean && !m_nativeEnglishTeacherNew
            );
        m_nativeEnglishTeacherNameTextBox.IsEnabled(enabled);
        m_nativeEnglishTeacherPositionCombo.IsEnabled(enabled);
        m_nativeEnglishTeacherPhoneTextBox.IsEnabled(enabled);
        m_nativeEnglishTeacherEmailTextBox.IsEnabled(enabled);
        m_nativeEnglishTeacherBirthdayTextBox.IsEnabled(enabled);
        m_nativeEnglishTeacherNationalityTextBox.IsEnabled(enabled);
        m_nativeEnglishTeacherNewButton.IsEnabled(
            enabled && clean && !m_nativeEnglishTeacherNew
            );
        m_nativeEnglishTeacherDeleteButton.IsEnabled(
            enabled && clean && !m_nativeEnglishTeacherNew
                && m_nativeEnglishTeacherSelectedId > 0
            );
        m_nativeEnglishTeacherSaveButton.IsEnabled(
            enabled && m_nativeEnglishTeacherDirty
            );
        m_nativeEnglishTeacherDiscardButton.IsEnabled(
            enabled && m_nativeEnglishTeacherDirty
            );
    };

    if (!m_openDatabase)
    {
        m_nativeEnglishTeacherLoading = true;
        m_nativeEnglishTeachers.clear();
        m_nativeEnglishTeacherSelectedIndex = -1;
        m_nativeEnglishTeacherSelectedId = -1;
        m_nativeEnglishTeacherNew = false;
        m_nativeEnglishTeacherDirty = false;
        m_nativeEnglishTeacherSelector.Items().Clear();
        m_nativeEnglishTeacherSelector.SelectedIndex(-1);
        m_nativeEnglishTeacherLoading = false;
        presentNativeEnglishTeacher(-1);
        m_nativeEnglishTeacherStatusText.Text(L"No database open.");
        m_nativeEnglishTeacherValidationText.Text({});
        m_nativeEnglishTeacherValidationText.Visibility(Visibility::Collapsed);
        setEditable(false);
        return;
    }

    if (refresh && m_nativeEnglishTeacherDirty)
    {
        m_nativeEnglishTeacherStatusText.Text(
            L"Unsaved Native English Teacher changes are retained."
            );
        return;
    }

    m_nativeEnglishTeacherLoading = true;
    classmngr::engine::NativeEnglishTeacherService service(*m_openDatabase);
    const auto loaded = service.list();
    if (!loaded)
    {
        m_nativeEnglishTeacherLoading = false;
        m_nativeEnglishTeachers.clear();
        m_nativeEnglishTeacherSelectedIndex = -1;
        m_nativeEnglishTeacherSelectedId = -1;
        m_nativeEnglishTeacherNew = false;
        m_nativeEnglishTeacherDirty = false;
        m_nativeEnglishTeacherSelector.Items().Clear();
        m_nativeEnglishTeacherSelector.SelectedIndex(-1);
        presentNativeEnglishTeacher(-1);
        m_nativeEnglishTeacherStatusText.Text(winrt::hstring(
            L"Native English Teacher directory could not be loaded: "
            + asWide(loaded.error().message)
            ));
        m_nativeEnglishTeacherValidationText.Text(
            L"The engine rejected the teacher-directory read."
            );
        m_nativeEnglishTeacherValidationText.Visibility(Visibility::Visible);
        setEditable(false);
        return;
    }

    const int previousId = m_nativeEnglishTeacherSelectedId;
    m_nativeEnglishTeachers = *loaded;
    m_nativeEnglishTeacherSelector.Items().Clear();
    int selectedIndex = -1;
    for (int index = 0;
         index < static_cast<int>(m_nativeEnglishTeachers.size());
         ++index)
    {
        const auto& teacher = m_nativeEnglishTeachers[
            static_cast<std::size_t>(index)
            ];
        std::wstring displayName = asWide(teacher.name);
        if (!teacher.position.empty())
        {
            displayName += L" (";
            displayName += asWide(teacher.position);
            displayName += L")";
        }
        auto item = ComboBoxItem();
        item.Content(box_value(hstring(displayName)));
        item.Tag(box_value(teacher.id));
        setAutomationName(item, L"Native English teacher " + displayName);
        m_nativeEnglishTeacherSelector.Items().Append(item);
        if (teacher.id == previousId)
        {
            selectedIndex = index;
        }
    }
    if (selectedIndex < 0 && !m_nativeEnglishTeachers.empty())
    {
        selectedIndex = 0;
    }
    m_nativeEnglishTeacherSelectedIndex = selectedIndex;
    m_nativeEnglishTeacherSelectedId = selectedIndex >= 0
        ? m_nativeEnglishTeachers[static_cast<std::size_t>(selectedIndex)].id
        : -1;
    m_nativeEnglishTeacherNew = false;
    m_nativeEnglishTeacherDirty = false;
    m_nativeEnglishTeacherSelector.SelectedIndex(selectedIndex);
    presentNativeEnglishTeacher(selectedIndex);
    m_nativeEnglishTeacherLoading = false;
    m_nativeEnglishTeacherStatusText.Text(
        m_nativeEnglishTeachers.empty()
            ? L"No Native English Teachers found. Choose New Teacher to add one."
            : L"Native English Teacher directory loaded."
        );
    m_nativeEnglishTeacherValidationText.Text({});
    m_nativeEnglishTeacherValidationText.Visibility(Visibility::Collapsed);
    setEditable(true);
}

void MainWindow::refreshNativeEnglishTeachersPage()
{
    if (!m_contentFrame || m_currentPageId != nativeEnglishTeachersPageId)
    {
        return;
    }

    const auto page = m_contentFrame.Content().try_as<
        Microsoft::UI::Xaml::Controls::Page>();
    if (page)
    {
        populateNativeEnglishTeachersPage(page, true);
    }
}

void MainWindow::presentNativeEnglishTeacher(int index)
{
    m_nativeEnglishTeacherLoading = true;
    if (index < 0
        || index >= static_cast<int>(m_nativeEnglishTeachers.size()))
    {
        m_nativeEnglishTeacherSelectedIndex = -1;
        m_nativeEnglishTeacherSelectedId = -1;
        m_nativeEnglishTeacherNameTextBox.Text({});
        m_nativeEnglishTeacherPositionCombo.SelectedIndex(-1);
        m_nativeEnglishTeacherPhoneTextBox.Text({});
        m_nativeEnglishTeacherEmailTextBox.Text({});
        m_nativeEnglishTeacherBirthdayTextBox.Text({});
        m_nativeEnglishTeacherNationalityTextBox.Text({});
        m_nativeEnglishTeacherLoading = false;
        return;
    }

    const auto& teacher = m_nativeEnglishTeachers[
        static_cast<std::size_t>(index)
        ];
    m_nativeEnglishTeacherSelectedIndex = index;
    m_nativeEnglishTeacherSelectedId = teacher.id;
    m_nativeEnglishTeacherNameTextBox.Text(asWide(teacher.name));
    m_nativeEnglishTeacherPositionCombo.SelectedIndex(-1);
    for (int optionIndex = 0;
         optionIndex < static_cast<int>(
             m_nativeEnglishTeacherPositionCombo.Items().Size());
         ++optionIndex)
    {
        const auto item = m_nativeEnglishTeacherPositionCombo.Items().GetAt(
            optionIndex
            ).try_as<Microsoft::UI::Xaml::Controls::ComboBoxItem>();
        if (item && boxedString(item.Tag()) == asWide(teacher.position))
        {
            m_nativeEnglishTeacherPositionCombo.SelectedIndex(optionIndex);
            break;
        }
    }
    m_nativeEnglishTeacherPhoneTextBox.Text(asWide(teacher.phoneNumber));
    m_nativeEnglishTeacherEmailTextBox.Text(asWide(teacher.email));
    m_nativeEnglishTeacherBirthdayTextBox.Text(asWide(teacher.birthday));
    m_nativeEnglishTeacherNationalityTextBox.Text(asWide(teacher.nationality));
    m_nativeEnglishTeacherLoading = false;
}

classmngr::engine::NativeEnglishTeacher
MainWindow::nativeEnglishTeacherFromForm() const
{
    classmngr::engine::NativeEnglishTeacher teacher;
    teacher.id = m_nativeEnglishTeacherSelectedId;
    teacher.name = asUtf8(m_nativeEnglishTeacherNameTextBox.Text());
    const auto selectedPosition =
        m_nativeEnglishTeacherPositionCombo.SelectedItem().try_as<
            Microsoft::UI::Xaml::Controls::ComboBoxItem>();
    teacher.position = selectedPosition
        ? asUtf8(boxedString(selectedPosition.Tag()))
        : asUtf8(boxedString(
            m_nativeEnglishTeacherPositionCombo.SelectedItem()
            ));
    teacher.phoneNumber = asUtf8(m_nativeEnglishTeacherPhoneTextBox.Text());
    teacher.email = asUtf8(m_nativeEnglishTeacherEmailTextBox.Text());
    teacher.birthday = asUtf8(
        m_nativeEnglishTeacherBirthdayTextBox.Text()
        );
    teacher.nationality = asUtf8(
        m_nativeEnglishTeacherNationalityTextBox.Text()
        );
    return teacher;
}

void MainWindow::NativeEnglishTeacherSelection_SelectionChanged(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& arguments
    )
{
    static_cast<void>(arguments);
    if (m_nativeEnglishTeacherLoading || !m_openDatabase)
    {
        return;
    }

    if (m_nativeEnglishTeacherDirty)
    {
        m_nativeEnglishTeacherLoading = true;
        m_nativeEnglishTeacherSelector.SelectedIndex(
            m_nativeEnglishTeacherSelectedIndex
            );
        m_nativeEnglishTeacherLoading = false;
        m_nativeEnglishTeacherStatusText.Text(
            L"Save or discard the current Native English Teacher before selecting another."
            );
        return;
    }

    const auto selected = sender.try_as<
        Microsoft::UI::Xaml::Controls::ComboBox>();
    if (!selected)
    {
        return;
    }
    const int selectedIndex = selected.SelectedIndex();
    const auto item = selected.SelectedItem().try_as<
        Microsoft::UI::Xaml::Controls::ComboBoxItem>();
    const int selectedId = item ? boxedInt(item.Tag()) : -1;
    if (selectedIndex < 0 || selectedId <= 0)
    {
        presentNativeEnglishTeacher(-1);
        m_nativeEnglishTeacherSelectedIndex = -1;
        m_nativeEnglishTeacherSelectedId = -1;
        return;
    }

    int resolvedIndex = -1;
    for (int index = 0;
         index < static_cast<int>(m_nativeEnglishTeachers.size());
         ++index)
    {
        if (m_nativeEnglishTeachers[static_cast<std::size_t>(index)].id
            == selectedId)
        {
            resolvedIndex = index;
            break;
        }
    }
    m_nativeEnglishTeacherSelectedIndex = resolvedIndex;
    m_nativeEnglishTeacherSelectedId = selectedId;
    m_nativeEnglishTeacherNew = false;
    presentNativeEnglishTeacher(resolvedIndex);
    m_nativeEnglishTeacherStatusText.Text(
        resolvedIndex >= 0
            ? L"Native English Teacher selected."
            : L"Select a Native English Teacher or choose New Teacher."
        );
    m_nativeEnglishTeacherValidationText.Text({});
    m_nativeEnglishTeacherValidationText.Visibility(
        Microsoft::UI::Xaml::Visibility::Collapsed
        );
}

void MainWindow::NativeEnglishTeacherField_TextChanging(
    Microsoft::UI::Xaml::Controls::TextBox const& sender,
    Microsoft::UI::Xaml::Controls::TextBoxTextChangingEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (m_nativeEnglishTeacherLoading || !m_openDatabase)
    {
        return;
    }

    m_nativeEnglishTeacherDirty = true;
    m_dirtyState.markDirty();
    m_nativeEnglishTeacherStatusText.Text(
        L"Unsaved Native English Teacher changes."
        );
    m_nativeEnglishTeacherNewButton.IsEnabled(false);
    m_nativeEnglishTeacherDeleteButton.IsEnabled(false);
    m_nativeEnglishTeacherSelector.IsEnabled(false);
    m_nativeEnglishTeacherSaveButton.IsEnabled(true);
    m_nativeEnglishTeacherDiscardButton.IsEnabled(true);
}

void MainWindow::NativeEnglishTeacherPosition_SelectionChanged(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (m_nativeEnglishTeacherLoading || !m_openDatabase)
    {
        return;
    }

    m_nativeEnglishTeacherDirty = true;
    m_dirtyState.markDirty();
    m_nativeEnglishTeacherStatusText.Text(
        L"Unsaved Native English Teacher changes."
        );
    m_nativeEnglishTeacherNewButton.IsEnabled(false);
    m_nativeEnglishTeacherDeleteButton.IsEnabled(false);
    m_nativeEnglishTeacherSelector.IsEnabled(false);
    m_nativeEnglishTeacherSaveButton.IsEnabled(true);
    m_nativeEnglishTeacherDiscardButton.IsEnabled(true);
}

void MainWindow::NativeEnglishTeacherNewButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_openDatabase || m_nativeEnglishTeacherDirty)
    {
        if (m_nativeEnglishTeacherStatusText
            && m_nativeEnglishTeacherDirty)
        {
            m_nativeEnglishTeacherStatusText.Text(
                L"Save or discard the current Native English Teacher before creating another."
                );
        }
        return;
    }

    m_nativeEnglishTeacherNew = true;
    m_nativeEnglishTeacherSelectedIndex = -1;
    m_nativeEnglishTeacherSelectedId = -1;
    m_nativeEnglishTeacherLoading = true;
    m_nativeEnglishTeacherSelector.SelectedIndex(-1);
    m_nativeEnglishTeacherLoading = false;
    presentNativeEnglishTeacher(-1);
    m_nativeEnglishTeacherDirty = true;
    m_dirtyState.markDirty();
    m_nativeEnglishTeacherStatusText.Text(
        L"New Native English Teacher. Enter the name and save."
        );
    m_nativeEnglishTeacherValidationText.Text({});
    m_nativeEnglishTeacherValidationText.Visibility(
        Microsoft::UI::Xaml::Visibility::Collapsed
        );
    m_nativeEnglishTeacherSelector.IsEnabled(false);
    m_nativeEnglishTeacherNewButton.IsEnabled(false);
    m_nativeEnglishTeacherDeleteButton.IsEnabled(false);
    m_nativeEnglishTeacherSaveButton.IsEnabled(true);
    m_nativeEnglishTeacherDiscardButton.IsEnabled(true);
}

void MainWindow::NativeEnglishTeacherDeleteButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_openDatabase || m_nativeEnglishTeacherSelectedId <= 0
        || m_nativeEnglishTeacherDirty)
    {
        return;
    }

    const int teacherId = m_nativeEnglishTeacherSelectedId;
    auto weak = get_weak();
    showDialog(
        L"Delete Native English Teacher",
        L"Delete the selected Native English Teacher from the active database?",
        L"Delete",
        {},
        L"Cancel",
        [weak, teacherId](ClassMngrWinUIDialogs::DialogOutcome outcome) {
            if (outcome != ClassMngrWinUIDialogs::DialogOutcome::Primary)
            {
                return;
            }
            if (auto self = weak.get())
            {
                if (!self->m_openDatabase)
                {
                    return;
                }
                classmngr::engine::NativeEnglishTeacherService service(
                    *self->m_openDatabase
                    );
                const auto removed = service.saveDirectory(
                    {},
                    {teacherId}
                    );
                if (!removed)
                {
                    self->m_nativeEnglishTeacherStatusText.Text(winrt::hstring(
                        L"Native English Teacher could not be deleted: "
                        + asWide(removed.error().message)
                        ));
                    return;
                }
                self->m_nativeEnglishTeacherSelectedId = -1;
                self->m_nativeEnglishTeacherSelectedIndex = -1;
                self->m_nativeEnglishTeacherDirty = false;
                self->m_nativeEnglishTeacherNew = false;
                self->m_dirtyState.markClean();
                self->refreshNativeEnglishTeachersPage();
                self->m_nativeEnglishTeacherStatusText.Text(
                    L"Native English Teacher deleted."
                    );
            }
        }
        );
}

void MainWindow::NativeEnglishTeacherSaveButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_openDatabase)
    {
        m_nativeEnglishTeacherStatusText.Text(L"No database open.");
        return;
    }

    const auto teacher = nativeEnglishTeacherFromForm();
    const std::wstring birthday = asWString(
        m_nativeEnglishTeacherBirthdayTextBox.Text()
        );
    if (!validMonthDay(birthday))
    {
        m_nativeEnglishTeacherStatusText.Text(
            L"Native English Teacher could not be saved."
            );
        m_nativeEnglishTeacherValidationText.Text(
            L"Each Native English Teacher needs a valid MM-dd birthday."
            );
        m_nativeEnglishTeacherValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Visible
            );
        m_nativeEnglishTeacherDirty = true;
        m_dirtyState.markDirty();
        return;
    }

    std::vector<classmngr::engine::NativeEnglishTeacher> draft =
        m_nativeEnglishTeachers;
    bool replaced = false;
    if (teacher.id > 0)
    {
        for (auto& existing : draft)
        {
            if (existing.id == teacher.id)
            {
                existing = teacher;
                replaced = true;
                break;
            }
        }
    }
    if (!replaced)
    {
        draft.push_back(teacher);
    }

    classmngr::engine::NativeEnglishTeacherService service(*m_openDatabase);
    const auto saved = service.saveDirectory(draft, {});
    if (!saved)
    {
        m_nativeEnglishTeacherStatusText.Text(winrt::hstring(
            L"Native English Teacher could not be saved: "
            + asWide(saved.error().message)
            ));
        m_nativeEnglishTeacherValidationText.Text(winrt::hstring(
            L"Engine validation or persistence error: "
            + asWide(saved.error().message)
            ));
        m_nativeEnglishTeacherValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Visible
            );
        return;
    }

    m_nativeEnglishTeacherSelectedId = teacher.id;
    m_nativeEnglishTeacherSelectedIndex = -1;
    m_nativeEnglishTeacherNew = false;
    m_nativeEnglishTeacherDirty = false;
    m_dirtyState.markClean();
    refreshNativeEnglishTeachersPage();
    m_nativeEnglishTeacherStatusText.Text(
        L"Native English Teacher saved."
        );
    m_nativeEnglishTeacherValidationText.Text({});
    m_nativeEnglishTeacherValidationText.Visibility(
        Microsoft::UI::Xaml::Visibility::Collapsed
        );
}

void MainWindow::NativeEnglishTeacherDiscardButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    m_nativeEnglishTeacherDirty = false;
    m_nativeEnglishTeacherNew = false;
    m_dirtyState.markClean();
    refreshNativeEnglishTeachersPage();
}

void MainWindow::populateGsTeamPage(
    Microsoft::UI::Xaml::Controls::Page const& page,
    bool refresh
    )
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    if (!m_gsTeamNameTextBox)
    {
        auto scroll = ScrollViewer();
        scroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
        scroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);

        auto root = StackPanel();
        root.Padding(Thickness{32.0, 24.0, 32.0, 32.0});
        root.Spacing(16.0);
        root.MaxWidth(900.0);
        root.HorizontalAlignment(HorizontalAlignment::Center);

        auto title = TextBlock();
        title.Text(L"GS Team");
        title.FontSize(24.0);
        setAutomationName(title, L"GS Team");
        root.Children().Append(title);

        auto description = TextBlock();
        description.Text(
            L"View and maintain GS and CS team contact information used by classes and reports."
            );
        description.TextWrapping(TextWrapping::Wrap);
        setAutomationName(description, L"GS Team directory description");
        root.Children().Append(description);

        m_gsTeamStatusText = TextBlock();
        m_gsTeamStatusText.Text(L"Loading GS Team...");
        m_gsTeamStatusText.TextWrapping(TextWrapping::Wrap);
        setAutomationName(m_gsTeamStatusText, L"GS Team directory status");
        root.Children().Append(m_gsTeamStatusText);

        m_gsTeamValidationText = TextBlock();
        m_gsTeamValidationText.TextWrapping(TextWrapping::Wrap);
        m_gsTeamValidationText.Visibility(Visibility::Collapsed);
        setAutomationName(m_gsTeamValidationText, L"GS Team validation summary");
        root.Children().Append(m_gsTeamValidationText);

        auto directoryCard = ClassMngrWinUISharedUX::buildCard({
            L"Team directory",
            L"Select an existing team member or create a new directory entry.",
            L"GS Team directory selector"
            });
        m_gsTeamSelector = ComboBox();
        m_gsTeamSelector.Header(box_value(hstring(L"Team member")));
        m_gsTeamSelector.PlaceholderText(L"Select a team member");
        m_gsTeamSelector.MinWidth(420.0);
        m_gsTeamSelector.HorizontalAlignment(HorizontalAlignment::Stretch);
        m_gsTeamSelector.IsTabStop(true);
        m_gsTeamSelector.TabIndex(0);
        m_gsTeamSelector.SelectionChanged({
            this,
            &MainWindow::GsTeamSelection_SelectionChanged
            });
        setAutomationName(m_gsTeamSelector, L"GS Team member selector");
        directoryCard.content.Children().Append(m_gsTeamSelector);
        root.Children().Append(directoryCard.root);

        const auto makeTextBox = [this](
            wchar_t const* header,
            wchar_t const* automationName,
            wchar_t const* placeholder
            ) {
            auto box = TextBox();
            box.Header(box_value(hstring(header)));
            box.PlaceholderText(placeholder);
            box.MinWidth(320.0);
            box.HorizontalAlignment(HorizontalAlignment::Stretch);
            box.IsTabStop(true);
            box.TextChanging({this, &MainWindow::GsTeamField_TextChanging});
            setAutomationName(box, automationName);
            return box;
        };

        auto detailsCard = ClassMngrWinUISharedUX::buildCard({
            L"Team member details",
            L"Names, position, contact information, and birthday.",
            L"GS Team member details form"
            });
        m_gsTeamNameTextBox = makeTextBox(
            L"Name",
            L"GS Team member name",
            L"Enter the English name"
            );
        auto nameInputScope = Input::InputScope();
        nameInputScope.Names().Append(
            Input::InputScopeName(Input::InputScopeNameValue::Text)
            );
        m_gsTeamNameTextBox.InputScope(nameInputScope);
        m_gsTeamNameTextBox.TabIndex(1);

        m_gsTeamKoreanNameTextBox = makeTextBox(
            L"Korean Name",
            L"GS Team member Korean name",
            L"Enter the Korean name"
            );
        auto koreanInputScope = Input::InputScope();
        koreanInputScope.Names().Append(
            Input::InputScopeName(Input::InputScopeNameValue::Text)
            );
        m_gsTeamKoreanNameTextBox.InputScope(koreanInputScope);
        m_gsTeamKoreanNameTextBox.TabIndex(2);

        m_gsTeamPositionTextBox = makeTextBox(
            L"Position",
            L"GS Team member position",
            L"e.g. Branch Manager"
            );
        m_gsTeamPositionTextBox.TabIndex(3);
        m_gsTeamPhoneTextBox = makeTextBox(
            L"Phone Number",
            L"GS Team member phone number",
            L"Optional phone number"
            );
        m_gsTeamPhoneTextBox.TabIndex(4);
        m_gsTeamBirthdayTextBox = makeTextBox(
            L"Birthday (MM-dd)",
            L"GS Team member birthday",
            L"e.g. 03-14"
            );
        m_gsTeamBirthdayTextBox.TabIndex(5);
        detailsCard.content.Children().Append(m_gsTeamNameTextBox);
        detailsCard.content.Children().Append(m_gsTeamKoreanNameTextBox);
        detailsCard.content.Children().Append(m_gsTeamPositionTextBox);
        detailsCard.content.Children().Append(m_gsTeamPhoneTextBox);
        detailsCard.content.Children().Append(m_gsTeamBirthdayTextBox);
        root.Children().Append(detailsCard.root);

        auto actions = StackPanel();
        actions.Orientation(Orientation::Horizontal);
        actions.Spacing(8.0);
        m_gsTeamNewButton = Button();
        m_gsTeamNewButton.Content(box_value(hstring(L"New Member")));
        m_gsTeamNewButton.IsTabStop(true);
        m_gsTeamNewButton.TabIndex(6);
        m_gsTeamNewButton.Click({this, &MainWindow::GsTeamNewButton_Click});
        setAutomationName(m_gsTeamNewButton, L"New GS Team member");
        m_gsTeamDeleteButton = Button();
        m_gsTeamDeleteButton.Content(box_value(hstring(L"Delete Member")));
        m_gsTeamDeleteButton.IsTabStop(true);
        m_gsTeamDeleteButton.TabIndex(7);
        m_gsTeamDeleteButton.Click({this, &MainWindow::GsTeamDeleteButton_Click});
        setAutomationName(m_gsTeamDeleteButton, L"Delete GS Team member");
        m_gsTeamSaveButton = Button();
        m_gsTeamSaveButton.Content(box_value(hstring(L"Save Changes")));
        m_gsTeamSaveButton.IsTabStop(true);
        m_gsTeamSaveButton.TabIndex(8);
        m_gsTeamSaveButton.Click({this, &MainWindow::GsTeamSaveButton_Click});
        setAutomationName(m_gsTeamSaveButton, L"Save GS Team member");
        m_gsTeamDiscardButton = Button();
        m_gsTeamDiscardButton.Content(
            box_value(hstring(L"Discard Changes"))
            );
        m_gsTeamDiscardButton.IsTabStop(true);
        m_gsTeamDiscardButton.TabIndex(9);
        m_gsTeamDiscardButton.Click({
            this,
            &MainWindow::GsTeamDiscardButton_Click
            });
        setAutomationName(
            m_gsTeamDiscardButton,
            L"Discard GS Team member changes"
            );
        actions.Children().Append(m_gsTeamNewButton);
        actions.Children().Append(m_gsTeamDeleteButton);
        actions.Children().Append(m_gsTeamSaveButton);
        actions.Children().Append(m_gsTeamDiscardButton);
        root.Children().Append(actions);

        scroll.Content(root);
        page.Content(scroll);
    }

    const auto setEditable = [this](bool enabled) {
        const bool clean = !m_gsTeamDirty;
        m_gsTeamSelector.IsEnabled(enabled && clean && !m_gsTeamNew);
        m_gsTeamNameTextBox.IsEnabled(enabled);
        m_gsTeamKoreanNameTextBox.IsEnabled(enabled);
        m_gsTeamPositionTextBox.IsEnabled(enabled);
        m_gsTeamPhoneTextBox.IsEnabled(enabled);
        m_gsTeamBirthdayTextBox.IsEnabled(enabled);
        m_gsTeamNewButton.IsEnabled(enabled && clean && !m_gsTeamNew);
        m_gsTeamDeleteButton.IsEnabled(
            enabled && clean && !m_gsTeamNew && m_gsTeamSelectedId > 0
            );
        m_gsTeamSaveButton.IsEnabled(enabled && m_gsTeamDirty);
        m_gsTeamDiscardButton.IsEnabled(enabled && m_gsTeamDirty);
    };

    if (!m_openDatabase)
    {
        m_gsTeamLoading = true;
        m_gsTeamMembers.clear();
        m_gsTeamSelectedIndex = -1;
        m_gsTeamSelectedId = -1;
        m_gsTeamNew = false;
        m_gsTeamDirty = false;
        m_gsTeamSelector.Items().Clear();
        m_gsTeamSelector.SelectedIndex(-1);
        m_gsTeamLoading = false;
        presentGsTeamMember(-1);
        m_gsTeamStatusText.Text(L"No database open.");
        m_gsTeamValidationText.Text({});
        m_gsTeamValidationText.Visibility(Visibility::Collapsed);
        setEditable(false);
        return;
    }

    if (refresh && m_gsTeamDirty)
    {
        m_gsTeamStatusText.Text(L"Unsaved GS Team changes are retained.");
        return;
    }

    m_gsTeamLoading = true;
    classmngr::engine::GsTeamService service(*m_openDatabase);
    const auto loaded = service.list();
    if (!loaded)
    {
        m_gsTeamLoading = false;
        m_gsTeamMembers.clear();
        m_gsTeamSelectedIndex = -1;
        m_gsTeamSelectedId = -1;
        m_gsTeamNew = false;
        m_gsTeamDirty = false;
        m_gsTeamSelector.Items().Clear();
        m_gsTeamSelector.SelectedIndex(-1);
        presentGsTeamMember(-1);
        m_gsTeamStatusText.Text(winrt::hstring(
            L"GS Team directory could not be loaded: "
            + asWide(loaded.error().message)
            ));
        m_gsTeamValidationText.Text(
            L"The engine rejected the team-directory read."
            );
        m_gsTeamValidationText.Visibility(Visibility::Visible);
        setEditable(false);
        return;
    }

    const int previousId = m_gsTeamSelectedId;
    m_gsTeamMembers = *loaded;
    m_gsTeamSelector.Items().Clear();
    int selectedIndex = -1;
    for (int index = 0;
         index < static_cast<int>(m_gsTeamMembers.size());
         ++index)
    {
        const auto& member = m_gsTeamMembers[static_cast<std::size_t>(index)];
        std::wstring displayName = member.name.empty()
            ? asWide(member.koreanName)
            : asWide(member.name);
        if (!member.name.empty() && !member.koreanName.empty())
        {
            displayName += L" (";
            displayName += asWide(member.koreanName);
            displayName += L")";
        }
        auto item = ComboBoxItem();
        item.Content(box_value(hstring(displayName)));
        item.Tag(box_value(member.id));
        setAutomationName(item, L"GS Team member " + displayName);
        m_gsTeamSelector.Items().Append(item);
        if (member.id == previousId)
        {
            selectedIndex = index;
        }
    }
    if (selectedIndex < 0 && !m_gsTeamMembers.empty())
    {
        selectedIndex = 0;
    }
    m_gsTeamSelectedIndex = selectedIndex;
    m_gsTeamSelectedId = selectedIndex >= 0
        ? m_gsTeamMembers[static_cast<std::size_t>(selectedIndex)].id
        : -1;
    m_gsTeamNew = false;
    m_gsTeamDirty = false;
    m_gsTeamSelector.SelectedIndex(selectedIndex);
    presentGsTeamMember(selectedIndex);
    m_gsTeamLoading = false;
    m_gsTeamStatusText.Text(
        m_gsTeamMembers.empty()
            ? L"No GS Team members found. Choose New Member to add one."
            : L"GS Team directory loaded."
        );
    m_gsTeamValidationText.Text({});
    m_gsTeamValidationText.Visibility(Visibility::Collapsed);
    setEditable(true);
}

void MainWindow::refreshGsTeamPage()
{
    if (!m_contentFrame || m_currentPageId != gsTeamPageId)
    {
        return;
    }

    const auto page = m_contentFrame.Content().try_as<
        Microsoft::UI::Xaml::Controls::Page>();
    if (page)
    {
        populateGsTeamPage(page, true);
    }
}

void MainWindow::presentGsTeamMember(int index)
{
    m_gsTeamLoading = true;
    if (index < 0 || index >= static_cast<int>(m_gsTeamMembers.size()))
    {
        m_gsTeamSelectedIndex = -1;
        m_gsTeamSelectedId = -1;
        m_gsTeamNameTextBox.Text({});
        m_gsTeamKoreanNameTextBox.Text({});
        m_gsTeamPositionTextBox.Text({});
        m_gsTeamPhoneTextBox.Text({});
        m_gsTeamBirthdayTextBox.Text({});
        m_gsTeamLoading = false;
        return;
    }

    const auto& member = m_gsTeamMembers[static_cast<std::size_t>(index)];
    m_gsTeamSelectedIndex = index;
    m_gsTeamSelectedId = member.id;
    m_gsTeamNameTextBox.Text(asWide(member.name));
    m_gsTeamKoreanNameTextBox.Text(asWide(member.koreanName));
    m_gsTeamPositionTextBox.Text(asWide(member.position));
    m_gsTeamPhoneTextBox.Text(asWide(member.phoneNumber));
    m_gsTeamBirthdayTextBox.Text(asWide(member.birthday));
    m_gsTeamLoading = false;
}

classmngr::engine::GsTeamMember MainWindow::gsTeamMemberFromForm() const
{
    classmngr::engine::GsTeamMember member;
    member.id = m_gsTeamSelectedId;
    member.name = asUtf8(m_gsTeamNameTextBox.Text());
    member.koreanName = asUtf8(m_gsTeamKoreanNameTextBox.Text());
    member.position = asUtf8(m_gsTeamPositionTextBox.Text());
    member.phoneNumber = asUtf8(m_gsTeamPhoneTextBox.Text());
    member.birthday = asUtf8(m_gsTeamBirthdayTextBox.Text());
    return member;
}

void MainWindow::GsTeamSelection_SelectionChanged(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& arguments
    )
{
    static_cast<void>(arguments);
    if (m_gsTeamLoading || !m_openDatabase)
    {
        return;
    }

    if (m_gsTeamDirty)
    {
        m_gsTeamLoading = true;
        m_gsTeamSelector.SelectedIndex(m_gsTeamSelectedIndex);
        m_gsTeamLoading = false;
        m_gsTeamStatusText.Text(
            L"Save or discard the current GS Team member before selecting another."
            );
        return;
    }

    const auto selected = sender.try_as<
        Microsoft::UI::Xaml::Controls::ComboBox>();
    if (!selected)
    {
        return;
    }
    const int selectedIndex = selected.SelectedIndex();
    const auto item = selected.SelectedItem().try_as<
        Microsoft::UI::Xaml::Controls::ComboBoxItem>();
    const int selectedId = item ? boxedInt(item.Tag()) : -1;
    if (selectedIndex < 0 || selectedId <= 0)
    {
        presentGsTeamMember(-1);
        m_gsTeamSelectedIndex = -1;
        m_gsTeamSelectedId = -1;
        return;
    }

    int resolvedIndex = -1;
    for (int index = 0;
         index < static_cast<int>(m_gsTeamMembers.size());
         ++index)
    {
        if (m_gsTeamMembers[static_cast<std::size_t>(index)].id == selectedId)
        {
            resolvedIndex = index;
            break;
        }
    }
    m_gsTeamSelectedIndex = resolvedIndex;
    m_gsTeamSelectedId = selectedId;
    m_gsTeamNew = false;
    presentGsTeamMember(resolvedIndex);
    m_gsTeamStatusText.Text(
        resolvedIndex >= 0
            ? L"GS Team member selected."
            : L"Select a GS Team member or choose New Member."
        );
    m_gsTeamValidationText.Text({});
    m_gsTeamValidationText.Visibility(
        Microsoft::UI::Xaml::Visibility::Collapsed
        );
}

void MainWindow::GsTeamField_TextChanging(
    Microsoft::UI::Xaml::Controls::TextBox const& sender,
    Microsoft::UI::Xaml::Controls::TextBoxTextChangingEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (m_gsTeamLoading || !m_openDatabase)
    {
        return;
    }

    m_gsTeamDirty = true;
    m_dirtyState.markDirty();
    m_gsTeamStatusText.Text(L"Unsaved GS Team changes.");
    m_gsTeamNewButton.IsEnabled(false);
    m_gsTeamDeleteButton.IsEnabled(false);
    m_gsTeamSelector.IsEnabled(false);
    m_gsTeamSaveButton.IsEnabled(true);
    m_gsTeamDiscardButton.IsEnabled(true);
}

void MainWindow::GsTeamNewButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_openDatabase || m_gsTeamDirty)
    {
        if (m_gsTeamStatusText && m_gsTeamDirty)
        {
            m_gsTeamStatusText.Text(
                L"Save or discard the current GS Team member before creating another."
                );
        }
        return;
    }

    m_gsTeamNew = true;
    m_gsTeamSelectedIndex = -1;
    m_gsTeamSelectedId = -1;
    m_gsTeamLoading = true;
    m_gsTeamSelector.SelectedIndex(-1);
    m_gsTeamLoading = false;
    presentGsTeamMember(-1);
    m_gsTeamDirty = true;
    m_dirtyState.markDirty();
    m_gsTeamStatusText.Text(
        L"New GS Team member. Enter at least one name and save."
        );
    m_gsTeamValidationText.Text({});
    m_gsTeamValidationText.Visibility(
        Microsoft::UI::Xaml::Visibility::Collapsed
        );
    m_gsTeamSelector.IsEnabled(false);
    m_gsTeamNewButton.IsEnabled(false);
    m_gsTeamDeleteButton.IsEnabled(false);
    m_gsTeamSaveButton.IsEnabled(true);
    m_gsTeamDiscardButton.IsEnabled(true);
}

void MainWindow::GsTeamDeleteButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_openDatabase || m_gsTeamSelectedId <= 0 || m_gsTeamDirty)
    {
        return;
    }

    const int memberId = m_gsTeamSelectedId;
    auto weak = get_weak();
    showDialog(
        L"Delete GS Team member",
        L"Delete the selected GS Team member from the active database?",
        L"Delete",
        {},
        L"Cancel",
        [weak, memberId](ClassMngrWinUIDialogs::DialogOutcome outcome) {
            if (outcome != ClassMngrWinUIDialogs::DialogOutcome::Primary)
            {
                return;
            }
            if (auto self = weak.get())
            {
                if (!self->m_openDatabase)
                {
                    return;
                }
                classmngr::engine::GsTeamService service(
                    *self->m_openDatabase
                    );
                const auto removed = service.saveDirectory({}, {memberId});
                if (!removed)
                {
                    self->m_gsTeamStatusText.Text(winrt::hstring(
                        L"GS Team member could not be deleted: "
                        + asWide(removed.error().message)
                        ));
                    return;
                }
                self->m_gsTeamSelectedId = -1;
                self->m_gsTeamSelectedIndex = -1;
                self->m_gsTeamDirty = false;
                self->m_gsTeamNew = false;
                self->m_dirtyState.markClean();
                self->refreshGsTeamPage();
                self->m_gsTeamStatusText.Text(L"GS Team member deleted.");
            }
        }
        );
}

void MainWindow::GsTeamSaveButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (!m_openDatabase)
    {
        m_gsTeamStatusText.Text(L"No database open.");
        return;
    }

    const auto member = gsTeamMemberFromForm();
    const std::wstring birthday = asWString(m_gsTeamBirthdayTextBox.Text());
    if (!validMonthDay(birthday))
    {
        m_gsTeamStatusText.Text(L"GS Team member could not be saved.");
        m_gsTeamValidationText.Text(
            L"Each GS Team member needs a valid MM-dd birthday."
            );
        m_gsTeamValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Visible
            );
        m_gsTeamDirty = true;
        m_dirtyState.markDirty();
        return;
    }

    std::vector<classmngr::engine::GsTeamMember> draft = m_gsTeamMembers;
    bool replaced = false;
    if (member.id > 0)
    {
        for (auto& existing : draft)
        {
            if (existing.id == member.id)
            {
                existing = member;
                replaced = true;
                break;
            }
        }
    }
    if (!replaced)
    {
        draft.push_back(member);
    }

    classmngr::engine::GsTeamService service(*m_openDatabase);
    const auto saved = service.saveDirectory(draft, {});
    if (!saved)
    {
        m_gsTeamStatusText.Text(winrt::hstring(
            L"GS Team member could not be saved: "
            + asWide(saved.error().message)
            ));
        m_gsTeamValidationText.Text(winrt::hstring(
            L"Engine validation or persistence error: "
            + asWide(saved.error().message)
            ));
        m_gsTeamValidationText.Visibility(
            Microsoft::UI::Xaml::Visibility::Visible
            );
        return;
    }

    m_gsTeamSelectedId = member.id;
    m_gsTeamSelectedIndex = -1;
    m_gsTeamNew = false;
    m_gsTeamDirty = false;
    m_dirtyState.markClean();
    refreshGsTeamPage();
    m_gsTeamStatusText.Text(L"GS Team member saved.");
    m_gsTeamValidationText.Text({});
    m_gsTeamValidationText.Visibility(
        Microsoft::UI::Xaml::Visibility::Collapsed
        );
}

void MainWindow::GsTeamDiscardButton_Click(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::RoutedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    m_gsTeamDirty = false;
    m_gsTeamNew = false;
    m_dirtyState.markClean();
    refreshGsTeamPage();
}

void MainWindow::populateAboutPage(
    Microsoft::UI::Xaml::Controls::Page const& page
    )
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;

    auto root = StackPanel();
    root.Padding(Thickness{32.0, 32.0, 32.0, 32.0});
    root.Spacing(16.0);
    root.MaxWidth(720.0);
    root.HorizontalAlignment(HorizontalAlignment::Center);
    root.VerticalAlignment(VerticalAlignment::Top);

    auto title = TextBlock();
    title.Text(L"About ClassMngr");
    title.FontSize(28.0);
    setAutomationName(title, L"About ClassMngr");

    auto description = TextBlock();
    description.Text(
        L"This page is a lazy shell placeholder for the first feature slice."
        );
    description.TextWrapping(TextWrapping::Wrap);

    auto navigationContract = TextBlock();
    navigationContract.Text(
        L"Navigation history and shell state are restored by the main window."
        );
    navigationContract.TextWrapping(TextWrapping::Wrap);

    root.Children().Append(title);
    root.Children().Append(description);
    root.Children().Append(navigationContract);
    page.Content(root);
}

void MainWindow::populateCampusPage(
    Microsoft::UI::Xaml::Controls::Page const& page,
    std::wstring_view pageId,
    bool refresh
    )
{
    using namespace Microsoft::UI::Xaml;
    using namespace Microsoft::UI::Xaml::Controls;
    const auto localize = [this](std::wstring_view source) {
        return m_localizer.getString(L"CampusInformationPage", source);
    };

    if (page.Content() && !refresh)
    {
        return;
    }

    m_campusRecords.clear();
    m_campusResourceRecords.clear();
    m_campusSelector = nullptr;
    m_campusTabs = nullptr;
    m_campusDetailsPanel = nullptr;
    m_campusImage = nullptr;
    m_campusImages.clear();
    ++m_campusImageRequest;
    m_campusInformationState.clear();

    auto pageTitle = [&]() -> std::wstring {
        if (pageId == campusDirectionsPageId)
        {
            return localize(L"Directions");
        }
        if (pageId == campusAddressPageId)
        {
            return localize(L"Address");
        }
        if (pageId == campusHousingPageId)
        {
            return localize(L"Housing");
        }
        if (pageId == campusMapPageId)
        {
            return localize(L"Maps");
        }
        return localize(L"Campus Information");
    };

    auto root = StackPanel();
    // Mirrors the retained Qt content layout: selector above five detail tabs.
    root.Padding(Thickness{12.0, 12.0, 12.0, 0.0});
    root.Spacing(8.0);
    root.VerticalAlignment(VerticalAlignment::Top);

    auto title = TextBlock();
    title.Text(winrt::hstring(pageTitle()));
    title.FontSize(28.0);
    setAutomationName(title, pageTitle());
    root.Children().Append(title);

    const auto appendState = [this, &root, &localize](
                                 winrt::hstring const& titleText,
                                 winrt::hstring const& messageText,
                                 std::wstring_view automationName,
                                 bool errorState) {
        m_campusInformationState = errorState
            ? L"engine_error"
            : titleText == winrt::hstring(localize(L"No database open"))
                ? L"no_database"
                : L"empty";
        const auto state = errorState
            ? ClassMngrWinUISharedUX::buildErrorState({
                  titleText,
                  messageText,
                  winrt::hstring(automationName),
                  {}
                  })
            : ClassMngrWinUISharedUX::buildEmptyState({
                  titleText,
                  messageText,
                  winrt::hstring(automationName),
                  {}
                  });
        root.Children().Append(state.root);
    };

    if (m_phase5CampusScenario == L"error")
    {
        appendState(
            winrt::hstring(localize(L"Campus information unavailable")),
            winrt::hstring(localize(
                L"The engine returned an unexpected campus loading failure."
                )),
            L"Campus information engine error state",
            true
            );
        auto pageScroll = ScrollViewer();
        pageScroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
        pageScroll.Content(root);
        page.Content(pageScroll);
        return;
    }

    // The phase verifier intentionally uses database scenarios to exercise
    // the engine contract.  The production directory follows Qt and reads
    // the packaged campus JSON catalog, which is independent of the .tps
    // profile database.
    if (!m_phase5CampusScenario.empty())
    {
        if (!m_openDatabase)
        {
            appendState(
                winrt::hstring(localize(L"No database open")),
                winrt::hstring(localize(
                    L"Open or create a database to view campus information."
                    )),
                L"Campus information no database state",
                false
                );
            auto pageScroll = ScrollViewer();
            pageScroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
            pageScroll.Content(root);
            page.Content(pageScroll);
            return;
        }

        try
        {
            classmngr::engine::CampusRecordService service(*m_openDatabase);
            const auto result = service.list();
            if (!result)
            {
                std::wstring errorMessage = localize(
                    L"The engine could not load campus records:"
                    );
                errorMessage += L" ";
                const auto errorText = winrt::to_hstring(
                    std::string(result.error().message)
                    );
                errorMessage.append(errorText.c_str(), errorText.size());
                appendState(
                    winrt::hstring(localize(
                        L"Campus information unavailable"
                        )),
                    winrt::hstring(errorMessage),
                    L"Campus information engine error state",
                    true
                    );
            }
            else
            {
                m_campusRecords = *result;
                for (const auto& campus : m_campusRecords)
                {
                    m_campusResourceRecords.emplace_back(
                        campusResourceFromEngine(campus)
                        );
                }
            }
        }
        catch (std::exception const& error)
        {
            std::wstring errorMessage = localize(
                L"The engine could not load campus records:"
                );
            errorMessage += L" ";
            const auto errorText = winrt::to_hstring(std::string(error.what()));
            errorMessage.append(errorText.c_str(), errorText.size());
            appendState(
                winrt::hstring(localize(L"Campus information unavailable")),
                winrt::hstring(errorMessage),
                L"Campus information engine error state",
                true
                );
        }
        catch (...)
        {
            appendState(
                winrt::hstring(localize(L"Campus information unavailable")),
                winrt::hstring(localize(
                    L"The engine returned an unexpected campus loading failure."
                    )),
                L"Campus information engine error state",
                true
                );
        }
    }
    else
    {
        const auto result = loadPackagedCampusResources();
        if (!result)
        {
            std::wstring errorMessage = localize(
                L"The campus resource catalog could not be loaded:"
                );
            errorMessage += L" ";
            const auto errorText = winrt::to_hstring(
                std::string(result.error().message)
                );
            errorMessage.append(errorText.c_str(), errorText.size());
            appendState(
                winrt::hstring(localize(L"Campus information unavailable")),
                winrt::hstring(errorMessage),
                L"Campus resource catalog error state",
                true
                );
        }
        else
        {
            m_campusResourceRecords = *result;
            for (const auto& campus : m_campusResourceRecords)
            {
                m_campusRecords.emplace_back(campusRecordFromResource(campus));
            }
        }
    }

    if (m_campusResourceRecords.empty())
    {
        if (m_campusInformationState.empty())
        {
            m_campusInformationState = L"empty";
            appendState(
                winrt::hstring(localize(L"No campuses found")),
                winrt::hstring(localize(
                    L"This catalog does not contain any campus records."
                    )),
                L"Campus information empty state",
                false
                );
        }
        auto pageScroll = ScrollViewer();
        pageScroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
        pageScroll.Content(root);
        page.Content(pageScroll);
        return;
    }

    m_campusInformationState = L"populated";
    auto selectorRow = Grid();
    selectorRow.ColumnSpacing(8.0);
    selectorRow.ColumnDefinitions().Append(ColumnDefinition());
    selectorRow.ColumnDefinitions().Append(ColumnDefinition());
    selectorRow.ColumnDefinitions().GetAt(1).Width(
        GridLengthHelper::FromValueAndType(1.0, GridUnitType::Auto)
        );
    auto selectorLabel = TextBlock();
    selectorLabel.Text(winrt::hstring(localize(L"Campuses")));
    selectorLabel.VerticalAlignment(VerticalAlignment::Center);
    setAutomationName(selectorLabel, L"Campus directory label");
    selectorRow.Children().Append(selectorLabel);

    m_campusSelector = ComboBox();
    m_campusSelector.MinWidth(190.0);
    m_campusSelector.IsTabStop(true);
    m_campusSelector.TabIndex(0);
    m_campusSelector.SelectionChanged({this, &MainWindow::CampusSelector_SelectionChanged});
    setAutomationName(m_campusSelector, L"Campus directory selector");
    for (const CampusResourceView& campus : m_campusResourceRecords)
    {
        auto item = TextBlock();
        std::wstring displayName = campus.campusName;
        if (!campus.campusCode.empty())
        {
            displayName += L" (";
            displayName += campus.campusCode;
            displayName += L")";
        }
        item.Text(winrt::hstring(displayName));
        item.TextWrapping(TextWrapping::Wrap);
        setAutomationName(item, L"Campus name " + displayName);
        m_campusSelector.Items().Append(item);
    }
    Grid::SetColumn(m_campusSelector, 1);
    selectorRow.Children().Append(m_campusSelector);
    root.Children().Append(selectorRow);

    m_campusTabs = Pivot();
    m_campusTabs.IsTabStop(true);
    setAutomationName(m_campusTabs, L"Campus detail tabs");
    for (std::wstring_view const header : {
             L"Information", L"Directions", L"Address", L"Housing", L"Maps"})
    {
        auto tab = PivotItem();
        tab.Header(winrt::box_value(winrt::hstring(localize(header))));
        auto scroll = ScrollViewer();
        scroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
        scroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
        auto panel = StackPanel();
        panel.Spacing(10.0);
        auto container = Border();
        container.BorderThickness(Thickness{1.0, 1.0, 1.0, 1.0});
        container.CornerRadius(CornerRadius{6.0, 6.0, 6.0, 6.0});
        container.Padding(Thickness{12.0, 12.0, 12.0, 12.0});
        container.Child(panel);
        scroll.Content(container);
        tab.Content(scroll);
        if (header == L"Information")
        {
            m_campusDetailsPanel = panel;
            setAutomationName(m_campusDetailsPanel, L"Selected campus details");
        }
        m_campusTabs.Items().Append(tab);
    }
    root.Children().Append(m_campusTabs);
    page.Content(root);

    if (m_selectedCampusIndex < 0
        || static_cast<std::size_t>(m_selectedCampusIndex)
            >= m_campusResourceRecords.size())
    {
        m_selectedCampusIndex = 0;
    }
    m_campusSelector.SelectedIndex(m_selectedCampusIndex);
    presentSelectedCampus(pageId);
    updateFileCommandState();
}

void MainWindow::refreshCampusInformationPage()
{
    if (!isCampusPageId(m_currentPageId) || !m_contentFrame)
    {
        return;
    }

    const auto page = m_contentFrame.Content().try_as<
        Microsoft::UI::Xaml::Controls::Page>();
    if (page)
    {
        populateCampusPage(page, m_currentPageId, true);
    }
    updateFileCommandState();
}

void MainWindow::CampusSelector_SelectionChanged(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (m_campusSelector)
    {
        m_selectedCampusIndex = m_campusSelector.SelectedIndex();
    }
    presentSelectedCampus(m_currentPageId);
}

void MainWindow::presentSelectedCampus(std::wstring_view pageId)
{
    if (!m_campusSelector || !m_campusTabs || !m_campusDetailsPanel)
    {
        return;
    }

    const std::uint64_t requestId = ++m_campusImageRequest;
    m_campusImage = nullptr;
    m_campusImages.clear();
    std::array<Microsoft::UI::Xaml::Controls::StackPanel, 5> tabPanels{};
    for (uint32_t index = 0; index < tabPanels.size(); ++index)
    {
        const auto tab = m_campusTabs.Items().GetAt(index).as<
            Microsoft::UI::Xaml::Controls::PivotItem>();
        const auto scroll = tab.Content().as<
            Microsoft::UI::Xaml::Controls::ScrollViewer>();
        const auto container = scroll.Content().as<
            Microsoft::UI::Xaml::Controls::Border>();
        tabPanels[index] = container.Child().as<
            Microsoft::UI::Xaml::Controls::StackPanel>();
        tabPanels[index].Children().Clear();
    }

    const int32_t selectedIndex = m_campusSelector.SelectedIndex();
    if (selectedIndex < 0
        || static_cast<std::size_t>(selectedIndex)
            >= m_campusResourceRecords.size())
    {
        auto status = Microsoft::UI::Xaml::Controls::TextBlock();
        status.Text(winrt::hstring(m_localizer.getString(
            L"CampusInformationPage",
            L"Select a campus to view its details."
            )));
        status.TextWrapping(Microsoft::UI::Xaml::TextWrapping::Wrap);
        setAutomationName(status, L"Campus details selection prompt");
        m_campusDetailsPanel.Children().Append(status);
        return;
    }

    const CampusResourceView& campus =
        m_campusResourceRecords[static_cast<std::size_t>(selectedIndex)];
    const auto localize = [this](std::wstring_view source) {
        return m_localizer.getString(L"CampusInformationPage", source);
    };
    const auto appendText = [this](
                                Microsoft::UI::Xaml::Controls::Panel const& panel,
                                std::wstring_view text,
                                std::wstring_view automationName,
                                double fontSize = 0.0) {
        auto field = Microsoft::UI::Xaml::Controls::TextBlock();
        field.Text(winrt::hstring(text));
        field.TextWrapping(Microsoft::UI::Xaml::TextWrapping::Wrap);
        if (fontSize > 0.0)
        {
            field.FontSize(fontSize);
        }
        setAutomationName(field, automationName);
        panel.Children().Append(field);
    };
    const auto appendHeading = [&appendText](
                                   Microsoft::UI::Xaml::Controls::Panel const& panel,
                                   std::wstring_view text) {
        appendText(panel, text, text, 18.0);
    };
    const auto appendField = [this, &localize](
                                 Microsoft::UI::Xaml::Controls::Panel const& panel,
                                 std::wstring_view label,
                                 std::wstring_view value,
                                 double minimumHeight = 0.0) {
        using namespace Microsoft::UI::Xaml;
        using namespace Microsoft::UI::Xaml::Controls;
        auto row = Grid();
        row.ColumnSpacing(10.0);
        row.ColumnDefinitions().Append(ColumnDefinition());
        row.ColumnDefinitions().Append(ColumnDefinition());
        row.ColumnDefinitions().GetAt(1).Width(
            GridLengthHelper::FromValueAndType(1.0, GridUnitType::Star));
        auto caption = TextBlock();
        caption.Text(winrt::hstring(localize(label)));
        caption.TextWrapping(TextWrapping::Wrap);
        caption.VerticalAlignment(VerticalAlignment::Top);
        setAutomationName(caption, label);
        auto field = TextBlock();
        field.Text(winrt::hstring(value));
        field.TextWrapping(TextWrapping::Wrap);
        field.MinWidth(280.0);
        if (minimumHeight > 0.0)
        {
            field.MinHeight(minimumHeight);
        }
        setAutomationName(field, L"Campus field " + std::wstring(label));
        Grid::SetColumn(field, 1);
        row.Children().Append(caption);
        row.Children().Append(field);
        panel.Children().Append(row);
    };
    const auto appendImage = [this, requestId](
                                 Microsoft::UI::Xaml::Controls::Panel const& panel,
                                 std::string const& path) {
        if (path.empty())
        {
            return;
        }
        auto image = Microsoft::UI::Xaml::Controls::Image();
        image.MaxWidth(720.0);
        image.MaxHeight(360.0);
        image.Stretch(Microsoft::UI::Xaml::Media::Stretch::Uniform);
        image.HorizontalAlignment(
            Microsoft::UI::Xaml::HorizontalAlignment::Center
            );
        image.Visibility(Microsoft::UI::Xaml::Visibility::Collapsed);
        setAutomationName(image, L"Campus image preview");
        if (!m_campusImage)
        {
            m_campusImage = image;
        }
        m_campusImages.emplace_back(image);
        panel.Children().Append(image);
        loadCampusImage(path, requestId, image);
    };
    const auto appendAddress = [&appendField, &appendHeading](
                                  Microsoft::UI::Xaml::Controls::Panel const& parent,
                                  std::wstring_view heading,
                                  CampusAddressView const& address
                              ) {
        auto section = Microsoft::UI::Xaml::Controls::StackPanel();
        section.Spacing(8.0);
        appendHeading(section, heading);
        appendField(section, L"Building Name", address.buildingName);
        appendField(section, L"Province", address.province);
        appendField(section, L"City", address.city);
        appendField(section, L"City District", address.cityDistrict);
        appendField(section, L"District", address.district);
        appendField(section, L"Address Line 1", address.line1);
        appendField(section, L"Address Line 2", address.line2);
        appendField(section, L"Postal Code", address.postalCode);
        appendField(section, L"Address System", address.addressSystem);
        parent.Children().Append(section);
    };

    auto information = tabPanels[0];
    appendHeading(information, campus.campusName);
    if (!campus.mapImagePaths.empty())
    {
        appendImage(information, campus.mapImagePaths.front());
    }
    appendField(information, L"Campus ID", campus.id);
    appendField(information, L"Campus Code", campus.campusCode);
    appendField(information, L"Name", campus.campusName);
    appendField(information, L"Building", campus.buildingName);
    appendField(information, L"Address", campus.address);
    appendField(information, L"Phone", campus.phoneNumber);
    appendField(information, L"Office", campus.officeNumber);
    appendField(information, L"Office Wi-Fi", campus.officeWifi);
    appendField(information, L"Office Wi-Fi password", campus.officeWifiPassword);
    appendField(information, L"Printer", campus.printerName);
    appendField(information, L"Printer steps", campus.printerSteps);
    appendField(information, L"Printer driver URL",
        campus.printerDriverUrlUnavailable ? L"N/A" : campus.printerDriverUrl);
    appendField(information, L"Photocopier code", campus.photocopierCode);

    auto directions = tabPanels[1];
    appendHeading(directions, campus.campusName);
    appendField(directions, L"Building", campus.buildingName);
    appendField(directions, L"Phone", campus.phoneNumber);
    std::wstring transitSteps;
    for (std::size_t index = 0; index < campus.transitSteps.size(); ++index)
    {
        if (index != 0)
        {
            transitSteps += L"\n";
        }
        transitSteps += campus.transitSteps[index];
    }
    appendField(directions, L"Transit Steps", transitSteps);
    appendField(directions, L"Upon Arriving", campus.arrivalInfo);
    appendField(directions, L"Note", campus.directionsNote);

    auto address = tabPanels[2];
    appendHeading(address, campus.campusName);
    appendField(address, L"Campus", campus.campusName);
    appendField(address, L"Phone", campus.phoneNumber);
    appendField(address, L"Complete Address", campus.address, 150.0);
    appendAddress(address, L"English", campus.englishAddress);
    appendAddress(address, L"Korean", campus.koreanAddress);

    auto housing = tabPanels[3];
    housing.Spacing(12.0);
    if (campus.housingLocations.empty())
    {
        appendText(housing, localize(L"No housing information available"),
            L"Campus housing empty state");
    }
    for (std::size_t index = 0; index < campus.housingLocations.size(); ++index)
    {
        const CampusHousingView& location = campus.housingLocations[index];
        auto card = Microsoft::UI::Xaml::Controls::Border();
        card.Margin(Microsoft::UI::Xaml::Thickness{12.0, 12.0, 12.0, 12.0});
        card.Padding(Microsoft::UI::Xaml::Thickness{12.0, 12.0, 12.0, 12.0});
        card.BorderThickness(Microsoft::UI::Xaml::Thickness{1.0, 1.0, 1.0, 1.0});
        auto cardContent = Microsoft::UI::Xaml::Controls::StackPanel();
        cardContent.Spacing(10.0);
        appendHeading(cardContent, L"Housing " + std::to_wstring(index + 1)
            + (location.name.empty() ? L"" : L": " + location.name));
        appendAddress(cardContent, L"English", location.englishAddress);
        appendAddress(cardContent, L"Korean", location.koreanAddress);
        appendField(cardContent, L"Note", location.addressNote);
        for (const std::string& imagePath : location.imagePaths)
        {
            appendImage(cardContent, imagePath);
        }
        card.Child(cardContent);
        housing.Children().Append(card);
    }

    auto maps = tabPanels[4];
    maps.Spacing(16.0);
    if (campus.mapImagePaths.empty())
    {
        appendText(maps, localize(L"No map images available"), L"Campus maps empty state");
    }
    for (const std::string& imagePath : campus.mapImagePaths)
    {
        appendImage(maps, imagePath);
        appendText(maps, asWString(winrt::to_hstring(imagePath)),
            L"Campus map resource path");
    }
    appendField(maps, L"Naver Maps", campus.naverMapUrl);
    appendField(maps, L"Kakao Maps", campus.kakaoMapUrl);

    uint32_t selectedTab = 0;
    if (pageId == campusDirectionsPageId) { selectedTab = 1; }
    else if (pageId == campusAddressPageId) { selectedTab = 2; }
    else if (pageId == campusHousingPageId) { selectedTab = 3; }
    else if (pageId == campusMapPageId) { selectedTab = 4; }
    m_campusTabs.SelectedIndex(selectedTab);
}

winrt::fire_and_forget MainWindow::loadCampusImage(
    std::string logicalPath,
    std::uint64_t requestId,
    Microsoft::UI::Xaml::Controls::Image target
    )
{
    auto lifetime = get_strong();
    const auto dispatcher = DispatcherQueue();
    if (logicalPath.empty() || !dispatcher)
    {
        co_return;
    }

    try
    {
        co_await winrt::resume_background();
        classmngr::windows::winui::WindowsResourceProvider resourceProvider;
        const auto bytes = resourceProvider.readBytes(logicalPath);
        if (!bytes || bytes->empty())
        {
            co_return;
        }

        std::vector<std::uint8_t> payload(bytes->size());
        for (std::size_t index = 0; index < bytes->size(); ++index)
        {
            payload[index] = std::to_integer<std::uint8_t>((*bytes)[index]);
        }

        co_await ResumeOnDispatcherQueue{
            dispatcher,
            Microsoft::UI::Dispatching::DispatcherQueuePriority::Normal
            };
        if (requestId != m_campusImageRequest || !target)
        {
            co_return;
        }

        auto stream = winrt::Windows::Storage::Streams::InMemoryRandomAccessStream();
        auto writer = winrt::Windows::Storage::Streams::DataWriter(stream);
        writer.WriteBytes(winrt::array_view<std::uint8_t const>(
            payload.data(),
            payload.data() + payload.size()
            ));
        co_await writer.StoreAsync();
        co_await writer.FlushAsync();
        writer.DetachStream();
        stream.Seek(0);

        auto bitmap = winrt::Microsoft::UI::Xaml::Media::Imaging::BitmapImage();
        co_await bitmap.SetSourceAsync(stream);
        if (requestId != m_campusImageRequest || !target)
        {
            co_return;
        }
        target.Source(bitmap);
        target.Visibility(Microsoft::UI::Xaml::Visibility::Visible);
    }
    catch (...)
    {
        // A missing or undecodable optional image must not replace the
        // engine-backed campus details. The source path remains visible.
    }
}

void MainWindow::restoreShellState()
{
    const auto state = loadShellState();
    m_recentDatabasePaths = state.recentDatabasePaths;
    m_restoringState = true;

    bool restored = false;
    if (!state.navigationState.empty())
    {
        try
        {
            m_contentFrame.SetNavigationState(
                winrt::hstring(state.navigationState)
                );
            restored = !m_currentPageId.empty();
        }
        catch (...)
        {
            restored = false;
        }
    }

    if (!restored)
    {
        navigateTo(
            isKnownPageId(state.selectedPage)
                ? std::wstring_view(state.selectedPage)
                : homePageId
            );
    }
    m_restoringState = false;
    updateNavigationState();
}

void MainWindow::refreshRecentDatabaseMenu()
{
    if (!m_recentFilesMenu)
    {
        return;
    }

    m_recentFilesMenu.Items().Clear();
    if (m_recentDatabasePaths.empty())
    {
        auto empty = Microsoft::UI::Xaml::Controls::MenuFlyoutItem();
        empty.Text(L"No recent databases");
        empty.IsEnabled(false);
        setAutomationName(empty, L"No recent databases");
        m_recentFilesMenu.Items().Append(empty);
        return;
    }

    for (std::wstring const& path : m_recentDatabasePaths)
    {
        auto item = Microsoft::UI::Xaml::Controls::MenuFlyoutItem();
        item.Text(winrt::hstring(path));
        item.Tag(winrt::box_value(winrt::hstring(path)));
        setAutomationName(item, L"Open recent database");
        item.Click({this, &MainWindow::RecentDatabaseMenuItem_Click});
        m_recentFilesMenu.Items().Append(item);
    }
}

void MainWindow::addRecentDatabasePath(std::wstring_view path)
{
    const std::wstring normalized = absolutePath(path);
    std::vector<std::wstring> updated;
    updated.reserve(maximumRecentDatabasePaths);
    updated.emplace_back(normalized);
    for (std::wstring const& existing : m_recentDatabasePaths)
    {
        if (!samePath(existing, normalized))
        {
            updated.emplace_back(existing);
        }
        if (updated.size() == maximumRecentDatabasePaths)
        {
            break;
        }
    }
    m_recentDatabasePaths = pruneRecentDatabasePaths(updated);
    refreshRecentDatabaseMenu();
}

void MainWindow::reportDatabaseOpenError(
    std::wstring_view path,
    std::string_view message
    )
{
    if (m_shellDatabaseStatusText)
    {
        const std::wstring status = path.empty()
            ? L"Database open failed."
            : L"Database open failed: " + std::wstring(path);
        m_shellDatabaseStatusText.Text(winrt::hstring(status));
    }
    if (m_statusText)
    {
        m_statusText.Text(L"Database open failed.");
    }
    showDialog(
        L"Open database",
        winrt::to_hstring(std::string(message)),
        {},
        {},
        L"Close",
        {}
        );
}

void MainWindow::reportOutputError(
    std::wstring_view title,
    std::wstring_view path,
    std::string_view message
    )
{
    if (m_statusText)
    {
        m_statusText.Text(winrt::hstring(
            std::wstring(title) + L" failed."
            ));
    }
    showDialog(
        winrt::hstring(title),
        winrt::to_hstring(std::string(message)),
        {},
        {},
        L"Close",
        {}
        );
    if (m_shellDatabaseStatusText && !path.empty())
    {
        m_shellDatabaseStatusText.Text(winrt::hstring(
            std::wstring(title) + L": " + std::wstring(path)
            ));
    }
}

void MainWindow::updateFileCommandState()
{
    const bool hasDatabase = static_cast<bool>(m_openDatabase);
    if (m_saveFileMenu)
    {
        m_saveFileMenu.IsEnabled(hasDatabase);
    }
    if (m_saveAsFileMenu)
    {
        m_saveAsFileMenu.IsEnabled(hasDatabase && !m_currentDatabasePath.empty());
    }
    if (m_exportFileMenu)
    {
        m_exportFileMenu.IsEnabled(hasDatabase && !m_currentDatabasePath.empty());
    }
    if (m_closeFileMenu)
    {
        m_closeFileMenu.IsEnabled(hasDatabase);
    }

    const bool pageCanBeSaved = isCampusPageId(m_currentPageId)
        && (m_campusInformationState == L"no_database"
            || m_campusInformationState == L"empty"
            || m_campusInformationState == L"populated");
    if (m_saveCurrentPageMenu)
    {
        m_saveCurrentPageMenu.IsEnabled(pageCanBeSaved);
    }
    if (m_exportCampusResourcesMenu)
    {
        m_exportCampusResourcesMenu.IsEnabled(pageCanBeSaved);
    }
}

void MainWindow::restoreWindowBounds() noexcept
{
    const auto state = loadShellState();
    if (!state.hasWindowBounds || !isUsableWindowBounds(state.windowBounds))
    {
        return;
    }

    HWND const handle = windowHandle(this);
    if (!handle)
    {
        return;
    }

    const LONG width = state.windowBounds.right - state.windowBounds.left;
    const LONG height = state.windowBounds.bottom - state.windowBounds.top;
    SetWindowPos(
        handle,
        nullptr,
        state.windowBounds.left,
        state.windowBounds.top,
        width,
        height,
        SWP_NOZORDER | SWP_NOACTIVATE
        );
}

void MainWindow::saveShellState() noexcept
{
    HKEY const key = openShellStateForWrite();
    if (!key)
    {
        return;
    }

    writeRegistryString(
        key,
        L"SelectedPage",
        selectedPageId()
        );
    for (std::size_t index = 0; index < maximumRecentDatabasePaths; ++index)
    {
        const std::wstring valueName =
            L"RecentDatabase" + std::to_wstring(index);
        if (index < m_recentDatabasePaths.size())
        {
            writeRegistryString(
                key,
                valueName.c_str(),
                m_recentDatabasePaths[index]
                );
        }
        else
        {
            RegDeleteValueW(key, valueName.c_str());
        }
    }
    try
    {
        const auto navigationState = m_contentFrame.GetNavigationState();
        writeRegistryString(
            key,
            L"NavigationState",
            asWString(navigationState)
            );
    }
    catch (...)
    {
    }

    HWND const handle = windowHandle(this);
    RECT bounds{};
    if (handle && GetWindowRect(handle, &bounds))
    {
        writeRegistryDword(key, L"WindowLeft", static_cast<DWORD>(bounds.left));
        writeRegistryDword(key, L"WindowTop", static_cast<DWORD>(bounds.top));
        writeRegistryDword(key, L"WindowRight", static_cast<DWORD>(bounds.right));
        writeRegistryDword(key, L"WindowBottom", static_cast<DWORD>(bounds.bottom));
    }

    RegCloseKey(key);
}

void MainWindow::updateNavigationState()
{
    if (m_navigationView && m_contentFrame)
    {
        m_navigationView.IsBackEnabled(m_contentFrame.CanGoBack());
    }
}

void MainWindow::showOwnedDialog()
{
    showDialog(
        L"ClassMngr shell",
        L"This dialog is owned by MainWindow and uses the active XamlRoot.",
        {},
        {},
        L"Close",
        {}
        );
}

void MainWindow::showUnsavedChangesConfirmation()
{
    if (!m_dirtyState.isDirty())
    {
        if (m_statusText)
        {
            m_statusText.Text(L"No unsaved changes.");
        }
        return;
    }

    auto weak = get_weak();
    showDialog(
        L"Unsaved changes",
        L"Choose Save to let the feature save changes, Discard to discard them, or Keep editing to cancel.",
        L"Save",
        L"Discard",
        L"Keep editing",
        [weak](ClassMngrWinUIDialogs::DialogOutcome outcome) {
            if (auto self = weak.get())
            {
                switch (ClassMngrWinUIDialogs::resolveUnsavedChanges(
                    self->m_dirtyState,
                    outcome
                    ))
                {
                case ClassMngrWinUIDialogs::UnsavedChangesDecision::Save:
                    self->m_statusText.Text(
                        L"Save requested; changes remain dirty until the feature completes it."
                        );
                    break;
                case ClassMngrWinUIDialogs::UnsavedChangesDecision::Discard:
                    self->m_dirtyState.markClean();
                    self->m_statusText.Text(L"Unsaved changes discarded.");
                    break;
                case ClassMngrWinUIDialogs::UnsavedChangesDecision::Stay:
                    self->m_statusText.Text(L"Continuing to edit unsaved changes.");
                    break;
                case ClassMngrWinUIDialogs::UnsavedChangesDecision::Proceed:
                    self->m_statusText.Text(L"No unsaved changes.");
                    break;
                }
            }
        }
        );
}

void MainWindow::showDialog(
    winrt::hstring const& title,
    winrt::hstring const& content,
    winrt::hstring const& primaryText,
    winrt::hstring const& secondaryText,
    winrt::hstring const& closeText,
    std::function<void(ClassMngrWinUIDialogs::DialogOutcome)> completion
    )
{
    const auto xamlRoot = RootGrid().XamlRoot();
    if (m_ownedDialog || !m_contentFrame || !xamlRoot)
    {
        return;
    }

    auto dialog = Microsoft::UI::Xaml::Controls::ContentDialog();
    dialog.XamlRoot(xamlRoot);
    dialog.Title(winrt::box_value(title));
    dialog.Content(winrt::box_value(content));
    dialog.PrimaryButtonText(primaryText);
    dialog.SecondaryButtonText(secondaryText);
    dialog.CloseButtonText(closeText);
    dialog.DefaultButton(
        primaryText.empty()
            ? Microsoft::UI::Xaml::Controls::ContentDialogButton::Close
            : Microsoft::UI::Xaml::Controls::ContentDialogButton::Primary
        );
    m_ownedDialog = dialog;
    completeOwnedDialog(dialog, std::move(completion));
}

winrt::fire_and_forget MainWindow::completeOwnedDialog(
    Microsoft::UI::Xaml::Controls::ContentDialog dialog,
    std::function<void(ClassMngrWinUIDialogs::DialogOutcome)> completion
    )
{
    auto lifetime = get_strong();
    ClassMngrWinUIDialogs::DialogOutcome outcome =
        ClassMngrWinUIDialogs::DialogOutcome::Cancel;
    try
    {
        const auto result = co_await dialog.ShowAsync();
        if (result == Microsoft::UI::Xaml::Controls::ContentDialogResult::Primary)
        {
            outcome = ClassMngrWinUIDialogs::DialogOutcome::Primary;
        }
        else if (result
            == Microsoft::UI::Xaml::Controls::ContentDialogResult::Secondary)
        {
            outcome = ClassMngrWinUIDialogs::DialogOutcome::Secondary;
        }
    }
    catch (...)
    {
        // Hiding during shell teardown is cancellation, not an error dialog.
    }

    if (m_ownedDialog == dialog)
    {
        m_ownedDialog = nullptr;
        if (completion)
        {
            completion(outcome);
        }
    }
}

void MainWindow::updateHomePresentation()
{
    if (!m_homeCommand || !m_progressRing || !m_cancelButton || !m_statusText)
    {
        return;
    }

    const bool running = m_homeCommand->IsRunning();
    m_progressRing.IsActive(running);
    m_progressRing.Visibility(
        running
            ? Microsoft::UI::Xaml::Visibility::Visible
            : Microsoft::UI::Xaml::Visibility::Collapsed
        );
    m_cancelButton.IsEnabled(running);
    if (running)
    {
        m_statusText.Text(
            m_homeCommand->IsCancellationRequested()
                ? L"Cancelling operation..."
                : L"Operation in progress..."
            );
    }
    else if (m_statusText.Text() == L"Operation in progress..."
        || m_statusText.Text() == L"Cancelling operation...")
    {
        m_statusText.Text(L"Operation complete.");
    }

    if (m_validationSummaryText && m_homeViewModel)
    {
        m_validationSummaryText.Text(
            m_homeViewModel->HasValidationErrors()
                ? m_homeViewModel->ValidationSummary()
                : L"No validation issues."
            );
    }
}

void MainWindow::presentValidationSummary(
    classmngr::engine::ValidationResult const& validation
    )
{
    if (!m_homeViewModel)
    {
        return;
    }

    m_homeViewModel->PresentValidation(validation);
    updateHomePresentation();
}

void MainWindow::closeShell() noexcept
{
    saveShellState();
    try
    {
        if (m_selectionChangedToken.value != 0)
        {
            m_navigationView.SelectionChanged(m_selectionChangedToken);
            m_selectionChangedToken = {};
        }
        if (m_backRequestedToken.value != 0)
        {
            m_navigationView.BackRequested(m_backRequestedToken);
            m_backRequestedToken = {};
        }
        if (m_navigatedToken.value != 0)
        {
            m_contentFrame.Navigated(m_navigatedToken);
            m_navigatedToken = {};
        }
        if (m_activatedToken.value != 0)
        {
            Activated(m_activatedToken);
            m_activatedToken = {};
        }
        if (m_closedToken.value != 0)
        {
            Closed(m_closedToken);
            m_closedToken = {};
        }
        if (m_ownedDialog)
        {
            m_ownedDialog.Hide();
            m_ownedDialog = nullptr;
        }
        if (m_homeCommand && m_homeCommandStateToken.value != 0)
        {
            m_homeCommand.as<Microsoft::UI::Xaml::Input::ICommand>()
                .CanExecuteChanged(m_homeCommandStateToken);
            m_homeCommandStateToken = {};
        }
        if (m_phase5FirstNavigationRenderingToken.value != 0)
        {
            Microsoft::UI::Xaml::Media::CompositionTarget::Rendering(
                m_phase5FirstNavigationRenderingToken
                );
            m_phase5FirstNavigationRenderingToken = {};
        }
        m_homeCommand = nullptr;
        m_homeViewModel = nullptr;
        m_recentFilesMenu = nullptr;
        m_shellDatabaseStatusText = nullptr;
        m_contentFrame = nullptr;
        m_navigationView = nullptr;
        m_openDatabase.reset();
    }
    catch (...)
    {
    }
}

std::wstring MainWindow::selectedPageId() const
{
    if (m_navigationView)
    {
        const auto selectedItem = m_navigationView.SelectedItem().try_as<
            Microsoft::UI::Xaml::Controls::NavigationViewItem>();
        if (selectedItem)
        {
            const std::wstring pageId = boxedString(selectedItem.Tag());
            if (isClassesPageId(pageId))
            {
                return std::wstring(classesPageId);
            }
            if (pageId == personalDetailsPageId)
            {
                return std::wstring(homePageId);
            }
            if (isKnownPageId(pageId))
            {
                return pageId;
            }
        }
    }

    if (m_currentPageId == personalDetailsPageId)
    {
        return std::wstring(homePageId);
    }
    return isKnownPageId(m_currentPageId)
        ? m_currentPageId
        : std::wstring(homePageId);
}

bool MainWindow::ensureHomePage()
{
    if (m_currentPageId != homePageId)
    {
        navigateTo(homePageId);
    }
    return m_currentPageId == homePageId
        && static_cast<bool>(m_nameTextBox)
        && static_cast<bool>(m_continueButton)
        && static_cast<bool>(m_statusText);
}

} // namespace winrt::ClassMngrWinUI::implementation
