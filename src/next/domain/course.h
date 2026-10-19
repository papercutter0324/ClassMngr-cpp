#pragma once

#include "next/domain/course_catalog_identity.h"
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
                return Course(entry);
            }
        }
        return std::nullopt;
    }

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
        const CatalogEntry* entry = catalogEntryForIdentity(m_catalogIdentity);
        return entry == nullptr ? std::string_view{} : entry->grade;
    }

    [[nodiscard]] std::string_view level() const noexcept
    {
        const CatalogEntry* entry = catalogEntryForIdentity(m_catalogIdentity);
        return entry == nullptr ? std::string_view{} : entry->level;
    }

    [[nodiscard]] CourseCatalogIdentity catalogIdentity() const noexcept
    {
        return m_catalogIdentity;
    }

    [[nodiscard]] std::optional<WeeklyMeetingDayRule>
    weeklyMeetingDayRule() const
    {
        return weeklyMeetingDayRuleFor(
            gradeBandForName(grade()),
            levelCategoryForName(level())
            );
    }

    [[nodiscard]] static std::optional<WeeklyMeetingDayRule>
    weeklyMeetingDayRuleFor(
        CourseGradeBand grade,
        CourseLevelCategory level
        )
    {
        switch (grade)
        {
        case CourseGradeBand::E4:
        case CourseGradeBand::E5:
        case CourseGradeBand::E6:
        case CourseGradeBand::M1:
        case CourseGradeBand::M2:
        case CourseGradeBand::M3:
        case CourseGradeBand::Other:
            break;
        default:
            return std::nullopt;
        }

        switch (level)
        {
        case CourseLevelCategory::Standard:
        case CourseLevelCategory::Athena:
        case CourseLevelCategory::Songs:
            break;
        default:
            return std::nullopt;
        }

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
        CourseCatalogIdentity identity;
        std::string_view grade;
        std::string_view level;
    };

    inline static constexpr std::array<CatalogEntry, 25> catalog{{
        {CourseCatalogIdentity::E4Theseus, "E4", "Theseus"},
        {CourseCatalogIdentity::E4Perseus, "E4", "Perseus"},
        {CourseCatalogIdentity::E4Odysseus, "E4", "Odysseus"},
        {CourseCatalogIdentity::E4Hercules, "E4", "Hercules"},
        {CourseCatalogIdentity::E5Artemis, "E5", "Artemis"},
        {CourseCatalogIdentity::E5Hermes, "E5", "Hermes"},
        {CourseCatalogIdentity::E5Apollo, "E5", "Apollo"},
        {CourseCatalogIdentity::E5Zeus, "E5", "Zeus"},
        {CourseCatalogIdentity::E5Athena, "E5", "Athena"},
        {CourseCatalogIdentity::E6Helios, "E6", "Helios"},
        {CourseCatalogIdentity::E6Poseidon, "E6", "Poseidon"},
        {CourseCatalogIdentity::E6Gaia, "E6", "Gaia"},
        {CourseCatalogIdentity::E6Hera, "E6", "Hera"},
        {CourseCatalogIdentity::E6Songs, "E6", "Song's"},
        {CourseCatalogIdentity::M1Elephantus, "M1", "Elephantus"},
        {CourseCatalogIdentity::M1Galaxia, "M1", "Galaxia"},
        {CourseCatalogIdentity::M1Solis, "M1", "Solis"},
        {CourseCatalogIdentity::M1Major, "M1", "Major"},
        {CourseCatalogIdentity::M1Songs, "M1", "Song's"},
        {CourseCatalogIdentity::M2Ursa, "M2", "Ursa"},
        {CourseCatalogIdentity::M2Leo, "M2", "Leo"},
        {CourseCatalogIdentity::M2Tigris, "M2", "Tigris"},
        {CourseCatalogIdentity::M2Major, "M2", "Major"},
        {CourseCatalogIdentity::M2Songs, "M2", "Song's"},
        {CourseCatalogIdentity::M3Songs, "M3", "Song's"}
    }};

    [[nodiscard]] static const CatalogEntry* catalogEntryForIdentity(
        CourseCatalogIdentity identity
        ) noexcept
    {
        const auto entry = std::find_if(
            catalog.begin(),
            catalog.end(),
            [identity](const CatalogEntry& candidate)
            {
                return candidate.identity == identity;
            }
            );
        return entry == catalog.end() ? nullptr : &*entry;
    }

    explicit Course(const CatalogEntry& entry) noexcept
        : m_catalogIdentity(entry.identity)
    {
    }

    CourseCatalogIdentity m_catalogIdentity;
};

} // namespace ClassMngr::Next::Domain
