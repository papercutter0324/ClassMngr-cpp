#pragma once

#include "resource_catalog.h"
#include "resource_cache.h"
#include "resource_diagnostics.h"
#include "resource_error.h"
#include "resource_scope.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QFile>
#include <QImage>
#include <QJsonDocument>
#include <QIODevice>
#include <QPointer>
#include <QStringList>
#include <QTranslator>

#include <memory>
#include <optional>

namespace ClassMngr::Next::Resources
{

class ResourceFileStream final
{
public:
    ResourceFileStream(const ResourceFileStream&) = delete;
    ResourceFileStream& operator=(const ResourceFileStream&) = delete;
    ResourceFileStream(ResourceFileStream&&) noexcept = default;
    ResourceFileStream& operator=(ResourceFileStream&&) noexcept = default;
    ~ResourceFileStream() = default;

    // The device is borrowed and remains valid only while this stream handle lives.
    [[nodiscard]] QIODevice& device() noexcept;
    [[nodiscard]] const QIODevice& device() const noexcept;
    [[nodiscard]] bool isOpen() const noexcept;
    void close() noexcept;

private:
    friend class ResourceLoader;

    explicit ResourceFileStream(std::unique_ptr<QFile> file);

    std::unique_ptr<QFile> m_file;
};

class RegisteredFont final
{
public:
    RegisteredFont(const RegisteredFont&) = delete;
    RegisteredFont& operator=(const RegisteredFont&) = delete;
    RegisteredFont(RegisteredFont&& other) noexcept;
    RegisteredFont& operator=(RegisteredFont&& other) noexcept;
    ~RegisteredFont();

    [[nodiscard]] int registrationId() const noexcept { return m_registrationId; }
    [[nodiscard]] const QStringList& familyNames() const noexcept { return m_familyNames; }

private:
    friend class ResourceLoader;

    RegisteredFont(int registrationId, QStringList familyNames) noexcept;
    void release() noexcept;

    int m_registrationId = -1;
    QStringList m_familyNames;
};

class ResourceTranslation final
{
public:
    ResourceTranslation(const ResourceTranslation&) = delete;
    ResourceTranslation& operator=(const ResourceTranslation&) = delete;
    ResourceTranslation(ResourceTranslation&& other) noexcept;
    ResourceTranslation& operator=(ResourceTranslation&& other) noexcept;
    ~ResourceTranslation();

    [[nodiscard]] bool isInstalled() const noexcept
    {
        return m_installed && m_application
            && QCoreApplication::instance() == m_application.data();
    }
    [[nodiscard]] const QTranslator& translator() const noexcept;

private:
    friend class ResourceLoader;

    ResourceTranslation(
        QByteArray translatorData,
        std::unique_ptr<QTranslator> translator,
        QCoreApplication* application
        ) noexcept;
    void release() noexcept;

    // Keep the data alive for the lifetime of QTranslator::load(data, size).
    QByteArray m_translatorData;
    std::unique_ptr<QTranslator> m_translator;
    QPointer<QCoreApplication> m_application;
    bool m_installed = false;
};

class ResourceLoader final
{
public:
    explicit ResourceLoader(ResourceCatalog catalog);
    // The injected cache is borrowed and must outlive this loader.
    ResourceLoader(ResourceCatalog catalog, ResourceCache& cache);
    ResourceLoader(ResourceCatalog catalog, ResourceDiagnosticsSink diagnostics);
    ResourceLoader(
        ResourceCatalog catalog,
        ResourceCache& cache,
        ResourceDiagnosticsSink diagnostics
        );

    [[nodiscard]] const ResourceCatalog& catalog() const noexcept
    {
        return m_catalog;
    }

    [[nodiscard]] ResourceResult<ScopedResource<QString>> loadText(
        const ResourceRequest& request
        ) const;
    [[nodiscard]] ResourceResult<ScopedResource<QJsonDocument>> loadJson(
        const ResourceRequest& request
        ) const;
    [[nodiscard]] ResourceResult<ScopedResource<QImage>> loadImage(
        const ResourceRequest& request
        ) const;
    // Decodes an image no larger than maximumDimension on either axis. The
    // bound is applied through QImageReader before decoding when source size
    // metadata is available, then checked again on the returned image.
    [[nodiscard]] ResourceResult<ScopedResource<QImage>> loadBoundedImage(
        const ResourceRequest& request,
        int maximumDimension
        ) const;
    [[nodiscard]] ResourceResult<ScopedResource<RegisteredFont>> loadFont(
        const ResourceRequest& request
        ) const;
    [[nodiscard]] ResourceResult<ScopedResource<QString>> loadStylesheet(
        const ResourceRequest& request
        ) const;
    [[nodiscard]] ResourceResult<ScopedResource<ResourceTranslation>> loadTranslation(
        const ResourceRequest& request,
        QCoreApplication& application
        ) const;
    [[nodiscard]] ResourceResult<ScopedResource<ResourceFileStream>> openStream(
        const ResourceRequest& request
        ) const;
    // Native output adapters consume a verified installed file during their
    // operation. Resolution applies the same path/hash checks as openStream.
    [[nodiscard]] ResourceResult<ScopedResource<QString>> resolveFile(
        const ResourceRequest& request
        ) const;

private:
    [[nodiscard]] ResourceResult<ScopedResource<QString>> loadTextResource(
        const ResourceRequest& request,
        std::optional<ResourceType> requiredType
        ) const;
    [[nodiscard]] ResourceResult<ScopedResource<QImage>> loadImageResource(
        const ResourceRequest& request,
        int maximumDimension
        ) const;
    [[nodiscard]] ResourceResult<const ResourceDescriptor*> descriptor(
        const ResourceId& id
        ) const;
    [[nodiscard]] ResourceResult<QByteArray> readBytes(
        const ResourceDescriptor& descriptor,
        qint64 maximumBytes
        ) const;
    [[nodiscard]] ResourceResult<QString> packageFilePath(
        const ResourceDescriptor& descriptor
        ) const;
    [[nodiscard]] ResourceResult<void> verifyFile(
        const ResourceDescriptor& descriptor,
        const QString& canonicalPath
        ) const;

    ResourceCatalog m_catalog;
    ResourceCache* m_cache = nullptr;
    ResourceDiagnosticsSink m_diagnostics;
};

}
