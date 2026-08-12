#include "resource_pack_manifest.h"

#include "core/network/http_request_policy.h"

#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>

namespace
{
constexpr int SupportedSchemaVersion = 1;

bool isValidPackId(
    const QString& packId
    )
{
    static const QRegularExpression pattern(
        QStringLiteral(R"(^[a-z0-9]+(?:-[a-z0-9]+)*$)")
        );

    return pattern.match(packId).hasMatch();
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
}

Result<ResourcePackManifest> ResourcePackManifest::fromJson(
    const QByteArray& data,
    const QStringList& requiredPackIds
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
            QStringLiteral("Resource-pack manifest JSON is invalid: %1")
                .arg(parseError.errorString())
            );
    }

    if (!document.isObject())
    {
        return std::unexpected(
            QStringLiteral("Resource-pack manifest root must be an object.")
            );
    }

    const QJsonObject root =
        document.object();

    const int schemaVersion =
        root.value(QStringLiteral("schemaVersion")).toInt(-1);

    if (schemaVersion != SupportedSchemaVersion)
    {
        return std::unexpected(
            QStringLiteral("Unsupported resource-pack manifest schema version.")
            );
    }

    const QJsonValue packsValue =
        root.value(QStringLiteral("packs"));

    if (!packsValue.isObject())
    {
        return std::unexpected(
            QStringLiteral("Resource-pack manifest packs object is required.")
            );
    }

    ResourcePackManifest manifest;
    manifest.m_schemaVersion =
        schemaVersion;

    const QJsonObject packs =
        packsValue.toObject();

    for (auto it = packs.constBegin(); it != packs.constEnd(); ++it)
    {
        const QString packId =
            it.key().trimmed();

        if (!isValidPackId(packId))
        {
            return std::unexpected(
                QStringLiteral("Resource-pack id '%1' is invalid.")
                    .arg(it.key())
                );
        }

        if (!it.value().isObject())
        {
            return std::unexpected(
                QStringLiteral("Resource pack '%1' must be an object.")
                    .arg(packId)
                );
        }

        const QJsonObject object =
            it.value().toObject();

        const auto version =
            Version::parse(
                object.value(QStringLiteral("version")).toString()
                );

        if (!version)
        {
            return std::unexpected(
                QStringLiteral("Resource pack '%1' has an invalid version: %2")
                    .arg(packId, version.error())
                );
        }

        const QUrl url(
            object.value(QStringLiteral("url")).toString()
            );

        if (!isAllowedDownloadUrl(url))
        {
            return std::unexpected(
                QStringLiteral("Resource pack '%1' must use an HTTPS download URL.")
                    .arg(packId)
                );
        }

        const QString fileName =
            object.value(QStringLiteral("fileName")).toString().trimmed();

        if (
            fileName.isEmpty()
            || QFileInfo(fileName).fileName() != fileName
            || !fileName.endsWith(QStringLiteral(".rcc"), Qt::CaseInsensitive)
            )
        {
            return std::unexpected(
                QStringLiteral("Resource pack '%1' fileName must be a plain .rcc file name.")
                    .arg(packId)
                );
        }

        const QString sha256 =
            object.value(QStringLiteral("sha256")).toString().trimmed();

        if (!isValidSha256(sha256))
        {
            return std::unexpected(
                QStringLiteral("Resource pack '%1' has an invalid SHA-256 checksum.")
                    .arg(packId)
                );
        }

        const QJsonValue sizeValue =
            object.value(QStringLiteral("sizeBytes"));

        if (!sizeValue.isDouble())
        {
            return std::unexpected(
                QStringLiteral("Resource pack '%1' sizeBytes must be a positive integer.")
                    .arg(packId)
                );
        }

        const qint64 sizeBytes =
            sizeValue.toInteger(-1);

        if (sizeBytes <= 0)
        {
            return std::unexpected(
                QStringLiteral("Resource pack '%1' sizeBytes must be a positive integer.")
                    .arg(packId)
                );
        }

        ResourcePackArtifact artifact;
        artifact.id =
            packId;
        artifact.version =
            *version;
        artifact.url =
            url;
        artifact.fileName =
            fileName;
        artifact.sha256 =
            sha256.toLower();
        artifact.sizeBytes =
            sizeBytes;

        manifest.m_artifacts.insert(
            packId,
            artifact
            );
    }

    if (manifest.m_artifacts.isEmpty())
    {
        return std::unexpected(
            QStringLiteral("Resource-pack manifest must include at least one pack.")
            );
    }

    for (const QString& requiredPackId : requiredPackIds)
    {
        if (!manifest.hasPack(requiredPackId))
        {
            return std::unexpected(
                QStringLiteral("Resource-pack manifest is missing '%1'.")
                    .arg(requiredPackId)
                );
        }
    }

    return manifest;
}

bool ResourcePackManifest::isAllowedDownloadUrl(
    const QUrl& url
    )
{
    return HttpRequestPolicy::isAllowedSecureUrl(url);
}

int ResourcePackManifest::schemaVersion() const
{
    return m_schemaVersion;
}

QStringList ResourcePackManifest::packIds() const
{
    QStringList ids =
        m_artifacts.keys();
    ids.sort();
    return ids;
}

bool ResourcePackManifest::hasPack(
    const QString& packId
    ) const
{
    return m_artifacts.contains(packId);
}

Result<ResourcePackArtifact> ResourcePackManifest::artifact(
    const QString& packId
    ) const
{
    const auto it =
        m_artifacts.constFind(packId);

    if (it == m_artifacts.constEnd())
    {
        return std::unexpected(
            QStringLiteral("Resource pack '%1' was not found in the manifest.")
                .arg(packId)
            );
    }

    return it.value();
}
