#include "github_release.h"

#include "core/network/http_request_policy.h"

#include <QJsonArray>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSysInfo>

#include <optional>
#include <utility>

namespace
{
QString architectureKey()
{
    const QString architecture =
        QSysInfo::currentCpuArchitecture().toLower();

    if (
        architecture == QStringLiteral("x86_64")
        || architecture == QStringLiteral("amd64")
        )
    {
        return QStringLiteral("x64");
    }

    if (
        architecture == QStringLiteral("arm64")
        || architecture == QStringLiteral("aarch64")
        )
    {
        return QStringLiteral("arm64");
    }

    return architecture;
}

QStringList assetNamesFor(
    const QString& platformKey,
    const Version& version
    )
{
    const QString versionText =
        version.toString();

    if (platformKey == QStringLiteral("windows-x64"))
    {
        return {
            QStringLiteral("ClassMngr-%1-win-x64.exe")
                .arg(versionText)
        };
    }

    if (platformKey == QStringLiteral("windows-arm64"))
    {
        return {
            QStringLiteral("ClassMngr-%1-win-arm64.exe")
                .arg(versionText)
        };
    }

    if (platformKey == QStringLiteral("macos-universal"))
    {
        return {
            QStringLiteral("ClassMngr-%1-macos-universal.dmg")
                .arg(versionText)
        };
    }

    if (
        platformKey == QStringLiteral("linux-x86_64")
        || platformKey == QStringLiteral("linux-x64")
        )
    {
        return {
            QStringLiteral("ClassMngr-%1-linux-x86_64.tar.gz")
                .arg(versionText),
            QStringLiteral("ClassMngr-linux-x86_64.tar.gz")
        };
    }

    return {};
}

Result<UpdateArtifact> parseArtifact(
    const QJsonArray& assets,
    const Version& version,
    const QStringList& platformKeys
    )
{
    static const QRegularExpression digestPattern(
        QStringLiteral(R"(^sha256:([0-9a-fA-F]{64})$)")
        );

    for (const QString& platformKey : platformKeys)
    {
        const QStringList acceptedNames =
            assetNamesFor(
                platformKey,
                version
                );

        for (const QString& acceptedName : acceptedNames)
        {
            for (const QJsonValue& assetValue : assets)
            {
                if (!assetValue.isObject())
                {
                    continue;
                }

                const QJsonObject asset =
                    assetValue.toObject();

                if (
                    asset.value(QStringLiteral("name")).toString()
                        != acceptedName
                    )
                {
                    continue;
                }

                const QString state =
                    asset.value(QStringLiteral("state"))
                        .toString(QStringLiteral("uploaded"));

                if (state != QStringLiteral("uploaded"))
                {
                    continue;
                }

                const QUrl url(
                    asset.value(
                        QStringLiteral("browser_download_url")
                        ).toString()
                    );

                if (!GitHubRelease::isAllowedDownloadUrl(url))
                {
                    continue;
                }

                const qint64 sizeBytes =
                    static_cast<qint64>(
                        asset.value(QStringLiteral("size")).toDouble(-1)
                        );

                if (sizeBytes <= 0)
                {
                    continue;
                }

                const QRegularExpressionMatch digestMatch =
                    digestPattern.match(
                        asset.value(QStringLiteral("digest"))
                            .toString()
                            .trimmed()
                        );

                if (!digestMatch.hasMatch())
                {
                    continue;
                }

                UpdateArtifact artifact;
                artifact.platformKey =
                    platformKey;
                artifact.url =
                    url;
                artifact.fileName =
                    acceptedName;
                artifact.sha256 =
                    digestMatch.captured(1).toLower();
                artifact.sizeBytes =
                    sizeBytes;

                return artifact;
            }
        }
    }

    return std::unexpected(
        QStringLiteral(
            "GitHub release does not contain a complete update asset for this platform."
            )
        );
}

Result<GitHubRelease> parseRelease(
    const QJsonObject& object,
    const QStringList& platformKeys
    )
{
    if (
        object.value(QStringLiteral("draft")).toBool()
        || object.value(QStringLiteral("prerelease")).toBool()
        )
    {
        return std::unexpected(
            QStringLiteral("Release is not a published stable release.")
            );
    }

    const QString tag =
        object.value(QStringLiteral("tag_name")).toString().trimmed();

    if (!tag.startsWith(QLatin1Char('v')))
    {
        return std::unexpected(
            QStringLiteral("GitHub release tag must use vX.Y.Z format.")
            );
    }

    const auto version =
        Version::parse(
            tag.sliced(1)
            );

    if (!version)
    {
        return std::unexpected(
            QStringLiteral("GitHub release tag is invalid: %1")
                .arg(version.error())
            );
    }

    const QUrl releaseUrl(
        object.value(QStringLiteral("html_url")).toString()
        );

    if (!GitHubRelease::isAllowedDownloadUrl(releaseUrl))
    {
        return std::unexpected(
            QStringLiteral("GitHub release URL must use HTTPS.")
            );
    }

    const QDateTime publishedAt =
        QDateTime::fromString(
            object.value(QStringLiteral("published_at")).toString(),
            Qt::ISODate
            );

    if (!publishedAt.isValid())
    {
        return std::unexpected(
            QStringLiteral("GitHub release published_at value is invalid.")
            );
    }

    const QJsonValue assetsValue =
        object.value(QStringLiteral("assets"));

    if (!assetsValue.isArray())
    {
        return std::unexpected(
            QStringLiteral("GitHub release assets must be an array.")
            );
    }

    const auto artifact =
        parseArtifact(
            assetsValue.toArray(),
            *version,
            platformKeys
            );

    if (!artifact)
    {
        return std::unexpected(
            artifact.error()
            );
    }

    return GitHubRelease(
        *version,
        *artifact,
        publishedAt.date(),
        releaseUrl
        );
}
}

