#include "resource_loader.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QImageReader>
#include <QJsonParseError>
#include <QStringDecoder>
#include <QUrl>

#include <limits>
#include <utility>

namespace ClassMngr::Next::Resources
{
namespace
{

constexpr qint64 MaximumTextBytes = 16 * 1024 * 1024;
constexpr qint64 MaximumImageBytes = 128 * 1024 * 1024;
constexpr qint64 MaximumFontBytes = 128 * 1024 * 1024;
constexpr qint64 MaximumTranslationBytes = 32 * 1024 * 1024;
constexpr qint64 MaximumQtResourceBytes = 32 * 1024 * 1024;
constexpr qint64 FileReadChunkBytes = 1024 * 1024;

ResourceError error(ResourceErrorCode code, QString message)
{
    return {code, std::move(message)};
}

bool isImageType(ResourceType type)
{
    return type == ResourceType::Png || type == ResourceType::Svg
        || type == ResourceType::Icns || type == ResourceType::Ico;
}

bool isFontType(ResourceType type)
{
    return type == ResourceType::Ttf || type == ResourceType::Ttc;
}

bool isTextType(ResourceType type)
{
    return type == ResourceType::Stylesheet || type == ResourceType::QmlModuleComponent;
}

bool isInsideRoot(const QString& root, const QString& canonicalFile)
{
    const QString relative = QDir::fromNativeSeparators(QDir(root).relativeFilePath(canonicalFile));
    return !QDir::isAbsolutePath(relative)
        && relative != QStringLiteral("..")
        && !relative.startsWith(QStringLiteral("../"));
}

}

ResourceFileStream::ResourceFileStream(std::unique_ptr<QFile> file)
    : m_file(std::move(file))
{
}

QIODevice& ResourceFileStream::device() noexcept
{
    return *m_file;
}

const QIODevice& ResourceFileStream::device() const noexcept
{
    return *m_file;
}

bool ResourceFileStream::isOpen() const noexcept
{
    return m_file && m_file->isOpen();
}

RegisteredFont::RegisteredFont(int registrationId, QStringList familyNames) noexcept
    : m_registrationId(registrationId)
    , m_familyNames(std::move(familyNames))
{
}

RegisteredFont::RegisteredFont(RegisteredFont&& other) noexcept
    : m_registrationId(std::exchange(other.m_registrationId, -1))
    , m_familyNames(std::move(other.m_familyNames))
{
}

RegisteredFont& RegisteredFont::operator=(RegisteredFont&& other) noexcept
{
    if (this != &other)
    {
        release();
        m_registrationId = std::exchange(other.m_registrationId, -1);
        m_familyNames = std::move(other.m_familyNames);
    }
    return *this;
}

RegisteredFont::~RegisteredFont()
{
    release();
}

void RegisteredFont::release() noexcept
{
    if (m_registrationId >= 0)
    {
        QFontDatabase::removeApplicationFont(m_registrationId);
        m_registrationId = -1;
    }
}

ResourceTranslation::ResourceTranslation(
    QByteArray translatorData,
    std::unique_ptr<QTranslator> translator,
    QCoreApplication* application
    ) noexcept
    : m_translatorData(std::move(translatorData))
    , m_translator(std::move(translator))
    , m_application(application)
{
}

ResourceTranslation::ResourceTranslation(ResourceTranslation&& other) noexcept
    : m_translatorData(std::move(other.m_translatorData))
    , m_translator(std::move(other.m_translator))
    , m_application(std::move(other.m_application))
    , m_installed(std::exchange(other.m_installed, false))
{
}

ResourceTranslation& ResourceTranslation::operator=(ResourceTranslation&& other) noexcept
{
    if (this != &other)
    {
        release();
        m_translatorData = std::move(other.m_translatorData);
        m_translator = std::move(other.m_translator);
        m_application = std::move(other.m_application);
        m_installed = std::exchange(other.m_installed, false);
    }
    return *this;
}

ResourceTranslation::~ResourceTranslation()
{
    release();
}

const QTranslator& ResourceTranslation::translator() const noexcept
{
    return *m_translator;
}

void ResourceTranslation::release() noexcept
{
    if (m_installed && m_translator && m_application
        && QCoreApplication::instance() == m_application.data())
    {
        QCoreApplication::removeTranslator(m_translator.get());
    }
    m_installed = false;
    m_application.clear();
    m_translator.reset();
    m_translatorData.clear();
}

ResourceLoader::ResourceLoader(ResourceCatalog catalog)
    : m_catalog(std::move(catalog))
{
}

ResourceResult<const ResourceDescriptor*> ResourceLoader::descriptor(
    const ResourceId& id
    ) const
{
    const ResourceDescriptor* found = m_catalog.find(id);
    if (found == nullptr)
    {
        return std::unexpected(error(
            ResourceErrorCode::NotFound,
            QStringLiteral("resource ID is not present in the loaded catalog: ") + id.value()
            ));
    }
    return found;
}

ResourceResult<QString> ResourceLoader::packageFilePath(
    const ResourceDescriptor& descriptor
    ) const
{
    if (descriptor.location() != ResourceLocation::PackageFile)
    {
        return std::unexpected(error(
            ResourceErrorCode::UnsupportedOperation,
            QStringLiteral("resource is a Qt resource URL, not a package file")
            ));
    }

    const QString candidate = QDir(m_catalog.resourceRoot()).filePath(
        descriptor.packageRelativePath()
        );
    const QFileInfo candidateInfo(candidate);
    if (candidateInfo.isSymLink() && candidateInfo.canonicalFilePath().isEmpty())
    {
        return std::unexpected(error(
            ResourceErrorCode::UnsafePath,
            QStringLiteral("resource path contains a dangling symlink")
            ));
    }
    if (!candidateInfo.exists())
    {
        return std::unexpected(error(
            ResourceErrorCode::MissingFile,
            QStringLiteral("packaged resource is missing: ") + descriptor.packageRelativePath()
            ));
    }

    const QString canonicalPath = candidateInfo.canonicalFilePath();
    if (canonicalPath.isEmpty() || !candidateInfo.isFile())
    {
        return std::unexpected(error(
            ResourceErrorCode::IoError,
            QStringLiteral("packaged resource is not a readable regular file: ")
                + descriptor.packageRelativePath()
            ));
    }
    if (!isInsideRoot(m_catalog.resourceRoot(), canonicalPath))
    {
        return std::unexpected(error(
            ResourceErrorCode::UnsafePath,
            QStringLiteral("packaged resource resolves outside the injected resource root")
            ));
    }
    return canonicalPath;
}

ResourceResult<void> ResourceLoader::verifyFile(
    const ResourceDescriptor& descriptor,
    const QString& canonicalPath
    ) const
{
    QFileInfo info(canonicalPath);
    if (!info.isFile())
    {
        return std::unexpected(error(
            ResourceErrorCode::MissingFile,
            QStringLiteral("packaged resource is no longer a regular file")
            ));
    }
    if (descriptor.expectedBytes() && info.size() != *descriptor.expectedBytes())
    {
        return std::unexpected(error(
            ResourceErrorCode::CorruptResource,
            QStringLiteral("packaged resource byte count differs from its catalog entry")
            ));
    }

    QFile file(canonicalPath);
    if (!file.open(QIODevice::ReadOnly))
    {
        return std::unexpected(error(
            ResourceErrorCode::IoError,
            QStringLiteral("cannot open packaged resource: ") + file.errorString()
            ));
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    qint64 totalBytes = 0;
    while (!file.atEnd())
    {
        const QByteArray chunk = file.read(FileReadChunkBytes);
        if (chunk.isEmpty())
        {
            if (file.error() != QFileDevice::NoError)
            {
                return std::unexpected(error(
                    ResourceErrorCode::IoError,
                    QStringLiteral("error while verifying packaged resource: ")
                        + file.errorString()
                    ));
            }
            break;
        }
        totalBytes += chunk.size();
        hash.addData(chunk);
    }
    if (descriptor.expectedBytes() && totalBytes != *descriptor.expectedBytes())
    {
        return std::unexpected(error(
            ResourceErrorCode::CorruptResource,
            QStringLiteral("packaged resource byte count changed while reading")
            ));
    }
    if (descriptor.expectedSha256() && hash.result() != *descriptor.expectedSha256())
    {
        return std::unexpected(error(
            ResourceErrorCode::CorruptResource,
            QStringLiteral("packaged resource SHA-256 differs from its catalog entry")
            ));
    }
    return {};
}

ResourceResult<QByteArray> ResourceLoader::readBytes(
    const ResourceDescriptor& descriptor,
    qint64 maximumBytes
    ) const
{
    if (descriptor.location() == ResourceLocation::PackageFile)
    {
        auto path = packageFilePath(descriptor);
        if (!path)
        {
            return std::unexpected(path.error());
        }
        const QFileInfo info(*path);
        if (info.size() > maximumBytes)
        {
            return std::unexpected(error(
                ResourceErrorCode::UnsupportedOperation,
                QStringLiteral("resource exceeds the bounded in-memory loading limit")
                ));
        }

        QFile file(*path);
        if (!file.open(QIODevice::ReadOnly))
        {
            return std::unexpected(error(
                ResourceErrorCode::IoError,
                QStringLiteral("cannot open packaged resource: ") + file.errorString()
                ));
        }
        const QByteArray bytes = file.readAll();
        if (file.error() != QFileDevice::NoError)
        {
            return std::unexpected(error(
                ResourceErrorCode::IoError,
                QStringLiteral("cannot finish reading packaged resource: ") + file.errorString()
                ));
        }
        if (bytes.size() > maximumBytes)
        {
            return std::unexpected(error(
                ResourceErrorCode::UnsupportedOperation,
                QStringLiteral("resource grew beyond the bounded in-memory loading limit")
                ));
        }
        if (descriptor.expectedBytes() && bytes.size() != *descriptor.expectedBytes())
        {
            return std::unexpected(error(
                ResourceErrorCode::CorruptResource,
                QStringLiteral("packaged resource byte count differs from its catalog entry")
                ));
        }
        if (descriptor.expectedSha256()
            && QCryptographicHash::hash(bytes, QCryptographicHash::Sha256)
                != *descriptor.expectedSha256())
        {
            return std::unexpected(error(
                ResourceErrorCode::CorruptResource,
                QStringLiteral("packaged resource SHA-256 differs from its catalog entry")
                ));
        }
        return bytes;
    }

    const QUrl runtimeUrl(descriptor.runtimeUrl());
    if (runtimeUrl.scheme() != QStringLiteral("qrc") || runtimeUrl.path().isEmpty())
    {
        return std::unexpected(error(
            ResourceErrorCode::CorruptResource,
            QStringLiteral("catalog contains an invalid Qt resource URL")
            ));
    }
    const QString qtResourcePath = QStringLiteral(":") + runtimeUrl.path();
    QFile file(qtResourcePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        return std::unexpected(error(
            ResourceErrorCode::MissingFile,
            QStringLiteral("compiled Qt resource is missing: ") + descriptor.runtimeUrl()
            ));
    }
    if (file.size() > maximumBytes)
    {
        return std::unexpected(error(
            ResourceErrorCode::UnsupportedOperation,
            QStringLiteral("Qt resource exceeds the bounded in-memory loading limit")
            ));
    }
    const QByteArray bytes = file.readAll();
    if (file.error() != QFileDevice::NoError || bytes.size() > maximumBytes || bytes.isEmpty())
    {
        return std::unexpected(error(
            ResourceErrorCode::CorruptResource,
            QStringLiteral("compiled Qt resource is unreadable, empty, or exceeds its limit")
            ));
    }
    return bytes;
}

ResourceResult<QString> ResourceLoader::loadText(const ResourceId& id) const
{
    auto found = descriptor(id);
    if (!found)
    {
        return std::unexpected(found.error());
    }
    if (!isTextType((*found)->expectedType()))
    {
        return std::unexpected(error(
            ResourceErrorCode::TypeMismatch,
            QStringLiteral("resource is not cataloged as text")
            ));
    }

    auto bytes = readBytes(**found, MaximumTextBytes);
    if (!bytes)
    {
        return std::unexpected(bytes.error());
    }
    QStringDecoder decoder(QStringDecoder::Utf8);
    const QString text = decoder(*bytes);
    if (decoder.hasError())
    {
        return std::unexpected(error(
            ResourceErrorCode::CorruptResource,
            QStringLiteral("text resource is not valid UTF-8")
            ));
    }
    return text;
}

ResourceResult<QJsonDocument> ResourceLoader::loadJson(const ResourceId& id) const
{
    auto found = descriptor(id);
    if (!found)
    {
        return std::unexpected(found.error());
    }
    if ((*found)->expectedType() != ResourceType::Json)
    {
        return std::unexpected(error(
            ResourceErrorCode::TypeMismatch,
            QStringLiteral("resource is not cataloged as JSON")
            ));
    }

    auto bytes = readBytes(**found, MaximumTextBytes);
    if (!bytes)
    {
        return std::unexpected(bytes.error());
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(*bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || document.isNull())
    {
        return std::unexpected(error(
            ResourceErrorCode::CorruptResource,
            QStringLiteral("JSON resource cannot be parsed: ") + parseError.errorString()
            ));
    }
    return document;
}

ResourceResult<QImage> ResourceLoader::loadImage(const ResourceId& id) const
{
    auto found = descriptor(id);
    if (!found)
    {
        return std::unexpected(found.error());
    }
    if (!isImageType((*found)->expectedType()))
    {
        return std::unexpected(error(
            ResourceErrorCode::TypeMismatch,
            QStringLiteral("resource is not cataloged as an image")
            ));
    }

    auto bytes = readBytes(**found, MaximumImageBytes);
    if (!bytes)
    {
        return std::unexpected(bytes.error());
    }
    QBuffer buffer(&*bytes);
    if (!buffer.open(QIODevice::ReadOnly))
    {
        return std::unexpected(error(
            ResourceErrorCode::DecodeError,
            QStringLiteral("cannot open image resource data")
            ));
    }
    QImageReader reader(&buffer);
    reader.setDecideFormatFromContent(true);
    const QImage image = reader.read();
    if (image.isNull())
    {
        return std::unexpected(error(
            ResourceErrorCode::DecodeError,
            QStringLiteral("image resource cannot be decoded: ") + reader.errorString()
            ));
    }
    return image;
}

ResourceResult<RegisteredFont> ResourceLoader::loadFont(const ResourceId& id) const
{
    auto found = descriptor(id);
    if (!found)
    {
        return std::unexpected(found.error());
    }
    if (!isFontType((*found)->expectedType()))
    {
        return std::unexpected(error(
            ResourceErrorCode::TypeMismatch,
            QStringLiteral("resource is not cataloged as a font")
            ));
    }

    auto bytes = readBytes(**found, MaximumFontBytes);
    if (!bytes)
    {
        return std::unexpected(bytes.error());
    }
    const int registrationId = QFontDatabase::addApplicationFontFromData(*bytes);
    if (registrationId < 0)
    {
        return std::unexpected(error(
            ResourceErrorCode::RegistrationFailed,
            QStringLiteral("Qt could not register the font resource")
            ));
    }
    const QStringList families = QFontDatabase::applicationFontFamilies(registrationId);
    if (families.isEmpty())
    {
        QFontDatabase::removeApplicationFont(registrationId);
        return std::unexpected(error(
            ResourceErrorCode::RegistrationFailed,
            QStringLiteral("registered font resource has no family names")
            ));
    }
    return RegisteredFont(registrationId, families);
}

ResourceResult<QString> ResourceLoader::loadStylesheet(const ResourceId& id) const
{
    const ResourceDescriptor* found = m_catalog.find(id);
    if (found == nullptr)
    {
        return std::unexpected(error(
            ResourceErrorCode::NotFound,
            QStringLiteral("resource ID is not present in the loaded catalog: ") + id.value()
            ));
    }
    if (found->expectedType() != ResourceType::Stylesheet)
    {
        return std::unexpected(error(
            ResourceErrorCode::TypeMismatch,
            QStringLiteral("resource is not cataloged as a stylesheet")
            ));
    }
    return loadText(id);
}

ResourceResult<ResourceTranslation> ResourceLoader::loadTranslation(
    const ResourceId& id,
    QCoreApplication& application
    ) const
{
    auto found = descriptor(id);
    if (!found)
    {
        return std::unexpected(found.error());
    }
    if ((*found)->expectedType() != ResourceType::Qm
        || (*found)->location() != ResourceLocation::PackageFile)
    {
        return std::unexpected(error(
            ResourceErrorCode::TypeMismatch,
            QStringLiteral("resource is not a file-backed Qt translation")
            ));
    }
    if (QCoreApplication::instance() != &application)
    {
        return std::unexpected(error(
            ResourceErrorCode::InvalidArgument,
            QStringLiteral("translation installation requires the current QCoreApplication")
            ));
    }

    auto bytes = readBytes(**found, MaximumTranslationBytes);
    if (!bytes)
    {
        return std::unexpected(bytes.error());
    }
    if (bytes->size() > std::numeric_limits<int>::max())
    {
        return std::unexpected(error(
            ResourceErrorCode::UnsupportedOperation,
            QStringLiteral("translation resource is too large for QTranslator")
            ));
    }

    auto translator = std::make_unique<QTranslator>();
    if (!translator->load(
            reinterpret_cast<const uchar*>(bytes->constData()),
            static_cast<int>(bytes->size())
            ))
    {
        return std::unexpected(error(
            ResourceErrorCode::CorruptResource,
            QStringLiteral("Qt cannot load the compiled translation resource")
            ));
    }

    ResourceTranslation result(std::move(*bytes), std::move(translator), &application);
    if (!QCoreApplication::installTranslator(result.m_translator.get()))
    {
        return std::unexpected(error(
            ResourceErrorCode::RegistrationFailed,
            QStringLiteral("Qt refused to install the translation resource")
            ));
    }
    result.m_installed = true;
    return result;
}

ResourceResult<ResourceFileStream> ResourceLoader::openStream(const ResourceId& id) const
{
    auto found = descriptor(id);
    if (!found)
    {
        return std::unexpected(found.error());
    }
    const ResourceDescriptor& resource = **found;
    if (resource.location() != ResourceLocation::PackageFile)
    {
        return std::unexpected(error(
            ResourceErrorCode::UnsupportedOperation,
            QStringLiteral("Qt resource URLs do not have package file streams")
            ));
    }
    if (resource.streamabilityStatus() == QStringLiteral("no"))
    {
        return std::unexpected(error(
            ResourceErrorCode::UnsupportedOperation,
            QStringLiteral("catalog marks this resource as non-streamable")
            ));
    }

    auto path = packageFilePath(resource);
    if (!path)
    {
        return std::unexpected(path.error());
    }
    auto verified = verifyFile(resource, *path);
    if (!verified)
    {
        return std::unexpected(verified.error());
    }

    // Re-resolve after verification so a changed symlink is checked before the
    // owning QFile handle is opened.
    path = packageFilePath(resource);
    if (!path)
    {
        return std::unexpected(path.error());
    }
    auto file = std::make_unique<QFile>(*path);
    if (!file->open(QIODevice::ReadOnly))
    {
        return std::unexpected(error(
            ResourceErrorCode::IoError,
            QStringLiteral("cannot open file-backed resource stream: ") + file->errorString()
            ));
    }
    return ResourceFileStream(std::move(file));
}

}
