#include "class_info_config.h"

#include "classmngr/engine/class_info_config.h"

#include <string>

namespace
{
QStringList toQtStringList(
    const classmngr::engine::ClassInfoConfig::StringList& values
    )
{
    QStringList result;

    for (const std::string& value : values)
    {
        result.append(QString::fromStdString(value));
    }

    return result;
}
} // namespace

namespace ClassInfoConfig
{

const QStringList Grades =
    toQtStringList(classmngr::engine::ClassInfoConfig::grades());
const QStringList Days =
    toQtStringList(classmngr::engine::ClassInfoConfig::days());
const QStringList RegularHours =
    toQtStringList(classmngr::engine::ClassInfoConfig::regularHours());
const QStringList IntensiveHours =
    toQtStringList(classmngr::engine::ClassInfoConfig::intensiveHours());
const QStringList StartMinutes =
    toQtStringList(classmngr::engine::ClassInfoConfig::startMinutes());
const QStringList EndMinutes =
    toQtStringList(classmngr::engine::ClassInfoConfig::endMinutes());

QStringList levelsForGrade(const QString& grade)
{
    return toQtStringList(
        classmngr::engine::ClassInfoConfig::levelsForGrade(grade.toStdString())
        );
}

QStringList readingBooks(const QString& grade, const QString& level)
{
    return toQtStringList(
        classmngr::engine::ClassInfoConfig::readingBooks(
            grade.toStdString(),
            level.toStdString()
            )
        );
}

QStringList essayBooks(const QString& grade, const QString& level)
{
    return toQtStringList(
        classmngr::engine::ClassInfoConfig::essayBooks(
            grade.toStdString(),
            level.toStdString()
            )
        );
}

} // namespace ClassInfoConfig
