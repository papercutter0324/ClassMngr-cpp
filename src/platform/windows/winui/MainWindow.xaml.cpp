#include "pch.h"

#include "MainWindow.xaml.h"
#include "classmngr/engine/campus_record_service.h"
#include "classmngr/engine/database_file_format.h"
#include "classmngr/engine/open_database.h"
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
        && !m_campusList
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
    if (!m_campusList || m_campusInformationState != L"populated"
        || m_campusList.Items().Size() != 1)
    {
        m_openDatabase.reset();
        refreshCampusInformationPage();
        return false;
    }

    m_campusList.SelectedIndex(0);
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
        && !m_campusList;
    m_phase5CampusScenario.clear();
    return noDatabaseReady && emptyReady && koreanTextReady
        && imageControlReady && localizationReady && resourceReady
        && resetReady;
}

void MainWindow::preparePhase5CampusScenario(std::wstring_view scenario)
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

    navigateTo(campusInformationPageId);
    refreshCampusInformationPage();
    updateFileCommandState();
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
    if (selectedItem == m_workspaceInformationNavigationItem
        || selectedItem == m_workspaceScheduleNavigationItem
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
    m_campusList = nullptr;
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
    root.Padding(Thickness{32.0, 32.0, 32.0, 32.0});
    root.Spacing(16.0);
    root.MaxWidth(1100.0);
    root.HorizontalAlignment(HorizontalAlignment::Center);
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
    auto layout = Grid();
    layout.ColumnDefinitions().Append(ColumnDefinition());
    layout.ColumnDefinitions().Append(ColumnDefinition());
    layout.ColumnDefinitions().GetAt(0).Width(
        GridLengthHelper::FromValueAndType(1.0, GridUnitType::Star)
        );
    layout.ColumnDefinitions().GetAt(1).Width(
        GridLengthHelper::FromValueAndType(2.0, GridUnitType::Star)
        );

    auto listCard = ClassMngrWinUISharedUX::buildCard({
        winrt::hstring(localize(L"Campuses")),
        winrt::hstring(localize(
            L"Select a campus to view its read-only information."
            )),
        L"Campus directory list"
        });
    m_campusList = ListView();
    m_campusList.SelectionMode(ListViewSelectionMode::Single);
    m_campusList.IsTabStop(true);
    m_campusList.TabIndex(0);
    m_campusList.Height(480.0);
    m_campusList.SelectionChanged({this, &MainWindow::CampusList_SelectionChanged});
    setAutomationName(m_campusList, L"Campus directory list");
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
        m_campusList.Items().Append(item);
    }
    listCard.content.Children().Append(m_campusList);
    Grid::SetColumn(listCard.root, 0);
    layout.Children().Append(listCard.root);

    auto detailsCard = ClassMngrWinUISharedUX::buildCard({
        winrt::hstring(localize(L"Campus details")),
        winrt::hstring(localize(
            L"Read-only information provided by the campus resource catalog."
            )),
        L"Selected campus details"
        });
    m_campusDetailsPanel = StackPanel();
    m_campusDetailsPanel.Spacing(8.0);
    setAutomationName(m_campusDetailsPanel, L"Selected campus details");
    detailsCard.content.Children().Append(m_campusDetailsPanel);
    Grid::SetColumn(detailsCard.root, 1);
    layout.Children().Append(detailsCard.root);

    root.Children().Append(layout);
    auto pageScroll = ScrollViewer();
    pageScroll.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
    pageScroll.HorizontalScrollBarVisibility(ScrollBarVisibility::Disabled);
    pageScroll.Content(root);
    page.Content(pageScroll);

    if (m_selectedCampusIndex < 0
        || static_cast<std::size_t>(m_selectedCampusIndex)
            >= m_campusResourceRecords.size())
    {
        m_selectedCampusIndex = 0;
    }
    m_campusList.SelectedIndex(m_selectedCampusIndex);
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

void MainWindow::CampusList_SelectionChanged(
    Windows::Foundation::IInspectable const& sender,
    Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const& arguments
    )
{
    static_cast<void>(sender);
    static_cast<void>(arguments);
    if (m_campusList)
    {
        m_selectedCampusIndex = m_campusList.SelectedIndex();
    }
    presentSelectedCampus(m_currentPageId);
}

