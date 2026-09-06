#include "core/database_file_format.h"

#include "classmngr/engine/database_file_format.h"

#include <QByteArray>

#include <cstddef>
#include <string_view>

namespace DatabaseFileFormat
{
QString nativeExtension()
{
    return QString::fromUtf8(
        classmngr::engine::DatabaseFileFormat::nativeExtension().data(),
        qsizetype(classmngr::engine::DatabaseFileFormat::nativeExtension().size())
        );
}

QString legacyExtension()
{
    return QString::fromUtf8(
        classmngr::engine::DatabaseFileFormat::legacyExtension().data(),
        qsizetype(classmngr::engine::DatabaseFileFormat::legacyExtension().size())
        );
}

bool isNativePath(
    const QString& filePath
    )
{
    const QByteArray utf8Path = filePath.toUtf8();
    return classmngr::engine::DatabaseFileFormat::isNativePath(
        std::string_view(utf8Path.constData(), std::size_t(utf8Path.size()))
        );
}

bool isLegacyPath(
    const QString& filePath
    )
{
    const QByteArray utf8Path = filePath.toUtf8();
    return classmngr::engine::DatabaseFileFormat::isLegacyPath(
        std::string_view(utf8Path.constData(), std::size_t(utf8Path.size()))
        );
}

bool isSupportedInputPath(
    const QString& filePath
    )
{
    return isNativePath(filePath)
        || isLegacyPath(filePath);
}

QString nativeOutputPath(
    const QString& filePath
    )
{
    const QByteArray utf8Path = filePath.toUtf8();
    const std::string result =
        classmngr::engine::DatabaseFileFormat::nativeOutputPath(
            std::string_view(utf8Path.constData(), std::size_t(utf8Path.size()))
            );
    return QString::fromUtf8(
        result.data(),
        qsizetype(result.size())
        );
}

QString supportedInputPath(
    const QString& filePath
    )
{
    const QByteArray utf8Path = filePath.toUtf8();
    const std::string result =
        classmngr::engine::DatabaseFileFormat::supportedInputPath(
            std::string_view(utf8Path.constData(), std::size_t(utf8Path.size()))
            );
    return QString::fromUtf8(
        result.data(),
        qsizetype(result.size())
        );
}
}
