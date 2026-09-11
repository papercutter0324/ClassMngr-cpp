#pragma once

#include "classmngr/engine/testing_class.h"

#include <QString>
#include <QStringList>

#include <cstddef>
#include <string>
#include <string_view>

namespace testing_class_detail
{
inline std::string toUtf8(const QString& value)
{
    return value.toUtf8().toStdString();
}

inline QString fromUtf8(std::string_view value)
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

inline QStringList fromEngineStrings(
    const std::vector<std::string>& values
    )
{
    QStringList result;
    result.reserve(static_cast<qsizetype>(values.size()));
    for (const std::string& value : values)
    {
        result.append(fromUtf8(value));
    }
    return result;
}
} // namespace testing_class_detail

struct TestingClass
{
    int classId{-1};
    QString name;
    QString grade;
    QString level;
    QString room;
    int teacherId{-1};
    QString classColor{"#FFFFFF"};
    QString fontColor{"#000000"};
    QString notes;
};

inline QStringList testingClassMixedLevels()
{
    return testing_class_detail::fromEngineStrings(
        classmngr::engine::testingClassMixedLevels()
        );
}

inline QStringList testingClassGrades()
{
    return testing_class_detail::fromEngineStrings(
        classmngr::engine::testingClassGrades()
        );
}

inline QStringList testingClassLevelsForGrade(
    const QString& grade
    )
{
    return testing_class_detail::fromEngineStrings(
        classmngr::engine::testingClassLevelsForGrade(
            testing_class_detail::toUtf8(grade)
            )
        );
}
