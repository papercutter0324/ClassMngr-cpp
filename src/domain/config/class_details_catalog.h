#pragma once

#include <array>
#include <string_view>
#include <vector>

namespace ClassMngr::Domain
{

struct ClassGradeLevel final
{
    std::string_view grade;
    std::string_view level;
};

// Shared class setup choices. This catalog is the common source for both the
// legacy class configuration and Next's course and setup flows.
class ClassDetailsCatalog final
{
public:
    inline static constexpr std::array<ClassGradeLevel, 25> gradeLevels{{
        {"E4", "Theseus"},
        {"E4", "Perseus"},
        {"E4", "Odysseus"},
        {"E4", "Hercules"},
        {"E5", "Artemis"},
        {"E5", "Hermes"},
        {"E5", "Apollo"},
        {"E5", "Zeus"},
        {"E5", "Athena"},
        {"E6", "Helios"},
        {"E6", "Poseidon"},
        {"E6", "Gaia"},
        {"E6", "Hera"},
        {"E6", "Song's"},
        {"M1", "Elephantus"},
        {"M1", "Galaxia"},
        {"M1", "Solis"},
        {"M1", "Major"},
        {"M1", "Song's"},
        {"M2", "Ursa"},
        {"M2", "Leo"},
        {"M2", "Tigris"},
        {"M2", "Major"},
        {"M2", "Song's"},
        {"M3", "Song's"}
    }};

    [[nodiscard]] static std::vector<std::string_view> grades()
    {
        std::vector<std::string_view> result;
        for (const ClassGradeLevel& entry : gradeLevels)
        {
            if (result.empty() || result.back() != entry.grade)
            {
                result.push_back(entry.grade);
            }
        }
        return result;
    }

    [[nodiscard]] static std::vector<std::string_view> levelsForGrade(
        const std::string_view grade
        )
    {
        std::vector<std::string_view> result;
        for (const ClassGradeLevel& entry : gradeLevels)
        {
            if (entry.grade == grade)
            {
                result.push_back(entry.level);
            }
        }
        return result;
    }

    [[nodiscard]] static std::vector<std::string_view> readingBooks(
        const std::string_view grade,
        const std::string_view level
        )
    {
        if (grade == "M1"
            && (level == "Elephantus" || level == "Galaxia"))
        {
            return {"", "GraVoca 1A", "GraVoca 1B", "GraVoca 1C"};
        }
        if (grade == "M2" && (level == "Ursa" || level == "Leo"))
        {
            return {"", "GraVoca 2A", "GraVoca 2B", "GraVoca 2C"};
        }

        if (grade == "E4")
        {
            if (level == "Theseus")
            {
                return {"", "Reading Explorer 1", "Reading Explorer 2"};
            }
            if (level == "Perseus")
            {
                return {"", "Reading Explorer 2", "Reading Explorer 3"};
            }
            if (level == "Odysseus")
            {
                return {"", "Reading Explorer 3", "Reading Explorer 4"};
            }
            if (level == "Hercules")
            {
                return {"", "Reading Explorer 4", "Reading Explorer 5"};
            }
        }
        else if (grade == "E5")
        {
            if (level == "Artemis")
            {
                return {"", "Reading Explorer 2", "Reading Explorer 3"};
            }
            if (level == "Hermes")
            {
                return {"", "Reading Explorer 3", "Reading Explorer 4"};
            }
            if (level == "Apollo")
            {
                return {"", "Reading Explorer 4", "Reading Explorer 5"};
            }
            if (level == "Zeus" || level == "Athena")
            {
                return {"", "Reading Explorer 5"};
            }
        }
        else if (grade == "E6")
        {
            if (level == "Helios")
            {
                return {"", "Reading Explorer 3", "Reading Explorer 4"};
            }
            if (level == "Poseidon")
            {
                return {"", "Reading Explorer 4", "Reading Explorer 5"};
            }
            if (level == "Gaia" || level == "Hera" || level == "Song's")
            {
                return {"", "Reading Explorer 5"};
            }
        }

        return readingSmartBooks();
    }

    [[nodiscard]] static std::vector<std::string_view> essayBooks(
        const std::string_view grade,
        const std::string_view level
        )
    {
        if (grade == "E4")
        {
            if (level == "Theseus" || level == "Perseus")
            {
                return {"", "4A", "4B", "4C", "4D"};
            }
            if (level == "Odysseus" || level == "Hercules")
            {
                return {"", "4E", "4F", "4G", "4H"};
            }
        }
        else if (grade == "E5")
        {
            if (level == "Artemis" || level == "Hermes")
            {
                return {"", "5A", "5B", "5C", "5D"};
            }
            if (level == "Apollo" || level == "Zeus")
            {
                return {"", "5E", "5F", "5G", "5H"};
            }
            if (level == "Athena")
            {
                return {"", "N/A"};
            }
        }
        else if (grade == "E6")
        {
            if (level == "Helios" || level == "Poseidon")
            {
                return {"", "6A", "6B", "6C", "6D"};
            }
            if (level == "Gaia" || level == "Hera")
            {
                return {"", "6E", "6F", "6G", "6H"};
            }
            if (level == "Song's")
            {
                return {"", "N/A"};
            }
        }

        return {"N/A"};
    }

private:
    [[nodiscard]] static std::vector<std::string_view> readingSmartBooks()
    {
        return {
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
    }
};

} // namespace ClassMngr::Domain
