#include "class_tab_navigation_model.h"

#include "features/classes/config/class_info_config.h"

#include <algorithm>
#include <utility>

#include <QObject>
#include <QTime>

namespace
{
constexpr int UnknownOrder = 1000;

struct TabCandidate
{
    ClassTabNavigation::ClassTab tab;
    QString baseLabel;
    QString teacherLabel;
};

QString trimmedOr(
    const QString& value,
    const QString& fallback
    )
{
    const QString trimmed =
        value.trimmed();

    return trimmed.isEmpty()
        ? fallback
        : trimmed;
}

int gradeOrder(
    const QString& grade
    )
{
    const int index =
        ClassInfoConfig::Grades.indexOf(
            grade.trimmed()
            );

    return index >= 0
        ? index
        : UnknownOrder;
}

int levelOrder(
    const QString& grade,
    const QString& level
    )
{
    const int index =
        ClassInfoConfig::levelsForGrade(
            grade.trimmed()
            )
            .indexOf(
                level.trimmed()
                );

    return index >= 0
        ? index
        : UnknownOrder;
}

QString gradeKey(
    const ClassTabNavigation::ClassEntry& entry
    )
{
    const QString grade =
        entry.grade.trimmed();

    return gradeOrder(grade) == UnknownOrder
        ? QString()
        : grade;
}

QString gradeLabel(
    const QString& key
    )
{
    return key.trimmed().isEmpty()
        ? QObject::tr("Other")
        : key.trimmed();
}

QString compactStartTime(
    const QString& value
    )
{
    const QString trimmed =
        value.trimmed();

    if (trimmed.isEmpty())
    {
        return QString();
    }

    const QStringList formats{
        QStringLiteral("h:mm AP"),
        QStringLiteral("h:mmAP"),
        QStringLiteral("hh:mm AP"),
        QStringLiteral("hh:mmAP"),
        QStringLiteral("H:mm"),
        QStringLiteral("HH:mm"),
        QStringLiteral("H:mm:ss"),
        QStringLiteral("HH:mm:ss")
    };

    const bool usesMeridiem =
        trimmed.contains(
            QStringLiteral("AM"),
            Qt::CaseInsensitive
            )
        || trimmed.contains(
            QStringLiteral("PM"),
            Qt::CaseInsensitive
            );

    for (const QString& format : formats)
    {
        const QTime time =
            QTime::fromString(
                trimmed,
                format
                );

        if (time.isValid())
        {
            if (usesMeridiem)
            {
                QString formatted =
                    time.toString(
                        QStringLiteral("h:mm AP")
                        );

                formatted.remove(
                    QStringLiteral(" AM")
                    );
                formatted.remove(
                    QStringLiteral(" PM")
                    );

                return formatted;
            }

            return time.toString(
                QStringLiteral("H:mm")
                );
        }
    }

    QString fallback =
        trimmed;

    fallback.remove(
        QStringLiteral(" AM"),
        Qt::CaseInsensitive
        );
    fallback.remove(
        QStringLiteral(" PM"),
        Qt::CaseInsensitive
        );

    return fallback;
}

QString dayCode(
    const QString& day
    )
{
    if (day == QStringLiteral("Monday"))
    {
        return QStringLiteral("M");
    }
    if (day == QStringLiteral("Tuesday"))
    {
        return QStringLiteral("T");
    }
    if (day == QStringLiteral("Wednesday"))
    {
        return QStringLiteral("W");
    }
    if (day == QStringLiteral("Thursday"))
    {
        return QStringLiteral("Th");
    }
    if (day == QStringLiteral("Friday"))
    {
        return QStringLiteral("F");
    }
    if (day == QStringLiteral("Saturday"))
    {
        return QStringLiteral("Sat");
    }
    if (day == QStringLiteral("Sunday"))
    {
        return QStringLiteral("Sun");
    }

    return day.trimmed();
}

int dayOrder(
    const QString& day
    )
{
    const int index =
        ClassInfoConfig::Days.indexOf(
            day.trimmed()
            );

    return index >= 0
        ? index
        : UnknownOrder;
}

QString compressedDays(
    QStringList days
    )
{
    days.removeDuplicates();

    std::sort(
        days.begin(),
        days.end(),
        [](const QString& left, const QString& right)
        {
            return dayOrder(left) < dayOrder(right);
        }
        );

    QStringList codes;

    for (const QString& day : std::as_const(days))
    {
        const QString code =
            dayCode(day);

        if (!code.isEmpty())
        {
            codes.append(code);
        }
    }

    if (codes == QStringList{QStringLiteral("M"), QStringLiteral("W")})
    {
        return QStringLiteral("M/W");
    }
    if (codes == QStringList{QStringLiteral("M"), QStringLiteral("F")})
    {
        return QStringLiteral("M/F");
    }
    if (codes == QStringList{QStringLiteral("W"), QStringLiteral("F")})
    {
        return QStringLiteral("W/F");
    }
    if (
        codes == QStringList{
            QStringLiteral("M"),
            QStringLiteral("W"),
            QStringLiteral("F")
        }
        )
    {
        return QStringLiteral("M/W/F");
    }
    if (codes == QStringList{QStringLiteral("T"), QStringLiteral("Th")})
    {
        return QStringLiteral("T/Th");
    }

    return codes.join(
        QStringLiteral("/")
        );
}

QString scheduleText(
    const QList<ClassTime>& times
    )
{
    if (times.isEmpty())
    {
        return QString();
    }

    struct TimeGroup
    {
        QString startTime;
        QStringList days;
    };

    QList<TimeGroup> groups;

    for (const ClassTime& time : times)
    {
        const QString start =
            compactStartTime(
                time.startTime
                );

        if (start.isEmpty())
        {
            continue;
        }

        auto group =
            std::find_if(
                groups.begin(),
                groups.end(),
                [&start](const TimeGroup& candidate)
                {
                    return candidate.startTime == start;
                }
                );

        if (group == groups.end())
        {
            TimeGroup newGroup;
            newGroup.startTime = start;
            newGroup.days.append(
                time.day.trimmed()
                );
            groups.append(newGroup);
        }
        else
        {
            group->days.append(
                time.day.trimmed()
                );
        }
    }

    QStringList labels;

    for (const TimeGroup& group : groups)
    {
        labels.append(
            QStringLiteral("%1 %2")
                .arg(
                    compressedDays(group.days),
                    group.startTime
                    )
            );
    }

    return labels.join(
        QStringLiteral("; ")
        );
}

const QList<ClassTime>& preferredTimes(
    const ClassTabNavigation::ClassEntry& entry
    )
{
    return entry.regularTimes.isEmpty()
        ? entry.intensiveTimes
        : entry.regularTimes;
}

const QList<ClassTime>& timesForFilter(
    const ClassTabNavigation::ClassEntry& entry,
    ClassTabNavigation::ScheduleSource scheduleSource
    )
{
    return scheduleSource == ClassTabNavigation::ScheduleSource::Intensive
        ? entry.intensiveTimes
        : entry.regularTimes;
}

QString normalizedDay(
    const QString& day
    )
{
    return day.trimmed().toCaseFolded();
}

QSet<QString> expandedFilterDays(
    const ClassTabNavigation::DayFilter& dayFilter
    )
{
    QSet<QString> result;

    for (const QString& day : dayFilter.selectedDays)
    {
        const QString normalized = normalizedDay(day);

        if (normalized == QStringLiteral("wkend")
            || normalized == QStringLiteral("weekend"))
        {
            result.insert(QStringLiteral("saturday"));
            result.insert(QStringLiteral("sunday"));
            continue;
        }

        if (!normalized.isEmpty())
        {
            result.insert(normalized);
        }
    }

    return result;
}

bool matchesDayFilter(
    const ClassTabNavigation::ClassEntry& entry,
    const ClassTabNavigation::DayFilter& dayFilter
    )
{
    const QList<ClassTime>& scheduleTimes =
        timesForFilter(entry, dayFilter.scheduleSource);

    if (
        dayFilter.visibilityScope
            == ClassTabNavigation::VisibilityScope::ActiveSchedule
        && scheduleTimes.isEmpty()
        )
    {
        return false;
    }

    const QSet<QString> selectedDays = expandedFilterDays(dayFilter);

    if (selectedDays.isEmpty())
    {
        return true;
    }

    for (const ClassTime& time : scheduleTimes)
    {
        if (selectedDays.contains(normalizedDay(time.day)))
        {
            return true;
        }
    }

    return false;
}

QList<ClassTabNavigation::ClassEntry> filteredEntries(
    const QList<ClassTabNavigation::ClassEntry>& entries,
    const ClassTabNavigation::DayFilter& dayFilter
    )
{
    QList<ClassTabNavigation::ClassEntry> result;

    for (const ClassTabNavigation::ClassEntry& entry : entries)
    {
        if (matchesDayFilter(entry, dayFilter))
        {
            result.append(entry);
        }
    }

    return result;
}

int timeOrder(
    const QString& value
    )
{
    const QString trimmed =
        value.trimmed();

    if (trimmed.isEmpty())
    {
        return UnknownOrder;
    }

    const QStringList formats{
        QStringLiteral("h:mm AP"),
        QStringLiteral("h:mmAP"),
        QStringLiteral("hh:mm AP"),
        QStringLiteral("hh:mmAP"),
        QStringLiteral("H:mm"),
        QStringLiteral("HH:mm"),
        QStringLiteral("H:mm:ss"),
        QStringLiteral("HH:mm:ss")
    };

    for (const QString& format : formats)
    {
        const QTime time =
            QTime::fromString(
                trimmed,
                format
                );

        if (time.isValid())
        {
            return (time.hour() * 60) + time.minute();
        }
    }

    return UnknownOrder;
}

int firstDayOrder(
    const ClassTabNavigation::ClassEntry& entry
    )
{
    int result = UnknownOrder;

    for (const ClassTime& time : preferredTimes(entry))
    {
        result =
            std::min(
                result,
                dayOrder(time.day)
                );
    }

    return result;
}

int firstTimeOrder(
    const ClassTabNavigation::ClassEntry& entry
    )
{
    int result = UnknownOrder;

    for (const ClassTime& time : preferredTimes(entry))
    {
        if (dayOrder(time.day) != firstDayOrder(entry))
        {
            continue;
        }

        result =
            std::min(
                result,
                timeOrder(time.startTime)
                );
    }

    return result;
}

QString preferredScheduleText(
    const ClassTabNavigation::ClassEntry& entry
    )
{
    const QString regular =
        scheduleText(
            entry.regularTimes
            );

    if (!regular.isEmpty())
    {
        return regular;
    }

    const QString intensive =
        scheduleText(
            entry.intensiveTimes
            );

    if (!intensive.isEmpty())
    {
        return QStringLiteral("%1 %2")
            .arg(
                QObject::tr("Int"),
                intensive
                );
    }

    return QObject::tr("No time");
}

QString classNameText(
    const ClassTabNavigation::ClassEntry& entry,
    bool includeGrade
    )
{
    const QString grade =
        entry.grade.trimmed();
    const QString level =
        entry.level.trimmed();

    if (includeGrade)
    {
        if (!grade.isEmpty() && !level.isEmpty())
        {
            return QStringLiteral("%1 %2")
                .arg(grade, level);
        }

        if (!grade.isEmpty())
        {
            return grade;
        }
    }

    if (!level.isEmpty())
    {
        return level;
    }

    if (!grade.isEmpty())
    {
        return grade;
    }

    return trimmedOr(
        entry.classroomName,
        QObject::tr("Class %1").arg(entry.classId)
        );
}

QString baseLabel(
    const ClassTabNavigation::ClassEntry& entry,
    bool includeGrade
    )
{
    return QStringLiteral("%1 %2 %3")
        .arg(
            classNameText(
                entry,
                includeGrade
                ),
            QStringLiteral("•"),
            preferredScheduleText(entry)
            );
}

QString teacherLabel(
    const ClassTabNavigation::ClassEntry& entry
    )
{
    const QString english =
        entry.teacherEn.trimmed();

    if (!english.isEmpty())
    {
        return english;
    }

    return entry.teacherKr.trimmed();
}

bool entryLessThan(
    const ClassTabNavigation::ClassEntry& left,
    const ClassTabNavigation::ClassEntry& right
    )
{
    const int leftGradeOrder =
        gradeOrder(
            left.grade
            );
    const int rightGradeOrder =
        gradeOrder(
            right.grade
            );

    if (leftGradeOrder != rightGradeOrder)
    {
        return leftGradeOrder < rightGradeOrder;
    }

    const int leftLevelOrder =
        levelOrder(
            left.grade,
            left.level
            );
    const int rightLevelOrder =
        levelOrder(
            right.grade,
            right.level
            );

    if (leftLevelOrder != rightLevelOrder)
    {
        return leftLevelOrder < rightLevelOrder;
    }

    const int leftDayOrder =
        firstDayOrder(left);
    const int rightDayOrder =
        firstDayOrder(right);

    if (leftDayOrder != rightDayOrder)
    {
        return leftDayOrder < rightDayOrder;
    }

    const int leftTimeOrder =
        firstTimeOrder(left);
    const int rightTimeOrder =
        firstTimeOrder(right);

    if (leftTimeOrder != rightTimeOrder)
    {
        return leftTimeOrder < rightTimeOrder;
    }

    const int labelComparison =
        QString::localeAwareCompare(
            baseLabel(left, true),
            baseLabel(right, true)
            );

    if (labelComparison != 0)
    {
        return labelComparison < 0;
    }

    return left.classId < right.classId;
}

QList<ClassTabNavigation::ClassEntry> sortedEntries(
    const QList<ClassTabNavigation::ClassEntry>& entries
    )
{
    QList<ClassTabNavigation::ClassEntry> result =
        entries;

    std::sort(
        result.begin(),
        result.end(),
        entryLessThan
        );

    return result;
}

void applyUniqueLabels(
    QList<TabCandidate>* candidates
    )
{
    if (!candidates)
    {
        return;
    }

    for (int index = 0; index < candidates->size(); ++index)
    {
        TabCandidate& candidate =
            (*candidates)[index];

        int duplicateCount = 0;

        for (const TabCandidate& other : std::as_const(*candidates))
        {
            if (other.baseLabel == candidate.baseLabel)
            {
                ++duplicateCount;
            }
        }

        if (duplicateCount <= 1)
        {
            candidate.tab.label =
                candidate.baseLabel;
            continue;
        }

        QString expandedLabel =
            candidate.baseLabel;

        if (!candidate.teacherLabel.isEmpty())
        {
            expandedLabel =
                QStringLiteral("%1 %2 %3")
                    .arg(
                        candidate.baseLabel,
                        QStringLiteral("•"),
                        candidate.teacherLabel
                        );
        }

        bool stillDuplicated =
            candidate.teacherLabel.isEmpty();

        if (!stillDuplicated)
        {
            for (int otherIndex = 0; otherIndex < candidates->size(); ++otherIndex)
            {
                if (otherIndex == index)
                {
                    continue;
                }

                const TabCandidate& other =
                    (*candidates)[otherIndex];

                if (other.baseLabel != candidate.baseLabel)
                {
                    continue;
                }

                QString otherExpandedLabel =
                    other.baseLabel;

                if (!other.teacherLabel.isEmpty())
                {
                    otherExpandedLabel =
                        QStringLiteral("%1 %2 %3")
                            .arg(
                                other.baseLabel,
                                QStringLiteral("•"),
                                other.teacherLabel
                                );
                }

                if (otherExpandedLabel == expandedLabel)
                {
                    stillDuplicated = true;
                    break;
                }
            }
        }

        candidate.tab.label =
            stillDuplicated
                ? QStringLiteral("%1 #%2")
                    .arg(
                        expandedLabel,
                        QString::number(candidate.tab.classId)
                        )
                : expandedLabel;
    }
}

QList<ClassTabNavigation::ClassTab> makeClassTabs(
    const QList<ClassTabNavigation::ClassEntry>& entries,
    bool includeGrade
    )
{
    QList<TabCandidate> candidates;

    for (const ClassTabNavigation::ClassEntry& entry : entries)
    {
        TabCandidate candidate;
        candidate.tab.classId =
            entry.classId;
        candidate.baseLabel =
            baseLabel(
                entry,
                includeGrade
                );
        candidate.teacherLabel =
            teacherLabel(entry);

        candidates.append(candidate);
    }

    applyUniqueLabels(&candidates);

    QList<ClassTabNavigation::ClassTab> tabs;

    for (const TabCandidate& candidate : std::as_const(candidates))
    {
        tabs.append(
            candidate.tab
            );
    }

    return tabs;
}
}

