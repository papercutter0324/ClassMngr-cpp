#pragma once

#include "next/domain/korean_teacher_key.h"

#include <QChar>
#include <QString>

#include <cstddef>
#include <string>

namespace TeacherImportNameUtils
{
inline bool isHangul(QChar character)
{
    return ClassMngr::Next::Domain::KoreanTeacherKey::isHangulCodeUnit(
        static_cast<char16_t>(character.unicode())
        );
}

inline QString hangulOnly(const QString& value)
{
    std::u16string codeUnits;
    codeUnits.reserve(static_cast<std::size_t>(value.size()));
    for (const QChar character : value)
    {
        codeUnits.push_back(static_cast<char16_t>(character.unicode()));
    }

    const auto key =
        ClassMngr::Next::Domain::KoreanTeacherKey::fromName(codeUnits);
    QString result;
    result.reserve(static_cast<qsizetype>(key.value().size()));
    for (const char16_t codeUnit : key.value())
    {
        result.append(QChar(static_cast<ushort>(codeUnit)));
    }
    return result;
}
}
