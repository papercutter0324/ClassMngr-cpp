#pragma once

#include <QChar>
#include <QString>

#include <optional>

namespace FileNameUtils
{

// Unlike filesystemSafeFileName(), this does not supply a fallback name.
// Callers that validate user input can therefore preserve an invalid/missing
// value instead of silently replacing it with an unrelated document name.
[[nodiscard]] std::optional<QString> normalizedFilesystemSafeFileName(
    QString suggestedBaseName,
    QString extension,
    QChar replacement = QChar(u'_')
    );

[[nodiscard]] QString filesystemSafeFileName(
    QString suggestedBaseName,
    QString extension,
    QString fallbackBaseName,
    QChar replacement = QChar(u'_')
    );

[[nodiscard]] QString filesystemSafeJsonFileName(
    QString suggestedBaseName,
    QString fallbackBaseName
    );

} // namespace FileNameUtils
