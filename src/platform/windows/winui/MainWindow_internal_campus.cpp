#include "pch.h"
#include "MainWindow_internal.h"

namespace winrt::ClassMngrWinUI::implementation::MainWindowDetail
{

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

} // namespace winrt::ClassMngrWinUI::implementation::MainWindowDetail
