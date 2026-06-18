#include "version.h"

#include <QRegularExpression>

#include <tuple>

Version::Version(
    int majorVersion,
    int minorVersion,
    int patchVersion
    )
    : m_majorVersion(majorVersion)
    , m_minorVersion(minorVersion)
    , m_patchVersion(patchVersion)
{
}

Result<Version> Version::parse(
    const QString& text
    )
{
    static const QRegularExpression pattern(
        QStringLiteral(R"(^(\d+)\.(\d+)\.(\d+)$)")
        );

    const QRegularExpressionMatch match =
        pattern.match(
            text.trimmed()
            );

    if (!match.hasMatch())
    {
        return std::unexpected(
            QStringLiteral("Version must use x.x.x format.")
            );
    }

    bool ok = false;
    const int majorVersion =
        match.captured(1).toInt(&ok);
    if (!ok)
    {
        return std::unexpected(
            QStringLiteral("Invalid major version.")
            );
    }

    const int minorVersion =
        match.captured(2).toInt(&ok);
    if (!ok)
    {
        return std::unexpected(
            QStringLiteral("Invalid minor version.")
            );
    }

    const int patchVersion =
        match.captured(3).toInt(&ok);
    if (!ok)
    {
        return std::unexpected(
            QStringLiteral("Invalid patch version.")
            );
    }

    return Version(
        majorVersion,
        minorVersion,
        patchVersion
        );
}

QString Version::toString() const
{
    if (!isValid())
    {
        return QString();
    }

    return QStringLiteral("%1.%2.%3")
        .arg(m_majorVersion)
        .arg(m_minorVersion)
        .arg(m_patchVersion);
}

bool Version::isValid() const
{
    return m_majorVersion >= 0
        && m_minorVersion >= 0
        && m_patchVersion >= 0;
}

int Version::majorVersion() const
{
    return m_majorVersion;
}

int Version::minorVersion() const
{
    return m_minorVersion;
}

int Version::patchVersion() const
{
    return m_patchVersion;
}

bool operator<(
    const Version& lhs,
    const Version& rhs
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
