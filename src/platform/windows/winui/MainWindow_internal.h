#pragma once

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
#include "classmngr/engine/class_naming.h"
#include "classmngr/engine/class_repository.h"
#include "classmngr/engine/class_schedule_service.h"
#include "classmngr/engine/database_file_format.h"
#include "classmngr/engine/gs_team_service.h"
#include "classmngr/engine/intensive_slot_state_service.h"
#include "classmngr/engine/native_english_teacher_service.h"
#include "classmngr/engine/open_database.h"
#include "classmngr/engine/personal_details_service.h"
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

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <coroutine>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace winrt::ClassMngrWinUI::implementation::MainWindowDetail
{

inline constexpr std::wstring_view homePageId = L"home";
inline constexpr std::wstring_view personalDetailsPageId = L"personal_details";
inline constexpr std::wstring_view koreanTeachersPageId = L"teachers_all_korean";
inline constexpr std::wstring_view nativeEnglishTeachersPageId =
    L"native_english_teachers";
inline constexpr std::wstring_view gsTeamPageId = L"gs_team";
inline constexpr std::wstring_view subPrepPageId = L"sub_prep";
inline constexpr std::wstring_view classesPageId = L"classes";
inline constexpr std::wstring_view classDetailsPageId = L"classes_details";
inline constexpr std::wstring_view classRosterPageId = L"classes_roster";
inline constexpr std::wstring_view classSpeakingEvaluationsPageId =
    L"classes_speaking_evaluations";
inline constexpr std::wstring_view classAnalyticsPageId =
    L"classes_analytics";
inline constexpr std::wstring_view classNotesPageId = L"classes_notes";
inline constexpr std::string_view classNavigationLocationKey =
    "classes/navigationLocation";
inline constexpr std::array<std::wstring_view, 6> classSectionTitles{
    L"Class Details",
    L"Class Roster",
    L"Class Analytics",
    L"Speaking Evaluations",
    L"Co-Teacher",
    L"Class Notes"
};
inline constexpr std::wstring_view aboutPageId = L"about";
inline constexpr std::wstring_view campusInformationPageId =
    L"campus_information";
inline constexpr std::wstring_view campusDirectionsPageId = L"campus_directions";
inline constexpr std::wstring_view campusAddressPageId = L"campus_address";
inline constexpr std::wstring_view campusHousingPageId = L"campus_housing";
inline constexpr std::wstring_view campusMapPageId = L"campus_map";
inline constexpr int32_t minimumShellWidth = 800;
inline constexpr int32_t minimumShellHeight = 600;
inline constexpr int32_t defaultShellWidth = 1270;
inline constexpr int32_t defaultShellHeight = 1040;
inline constexpr std::size_t maximumRecentDatabasePaths = 10;

inline constexpr std::wstring_view calendarShowAllCampusesKey =
    L"calendar/showEventsAtAllCampuses";
inline constexpr std::wstring_view calendarFirstDayOfWeekKey =
    L"calendar/firstDayOfWeek";
inline constexpr std::wstring_view calendarHideStartOfTermKey =
    L"calendar/hideStartOfTermEvents";
inline constexpr int calendarFirstTermYear =
    classmngr::engine::AcademicCalendarSchedule::FirstTermYear;

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

struct SpeakingAiPrivateNotes
{
    std::string didWell;
    std::string needsImprovement;
};

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

struct ScheduleSelection
{
    int classId = -1;
    classmngr::engine::ScheduleType type =
        classmngr::engine::ScheduleType::Regular;
    std::wstring day;
    std::wstring startTime;
    std::wstring endTime;
};

void appendJsonEscaped(std::string& output, std::string_view value);
std::string campusResourceFileName(std::string_view reference);
std::string uniqueCampusResourceFileName(
    std::string name,
    std::set<std::string>& usedNames
    );
[[nodiscard]] bool isKnownPageId(std::wstring_view pageId) noexcept;
[[nodiscard]] bool isClassesPageId(std::wstring_view pageId) noexcept;
[[nodiscard]] bool isCampusPageId(std::wstring_view pageId) noexcept;
std::string asUtf8(std::wstring_view value);
[[nodiscard]] bool isSupportedDatabasePath(std::wstring_view path) noexcept;
[[nodiscard]] bool pathExists(std::wstring_view path) noexcept;
std::wstring absolutePath(std::wstring_view path);
[[nodiscard]] bool samePath(
    std::wstring_view lhs,
    std::wstring_view rhs
    ) noexcept;
std::vector<std::wstring> pruneRecentDatabasePaths(
    std::vector<std::wstring> const& paths
    );
[[nodiscard]] bool rosterRowHasData(
    std::vector<std::string> const& row
    );
[[nodiscard]] bool rosterColumnEquals(
    std::string_view left,
    std::string_view right
    );
[[nodiscard]] bool rosterRequiredColumn(std::string_view column);
void padRosterRows(classmngr::engine::Roster& roster);
[[nodiscard]] bool rosterHasAvailableRow(
    classmngr::engine::Roster const& roster
    );
std::string rosterStudentNamePairKey(
    classmngr::engine::Roster const& roster,
    std::vector<std::string> const& row
    );
std::wstring asWString(winrt::hstring const& value);
std::wstring asWide(std::string_view value);
std::string normalizeSpeakingAiLineEndings(std::string value);
SpeakingAiPrivateNotes splitSpeakingAiPrivateNotes(std::string notes);
std::string joinSpeakingAiPrivateNotes(
    std::string didWell,
    std::string needsImprovement
    );
std::string bulletizeSpeakingAiNotes(std::string notes);
void replaceSpeakingAiPlaceholder(
    std::wstring& value,
    std::wstring_view replacement
    );
std::string speakingAiStudentId(std::size_t row);

[[nodiscard]] bool calendarDateLess(
    EngineCalendarDate const& left,
    EngineCalendarDate const& right
    ) noexcept;
[[nodiscard]] bool calendarDateEqual(
    EngineCalendarDate const& left,
    EngineCalendarDate const& right
    ) noexcept;
EngineCalendarDate calendarToday();
int calendarDaysInMonth(EngineCalendarDate const& date) noexcept;
EngineCalendarDate calendarMonthStart(EngineCalendarDate const& date);
EngineCalendarDate calendarAddDays(
    EngineCalendarDate const& date,
    int count
    );
EngineCalendarDate calendarAddMonths(
    EngineCalendarDate const& date,
    int count
    );
std::wstring calendarDateText(EngineCalendarDate const& date);
[[nodiscard]] bool calendarDateFromText(
    std::wstring_view value,
    EngineCalendarDate& result
    ) noexcept;
std::wstring calendarMonthTitle(EngineCalendarDate const& date);
std::optional<std::chrono::minutes> calendarTimeFromText(
    std::wstring_view value
    ) noexcept;
std::wstring calendarTimeText(
    std::optional<std::chrono::minutes> value
    );
[[nodiscard]] bool settingBool(
    classmngr::engine::SettingValue const& value,
    bool fallback
    ) noexcept;
std::int64_t settingInteger(
    classmngr::engine::SettingValue const& value,
    std::int64_t fallback
    ) noexcept;
std::optional<ClassNavigationLocation> classNavigationLocationFromSetting(
    classmngr::engine::SettingValue const& value
    ) noexcept;
classmngr::engine::Result<std::string> subPrepTextSetting(
    classmngr::engine::ApplicationSettingsService& settings,
    std::string_view key,
    std::string defaultValue
    );
std::string calendarScheduleKey(
    int termYear,
    int school,
    int term,
    bool winterStart
    );

winrt::hstring jsonString(
    JsonObject const& object,
    std::wstring_view key
    );
JsonObject jsonObject(
    JsonObject const& object,
    std::wstring_view key
    );
JsonArray jsonArray(
    JsonObject const& object,
    std::wstring_view key
    );
std::wstring jsonWideString(
    JsonObject const& object,
    std::wstring_view key
    );
std::vector<std::wstring> jsonWideStringArray(JsonArray const& values);
std::vector<std::string> jsonResourceStringArray(JsonArray const& values);
CampusAddressView parseCampusAddress(JsonObject const& object);
CampusHousingView parseCampusHousing(JsonObject const& object);
CampusResourceView parseCampusResource(JsonObject const& object);
[[nodiscard]] bool equalsIgnoreCase(
    std::wstring_view lhs,
    std::wstring_view rhs
    ) noexcept;
classmngr::engine::Result<std::vector<CampusResourceView>>
loadPackagedCampusResources();
CampusResourceView campusResourceFromEngine(
    classmngr::engine::CampusRecord const& source
    );
classmngr::engine::CampusRecord campusRecordFromResource(
    CampusResourceView const& source
    );

std::wstring boxedString(
    winrt::Windows::Foundation::IInspectable const& value
    );
std::wstring selectedComboValue(
    winrt::Microsoft::UI::Xaml::Controls::ComboBox const& combo
    );
int boxedInt(
    winrt::Windows::Foundation::IInspectable const& value
    ) noexcept;
std::vector<std::wstring> splitScheduleKey(std::wstring_view value);
std::optional<ScheduleSelection> scheduleSelectionFromKey(
    std::wstring_view value
    );
std::wstring scheduleSelectionKey(
    int classId,
    classmngr::engine::ScheduleType type,
    std::wstring_view day,
    std::wstring_view startTime,
    std::wstring_view endTime
    );
std::wstring scheduleTypeText(classmngr::engine::ScheduleType type);
std::vector<std::wstring> scheduleImportDays(std::wstring_view value);
classmngr::engine::Roster defaultRoster();
[[nodiscard]] bool validMonthDay(std::wstring_view value) noexcept;
winrt::Windows::Foundation::IAsyncAction phase3PresentationWork(
    classmngr::engine::CancellationToken const& cancellation
    );

bool readRegistryString(
    HKEY key,
    wchar_t const* valueName,
    std::wstring& value
    ) noexcept;
bool readRegistryDword(
    HKEY key,
    wchar_t const* valueName,
    DWORD& value
    ) noexcept;
void writeRegistryString(
    HKEY key,
    wchar_t const* valueName,
    std::wstring const& value
    ) noexcept;
void writeRegistryDword(
    HKEY key,
    wchar_t const* valueName,
    DWORD value
    ) noexcept;
PersistedShellState loadShellState() noexcept;
HKEY openShellStateForWrite() noexcept;
HWND windowHandle(
    winrt::ClassMngrWinUI::implementation::MainWindow* window
    ) noexcept;
[[nodiscard]] bool isUsableWindowBounds(RECT const& bounds) noexcept;
bool moveWindowBoundsIntoWorkArea(RECT* bounds) noexcept;

void setAutomationName(
    winrt::Microsoft::UI::Xaml::DependencyObject const& element,
    std::wstring_view name
    );
void applyResourceStyle(
    winrt::Microsoft::UI::Xaml::FrameworkElement const& element,
    std::wstring_view key
    );
winrt::Windows::UI::Color uiColorFromHex(std::string_view value) noexcept;
std::string uiHexFromColor(winrt::Windows::UI::Color color);
std::vector<std::wstring> splitPastedRangeRow(std::wstring_view row);
std::vector<std::vector<std::wstring>> parsePastedRange(
    std::wstring_view text
    );

} // namespace winrt::ClassMngrWinUI::implementation::MainWindowDetail
