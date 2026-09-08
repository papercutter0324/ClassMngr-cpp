#include "pch.h"

#include "MainWindow.xaml.h"
#include "classmngr/engine/application_settings_service.h"
#include "classmngr/engine/campus_record_service.h"
#include "classmngr/engine/database_file_format.h"
#include "classmngr/engine/gs_team_service.h"
#include "classmngr/engine/native_english_teacher_service.h"
#include "classmngr/engine/open_database.h"
#include "classmngr/engine/personal_details_service.h"
#include "classmngr/engine/teacher_service.h"
#include "winui_build_info.h"
#include "winui_identity.h"
#include "winui_platform_services.h"
#include "winui_shared_ux.h"

#include <microsoft.ui.xaml.window.h>
#include <shobjidl_core.h>

#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Storage.Pickers.h>
#include <winrt/Windows.Storage.Streams.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cwctype>
#include <coroutine>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <string_view>
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
constexpr std::wstring_view classesPageId = L"classes";
constexpr std::wstring_view classDetailsPageId = L"classes_details";
constexpr std::wstring_view classRosterPageId = L"classes_roster";
constexpr std::wstring_view classSpeakingEvaluationsPageId = L"classes_speaking_evaluations";
constexpr std::wstring_view classAnalyticsPageId = L"classes_analytics";
constexpr std::wstring_view classNotesPageId = L"classes_notes";
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

using JsonArray = winrt::Windows::Data::Json::JsonArray;
using JsonObject = winrt::Windows::Data::Json::JsonObject;
using JsonValueType = winrt::Windows::Data::Json::JsonValueType;
using CampusAddressView =
    winrt::ClassMngrWinUI::implementation::CampusAddressView;
using CampusHousingView =
    winrt::ClassMngrWinUI::implementation::CampusHousingView;