namespace ClassTabNavigation
{
Model build(
    const QList<ClassEntry>& entries,
    GroupingPolicy groupingPolicy,
    const DayFilter& dayFilter
    )
{
    const QList<ClassEntry> filtered =
        filteredEntries(entries, dayFilter);

    Model model;
    model.mode =
        groupingPolicy == GroupingPolicy::AlwaysGradeGrouped
        || filtered.size() > FlatClassThreshold
            ? Mode::GradeGrouped
            : Mode::Flat;

    const QList<ClassEntry> sorted =
        sortedEntries(filtered);

    if (model.mode == Mode::Flat)
    {
        model.flatClasses =
            makeClassTabs(
                sorted,
                true
                );
        return model;
    }

    QList<QString> groupKeys;

    for (const ClassEntry& entry : sorted)
    {
        const QString key =
            gradeKey(entry);

        if (!groupKeys.contains(key))
        {
            groupKeys.append(key);
        }
    }

    std::sort(
        groupKeys.begin(),
        groupKeys.end(),
        [](const QString& left, const QString& right)
        {
            const int leftOrder =
                left.isEmpty()
                    ? UnknownOrder
                    : gradeOrder(left);
            const int rightOrder =
                right.isEmpty()
                    ? UnknownOrder
                    : gradeOrder(right);

            return leftOrder < rightOrder;
        }
        );

    for (const QString& key : std::as_const(groupKeys))
    {
        QList<ClassEntry> groupEntries;

        for (const ClassEntry& entry : sorted)
        {
            if (gradeKey(entry) == key)
            {
                groupEntries.append(entry);
            }
        }

        GradeGroup group;
        group.grade = key;
        group.label =
            gradeLabel(key);
        group.classes =
            makeClassTabs(
                groupEntries,
                false
                );

        model.gradeGroups.append(group);
    }

    return model;
}
}