GitHubRelease::GitHubRelease(
    Version version,
    UpdateArtifact artifact,
    QDate releaseDate,
    QUrl releaseUrl
    )
    : m_version(std::move(version))
    , m_artifact(std::move(artifact))
    , m_releaseDate(std::move(releaseDate))
    , m_releaseUrl(std::move(releaseUrl))
{
}

Result<GitHubRelease> GitHubRelease::latestCompatibleFromJson(
    const QByteArray& data,
    const QStringList& platformKeys
    )
{
    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(
            data,
            &parseError
            );

    if (parseError.error != QJsonParseError::NoError)
    {
        return std::unexpected(
            QStringLiteral("GitHub release JSON is invalid: %1")
                .arg(parseError.errorString())
            );
    }

    QJsonArray releases;

    if (document.isArray())
    {
        releases =
            document.array();
    }
    else if (document.isObject())
    {
        releases.append(
            document.object()
            );
    }
    else
    {
        return std::unexpected(
            QStringLiteral("GitHub release response must be an array.")
            );
    }

    std::optional<GitHubRelease> latest;

    for (const QJsonValue& value : releases)
    {
        if (!value.isObject())
        {
            continue;
        }

        const auto release =
            parseRelease(
                value.toObject(),
                platformKeys
                );

        if (
            release
            && (
                !latest.has_value()
                || release->version() > latest->version()
                )
            )
        {
            latest =
                *release;
        }
    }

    if (!latest.has_value())
    {
        return std::unexpected(
            QStringLiteral(
                "No published stable GitHub release contains a valid update asset for this platform."
                )
            );
    }

    return *latest;
}

QStringList GitHubRelease::currentPlatformKeys()
{
    const QString architecture =
        architectureKey();

#if defined(Q_OS_WIN)
    QStringList keys = {
        QStringLiteral("windows-%1").arg(architecture)
    };

    if (architecture == QStringLiteral("arm64"))
    {
        keys.append(
            QStringLiteral("windows-x64")
            );
    }

    return keys;
#elif defined(Q_OS_MACOS)
    return {
        QStringLiteral("macos-universal")
    };
#else
    return {
        QStringLiteral("linux-%1").arg(architecture),
        QStringLiteral("linux-x86_64"),
        QStringLiteral("linux-x64")
    };
#endif
}

bool GitHubRelease::isAllowedDownloadUrl(
    const QUrl& url
    )
{
    return HttpRequestPolicy::isAllowedSecureUrl(url);
}

Version GitHubRelease::version() const
{
    return m_version;
}

UpdateArtifact GitHubRelease::artifact() const
{
    return m_artifact;
}

QDate GitHubRelease::releaseDate() const
{
    return m_releaseDate;
}

QUrl GitHubRelease::releaseUrl() const
{
    return m_releaseUrl;
}