using CampusResourceView =
    winrt::ClassMngrWinUI::implementation::CampusResourceView;

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
    m_workspaceInformationNavigationItem = RootGrid().FindName(
        L"WorkspaceInformationNavigationItem"
        ).as<Microsoft::UI::Xaml::Controls::NavigationViewItem>();
    m_workspaceScheduleNavigationItem = RootGrid().FindName(
        L"WorkspaceScheduleNavigationItem"
        ).as<Microsoft::UI::Xaml::Controls::NavigationViewItem>();
    m_workspaceCalendarNavigationItem = RootGrid().FindName(
        L"WorkspaceCalendarNavigationItem"
        ).as<Microsoft::UI::Xaml::Controls::NavigationViewItem>();
    m_classesNavigationItem = RootGrid().FindName(L"ClassesNavigationItem").as<
        Microsoft::UI::Xaml::Controls::NavigationViewItem>();
    m_classDetailsNavigationItem = RootGrid().FindName(
        L"ClassDetailsNavigationItem"
        ).as<Microsoft::UI::Xaml::Controls::NavigationViewItem>();
    m_classRosterNavigationItem = RootGrid().FindName(
        L"ClassRosterNavigationItem"
        ).as<Microsoft::UI::Xaml::Controls::NavigationViewItem>();
    m_classSpeakingEvaluationsNavigationItem = RootGrid().FindName(
        L"ClassSpeakingEvaluationsNavigationItem"
        ).as<Microsoft::UI::Xaml::Controls::NavigationViewItem>();
    m_classAnalyticsNavigationItem = RootGrid().FindName(
        L"ClassAnalyticsNavigationItem"
        ).as<Microsoft::UI::Xaml::Controls::NavigationViewItem>();
    m_classNotesNavigationItem = RootGrid().FindName(
        L"ClassNotesNavigationItem"
        ).as<Microsoft::UI::Xaml::Controls::NavigationViewItem>();
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
        }
        if (aboutPageReady && ensureHomePage() && m_nameTextBox.XamlRoot())
        {
            const bool focusRequested = m_nameTextBox.Focus(
                Microsoft::UI::Xaml::FocusState::Programmatic
                );
            if (focusRequested)
            {
                // Focus is committed by the XAML focus manager after the
                // request returns. Observe the manager on a later UI turn.
                co_await ResumeOnDispatcherQueue{
                    DispatcherQueue(),
                    Microsoft::UI::Dispatching::DispatcherQueuePriority::Low
                    };
                const auto focusedElement =
                    Microsoft::UI::Xaml::Input::FocusManager::GetFocusedElement(
                        m_nameTextBox.XamlRoot()
                        );
                focusReady = focusedElement == m_nameTextBox;
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
    const bool noDatabaseReady =
        m_currentPageId == personalDetailsPageId
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
    m_personalCampusTextBox.Text(L"서울 캠퍼스");
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
        && asWString(m_personalCampusTextBox.Text()) == L"서울 캠퍼스"
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
    if (selectedItem == m_workspaceInformationNavigationItem)
    {
        navigateTo(personalDetailsPageId);
        return;
    }
    if (selectedItem == m_workspaceScheduleNavigationItem
        || selectedItem == m_workspaceCalendarNavigationItem)
    {
        navigateTo(homePageId);
        const auto homePage = m_contentFrame.Content().try_as<
            Microsoft::UI::Xaml::Controls::Page>();
        const auto homeTabs = homePage
            ? homePage.Content().try_as<Microsoft::UI::Xaml::Controls::Pivot>()
            : nullptr;
        if (homeTabs)
        {
            homeTabs.SelectedIndex(
                selectedItem == m_workspaceScheduleNavigationItem
                    ? 1
                    : selectedItem == m_workspaceCalendarNavigationItem
                        ? 2
                        : 0
                );
        }
        return;
    }
    if (isClassesPageId(pageId))
    {
        navigateTo(classesPageId);
        const auto classesPage = m_contentFrame.Content().try_as<
            Microsoft::UI::Xaml::Controls::Page>();
        const auto classesTabs = classesPage
            ? classesPage.Content().try_as<Microsoft::UI::Xaml::Controls::Pivot>()
            : nullptr;
        if (classesTabs)
        {
            classesTabs.SelectedIndex(
                selectedItem == m_classRosterNavigationItem
                    ? 1
                    : selectedItem == m_classSpeakingEvaluationsNavigationItem
                        ? 2
                        : selectedItem == m_classAnalyticsNavigationItem
                            ? 3
                            : selectedItem == m_classNotesNavigationItem
                                ? 4
                                : 0
                );
        }
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
            : pageId == personalDetailsPageId
                ? m_workspaceInformationNavigationItem
            : pageId == koreanTeachersPageId
                ? m_koreanTeachersNavigationItem
            : pageId == nativeEnglishTeachersPageId
                ? m_nativeEnglishTeachersNavigationItem
            : pageId == gsTeamPageId
                ? m_gsTeamNavigationItem
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
        else if (pageId == homePageId && !m_engineVersionText)
        {
            populateHomePage(page);
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
    scheduleRoot.Padding(Thickness{32.0, 16.0, 32.0, 32.0});
    scheduleRoot.Spacing(16.0);
    scheduleRoot.MaxWidth(900.0);
    scheduleRoot.HorizontalAlignment(HorizontalAlignment::Center);
    scheduleRoot.Children().Append(scheduleCard.root);

    auto calendarRoot = StackPanel();
    calendarRoot.Padding(Thickness{32.0, 16.0, 32.0, 32.0});
    calendarRoot.Spacing(16.0);
    calendarRoot.MaxWidth(900.0);
    calendarRoot.HorizontalAlignment(HorizontalAlignment::Center);
    auto calendarTitle = TextBlock();
    calendarTitle.Text(L"Calendar");
    calendarTitle.FontSize(24.0);
    auto calendarDescription = TextBlock();
    calendarDescription.Text(
        L"Calendar will load when the calendar feature slice is migrated."
        );
    calendarDescription.TextWrapping(TextWrapping::Wrap);
    setAutomationName(calendarTitle, L"Calendar");
    setAutomationName(calendarDescription, L"Calendar migration status");
    calendarRoot.Children().Append(calendarTitle);
    calendarRoot.Children().Append(calendarDescription);

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

    auto tabs = Pivot();
    tabs.IsTabStop(true);
    tabs.TabIndex(0);
    setAutomationName(tabs, L"My Workspace tabs");
    tabs.Items().Append(makePivotItem(
        L"My Information",
        root,
        L"My Information workspace tab"
        ));
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
    page.Content(tabs);
}

void MainWindow::populatePersonalDetailsPage(
    Microsoft::UI::Xaml::Controls::Page const& page,
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
        root.Spacing(16.0);
        root.MaxWidth(780.0);
        root.HorizontalAlignment(HorizontalAlignment::Center);

        auto title = TextBlock();
        title.Text(L"My Details");
        title.FontSize(24.0);
        setAutomationName(title, L"My Details");
        root.Children().Append(title);

        auto description = TextBlock();
        description.Text(
            L"Manage your personal information, Zoom details, and signature."
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
            box.MinWidth(320.0);
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
        m_personalCampusTextBox = makeTextBox(
            L"My Campus",
            L"Personal campus",
            L"Enter your campus"
            );
        m_personalCampusTextBox.TabIndex(1);
        detailsCard.content.Children().Append(m_personalNameTextBox);
        detailsCard.content.Children().Append(m_personalCampusTextBox);
        root.Children().Append(detailsCard.root);

        auto zoomCard = ClassMngrWinUISharedUX::buildCard({
            L"Zoom",
            L"Keep your Zoom sign-in details available to the desktop features.",
            L"Personal Zoom details"
            });
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
        m_personalZoomPasswordBox.MinWidth(320.0);
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
            box_value(hstring(L"Zoom is not available (N/A)"))
            );
        m_personalZoomNotAvailableCheck.IsTabStop(true);
        m_personalZoomNotAvailableCheck.TabIndex(4);
        m_personalZoomNotAvailableCheck.Checked(
            {this, &MainWindow::PersonalDetailsZoomAvailability_Changed}
            );
        m_personalZoomNotAvailableCheck.Unchecked(
            {this, &MainWindow::PersonalDetailsZoomAvailability_Changed}
            );
        setAutomationName(
            m_personalZoomNotAvailableCheck,
            L"Zoom not available"
            );
        zoomCard.content.Children().Append(m_personalZoomLoginIdTextBox);
        zoomCard.content.Children().Append(m_personalZoomPasswordBox);
        zoomCard.content.Children().Append(m_personalZoomNotAvailableCheck);
        root.Children().Append(zoomCard.root);

        auto signatureCard = ClassMngrWinUISharedUX::buildCard({
            L"Signature",
            L"Choose the stored signature mode. Typed signatures are editable in this slice.",
            L"Personal signature form"
            });
        m_personalSignatureModeCombo = ComboBox();
        m_personalSignatureModeCombo.Header(
            box_value(hstring(L"Signature mode"))
            );
        m_personalSignatureModeCombo.MinWidth(320.0);
        m_personalSignatureModeCombo.IsTabStop(true);
        m_personalSignatureModeCombo.TabIndex(5);
        auto imageMode = ComboBoxItem();
        imageMode.Content(box_value(hstring(L"Image (existing image retained)")));
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

        m_personalTypedSignatureTextBox = makeTextBox(
            L"Type your signature",
            L"Typed signature text",
            L"Type your name"
            );
        m_personalTypedSignatureTextBox.TabIndex(6);
        m_personalSignatureFontCombo = ComboBox();
        m_personalSignatureFontCombo.Header(
            box_value(hstring(L"Signature style"))
            );
        m_personalSignatureFontCombo.MinWidth(320.0);
        m_personalSignatureFontCombo.IsTabStop(true);
        m_personalSignatureFontCombo.TabIndex(7);
        for (auto const& font : {
                 std::pair{0, L"Just Another Hand"},
                 std::pair{1, L"Caveat"},
                 std::pair{2, L"Dancing Script"},
                 std::pair{3, L"Pacifico"}
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

        m_personalImageStatusText = TextBlock();
        m_personalImageStatusText.TextWrapping(TextWrapping::Wrap);
        setAutomationName(
            m_personalImageStatusText,
            L"Signature image status"
            );
        signatureCard.content.Children().Append(m_personalSignatureModeCombo);
        signatureCard.content.Children().Append(m_personalTypedSignatureTextBox);
        signatureCard.content.Children().Append(m_personalSignatureFontCombo);
        signatureCard.content.Children().Append(m_personalImageStatusText);
        root.Children().Append(signatureCard.root);

        auto actions = StackPanel();
        actions.Orientation(Orientation::Horizontal);
        actions.Spacing(8.0);
        m_personalSaveButton = Button();
        m_personalSaveButton.Content(box_value(hstring(L"Save Changes")));
        m_personalSaveButton.IsTabStop(true);
        m_personalSaveButton.TabIndex(8);
        m_personalSaveButton.Click(
            {this, &MainWindow::PersonalDetailsSaveButton_Click}
            );
        setAutomationName(m_personalSaveButton, L"Save personal details");
        m_personalDiscardButton = Button();
        m_personalDiscardButton.Content(box_value(hstring(L"Discard Changes")));
        m_personalDiscardButton.IsTabStop(true);
        m_personalDiscardButton.TabIndex(9);
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
        page.Content(scroll);
    }

    const auto setEditable = [this](bool enabled) {
        const auto checkedValue = m_personalZoomNotAvailableCheck.IsChecked();
        const bool zoomNotAvailable = checkedValue && checkedValue.Value();
        if (m_personalNameTextBox)
        {
            m_personalNameTextBox.IsEnabled(enabled);
        }
        if (m_personalCampusTextBox)
        {
            m_personalCampusTextBox.IsEnabled(enabled);
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
        if (m_personalSaveButton)
        {
            m_personalSaveButton.IsEnabled(enabled && m_personalDetailsDirty);
        }
        if (m_personalDiscardButton)
        {
            m_personalDiscardButton.IsEnabled(enabled && m_personalDetailsDirty);
        }
    };

    if (!m_openDatabase)
    {
        m_personalDetailsLoading = true;
        m_personalDetailsLoaded = false;
        m_personalDetailsDirty = false;
        m_personalNameTextBox.Text({});
        m_personalCampusTextBox.Text({});
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
    m_personalCampusTextBox.Text(asWide(m_personalDetails.campus));
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
}

void MainWindow::refreshPersonalDetailsPage()
{
    if (!m_contentFrame || m_currentPageId != personalDetailsPageId)
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
        content.Padding(Thickness{32.0, 16.0, 32.0, 32.0});
        content.Spacing(16.0);
        content.MaxWidth(900.0);
        content.HorizontalAlignment(HorizontalAlignment::Center);
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

    auto detailsRoot = makeTextSection(
        L"Details",
        L"Class details will load when the class feature slice is migrated.",
        L"Class details prototype"
        );

    auto rosterRoot = makeRoot(StackPanel());
    auto rosterCard = ClassMngrWinUISharedUX::buildCard({
        L"Roster selection, transfer, and keyboard editing prototype",
        L"Select a student, edit the name with the keyboard, and simulate a transfer between lists.",
        L"Roster editor"
        });
    if (m_rosterSourceList && m_rosterTransferredList && m_rosterStatusText)
    {
        auto rosterLists = StackPanel();
        rosterLists.Orientation(Orientation::Horizontal);
        rosterLists.Spacing(8.0);
        rosterLists.Children().Append(m_rosterSourceList);
        rosterLists.Children().Append(m_rosterTransferredList);
        auto rosterTransfer = Button();
        rosterTransfer.Content(winrt::box_value(winrt::hstring(L"Transfer selected")));
        rosterTransfer.IsTabStop(true);
        rosterTransfer.HorizontalAlignment(HorizontalAlignment::Left);
        rosterTransfer.Click({this, &MainWindow::RosterTransferButton_Click});
        setAutomationName(rosterTransfer, L"Roster transfer selected student");
        rosterCard.content.Children().Append(rosterLists);
        rosterCard.content.Children().Append(rosterTransfer);
        rosterCard.content.Children().Append(m_rosterStatusText);
    }
    else
    {
        auto placeholder = TextBlock();
        placeholder.Text(L"Roster prototype controls are not initialized yet.");
        placeholder.TextWrapping(TextWrapping::Wrap);
        rosterCard.content.Children().Append(placeholder);
    }
    rosterRoot.Children().Append(rosterCard.root);

    auto speakingRoot = makeRoot(StackPanel());
    auto speakingCard = ClassMngrWinUISharedUX::buildCard({
        L"Speaking-evaluation scores and analytics prototype",
        L"Edit score cells, apply a tab/newline range, and request analytics navigation using standard controls.",
        L"Speaking evaluation editor"
        });
    if (m_speakingPasteTextBox && m_speakingStatusText
        && m_speakingScoreCells.size() == 9)
    {
        auto scoreGrid = Grid();
        for (size_t column = 0; column < 4; ++column)
        {
            scoreGrid.ColumnDefinitions().Append(ColumnDefinition());
        }
        for (size_t row = 0; row < 4; ++row)
        {
            scoreGrid.RowDefinitions().Append(RowDefinition());
        }
        const auto addScoreHeader = [&scoreGrid](wchar_t const* text,
                                                  uint32_t row,
                                                  uint32_t column) {
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
        for (uint32_t row = 0; row < 3; ++row)
        {
            auto student = TextBlock();
            student.Text(row == 0 ? L"Student 1" : row == 1 ? L"Student 2" : L"Student 3");
            Grid::SetRow(student, row + 1);
            Grid::SetColumn(student, 0);
            scoreGrid.Children().Append(student);
            for (uint32_t column = 0; column < 3; ++column)
            {
                auto score = m_speakingScoreCells[row * 3 + column];
                Grid::SetRow(score, row + 1);
                Grid::SetColumn(score, column + 1);
                scoreGrid.Children().Append(score);
            }
        }
        speakingCard.content.Children().Append(scoreGrid);
        speakingCard.content.Children().Append(m_speakingPasteTextBox);
        auto speakingPaste = Button();
        speakingPaste.Content(winrt::box_value(winrt::hstring(L"Apply pasted range")));
        speakingPaste.Click({this, &MainWindow::SpeakingPasteButton_Click});
        setAutomationName(speakingPaste, L"Apply speaking pasted score range");
        speakingCard.content.Children().Append(speakingPaste);
        auto speakingAnalytics = Button();
        speakingAnalytics.Content(winrt::box_value(winrt::hstring(L"Open analytics")));
        speakingAnalytics.Click({this, &MainWindow::SpeakingAnalyticsButton_Click});
        setAutomationName(speakingAnalytics, L"Open speaking analytics");
        speakingCard.content.Children().Append(speakingAnalytics);
        speakingCard.content.Children().Append(m_speakingStatusText);
    }
    else
    {
        auto placeholder = TextBlock();
        placeholder.Text(L"Speaking evaluation prototype controls are not initialized yet.");
        placeholder.TextWrapping(TextWrapping::Wrap);
        speakingCard.content.Children().Append(placeholder);
    }
    speakingRoot.Children().Append(speakingCard.root);

    auto analyticsRoot = makeTextSection(
        L"Analytics",
        L"Class analytics will load when the analytics feature slice is migrated.",
        L"Class analytics prototype"
        );
    auto notesRoot = makeTextSection(
        L"Notes",
        L"Class notes will load when the notes feature slice is migrated.",
        L"Class notes prototype"
        );

    const auto scrollTab = [](StackPanel const& content) {
        auto scroll = ScrollViewer();
        scroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
        scroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
        scroll.Content(content);
        return scroll;
    };
    const auto makePivotItem = [&scrollTab](wchar_t const* header,
                                            StackPanel const& content,
                                            wchar_t const* automationName) {
        auto item = PivotItem();
        item.Header(winrt::box_value(winrt::hstring(header)));
        item.Content(scrollTab(content));
        setAutomationName(item, automationName);
        return item;
    };

    auto tabs = Pivot();
    tabs.IsTabStop(true);
    setAutomationName(tabs, L"Classes tabs");
    tabs.Items().Append(makePivotItem(L"Details", detailsRoot, L"Class Details tab"));
    tabs.Items().Append(makePivotItem(L"Roster", rosterRoot, L"Class Roster tab"));
    tabs.Items().Append(makePivotItem(
        L"Speaking Evaluations",
        speakingRoot,
        L"Class Speaking Evaluations tab"
        ));
    tabs.Items().Append(makePivotItem(L"Analytics", analyticsRoot, L"Class Analytics tab"));
    tabs.Items().Append(makePivotItem(L"Notes", notesRoot, L"Class Notes tab"));
    page.Content(tabs);
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

void MainWindow::PersonalDetailsSignatureMode_SelectionChanged(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& arguments
    )
{
    static_cast<void>(arguments);
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
                : L"Existing image data is retained; image selection is not available in this slice."
            );
    }

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
    draft.campus = asUtf8(asWString(m_personalCampusTextBox.Text()));
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
            if (isKnownPageId(pageId))
            {
                return pageId;
            }
        }
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
