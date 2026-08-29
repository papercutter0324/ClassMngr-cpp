#include "classmngr/engine/semantic_version.h"

#include <charconv>
#include <cctype>
#include <tuple>

namespace classmngr::engine
{
namespace
{
std::string_view trimAsciiWhitespace(std::string_view text)
{
    while (!text.empty()
           && std::isspace(static_cast<unsigned char>(text.front())))
    {
        text.remove_prefix(1);
    }

    while (!text.empty()
           && std::isspace(static_cast<unsigned char>(text.back())))
    {
        text.remove_suffix(1);
    }

    return text;
}

bool isDigits(std::string_view text)
{
    if (text.empty())
    {
        return false;
    }

    for (const char character : text)
    {
        if (character < '0' || character > '9')
        {
            return false;
        }
    }

    return true;
}

std::expected<int, std::string> parseComponent(
    std::string_view text,
    std::string_view componentName
    )
{
    int value = 0;
    const auto result = std::from_chars(
        text.data(),
        text.data() + text.size(),
        value,
        10
        );

    if (result.ec != std::errc{}
        || result.ptr != text.data() + text.size()
        || value < 0)
    {
        return std::unexpected(
            "Invalid " + std::string(componentName) + " version."
            );
    }

    return value;
}
}

SemanticVersion::SemanticVersion(
    int majorVersion,
    int minorVersion,
    int patchVersion
    )
    : m_majorVersion(majorVersion)
    , m_minorVersion(minorVersion)
    , m_patchVersion(patchVersion)
{
}

std::expected<SemanticVersion, std::string> SemanticVersion::parse(
    std::string_view text
    )
{
    const std::string_view normalized = trimAsciiWhitespace(text);
    const std::size_t firstSeparator = normalized.find('.');
    const std::size_t secondSeparator = normalized.find(
        '.',
        firstSeparator == std::string_view::npos
            ? 0
            : firstSeparator + 1
        );

    if (firstSeparator == std::string_view::npos
        || secondSeparator == std::string_view::npos
        || normalized.find('.', secondSeparator + 1)
            != std::string_view::npos)
    {
        return std::unexpected("Version must use x.x.x format.");
    }

    const std::string_view major = normalized.substr(0, firstSeparator);
    const std::string_view minor = normalized.substr(
        firstSeparator + 1,
        secondSeparator - firstSeparator - 1
        );
    const std::string_view patch = normalized.substr(secondSeparator + 1);

    if (!isDigits(major) || !isDigits(minor) || !isDigits(patch))
    {
        return std::unexpected("Version must use x.x.x format.");
    }

    const auto parsedMajor = parseComponent(major, "major");
    if (!parsedMajor)
    {
        return std::unexpected(parsedMajor.error());
    }

    const auto parsedMinor = parseComponent(minor, "minor");
    if (!parsedMinor)
    {
        return std::unexpected(parsedMinor.error());
    }

    const auto parsedPatch = parseComponent(patch, "patch");
    if (!parsedPatch)
    {
        return std::unexpected(parsedPatch.error());
    }

    return SemanticVersion(
        *parsedMajor,
        *parsedMinor,
        *parsedPatch
        );
}

std::string SemanticVersion::toString() const
{
    if (!isValid())
    {
        return {};
    }

    return std::to_string(m_majorVersion)
        + '.'
        + std::to_string(m_minorVersion)
        + '.'
        + std::to_string(m_patchVersion);
}

bool SemanticVersion::isValid() const
{
    return m_majorVersion >= 0
        && m_minorVersion >= 0
        && m_patchVersion >= 0;
}

int SemanticVersion::majorVersion() const
{
    return m_majorVersion;
}

int SemanticVersion::minorVersion() const
{
    return m_minorVersion;
}

int SemanticVersion::patchVersion() const
{
    return m_patchVersion;
}

bool operator<(
    const SemanticVersion& lhs,
    const SemanticVersion& rhs
    )
{
    return std::tie(
        lhs.m_majorVersion,
        lhs.m_minorVersion,
        lhs.m_patchVersion
        )
        < std::tie(
            rhs.m_majorVersion,
            rhs.m_minorVersion,
            rhs.m_patchVersion
            );
}

bool operator!=(
    const SemanticVersion& lhs,
    const SemanticVersion& rhs
    )
{
    return !(lhs == rhs);
}

bool operator>(
    const SemanticVersion& lhs,
    const SemanticVersion& rhs
    )
{
    return rhs < lhs;
}

bool operator<=(
    const SemanticVersion& lhs,
    const SemanticVersion& rhs
    )
{
    return !(rhs < lhs);
}

bool operator>=(
    const SemanticVersion& lhs,
    const SemanticVersion& rhs
    )
{
    return !(lhs < rhs);
}

} // namespace classmngr::engine
