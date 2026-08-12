#pragma once

#include <QChar>
#include <QString>

namespace FileNameUtils
{

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
