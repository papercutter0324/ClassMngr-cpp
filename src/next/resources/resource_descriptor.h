#pragma once

#include "resource_id.h"

#include <QByteArray>
#include <QString>

#include <optional>

namespace ClassMngr::Next::Resources
{

enum class ResourceLocation
{
    PackageFile,
    QtResource
};

enum class ResourceType
{
    Json,
    Pdf,
    Pptx,
    Docx,
    Odp,
    Odt,
    Ods,
    Xlsx,
    Svg,
    Png,
    Icns,
    Ico,
    Ttf,
    Ttc,
    Stylesheet,
    Qm,
    QmlModuleComponent
};

class ResourceDescriptor final
{
public:
    ResourceDescriptor(const ResourceDescriptor&) = default;
    ResourceDescriptor(ResourceDescriptor&&) noexcept = default;
    ResourceDescriptor& operator=(const ResourceDescriptor&) = delete;
    ResourceDescriptor& operator=(ResourceDescriptor&&) = delete;

    [[nodiscard]] const ResourceId& id() const noexcept { return m_id; }
    [[nodiscard]] const QString& packageRelativePath() const noexcept
    {
        return m_packageRelativePath;
    }
    [[nodiscard]] ResourceLocation location() const noexcept { return m_location; }
    [[nodiscard]] ResourceType expectedType() const noexcept { return m_expectedType; }
    [[nodiscard]] const QString& expectedTypeName() const noexcept
    {
        return m_expectedTypeName;
    }
    [[nodiscard]] const QString& category() const noexcept { return m_category; }
    [[nodiscard]] const QString& resourceKind() const noexcept { return m_resourceKind; }
    [[nodiscard]] const QString& owner() const noexcept { return m_owner; }
    [[nodiscard]] const QString& startupNecessity() const noexcept
    {
        return m_startupNecessity;
    }
    [[nodiscard]] const QString& loadingTier() const noexcept { return m_loadingTier; }
    [[nodiscard]] const QString& loadingPolicy() const noexcept { return m_loadingPolicy; }
    [[nodiscard]] const QString& lifetime() const noexcept { return m_lifetime; }
    [[nodiscard]] const QString& releasePolicy() const noexcept { return m_releasePolicy; }
    [[nodiscard]] const QString& streamabilityStatus() const noexcept
    {
        return m_streamabilityStatus;
    }
    [[nodiscard]] const QString& streamabilityMethod() const noexcept
    {
        return m_streamabilityMethod;
    }
    [[nodiscard]] const QString& runtimeUrl() const noexcept { return m_runtimeUrl; }
    [[nodiscard]] const QString& sourcePath() const noexcept { return m_sourcePath; }
    [[nodiscard]] qint64 sourceBytes() const noexcept { return m_sourceBytes; }
    [[nodiscard]] const QByteArray& sourceSha256() const noexcept { return m_sourceSha256; }
    [[nodiscard]] const QString& generationStatus() const noexcept
    {
        return m_generationStatus;
    }
    [[nodiscard]] const QString& generationGenerator() const noexcept
    {
        return m_generationGenerator;
    }
    [[nodiscard]] const std::optional<qint64>& expectedBytes() const noexcept
    {
        return m_expectedBytes;
    }
    [[nodiscard]] const std::optional<QByteArray>& expectedSha256() const noexcept
    {
        return m_expectedSha256;
    }

private:
    friend class ResourceCatalog;

    ResourceDescriptor(
        ResourceId id,
        QString packageRelativePath,
        ResourceLocation location,
        ResourceType expectedType,
        QString expectedTypeName,
        QString category,
        QString resourceKind,
        QString owner,
        QString startupNecessity,
        QString loadingTier,
        QString loadingPolicy,
        QString lifetime,
        QString releasePolicy,
        QString streamabilityStatus,
        QString streamabilityMethod,
        QString runtimeUrl,
        QString sourcePath,
        qint64 sourceBytes,
        QByteArray sourceSha256,
        QString generationStatus,
        QString generationGenerator,
        std::optional<qint64> expectedBytes,
        std::optional<QByteArray> expectedSha256
        )
        : m_id(std::move(id))
        , m_packageRelativePath(std::move(packageRelativePath))
        , m_location(location)
        , m_expectedType(expectedType)
        , m_expectedTypeName(std::move(expectedTypeName))
        , m_category(std::move(category))
        , m_resourceKind(std::move(resourceKind))
        , m_owner(std::move(owner))
        , m_startupNecessity(std::move(startupNecessity))
        , m_loadingTier(std::move(loadingTier))
        , m_loadingPolicy(std::move(loadingPolicy))
        , m_lifetime(std::move(lifetime))
        , m_releasePolicy(std::move(releasePolicy))
        , m_streamabilityStatus(std::move(streamabilityStatus))
        , m_streamabilityMethod(std::move(streamabilityMethod))
        , m_runtimeUrl(std::move(runtimeUrl))
        , m_sourcePath(std::move(sourcePath))
        , m_sourceBytes(sourceBytes)
        , m_sourceSha256(std::move(sourceSha256))
        , m_generationStatus(std::move(generationStatus))
        , m_generationGenerator(std::move(generationGenerator))
        , m_expectedBytes(expectedBytes)
        , m_expectedSha256(std::move(expectedSha256))
    {
    }

    ResourceId m_id;
    QString m_packageRelativePath;
    ResourceLocation m_location;
    ResourceType m_expectedType;
    QString m_expectedTypeName;
    QString m_category;
    QString m_resourceKind;
    QString m_owner;
    QString m_startupNecessity;
    QString m_loadingTier;
    QString m_loadingPolicy;
    QString m_lifetime;
    QString m_releasePolicy;
    QString m_streamabilityStatus;
    QString m_streamabilityMethod;
    QString m_runtimeUrl;
    QString m_sourcePath;
    qint64 m_sourceBytes = 0;
    QByteArray m_sourceSha256;
    QString m_generationStatus;
    QString m_generationGenerator;
    std::optional<qint64> m_expectedBytes;
    std::optional<QByteArray> m_expectedSha256;
};

}
