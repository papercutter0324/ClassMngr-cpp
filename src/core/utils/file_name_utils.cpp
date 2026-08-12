#include "file_name_utils.h"

#include <QSet>

#include <utility>

namespace FileNameUtils
{
namespace
{
bool isUnsafeFileNameCharacter(QChar character)
{
    const ushort code = character.unicode();
    return code <= 0x1f
        || (code >= 0x7f && code <= 0x9f)
        || QStringLiteral("\\\\/:*?\"<>|").contains(character);
}

QString sanitizedBaseName(QString name, QChar replacement)
{
    name = name.normalized(QString::NormalizationForm_C).trimmed();

    QString result;
    result.reserve(name.size());
    for (const QChar character : name)
    {
        result.append(
            isUnsafeFileNameCharacter(character) ? replacement : character
            );
    }

    while (result.endsWith(u' ') || result.endsWith(u'.'))
    {
        result.chop(1);
    }
    return result;
}

bool isWindowsReservedName(const QString& name)
{
    static const QSet<QString> reservedNames{
        QStringLiteral("CON"), QStringLiteral("PRN"),
        QStringLiteral("AUX"), QStringLiteral("NUL"),
        QStringLiteral("COM1"), QStringLiteral("COM2"),
        QStringLiteral("COM3"), QStringLiteral("COM4"),
        QStringLiteral("COM5"), QStringLiteral("COM6"),
        QStringLiteral("COM7"), QStringLiteral("COM8"),
        QStringLiteral("COM9"), QStringLiteral("LPT1"),
        QStringLiteral("LPT2"), QStringLiteral("LPT3"),
        QStringLiteral("LPT4"), QStringLiteral("LPT5"),
        QStringLiteral("LPT6"), QStringLiteral("LPT7"),
        QStringLiteral("LPT8"), QStringLiteral("LPT9")
    };
    return reservedNames.contains(name.section(u'.', 0, 0).toUpper());
}

QString limitedBaseName(QString name)
{
    constexpr qsizetype MaximumUtf8Bytes = 240;
    while (name.toUtf8().size() > MaximumUtf8Bytes)
    {
        name.chop(1);
    }
    return name;
}
}

QString filesystemSafeFileName(
    QString suggestedBaseName,
    QString extension,
    QString fallbackBaseName,
    QChar replacement
    )
{
    extension = extension.trimmed();
    if (!extension.isEmpty() && !extension.startsWith(u'.'))
    {
        extension.prepend(u'.');
    }

    while (!extension.isEmpty()
        && suggestedBaseName.endsWith(extension, Qt::CaseInsensitive))
    {
        suggestedBaseName.chop(extension.size());
    }

    QString baseName = sanitizedBaseName(suggestedBaseName, replacement);
    if (baseName.isEmpty())
    {
        baseName = sanitizedBaseName(fallbackBaseName, replacement);
    }
    if (baseName.isEmpty())
    {
        baseName = QStringLiteral("Document");
    }
    if (isWindowsReservedName(baseName))
    {
        baseName.prepend(u'_');
    }

    return limitedBaseName(baseName) + extension;
}

QString filesystemSafeJsonFileName(
    QString suggestedBaseName,
    QString fallbackBaseName
    )
{
    if (fallbackBaseName.trimmed().isEmpty())
    {
        fallbackBaseName = QStringLiteral("Class");
    }
    return filesystemSafeFileName(
        std::move(suggestedBaseName),
        QStringLiteral(".json"),
        std::move(fallbackBaseName)
        );
}

} // namespace FileNameUtils
