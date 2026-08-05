#ifndef DATABASE_FILE_FORMAT_H
#define DATABASE_FILE_FORMAT_H

#include <QString>

namespace DatabaseFileFormat
{
QString nativeExtension();
QString legacyExtension();

bool isNativePath(
    const QString& filePath
    );

bool isLegacyPath(
    const QString& filePath
    );

bool isSupportedInputPath(
    const QString& filePath
    );

QString nativeOutputPath(
    const QString& filePath
    );

QString supportedInputPath(
    const QString& filePath
    );
}

#endif // DATABASE_FILE_FORMAT_H