void MainWindow::presentSelectedCampus(std::wstring_view pageId)
{
    if (!m_campusList || !m_campusDetailsPanel)
    {
        return;
    }

    const std::uint64_t requestId = ++m_campusImageRequest;
    m_campusImage = nullptr;
    m_campusImages.clear();
    m_campusDetailsPanel.Children().Clear();
    const int32_t selectedIndex = m_campusList.SelectedIndex();
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
        m_campusDetailsPanel.Children().Append(field);
    };
    const auto appendHeading = [&appendText](std::wstring_view text) {
        appendText(text, text, 18.0);
    };
    const auto appendField = [&localize, &appendText](
                                 std::wstring_view label,
                                 std::wstring_view value) {
        std::wstring text = localize(label);
        text += L": ";
        text += value;
        appendText(text, label);
    };
    const auto appendImage = [this, requestId](std::string const& path) {
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
        m_campusDetailsPanel.Children().Append(image);
        loadCampusImage(path, requestId, image);
    };
    const auto appendAddress = [
                                  &appendField,
                                  &appendHeading
                              ](
                                  std::wstring_view heading,
                                  CampusAddressView const& address
                              ) {
        appendHeading(heading);
        appendField(L"Building Name", address.buildingName);
        appendField(L"Province", address.province);
        appendField(L"City", address.city);
        appendField(L"City District", address.cityDistrict);
        appendField(L"District", address.district);
        appendField(L"Address Line 1", address.line1);
        appendField(L"Address Line 2", address.line2);
        appendField(L"Postal Code", address.postalCode);
        appendField(L"Address System", address.addressSystem);
    };

    appendHeading(campus.campusName);
    if (!campus.campusCode.empty())
    {
        appendField(L"Campus Code", campus.campusCode);
    }

    if (pageId == campusInformationPageId)
    {
        if (!campus.mapImagePaths.empty())
        {
            appendImage(campus.mapImagePaths.front());
        }
        appendField(L"Campus ID", campus.id);
        appendField(L"Name", campus.campusName);
        appendField(L"Building", campus.buildingName);
        appendField(L"Address", campus.address);
        appendField(L"Phone", campus.phoneNumber);
        appendField(L"Office", campus.officeNumber);
        appendField(L"Office Wi-Fi", campus.officeWifi);
        appendField(L"Office Wi-Fi password", campus.officeWifiPassword);
        appendField(L"Printer", campus.printerName);
        appendField(L"Printer steps", campus.printerSteps);
        appendField(
            L"Printer driver URL",
            campus.printerDriverUrlUnavailable
                ? L"N/A"
                : campus.printerDriverUrl
            );
        appendField(L"Photocopier code", campus.photocopierCode);
        return;
    }

    if (pageId == campusDirectionsPageId)
    {
        appendField(L"Building", campus.buildingName);
        appendField(L"Phone", campus.phoneNumber);
        appendField(
            L"Transit Steps",
            [&campus]() {
                std::wstring result;
                for (std::size_t index = 0; index < campus.transitSteps.size(); ++index)
                {
                    if (index != 0)
                    {
                        result += L"\n";
                    }
                    result += campus.transitSteps[index];
                }
                return result;
            }()
            );
        appendField(L"Upon Arriving", campus.arrivalInfo);
        appendField(L"Note", campus.directionsNote);
        return;
    }

    if (pageId == campusAddressPageId)
    {
        appendField(L"Campus", campus.campusName);
        appendField(L"Phone", campus.phoneNumber);
        appendField(L"Complete Address", campus.address);
        appendAddress(L"English", campus.englishAddress);
        appendAddress(L"Korean", campus.koreanAddress);
        return;
    }

    if (pageId == campusHousingPageId)
    {
        if (campus.housingLocations.empty())
        {
            appendText(
                localize(L"No housing information available"),
                L"Campus housing empty state"
                );
            return;
        }

        for (std::size_t index = 0;
             index < campus.housingLocations.size();
             ++index)
        {
            const CampusHousingView& housing = campus.housingLocations[index];
            appendHeading(
                L"Housing " + std::to_wstring(index + 1)
                + (housing.name.empty() ? L"" : L": " + housing.name)
                );
            appendAddress(L"English", housing.englishAddress);
            appendAddress(L"Korean", housing.koreanAddress);
            appendField(L"Note", housing.addressNote);
            for (const std::string& imagePath : housing.imagePaths)
            {
                appendImage(imagePath);
            }
        }
        return;
    }

    if (pageId == campusMapPageId)
    {
        if (campus.mapImagePaths.empty())
        {
            appendText(
                localize(L"No map images available"),
                L"Campus maps empty state"
                );
        }
        else
        {
            for (const std::string& imagePath : campus.mapImagePaths)
            {
                appendImage(imagePath);
                appendText(
                    asWString(winrt::to_hstring(imagePath)),
                    L"Campus map resource path"
                    );
            }
        }
        appendField(L"Naver Maps", campus.naverMapUrl);
        appendField(L"Kakao Maps", campus.kakaoMapUrl);
    }
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
