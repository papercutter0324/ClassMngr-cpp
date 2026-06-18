#include "update_manifest.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSysInfo>

namespace
{
constexpr int SupportedSchemaVersion = 1;

bool isLocalHttpUrl(
    const QUrl& url
    )
{
    if (url.scheme() != QStringLiteral("http"))
    {
        return false;
    }

    const QString host =
        url.host().toLower();

    return host == QStringLiteral("localhost")
        || host == QStringLiteral("127.0.0.1")
        || host == QStringLiteral("::1");
}

bool isValidSha256(
    const QString& value
    )
{
    static const QRegularExpression pattern(
        QStringLiteral(R"(^[0-9a-fA-F]{64}$)")
        );

    return pattern.match(value.trimmed()).hasMatch();
}

QString architectureKey()
{
    const QString arch =
        QSysInfo::currentCpuArchitecture().toLower();

    if (
        arch == QStringLiteral("x86_64")
        || arch == QStringLiteral("amd64")
        )
    {
        return QStringLiteral("x64");
    }

    if (
        arch == QStringLiteral("arm64")
        || arch == QStringLiteral("aarch64")
        )
    {
        return QStringLiteral("arm64");
    }

    return arch;
}

Result<QDate> parseReleaseDate(
    const QJsonValue& value
    )
{
    if (value.isUndefined() || value.isNull())
    {
        return QDate();
    }

    if (!value.isString())
    {
        return std::unexpected(
            QStringLiteral("releaseDate must be a string.")
            );
    }

    const QDate date =
        QDate::fromString(
            value.toString(),
            Qt::ISODate
            );

    if (!date.isValid())
    {
        return std::unexpected(
            QStringLiteral("releaseDate must use yyyy-MM-dd format.")
            );
    }

    return date;
}

Result<QUrl> parseOptionalHttpsUrl(
    const QJsonValue& value,
    const QString& fieldName
    )
{
    if (value.isUndefined() || value.isNull())
    {
        return QUrl();
    }

    if (!value.isString())
    {
        return std::unexpected(
            QStringLiteral("%1 must be a string.").arg(fieldName)
            );
    }

    const QUrl url(
        value.toString()
        );

    if (
        !url.isValid()
        || url.isRelative()
        || (url.scheme() != QStringLiteral("https") && !isLocalHttpUrl(url))
        )
    {
        return std::unexpected(
            QStringLiteral("%1 must be an HTTPS URL.").arg(fieldName)
            );
    }

    return url;
}
}

Result<UpdateManifest> UpdateManifest::fromJson(
    const QByteArray& data,
    const QStringList& requiredPlatformKeys
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
            QStringLiteral("Update manifest JSON is invalid: %1")
                .arg(parseError.errorString())
            );
    }

    if (!document.isObject())
    {
        return std::unexpected(
            QStringLiteral("Update manifest root must be an object.")
            );
    }

    const QJsonObject root =
        document.object();

    const int schemaVersion =
        root.value(QStringLiteral("schemaVersion")).toInt(-1);

    if (schemaVersion != SupportedSchemaVersion)
    {
        return std::unexpected(
            QStringLiteral("Unsupported update manifest schema version.")
            );
    }

    const QString channel =
        root.value(QStringLiteral("channel")).toString().trimmed();

    if (channel.isEmpty())
    {
        return std::unexpected(
            QStringLiteral("Update manifest channel is required.")
            );
    }

    const auto latestVersion =
        Version::parse(
            root.value(QStringLiteral("latestVersion")).toString()
            );

    if (!latestVersion)
    {
        return std::unexpected(
            QStringLiteral("latestVersion is invalid: %1")
                .arg(latestVersion.error())
            );
    }

    Version minimumSupportedVersion;
    const QJsonValue minimumValue =
        root.value(QStringLiteral("minimumSupportedVersion"));

    if (!minimumValue.isUndefined() && !minimumValue.isNull())
    {
        const auto parsedMinimumVersion =
            Version::parse(
                minimumValue.toString()
                );

        if (!parsedMinimumVersion)
        {
            return std::unexpected(
                QStringLiteral("minimumSupportedVersion is invalid: %1")
                    .arg(parsedMinimumVersion.error())
                );
        }

        minimumSupportedVersion =
            *parsedMinimumVersion;
    }

    const auto releaseDate =
        parseReleaseDate(
            root.value(QStringLiteral("releaseDate"))
            );

    if (!releaseDate)
    {
        return std::unexpected(
            releaseDate.error()
            );
    }

    const auto notesUrl =
        parseOptionalHttpsUrl(
            root.value(QStringLiteral("notesUrl")),
            QStringLiteral("notesUrl")
            );

    if (!notesUrl)
    {
        return std::unexpected(
            notesUrl.error()
            );
    }

    const QJsonValue platformsValue =
        root.value(QStringLiteral("platforms"));

    if (!platformsValue.isObject())
    {
        return std::unexpected(
            QStringLiteral("Update manifest platforms object is required.")
            );
    }

    UpdateManifest manifest;
    manifest.m_schemaVersion =
        schemaVersion;
    manifest.m_channel =
        channel;
    manifest.m_latestVersion =
        *latestVersion;
    manifest.m_minimumSupportedVersion =
        minimumSupportedVersion;
    manifest.m_releaseDate =
        *releaseDate;
    manifest.m_notesUrl =
        *notesUrl;

    const QJsonObject platforms =
        platformsValue.toObject();

    for (auto it = platforms.constBegin(); it != platforms.constEnd(); ++it)
    {
        if (!it.value().isObject())
        {
            return std::unexpected(
                QStringLiteral("Platform '%1' must be an object.")
                    .arg(it.key())
                );
        }

        const auto artifact =
            parseArtifact(
                it.key(),
                it.value().toObject()
                );

        if (!artifact)
        {
            return std::unexpected(
                artifact.error()
                );
        }

        manifest.m_artifacts.insert(
            it.key(),
            *artifact
            );
    }

    if (manifest.m_artifacts.isEmpty())
    {
        return std::unexpected(
            QStringLiteral("Update manifest must include at least one platform.")
            );
    }

    if (!requiredPlatformKeys.isEmpty())
    {
        const auto artifact =
            manifest.artifactForAny(
                requiredPlatformKeys
                );

        if (!artifact)
        {
            return std::unexpected(
                artifact.error()
                );
        }
    }

    return manifest;
}

