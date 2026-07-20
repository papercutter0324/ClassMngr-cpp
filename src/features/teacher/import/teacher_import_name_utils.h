#pragma once

#include <QChar>
#include <QString>

namespace TeacherImportNameUtils
{
inline bool isHangul(QChar character)
{
    const ushort code = character.unicode();
    return (code >= 0x1100 && code <= 0x11ff)
        || (code >= 0x3130 && code <= 0x318f)
        || (code >= 0xa960 && code <= 0xa97f)
        || (code >= 0xac00 && code <= 0xd7af)
        || (code >= 0xd7b0 && code <= 0xd7ff);
}

inline QString hangulOnly(const QString& value)
{
    QString result;
    result.reserve(value.size());
    for (const QChar character : value)
    {
        if (isHangul(character))
        {
            result.append(character);
        }
    }
    return result;
}
}
