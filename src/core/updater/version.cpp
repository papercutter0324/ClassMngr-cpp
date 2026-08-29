#include "version.h"

Version::Version(
    int majorVersion,
    int minorVersion,
    int patchVersion
    )
    : m_value(
        majorVersion,
        minorVersion,
        patchVersion
        )
{
}

Result<Version> Version::parse(
    const QString& text
    )
{
    const auto parsed =
        classmngr::engine::SemanticVersion::parse(
            text.trimmed().toStdString()
            );

    if (!parsed)
    {
        return std::unexpected(
            QString::fromStdString(parsed.error())
            );
    }

    return Version(
        parsed->majorVersion(),
        parsed->minorVersion(),
        parsed->patchVersion()
        );
}

QString Version::toString() const
{
    if (!isValid())
    {
        return QString();
    }

    return QString::fromStdString(m_value.toString());
}

bool Version::isValid() const
{
    return m_value.isValid();
}

int Version::majorVersion() const
{
    return m_value.majorVersion();
}

int Version::minorVersion() const
{
    return m_value.minorVersion();
}

int Version::patchVersion() const
{
    return m_value.patchVersion();
}

bool operator<(
    const Version& lhs,
    const Version& rhs
    )
{
    return lhs.m_value < rhs.m_value;
}

bool operator!=(
    const Version& lhs,
    const Version& rhs
    )
{
    return !(lhs == rhs);
}

bool operator>(
    const Version& lhs,
    const Version& rhs
    )
{
    return rhs < lhs;
}

bool operator<=(
    const Version& lhs,
    const Version& rhs
    )
{
    return !(rhs < lhs);
}

bool operator>=(
    const Version& lhs,
    const Version& rhs
    )
{
    return !(lhs < rhs);
}
