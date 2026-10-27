#include "class_info_config.h"

#include "domain/config/class_details_catalog.h"

#include <string_view>

namespace ClassInfoConfig
{

namespace
{
QString fromDomainName(const std::string_view value)
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

QStringList fromDomainNames(const std::vector<std::string_view>& values)
{
    QStringList result;
    result.reserve(static_cast<qsizetype>(values.size()));
    for (const std::string_view value : values)
    {
        result.append(fromDomainName(value));
    }
    return result;
}
}

const QStringList Grades = fromDomainNames(
    ClassMngr::Domain::ClassDetailsCatalog::grades()
    );

const QStringList Days{
    "Monday", "Tuesday", "Wednesday", "Thursday",
    "Friday", "Saturday", "Sunday"
};

const QStringList RegularHours{
    "3", "4", "5", "6", "7", "8", "9"
};

const QStringList IntensiveHours{
    "1", "2", "3", "4", "5", "6",
    "7", "8", "9", "10", "11", "12"
};

const QStringList StartMinutes{
    ":00", ":05", ":25", ":30", ":35"
};

const QStringList EndMinutes{
    ":55", ":50", ":35", ":30", ":25"
};

QStringList levelsForGrade(const QString& grade)
{
    return fromDomainNames(
        ClassMngr::Domain::ClassDetailsCatalog::levelsForGrade(
            grade.toStdString()
            )
        );
}

QStringList readingBooks(const QString& grade, const QString& level)
{
    return fromDomainNames(
        ClassMngr::Domain::ClassDetailsCatalog::readingBooks(
            grade.toStdString(),
            level.toStdString()
            )
        );
}

QStringList essayBooks(const QString& grade, const QString& level)
{
    return fromDomainNames(
        ClassMngr::Domain::ClassDetailsCatalog::essayBooks(
            grade.toStdString(),
            level.toStdString()
            )
        );
}

} // namespace ClassInfoConfig
