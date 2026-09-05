#include "pch.h"

#include "winui_localization.h"

#include <bcrypt.h>

#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

namespace
{

struct BcryptHandles final
{
    BCRYPT_ALG_HANDLE algorithm{};
    BCRYPT_HASH_HANDLE hash{};

    ~BcryptHandles() noexcept
    {
        if (hash)
        {
            BCryptDestroyHash(hash);
        }
        if (algorithm)
        {
            BCryptCloseAlgorithmProvider(algorithm, 0);
        }
    }
};

std::string toUtf8(std::wstring_view value)
{
    if (value.empty())
    {
        return {};
    }

    const int inputLength = static_cast<int>(value.size());
    const int outputLength = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        value.data(),
        inputLength,
        nullptr,
        0,
        nullptr,
        nullptr
        );
    if (outputLength <= 0)
    {
        return {};
    }

    std::string result(static_cast<std::size_t>(outputLength), '\0');
    if (WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            value.data(),
            inputLength,
            result.data(),
            outputLength,
            nullptr,
            nullptr
            ) != outputLength)
    {
        return {};
    }
    return result;
}

std::wstring sha256Hex(std::string_view value)
{
    if (value.size() > static_cast<std::size_t>(std::numeric_limits<ULONG>::max()))
    {
        return {};
    }

    BcryptHandles handles;
    NTSTATUS status = BCryptOpenAlgorithmProvider(
        &handles.algorithm,
        BCRYPT_SHA256_ALGORITHM,
        nullptr,
        0
        );
    if (!BCRYPT_SUCCESS(status))
    {
        return {};
    }

    status = BCryptCreateHash(
        handles.algorithm,
        &handles.hash,
        nullptr,
        0,
        nullptr,
        0,
        0
        );
    if (!BCRYPT_SUCCESS(status))
    {
        return {};
    }

    status = BCryptHashData(
        handles.hash,
        reinterpret_cast<PUCHAR>(const_cast<char*>(value.data())),
        static_cast<ULONG>(value.size()),
        0
        );
    if (!BCRYPT_SUCCESS(status))
    {
        return {};
    }

    std::array<std::uint8_t, 32> digest{};
    status = BCryptFinishHash(
        handles.hash,
        digest.data(),
        static_cast<ULONG>(digest.size()),
        0
        );
    if (!BCRYPT_SUCCESS(status))
    {
        return {};
    }

    constexpr wchar_t hex[] = L"0123456789abcdef";
    std::wstring result;
    result.reserve(digest.size() * 2);
    for (const auto byte : digest)
    {
        result.push_back(hex[(byte >> 4) & 0x0f]);
        result.push_back(hex[byte & 0x0f]);
    }
    return result;
}

std::wstring sanitizeContext(std::wstring_view context)
{
    std::wstring result;
    result.reserve(context.size());
    for (const wchar_t character : context)
    {
        const bool asciiLetter =
            (character >= L'A' && character <= L'Z')
            || (character >= L'a' && character <= L'z');
        const bool asciiDigit = character >= L'0' && character <= L'9';
        const wchar_t replacement =
            asciiLetter || asciiDigit || character == L'_' ? character : L'_';
        result.push_back(replacement);
    }
    if (result.empty())
    {
        result = L"Context";
    }
    if (result.front() >= L'0' && result.front() <= L'9')
    {
        result.insert(result.begin(), L'_');
    }
    return result;
}

std::wstring normalizeLanguageTag(std::wstring_view languageTag)
{
    std::wstring normalized(languageTag);
    for (auto& character : normalized)
    {
        if (character == L'_')
        {
            character = L'-';
        }
    }
    return normalized;
}

std::wstring applicationPriPath()
{
    std::array<wchar_t, MAX_PATH> modulePath{};
    const DWORD moduleLength = GetModuleFileNameW(
        nullptr,
        modulePath.data(),
        static_cast<DWORD>(modulePath.size())
        );
    if (moduleLength == 0 || moduleLength >= modulePath.size())
    {
        return {};
    }

    std::wstring resourcePath(modulePath.data(), moduleLength);
    const auto separator = resourcePath.find_last_of(L"\\/");
    if (separator == std::wstring::npos)
    {
        return {};
    }
    resourcePath.resize(separator + 1);
    resourcePath += L"ClassMngrWinUI.pri";
    if (GetFileAttributesW(resourcePath.c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        return {};
    }
    return resourcePath;
}

std::wstring resourceMapPath(std::wstring_view resourceId)
{
    std::wstring result(resourceId);
    for (auto& character : result)
    {
        if (character == L'.')
        {
            character = L'/';
        }
    }
    return result;
}

} // namespace

