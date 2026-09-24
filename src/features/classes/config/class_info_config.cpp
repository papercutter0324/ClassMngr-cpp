#include "class_info_config.h"

#include "next/domain/course.h"

#include <string_view>

namespace ClassInfoConfig
{

namespace
{
QString fromDomainName(std::string_view value)
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}
}

// =====================================================
// Static UI Lists
// =====================================================

const QStringList Grades = []()
{
    QStringList grades;
    for (const std::string_view grade :
         ClassMngr::Next::Domain::Course::grades())
    {
        grades.append(fromDomainName(grade));
    }
    return grades;
}();

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
    ":00",
    ":05",
    ":25",
    ":30",
    ":35"
};

const QStringList EndMinutes{
    ":55",
    ":50",
    ":35",
    ":30",
    ":25"
};

// =====================================================
// Grade → Levels (NO IF CHAINS)
// =====================================================

// =====================================================
// Reading Books Map
// =====================================================

static const QHash<QString, QHash<QString, QStringList>> READING_MAP = {
    { "E4", {
               { "Theseus",   { "", "Reading Explorer 1", "Reading Explorer 2" } },
               { "Perseus",   { "", "Reading Explorer 2", "Reading Explorer 3" } },
               { "Odysseus",  { "", "Reading Explorer 3", "Reading Explorer 4" } },
               { "Hercules",  { "", "Reading Explorer 4", "Reading Explorer 5" } }
           }},

    { "E5", {
               { "Artemis", { "", "Reading Explorer 2", "Reading Explorer 3" } },
               { "Hermes",  { "", "Reading Explorer 3", "Reading Explorer 4" } },
               { "Apollo",  { "", "Reading Explorer 4", "Reading Explorer 5" } },
               { "Zeus",    { "", "Reading Explorer 5" } },
               { "Athena",  { "", "Reading Explorer 5" } }
           }},

    { "E6", {
               { "Helios",   { "", "Reading Explorer 3", "Reading Explorer 4" } },
               { "Poseidon", { "", "Reading Explorer 4", "Reading Explorer 5" } },
               { "Gaia",     { "", "Reading Explorer 5" } },
               { "Hera",     { "", "Reading Explorer 5" } },
               { "Song's",   { "", "Reading Explorer 5" } }
           }}
};

// =====================================================
// Middle School Rules (clean grouping)
// =====================================================

static const QStringList M1_GRAVOCA_LEVELS = { "Elephantus", "Galaxia" };
static const QStringList M2_GRAVOCA_LEVELS = { "Ursa", "Leo" };

static const QStringList M1_GRAVOCA_BOOKS = {
    "", "GraVoca 1A", "GraVoca 1B", "GraVoca 1C"
};

static const QStringList M2_GRAVOCA_BOOKS = {
    "", "GraVoca 2A", "GraVoca 2B", "GraVoca 2C"
};

static const QStringList READING_SMART_BOOKS = {
    "",
    "Reading Smart Step 1 Book 1",
    "Reading Smart Step 1 Book 2",
    "Reading Smart Step 1 Book 3",
    "Reading Smart Step 1 Book 4",
    "Reading Smart Step 1 Book 5",
    "Reading Smart Step 1 Book 6",
    "Reading Smart Step 1 Book 7",
    "Reading Smart Step 2 Book 1",
    "Reading Smart Step 2 Book 2",
    "Reading Smart Step 2 Book 3",
    "Reading Smart Step 2 Book 4",
    "Reading Smart Step 2 Book 5",
    "Reading Smart Step 2 Book 6",
    "Reading Smart Step 2 Book 7"
};

static const QStringList MS_ESSAY_BOOKS = { "N/A" };

// =====================================================
// Essay Map
// =====================================================

static const QHash<QString, QHash<QString, QStringList>> ESSAY_MAP = {
    { "E4", {
               { "Theseus",  { "", "4A","4B","4C","4D" } },
               { "Perseus",  { "", "4A","4B","4C","4D" } },
               { "Odysseus", { "", "4E","4F","4G","4H" } },
               { "Hercules", { "", "4E","4F","4G","4H" } }
           }},

    { "E5", {
               { "Artemis", { "", "5A","5B","5C","5D" } },
               { "Hermes",  { "", "5A","5B","5C","5D" } },
               { "Apollo",  { "", "5E","5F","5G","5H" } },
               { "Zeus",    { "", "5E","5F","5G","5H" } },
               { "Athena",  { "", "N/A" } }
           }},

    { "E6", {
               { "Helios",   { "", "6A","6B","6C","6D" } },
               { "Poseidon", { "", "6A","6B","6C","6D" } },
               { "Gaia",     { "", "6E","6F","6G","6H" } },
               { "Hera",     { "", "6E","6F","6G","6H" } },
               { "Song's",   { "", "N/A" } }
           }}
};

// =====================================================
// API IMPLEMENTATION (CLEAN & FAST)
// =====================================================

QStringList levelsForGrade(const QString& grade)
{
    QStringList levels;
    for (const std::string_view level :
         ClassMngr::Next::Domain::Course::levelsForGrade(
             grade.toStdString()
             ))
    {
        levels.append(fromDomainName(level));
    }
    return levels;
}

QStringList readingBooks(const QString& grade, const QString& level)
{
    if (grade == "M1" && M1_GRAVOCA_LEVELS.contains(level))
        return M1_GRAVOCA_BOOKS;

    if (grade == "M2" && M2_GRAVOCA_LEVELS.contains(level))
        return M2_GRAVOCA_BOOKS;

    auto gradeMap = READING_MAP.value(grade);
    if (gradeMap.isEmpty())
        return READING_SMART_BOOKS;

    return gradeMap.value(level, READING_SMART_BOOKS);
}

QStringList essayBooks(const QString& grade, const QString& level)
{
    auto gradeMap = ESSAY_MAP.value(grade);
    if (gradeMap.isEmpty())
        return MS_ESSAY_BOOKS;

    return gradeMap.value(level, MS_ESSAY_BOOKS);
}

} // namespace
