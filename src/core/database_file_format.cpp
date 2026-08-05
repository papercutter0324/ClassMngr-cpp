#include "core/database_file_format.h"

#include <QFileInfo>

namespace DatabaseFileFormat
{
QString nativeExtension()
{
    return QStringLiteral(".tps");
}

QString legacyExtension()
{
    return QStringLiteral(".db");
}

bool isNativePath(
    const QString& filePath
    )
{
    return filePath.endsWith(
        nativeExtension(),
        Qt::CaseInsensitive
        );
}

bool isLegacyPath(
    const QString& filePath
    )
{
    return filePath.endsWith(
        legacyExtension(),
        Qt::CaseInsensitive
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
    if (
        filePath.trimmed().isEmpty()
        || isNativePath(filePath)
        )
    {
        return filePath;
    }

    QString result = filePath;

    if (isLegacyPath(result))
    {
        result.chop(
            legacyExtension().size()
            );
    }

    result += nativeExtension();
    return result;
}

QString supportedInputPath(
    const QString& filePath
    )
{
    if (
        filePath.trimmed().isEmpty()
        || isSupportedInputPath(filePath)
        )
    {
        return filePath;
    }

    if (QFileInfo(filePath).suffix().isEmpty())
    {
        return filePath + nativeExtension();
    }

    return filePath;
}
}
