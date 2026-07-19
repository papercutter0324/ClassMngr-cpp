#pragma once

#include <QSet>
#include <QString>

namespace FileNameUtils
{
namespace Detail
{
inline bool isUnsafeFileNameCharacter(
    QChar character
    )
{
    const ushort code = character.unicode();

    return code <= 0x1f
        || (code >= 0x7f && code <= 0x9f)
        || QStringLiteral("\\/:*?\"<>|").contains(character);
}

inline QString sanitizedBaseName(
    QString name
    )
{
    name = name.normalized(QString::NormalizationForm_C).trimmed();

    QString result;
    result.reserve(name.size());

    for (const QChar character : name)
    {
        result.append(
            isUnsafeFileNameCharacter(character)
                ? QChar(u'_')
                : character
            );
    }

    while (result.endsWith(u' ') || result.endsWith(u'.'))
    {
        result.chop(1);
    }

    return result;
}

inline bool isWindowsReservedName(
    const QString& name
    )
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

    return reservedNames.contains(
        name.section(u'.', 0, 0).toUpper()
        );
}

inline QString limitedBaseName(
    QString name
    )
{
    constexpr qsizetype MaximumUtf8Bytes = 240;

    while (name.toUtf8().size() > MaximumUtf8Bytes)
    {
        name.chop(1);
    }

    return name;
}
}

inline QString filesystemSafeJsonFileName(
    QString suggestedBaseName,
    QString fallbackBaseName
    )
{
    constexpr auto JsonSuffix = ".json";

    while (suggestedBaseName.endsWith(
        QString::fromLatin1(JsonSuffix), Qt::CaseInsensitive))
    {
        suggestedBaseName.chop(5);
    }

    QString baseName = Detail::sanitizedBaseName(suggestedBaseName);

    if (baseName.isEmpty())
    {
        baseName = Detail::sanitizedBaseName(fallbackBaseName);
    }

    if (baseName.isEmpty())
    {
        baseName = QStringLiteral("Class");
    }

    if (Detail::isWindowsReservedName(baseName))
    {
        baseName.prepend(u'_');
    }

    return Detail::limitedBaseName(baseName)
        + QString::fromLatin1(JsonSuffix);
}
}
