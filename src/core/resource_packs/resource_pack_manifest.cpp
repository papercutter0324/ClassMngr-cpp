#include "resource_pack_manifest.h"

#include "classmngr/engine/resource_pack_policy.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <string>
#include <vector>

namespace
{
QString engineMessage(const classmngr::engine::Error& error)
{
    return QString::fromUtf8(
        error.message.data(),
        static_cast<qsizetype>(error.message.size())
        );
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

    const QJsonValue packsValue =
        root.value(QStringLiteral("packs"));

    if (!packsValue.isObject())
    {
        return std::unexpected(
            QStringLiteral("Resource-pack manifest packs object is required.")
            );
    }

    classmngr::engine::ResourcePackRawManifest engineRaw;
    engineRaw.schemaVersion = root.value(QStringLiteral("schemaVersion")).toInt(-1);

    const QJsonObject packs =
        packsValue.toObject();

    for (auto it = packs.constBegin(); it != packs.constEnd(); ++it)
    {
        if (!it.value().isObject())
        {
            return std::unexpected(
                QStringLiteral("Resource-pack entries must be objects.")
                );
        }

        const QJsonObject object =
            it.value().toObject();

        engineRaw.artifacts.push_back({
            it.key().toUtf8().toStdString(),
            object.value(QStringLiteral("version")).toString().toUtf8().toStdString(),
            object.value(QStringLiteral("url")).toString().toUtf8().toStdString(),
            object.value(QStringLiteral("fileName")).toString().toUtf8().toStdString(),
            object.value(QStringLiteral("sha256")).toString().toUtf8().toStdString(),
            object.value(QStringLiteral("sizeBytes")).toInteger(-1)
        });

    }

    std::vector<std::string> engineRequired;
    engineRequired.reserve(requiredPackIds.size());
    for (const QString& id : requiredPackIds)
    {
        engineRequired.push_back(id.toUtf8().toStdString());
    }
    const auto engineManifest =
        classmngr::engine::ResourcePackPolicy::validateManifest(
            engineRaw,
            engineRequired
            );
    if (!engineManifest)
    {
        return std::unexpected(engineMessage(engineManifest.error()));
    }

    ResourcePackManifest manifest;
    manifest.m_schemaVersion = engineManifest->schemaVersion;
    for (const classmngr::engine::ResourcePackArtifact& engineArtifact : engineManifest->artifacts)
    {
        const auto version = Version::parse(QString::fromStdString(engineArtifact.version.toString()));
        if (!version) return std::unexpected(version.error());
        manifest.m_artifacts.insert(QString::fromStdString(engineArtifact.id), {
            QString::fromStdString(engineArtifact.id), *version,
            QUrl(QString::fromStdString(engineArtifact.url)),
            QString::fromStdString(engineArtifact.fileName),
            QString::fromStdString(engineArtifact.sha256), engineArtifact.sizeBytes
        });
    }

    return manifest;
}

bool ResourcePackManifest::isAllowedDownloadUrl(
    const QUrl& url
    )
{
    return classmngr::engine::ResourcePackPolicy::isAllowedHttpsUrl(
        url.toString(QUrl::FullyEncoded).toUtf8().toStdString()
        );
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
