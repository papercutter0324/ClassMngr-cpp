#pragma once

#include "next/domain/schedule_time.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Domain
{

enum class CourseGradeBand
{
    E4,
    E5,
    E6,
    M1,
    M2,
    M3,
    Other
};

enum class CourseLevelCategory
{
    Standard,
    Athena,
    Songs
};

class Course final
{
public:
    using WeeklyMeetingDayPattern = std::vector<Weekday>;

    enum class WeeklyMeetingDayRuleKind
    {
        PairedWeekdays,
        ThreeDayOrTuesdayThursday,
        SingleWeekday
    };

    class WeeklyMeetingDayRule final
    {
    public:
        [[nodiscard]] WeeklyMeetingDayRuleKind kind() const noexcept
        {
            return m_kind;
        }

        [[nodiscard]] const std::vector<WeeklyMeetingDayPattern>&
        allowedPatterns() const noexcept
        {
            return m_allowedPatterns;
        }

        [[nodiscard]] bool allows(
            const WeeklyMeetingDayPattern& days
            ) const
        {
            WeeklyMeetingDayPattern sortedDays = days;
            for (auto current = sortedDays.begin();
                 current != sortedDays.end();
                 ++current)
            {
                const Weekday day = *current;
                if (
                    day < Weekday::Monday
                    || day > Weekday::Friday
                    || std::find(
                        sortedDays.begin(),
                        current,
                        day
                        ) != current
                    )
                {
                    return false;
                }
            }
            std::sort(sortedDays.begin(), sortedDays.end());
            return std::find(
                m_allowedPatterns.begin(),
                m_allowedPatterns.end(),
                sortedDays
                ) != m_allowedPatterns.end();
        }

    private:
        friend class Course;

        WeeklyMeetingDayRule(
            WeeklyMeetingDayRuleKind kind,
            std::vector<WeeklyMeetingDayPattern> allowedPatterns
            )
            : m_kind(kind)
            , m_allowedPatterns(std::move(allowedPatterns))
        {
        }

        WeeklyMeetingDayRuleKind m_kind;
        std::vector<WeeklyMeetingDayPattern> m_allowedPatterns;
    };

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

    [[nodiscard]] std::optional<WeeklyMeetingDayRule>
    weeklyMeetingDayRule() const
    {
        return weeklyMeetingDayRuleFor(
            gradeBandForName(m_grade),
            levelCategoryForName(m_level)
            );
    }

    [[nodiscard]] static std::optional<WeeklyMeetingDayRule>
    weeklyMeetingDayRuleFor(
        CourseGradeBand grade,
        CourseLevelCategory level
        )
    {
        using Pattern = WeeklyMeetingDayPattern;
        using RuleKind = WeeklyMeetingDayRuleKind;
        const Pattern mondayWednesday{
            Weekday::Monday,
            Weekday::Wednesday
        };
        const Pattern mondayFriday{
            Weekday::Monday,
            Weekday::Friday
        };
        const Pattern wednesdayFriday{
            Weekday::Wednesday,
            Weekday::Friday
        };
        const Pattern tuesdayThursday{
            Weekday::Tuesday,
            Weekday::Thursday
        };
        const Pattern mondayWednesdayFriday{
            Weekday::Monday,
            Weekday::Wednesday,
            Weekday::Friday
        };

        if (
            grade == CourseGradeBand::E4
            || (
                grade == CourseGradeBand::E5
                && level != CourseLevelCategory::Athena
                )
            || (
                (
                    grade == CourseGradeBand::M1
                    || grade == CourseGradeBand::M2
                    || grade == CourseGradeBand::M3
                    )
                && level == CourseLevelCategory::Songs
                )
            )
        {
            return WeeklyMeetingDayRule(
                RuleKind::PairedWeekdays,
                {
                    mondayWednesday,
                    mondayFriday,
                    wednesdayFriday,
                    tuesdayThursday
                }
                );
        }
        if (
            (
                grade == CourseGradeBand::E5
                && level == CourseLevelCategory::Athena
                )
            || (
                grade == CourseGradeBand::E6
                && level == CourseLevelCategory::Songs
                )
            )
        {
            return WeeklyMeetingDayRule(
                RuleKind::ThreeDayOrTuesdayThursday,
                {
                    mondayWednesdayFriday,
                    tuesdayThursday
                }
                );
        }
        if (
            grade == CourseGradeBand::E6
            || grade == CourseGradeBand::M1
            || grade == CourseGradeBand::M2
        )
        {
            return WeeklyMeetingDayRule(
                RuleKind::SingleWeekday,
                {
                    {Weekday::Monday},
                    {Weekday::Tuesday},
                    {Weekday::Wednesday},
                    {Weekday::Thursday},
                    {Weekday::Friday}
                }
                );
        }
        return std::nullopt;
    }

    friend bool operator==(
        const Course&,
        const Course&
        ) = default;

private:
    [[nodiscard]] static CourseGradeBand gradeBandForName(
        std::string_view grade
        ) noexcept
    {
        if (grade == "E4")
        {
            return CourseGradeBand::E4;
        }
        if (grade == "E5")
        {
            return CourseGradeBand::E5;
        }
        if (grade == "E6")
        {
            return CourseGradeBand::E6;
        }
        if (grade == "M1")
        {
            return CourseGradeBand::M1;
        }
        if (grade == "M2")
        {
            return CourseGradeBand::M2;
        }
        if (grade == "M3")
        {
            return CourseGradeBand::M3;
        }
        return CourseGradeBand::Other;
    }

    [[nodiscard]] static CourseLevelCategory levelCategoryForName(
        std::string_view level
        ) noexcept
    {
        if (level == "Athena")
        {
            return CourseLevelCategory::Athena;
        }
        if (level == "Song's")
        {
            return CourseLevelCategory::Songs;
        }
        return CourseLevelCategory::Standard;
    }

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
