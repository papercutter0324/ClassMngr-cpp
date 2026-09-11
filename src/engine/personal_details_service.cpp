#include "classmngr/engine/personal_details_service.h"

#include "classmngr/engine/application_settings_service.h"

#include <charconv>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>

namespace classmngr::engine
{
namespace
{
constexpr std::string_view NameKey = "myInfo/name";
constexpr std::string_view CampusKey = "myInfo/campus";
constexpr std::string_view ZoomLoginIdKey = "myInfo/zoomLoginId";
constexpr std::string_view ZoomPasswordKey = "myInfo/zoomPassword";
constexpr std::string_view ZoomNotAvailableKey = "myInfo/zoomNotAvailable";
constexpr std::string_view SignatureImageKey = "myInfo/signatureImage";
constexpr std::string_view SignatureModeKey = "myInfo/signatureMode";
constexpr std::string_view TypedSignatureTextKey =
    "myInfo/typedSignatureText";
constexpr std::string_view TypedSignatureFontKey =
    "myInfo/typedSignatureFont";

constexpr std::string_view LegacyZoomEmailKey =
    "subPrep/personalZoomEmail";
constexpr std::string_view LegacyZoomPasswordKey =
    "subPrep/personalZoomPassword";
constexpr std::string_view LegacyZoomNotAvailableKey =
    "subPrep/personalZoomNotAvailable";

Error error(
    ErrorCode code,
    std::string message
    )
{
    return {
        code,
        std::move(message),
        std::nullopt
    };
}

Error unexpectedSettingType(
    std::string_view key,
    std::string_view expected
    )
{
    return error(
        ErrorCode::Schema,
        "Application setting '"
            + std::string(key)
            + "' must contain a "
            + std::string(expected)
            + " value."
        );
}

Error invalidSettingInteger(
    std::string_view key
    )
{
    return error(
        ErrorCode::Schema,
        "Application setting '"
            + std::string(key)
            + "' must contain a valid integer value."
        );
}

Error settingIntegerOutOfRange(
    std::string_view key
    )
{
    return error(
        ErrorCode::Schema,
        "Application setting '"
            + std::string(key)
            + "' is outside the supported integer range."
        );
}

bool isMissing(
    const SettingValue& value
    ) noexcept
{
    return std::holds_alternative<std::monostate>(value);
}

Result<std::string> textValue(
    const SettingValue& value,
    std::string_view key
    )
{
    const auto* text = std::get_if<std::string>(&value);
    if (text == nullptr)
    {
        return std::unexpected(unexpectedSettingType(key, "text"));
    }

    return *text;
}

Result<std::int64_t> integerValue(
    const SettingValue& value,
    std::string_view key
    )
{
    if (const auto* integer = std::get_if<std::int64_t>(&value);
        integer != nullptr)
    {
        return *integer;
    }

    const auto* text = std::get_if<std::string>(&value);
    if (text == nullptr)
    {
        return std::unexpected(unexpectedSettingType(key, "integer"));
    }

    std::int64_t parsed = 0;
    const auto parsedResult = std::from_chars(
        text->data(),
        text->data() + text->size(),
        parsed
        );
    if (parsedResult.ec != std::errc{}
        || parsedResult.ptr != text->data() + text->size())
    {
        return std::unexpected(invalidSettingInteger(key));
    }

    return parsed;
}

Result<std::string> loadText(
    ApplicationSettingsService& settings,
    std::string_view key,
    std::string defaultValue
    )
{
    const Result<SettingValue> loaded = settings.load(key);
    if (!loaded)
    {
        return std::unexpected(loaded.error());
    }
    if (isMissing(*loaded))
    {
        return defaultValue;
    }

    return textValue(*loaded, key);
}

Result<std::int64_t> loadInteger(
    ApplicationSettingsService& settings,
    std::string_view key,
    std::int64_t defaultValue
    )
{
    const Result<SettingValue> loaded = settings.load(key);
    if (!loaded)
    {
        return std::unexpected(loaded.error());
    }
    if (isMissing(*loaded))
    {
        return defaultValue;
    }

    return integerValue(*loaded, key);
}

Result<std::string> loadTextWithLegacyFallback(
    ApplicationSettingsService& settings,
    std::string_view primaryKey,
    std::string_view legacyKey,
    std::string defaultValue
    )
{
    const Result<SettingValue> primary = settings.load(primaryKey);
    if (!primary)
    {
        return std::unexpected(primary.error());
    }
    if (!isMissing(*primary))
    {
        return textValue(*primary, primaryKey);
    }

    const Result<SettingValue> legacy = settings.load(legacyKey);
    if (!legacy)
    {
        return std::unexpected(legacy.error());
    }
    if (isMissing(*legacy))
    {
        return defaultValue;
    }

    const Result<std::string> value = textValue(*legacy, legacyKey);
    if (!value)
    {
        return std::unexpected(value.error());
    }

    const Status promoted = settings.save(
        primaryKey,
        SettingValue{*value}
        );
    if (!promoted)
    {
        return std::unexpected(promoted.error());
    }

    return *value;
}

Result<bool> loadBoolWithLegacyFallback(
    ApplicationSettingsService& settings,
    std::string_view primaryKey,
    std::string_view legacyKey,
    bool defaultValue
    )
{
    const Result<SettingValue> primary = settings.load(primaryKey);
    if (!primary)
    {
        return std::unexpected(primary.error());
    }
    if (!isMissing(*primary))
    {
        const Result<std::int64_t> value = integerValue(*primary, primaryKey);
        if (!value)
        {
            return std::unexpected(value.error());
        }
        return *value != 0;
    }

    const Result<SettingValue> legacy = settings.load(legacyKey);
    if (!legacy)
    {
        return std::unexpected(legacy.error());
    }
    if (isMissing(*legacy))
    {
        return defaultValue;
    }

    const Result<std::int64_t> value = integerValue(*legacy, legacyKey);
    if (!value)
    {
        return std::unexpected(value.error());
    }
    const bool result = *value != 0;

    const Status promoted = settings.save(
        primaryKey,
        SettingValue{std::int64_t{result ? 1 : 0}}
        );
    if (!promoted)
    {
        return std::unexpected(promoted.error());
    }

    return result;
}

Result<int> intValue(
    const SettingValue& value,
    std::string_view key
    )
{
    const Result<std::int64_t> integer = integerValue(value, key);
    if (!integer)
    {
        return std::unexpected(integer.error());
    }
    if (*integer < static_cast<std::int64_t>(
            std::numeric_limits<int>::min()
            )
        || *integer > static_cast<std::int64_t>(
            std::numeric_limits<int>::max()
            ))
    {
        return std::unexpected(settingIntegerOutOfRange(key));
    }

    return static_cast<int>(*integer);
}
} // namespace

PersonalDetailsService::PersonalDetailsService(
    ApplicationSettingsService& settings
    )
    : m_settings(settings)
{
}

Result<PersonalDetails> PersonalDetailsService::load()
{
    PersonalDetails details;

    const Result<std::string> name = loadText(
        m_settings,
        NameKey,
        {}
        );
    if (!name)
    {
        return std::unexpected(name.error());
    }
    details.name = *name;

    const Result<std::string> campus = loadText(
        m_settings,
        CampusKey,
        {}
        );
    if (!campus)
    {
        return std::unexpected(campus.error());
    }
    details.campus = *campus;

    const Result<std::string> zoomLoginId = loadTextWithLegacyFallback(
        m_settings,
        ZoomLoginIdKey,
        LegacyZoomEmailKey,
        "N/A"
        );
    if (!zoomLoginId)
    {
        return std::unexpected(zoomLoginId.error());
    }
    details.zoomLoginId = *zoomLoginId;

    const Result<std::string> zoomPassword = loadTextWithLegacyFallback(
        m_settings,
        ZoomPasswordKey,
        LegacyZoomPasswordKey,
        "N/A"
        );
    if (!zoomPassword)
    {
        return std::unexpected(zoomPassword.error());
    }
    details.zoomPassword = *zoomPassword;

    const Result<bool> zoomNotAvailable = loadBoolWithLegacyFallback(
        m_settings,
        ZoomNotAvailableKey,
        LegacyZoomNotAvailableKey,
        true
        );
    if (!zoomNotAvailable)
    {
        return std::unexpected(zoomNotAvailable.error());
    }
    details.zoomNotAvailable = *zoomNotAvailable;

    const Result<std::string> signatureImageBase64 = loadText(
        m_settings,
        SignatureImageKey,
        {}
        );
    if (!signatureImageBase64)
    {
        return std::unexpected(signatureImageBase64.error());
    }
    details.signatureImageBase64 = *signatureImageBase64;

    const Result<std::int64_t> signatureMode = loadInteger(
        m_settings,
        SignatureModeKey,
        static_cast<std::int64_t>(SignatureMode::Image)
        );
    if (!signatureMode)
    {
        return std::unexpected(signatureMode.error());
    }
    details.signatureMode = *signatureMode
            == static_cast<std::int64_t>(SignatureMode::Type)
        ? SignatureMode::Type
        : SignatureMode::Image;

    const Result<std::string> typedSignatureText = loadText(
        m_settings,
        TypedSignatureTextKey,
        {}
        );
    if (!typedSignatureText)
    {
        return std::unexpected(typedSignatureText.error());
    }
    details.typedSignatureText = *typedSignatureText;

    const Result<SettingValue> loadedTypedSignatureFont =
        m_settings.load(TypedSignatureFontKey);
    if (!loadedTypedSignatureFont)
    {
        return std::unexpected(loadedTypedSignatureFont.error());
    }
    if (!isMissing(*loadedTypedSignatureFont))
    {
        const Result<int> typedSignatureFont = intValue(
            *loadedTypedSignatureFont,
            TypedSignatureFontKey
            );
        if (!typedSignatureFont)
        {
            return std::unexpected(typedSignatureFont.error());
        }
        details.typedSignatureFont = *typedSignatureFont;
    }

    return details;
}

Status PersonalDetailsService::save(
    const PersonalDetails& details
    )
{
    return m_settings.saveBatch({
        {std::string(NameKey), SettingValue{details.name}},
        {std::string(CampusKey), SettingValue{details.campus}},
        {std::string(ZoomLoginIdKey), SettingValue{details.zoomLoginId}},
        {std::string(ZoomPasswordKey), SettingValue{details.zoomPassword}},
        {
            std::string(ZoomNotAvailableKey),
            SettingValue{
                std::int64_t{details.zoomNotAvailable ? 1 : 0}
            }
        },
        {
            std::string(SignatureImageKey),
            SettingValue{details.signatureImageBase64}
        },
        {
            std::string(SignatureModeKey),
            SettingValue{
                std::int64_t{static_cast<int>(details.signatureMode)}
            }
        },
        {
            std::string(TypedSignatureTextKey),
            SettingValue{details.typedSignatureText}
        },
        {
            std::string(TypedSignatureFontKey),
            SettingValue{std::int64_t{details.typedSignatureFont}}
        }
    });
}

Status PersonalDetailsService::saveCampus(
    std::string_view campus
    )
{
    return m_settings.save(
        CampusKey,
        SettingValue{std::string(campus)}
        );
}

} // namespace classmngr::engine