namespace winrt::ClassMngrWinUI
{

WinUILocalizer::WinUILocalizer()
    : WinUILocalizer(std::wstring_view{})
{
}

WinUILocalizer::WinUILocalizer(std::wstring_view languageTag)
    : m_languageTag(normalizeLanguageTag(languageTag))
{
    try
    {
        const std::wstring priPath = applicationPriPath();
        if (!priPath.empty())
        {
            m_resourceManager =
                Microsoft::Windows::ApplicationModel::Resources::ResourceManager{
                    winrt::hstring{priPath}
                };
        }
        else
        {
            m_resourceManager =
                Microsoft::Windows::ApplicationModel::Resources::ResourceManager{};
        }
        m_resourceContext = m_resourceManager.CreateResourceContext();
        if (!m_languageTag.empty())
        {
            m_resourceContext.QualifierValues().Insert(
                Microsoft::Windows::ApplicationModel::Resources::
                    KnownResourceQualifierName::Language(),
                winrt::hstring{m_languageTag}
                );
        }
        m_stringMap = m_resourceManager.MainResourceMap().TryGetSubtree(
            L"Resources"
            );
    }
    catch (...)
    {
        m_resourceManager = nullptr;
        m_resourceContext = nullptr;
        m_stringMap = nullptr;
    }
}

std::wstring WinUILocalizer::makeResourceId(
    std::wstring_view context,
    std::wstring_view source
    )
{
    const std::string contextUtf8 = toUtf8(context);
    const std::string sourceUtf8 = toUtf8(source);
    if ((!context.empty() && contextUtf8.empty())
        || (!source.empty() && sourceUtf8.empty()))
    {
        return {};
    }

    std::string hashInput;
    hashInput.reserve(contextUtf8.size() + sourceUtf8.size() + 1);
    hashInput.append(contextUtf8);
    hashInput.push_back('\n');
    hashInput.append(sourceUtf8);
    const std::wstring hash = sha256Hex(hashInput);
    if (hash.empty())
    {
        return {};
    }

    return L"ClassMngr.Strings."
        + sanitizeContext(context)
        + L"."
        + hash;
}

std::optional<std::wstring> WinUILocalizer::tryGetString(
    std::wstring_view context,
    std::wstring_view source
    ) const
{
    if (!m_resourceManager || !m_resourceContext)
    {
        return std::nullopt;
    }

    const std::wstring resourceId = makeResourceId(context, source);
    if (resourceId.empty())
    {
        return std::nullopt;
    }

    try
    {
        const std::wstring mapId = resourceMapPath(resourceId);
        auto map = m_stringMap;
        if (!map)
        {
            map = m_resourceManager.MainResourceMap();
        }
        auto candidate = map.TryGetValue(resourceId, m_resourceContext);
        if (!candidate)
        {
            candidate = map.TryGetValue(mapId, m_resourceContext);
        }
        auto mainMap = m_resourceManager.MainResourceMap();
        if (!candidate && m_stringMap)
        {
            std::wstring qualifiedId = L"Resources/";
            qualifiedId += resourceId;
            candidate = mainMap.TryGetValue(
                qualifiedId,
                m_resourceContext
                );
            if (!candidate)
            {
                qualifiedId = L"Resources/";
                qualifiedId += mapId;
                candidate = mainMap.TryGetValue(
                    qualifiedId,
                    m_resourceContext
                    );
            }
        }
        if (!candidate && !m_stringMap)
        {
            candidate = mainMap.TryGetValue(mapId, m_resourceContext);
        }
        if (candidate
            && candidate.Kind()
                == Microsoft::Windows::ApplicationModel::Resources::
                    ResourceCandidateKind::String)
        {
            return std::wstring(candidate.ValueAsString());
        }
    }
    catch (...)
    {
        // Presentation localization must fall back to its source text when
        // the resource package is unavailable or a key is missing.
    }
    return std::nullopt;
}

std::wstring WinUILocalizer::getString(
    std::wstring_view context,
    std::wstring_view source
    ) const
{
    const auto localized = tryGetString(context, source);
    if (localized && !localized->empty())
    {
        return *localized;
    }
    return std::wstring(source);
}

bool WinUILocalizer::hasString(
    std::wstring_view context,
    std::wstring_view source
    ) const
{
    return tryGetString(context, source).has_value();
}

} // namespace winrt::ClassMngrWinUI