QStringList UpdateManifest::currentPlatformKeys()
{
    const QString arch =
        architectureKey();

#if defined(Q_OS_WIN)
    return {
        QStringLiteral("windows-%1").arg(arch),
        QStringLiteral("windows-x64")
    };
#elif defined(Q_OS_MACOS)
    return {
        QStringLiteral("macos-universal"),
        QStringLiteral("macos-%1").arg(arch)
    };
#else
    return {
        QStringLiteral("linux-%1").arg(arch),
        QStringLiteral("linux-x86_64"),
        QStringLiteral("linux-x64")
    };
#endif
}

bool UpdateManifest::isAllowedDownloadUrl(
    const QUrl& url
    )
{
    return url.isValid()
        && !url.isRelative()
        && (
            url.scheme() == QStringLiteral("https")
            || isLocalHttpUrl(url)
            );
}

int UpdateManifest::schemaVersion() const
{
    return m_schemaVersion;
}

QString UpdateManifest::channel() const
{
    return m_channel;
}

Version UpdateManifest::latestVersion() const
{
    return m_latestVersion;
}

Version UpdateManifest::minimumSupportedVersion() const
{
    return m_minimumSupportedVersion;
}

QDate UpdateManifest::releaseDate() const
{
    return m_releaseDate;
}

QUrl UpdateManifest::notesUrl() const
{
    return m_notesUrl;
}

QStringList UpdateManifest::platformKeys() const
{
    return m_artifacts.keys();
}

bool UpdateManifest::hasArtifactFor(
    const QString& platformKey
    ) const
{
    return m_artifacts.contains(
        platformKey
        );
}

Result<UpdateArtifact> UpdateManifest::artifactForAny(
    const QStringList& platformKeys
    ) const
{
    for (const QString& platformKey : platformKeys)
    {
        if (m_artifacts.contains(platformKey))
        {
            return m_artifacts.value(platformKey);
        }
    }

    return std::unexpected(
        QStringLiteral("Update manifest does not contain an artifact for this platform.")
        );
}

Result<UpdateArtifact> UpdateManifest::parseArtifact(
    const QString& platformKey,
    const QJsonObject& object
    )
{
    const QUrl url(
        object.value(QStringLiteral("url")).toString()
        );

    if (!isAllowedDownloadUrl(url))
    {
        return std::unexpected(
            QStringLiteral("Platform '%1' has an invalid or non-HTTPS url.")
                .arg(platformKey)
            );
    }

    const QString fileName =
        object.value(QStringLiteral("fileName")).toString().trimmed();

    if (fileName.isEmpty())
    {
        return std::unexpected(
            QStringLiteral("Platform '%1' fileName is required.")
                .arg(platformKey)
            );
    }

    const QString sha256 =
        object.value(QStringLiteral("sha256")).toString().trimmed().toLower();

    if (!isValidSha256(sha256))
    {
        return std::unexpected(
            QStringLiteral("Platform '%1' sha256 must be 64 hexadecimal characters.")
                .arg(platformKey)
            );
    }

    const QJsonValue sizeValue =
        object.value(QStringLiteral("sizeBytes"));

    if (!sizeValue.isDouble())
    {
        return std::unexpected(
            QStringLiteral("Platform '%1' sizeBytes is required.")
                .arg(platformKey)
            );
    }

    const qint64 sizeBytes =
        static_cast<qint64>(sizeValue.toDouble());

    if (sizeBytes <= 0)
    {
        return std::unexpected(
            QStringLiteral("Platform '%1' sizeBytes must be greater than zero.")
                .arg(platformKey)
            );
    }

    UpdateArtifact artifact;
    artifact.platformKey =
        platformKey;
    artifact.url =
        url;
    artifact.fileName =
        fileName;
    artifact.sha256 =
        sha256;
    artifact.sizeBytes =
        sizeBytes;

    return artifact;
}
