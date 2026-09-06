#include "classmngr/engine/class_info_config.h"

#include <map>
#include <utility>

namespace classmngr::engine::ClassInfoConfig
{
namespace
{
using BookMap = std::map<std::string, StringList>;
using GradeBookMap = std::map<std::string, BookMap>;

const std::map<std::string, StringList>& levelMap()
{
    static const std::map<std::string, StringList> values{
        {"E4", {"Theseus", "Perseus", "Odysseus", "Hercules"}},
        {"E5", {"Artemis", "Hermes", "Apollo", "Zeus", "Athena"}},
        {"E6", {"Helios", "Poseidon", "Gaia", "Hera", "Song's"}},
        {"M1", {"Elephantus", "Galaxia", "Solis", "Major", "Song's"}},
        {"M2", {"Ursa", "Leo", "Tigris", "Major", "Song's"}},
        {"M3", {"Song's"}}
    };
    return values;
}

const GradeBookMap& readingMap()
{
    static const GradeBookMap values{
        {
            "E4",
            {
                {"Theseus", {"", "Reading Explorer 1", "Reading Explorer 2"}},
                {"Perseus", {"", "Reading Explorer 2", "Reading Explorer 3"}},
                {"Odysseus", {"", "Reading Explorer 3", "Reading Explorer 4"}},
                {"Hercules", {"", "Reading Explorer 4", "Reading Explorer 5"}}
            }
        },
        {
            "E5",
            {
                {"Artemis", {"", "Reading Explorer 2", "Reading Explorer 3"}},
                {"Hermes", {"", "Reading Explorer 3", "Reading Explorer 4"}},
                {"Apollo", {"", "Reading Explorer 4", "Reading Explorer 5"}},
                {"Zeus", {"", "Reading Explorer 5"}},
                {"Athena", {"", "Reading Explorer 5"}}
            }
        },
        {
            "E6",
            {
                {"Helios", {"", "Reading Explorer 3", "Reading Explorer 4"}},
                {"Poseidon", {"", "Reading Explorer 4", "Reading Explorer 5"}},
                {"Gaia", {"", "Reading Explorer 5"}},
                {"Hera", {"", "Reading Explorer 5"}},
                {"Song's", {"", "Reading Explorer 5"}}
            }
        }
    };
    return values;
}

const GradeBookMap& essayMap()
{
    static const GradeBookMap values{
        {
            "E4",
            {
                {"Theseus", {"", "4A", "4B", "4C", "4D"}},
                {"Perseus", {"", "4A", "4B", "4C", "4D"}},
                {"Odysseus", {"", "4E", "4F", "4G", "4H"}},
                {"Hercules", {"", "4E", "4F", "4G", "4H"}}
            }
        },
        {
            "E5",
            {
                {"Artemis", {"", "5A", "5B", "5C", "5D"}},
                {"Hermes", {"", "5A", "5B", "5C", "5D"}},
                {"Apollo", {"", "5E", "5F", "5G", "5H"}},
                {"Zeus", {"", "5E", "5F", "5G", "5H"}},
                {"Athena", {"", "N/A"}}
            }
        },
        {
            "E6",
            {
                {"Helios", {"", "6A", "6B", "6C", "6D"}},
                {"Poseidon", {"", "6A", "6B", "6C", "6D"}},
                {"Gaia", {"", "6E", "6F", "6G", "6H"}},
                {"Hera", {"", "6E", "6F", "6G", "6H"}},
                {"Song's", {"", "N/A"}}
            }
        }
    };
    return values;
}

const StringList& m1GraVocaLevels()
{
    static const StringList values{"Elephantus", "Galaxia"};
    return values;
}

const StringList& m2GraVocaLevels()
{
    static const StringList values{"Ursa", "Leo"};
    return values;
}

const StringList& m1GraVocaBooks()
{
    static const StringList values{"", "GraVoca 1A", "GraVoca 1B", "GraVoca 1C"};
    return values;
}

const StringList& m2GraVocaBooks()
{
    static const StringList values{"", "GraVoca 2A", "GraVoca 2B", "GraVoca 2C"};
    return values;
}

const StringList& readingSmartBooks()
{
    static const StringList values{
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
    return values;
}

const StringList& middleSchoolEssayBooks()
{
    static const StringList values{"N/A"};
    return values;
}

bool contains(
    const StringList& values,
    std::string_view value
    )
{
    for (const std::string& candidate : values)
    {
        if (candidate == value)
        {
            return true;
        }
    }
    return false;
}
} // namespace

const StringList& grades() noexcept
{
    static const StringList values{"E4", "E5", "E6", "M1", "M2", "M3"};
    return values;
}

const StringList& days() noexcept
{
    static const StringList values{
        "Monday", "Tuesday", "Wednesday", "Thursday",
        "Friday", "Saturday", "Sunday"
    };
    return values;
}

const StringList& regularHours() noexcept
{
    static const StringList values{"3", "4", "5", "6", "7", "8", "9"};
    return values;
}

const StringList& intensiveHours() noexcept
{
    static const StringList values{
        "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12"
    };
    return values;
}

const StringList& startMinutes() noexcept
{
    static const StringList values{":00", ":05", ":25", ":30", ":35"};
    return values;
}

const StringList& endMinutes() noexcept
{
    static const StringList values{":55", ":50", ":35", ":30", ":25"};
    return values;
}

StringList levelsForGrade(std::string_view grade)
{
    const auto found = levelMap().find(std::string(grade));
    return found == levelMap().end() ? StringList{} : found->second;
}

StringList readingBooks(
    std::string_view grade,
    std::string_view level
    )
{
    if (grade == "M1" && contains(m1GraVocaLevels(), level))
    {
        return m1GraVocaBooks();
    }
    if (grade == "M2" && contains(m2GraVocaLevels(), level))
    {
        return m2GraVocaBooks();
    }

    const auto gradeFound = readingMap().find(std::string(grade));
    if (gradeFound == readingMap().end())
    {
        return readingSmartBooks();
    }

    const auto levelFound = gradeFound->second.find(std::string(level));
    return levelFound == gradeFound->second.end()
        ? readingSmartBooks()
        : levelFound->second;
}

StringList essayBooks(
    std::string_view grade,
    std::string_view level
    )
{
    const auto gradeFound = essayMap().find(std::string(grade));
    if (gradeFound == essayMap().end())
    {
        return middleSchoolEssayBooks();
    }

    const auto levelFound = gradeFound->second.find(std::string(level));
    return levelFound == gradeFound->second.end()
        ? middleSchoolEssayBooks()
        : levelFound->second;
}

} // namespace classmngr::engine::ClassInfoConfig
