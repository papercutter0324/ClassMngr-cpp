#pragma once

#include <QDate>
#include <QJsonObject>
#include <QMap>

#include <array>

enum class SchoolLevel
{
    Elementary,
    Middle
};

enum class AcademicTerm
{
    Winter,
    Spring,
    Summer,
    Fall
};

inline constexpr int AcademicTermCount = 4;

struct AcademicYearSchedule
{
    int termYear = 0;
    QDate winterStart;
    std::array<int, AcademicTermCount> weeks{};

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] QDate termStart(AcademicTerm term) const;
    [[nodiscard]] QDate endDate() const;
};

struct AcademicTermPosition
{
    bool valid = false;
    int termYear = 0;
    AcademicTerm term = AcademicTerm::Winter;
    int week = 0;
    QDate weekStart;
};

class AcademicCalendarSchedule
{
public:
    static constexpr int FirstTermYear = 2026;

    [[nodiscard]] static QDate initialWinterStart();
    [[nodiscard]] static std::array<int, AcademicTermCount> defaultWeeks(
        SchoolLevel level
        );

    [[nodiscard]] AcademicYearSchedule yearSchedule(
        SchoolLevel level,
        int termYear
        ) const;
    [[nodiscard]] AcademicYearSchedule defaultYearSchedule(
        SchoolLevel level,
        int termYear
        ) const;
    [[nodiscard]] AcademicTermPosition termAt(
        SchoolLevel level,
        const QDate& date
        ) const;

    [[nodiscard]] bool hasCustomYearAfter(int termYear) const;
    [[nodiscard]] bool hasSavedSchedules() const;
    void setYearSchedules(
        int termYear,
        const AcademicYearSchedule& elementary,
        const AcademicYearSchedule& middle
        );

    void clear();
    [[nodiscard]] QJsonObject toJson() const;
    [[nodiscard]] bool fromJson(const QJsonObject& root);

private:
    using ScheduleMap = QMap<int, AcademicYearSchedule>;

    [[nodiscard]] const ScheduleMap& schedules(SchoolLevel level) const;
    [[nodiscard]] ScheduleMap& schedules(SchoolLevel level);

    ScheduleMap m_elementarySchedules;
    ScheduleMap m_middleSchedules;
};
