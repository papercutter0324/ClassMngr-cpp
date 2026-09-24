#pragma once

#include <array>
#include <optional>
#include <string_view>
#include <vector>

namespace ClassMngr::Next::Domain
{

class Course final
{
public:
    [[nodiscard]] static std::optional<Course> fromNames(
        std::string_view grade,
        std::string_view level
        )
    {
        for (const CatalogEntry& entry : catalog)
        {
            if (entry.grade == grade && entry.level == level)
            {
                return Course(entry.grade, entry.level);
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] static std::vector<std::string_view> grades()
    {
        std::vector<std::string_view> result;
        for (const CatalogEntry& entry : catalog)
        {
            if (result.empty() || result.back() != entry.grade)
            {
                result.push_back(entry.grade);
            }
        }
        return result;
    }

    [[nodiscard]] static std::vector<std::string_view> levelsForGrade(
        std::string_view grade
        )
    {
        std::vector<std::string_view> result;
        for (const CatalogEntry& entry : catalog)
        {
            if (entry.grade == grade)
            {
                result.push_back(entry.level);
            }
        }
        return result;
    }

    [[nodiscard]] std::string_view grade() const noexcept
    {
        return m_grade;
    }

    [[nodiscard]] std::string_view level() const noexcept
    {
        return m_level;
    }

    friend bool operator==(
        const Course&,
        const Course&
        ) = default;

private:
    struct CatalogEntry final
    {
        std::string_view grade;
        std::string_view level;
    };

    inline static constexpr std::array<CatalogEntry, 25> catalog{{
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

    Course(
        std::string_view grade,
        std::string_view level
        ) noexcept
        : m_grade(grade)
        , m_level(level)
    {
    }

    std::string_view m_grade;
    std::string_view m_level;
};

} // namespace ClassMngr::Next::Domain
