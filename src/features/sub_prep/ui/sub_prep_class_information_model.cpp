#include "sub_prep_class_information_model.h"

#include "core/utils/sidebar_node_naming.h"
#include "features/classes/config/class_info_config.h"

#include <algorithm>
#include <limits>

#include <QHash>
#include <QCoreApplication>
#include <QTime>

namespace
{
constexpr int UnknownOrder =
    std::numeric_limits<int>::max();

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

QString dayAbbreviation(
    const QString& day
    )
{
    if (day == QStringLiteral("Monday"))
    {
        return QCoreApplication::translate(
            "SubPrepClassInformation",
            "Mon"
            );
    }
    if (day == QStringLiteral("Tuesday"))
    {
        return QCoreApplication::translate(
            "SubPrepClassInformation",
            "Tues"
            );
    }
    if (day == QStringLiteral("Wednesday"))
    {
        return QCoreApplication::translate(
            "SubPrepClassInformation",
            "Wed"
            );
    }
    if (day == QStringLiteral("Thursday"))
    {
        return QCoreApplication::translate(
            "SubPrepClassInformation",
            "Thurs"
            );
    }
    if (day == QStringLiteral("Friday"))
    {
        return QCoreApplication::translate(
            "SubPrepClassInformation",
            "Fri"
            );
    }
    if (day == QStringLiteral("Saturday"))
    {
        return QCoreApplication::translate(
            "SubPrepClassInformation",
            "Sat"
            );
    }
    if (day == QStringLiteral("Sunday"))
    {
        return QCoreApplication::translate(
            "SubPrepClassInformation",
            "Sun"
            );
    }

    return day.trimmed();
}

QTime parseTime(
    const QString& value
    )
{
    const QString trimmed =
        value.trimmed();

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
        const QTime parsed =
            QTime::fromString(
                trimmed,
                format
                );

        if (parsed.isValid())
        {
            return parsed;
        }
    }

    return {};
}

QString compactTime(
    const QTime& time
    )
{
    if (!time.isValid())
    {
        return {};
    }

    const QString format =
        time.minute() == 0
            ? QStringLiteral("hap")
            : QStringLiteral("h:mmap");

    return time
        .toString(format)
        .toLower();
}

QString classLabel(
    const ClassInfo& info
    )
{
    const QString grade =
        info.classGrade.trimmed();
    const QString level =
        info.classLevel.trimmed();

    if (!grade.isEmpty() && !level.isEmpty())
    {
        return QStringLiteral("%1 %2")
            .arg(grade, level);
    }

    if (!grade.isEmpty())
    {
        return grade;
    }

    if (!level.isEmpty())
    {
        return level;
    }

    return QStringLiteral("N/A");
}

QString teacherName(
    const Teacher& teacher
    )
{
    const QString preferredName =
        teacher.preferredDisplayName();

    return preferredName.isEmpty()
        ? QStringLiteral("N/A")
        : preferredName;
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
    const ClassInfo& info
    )
{
    const int index =
        ClassInfoConfig::levelsForGrade(
            info.classGrade.trimmed()
            )
            .indexOf(
                info.classLevel.trimmed()
                );

    return index >= 0
        ? index
        : UnknownOrder;
}

QPair<int, int> firstMeetingOrder(
    const QList<ClassTime>& times,
    const QStringList& visibleDays
    )
{
    QPair<int, int> result{
        UnknownOrder,
        UnknownOrder
    };

    for (const ClassTime& meeting : times)
    {
        if (!visibleDays.contains(meeting.day))
        {
            continue;
        }

        const QTime time =
            parseTime(meeting.startTime);

        if (!time.isValid())
        {
            continue;
        }

        const QPair<int, int> candidate{
            dayOrder(meeting.day),
            (time.hour() * 60) + time.minute()
        };

        if (candidate < result)
        {
            result = candidate;
        }
    }

    return result;
}
}

QString SubPrepClassInformation::formatMeetingTimes(
    const QList<ClassTime>& times,
    const QStringList& visibleDays
    )
{
    struct Meeting
    {
        QString day;
        QTime time;
    };

    QList<Meeting> meetings;

    for (const ClassTime& source : times)
    {
        if (!visibleDays.contains(source.day))
        {
            continue;
        }

        const QTime time =
            parseTime(source.startTime);

        if (!time.isValid())
        {
            continue;
        }

        meetings.append(
            {
                source.day,
                time
            }
            );
    }

    std::sort(
        meetings.begin(),
        meetings.end(),
        [](const Meeting& left, const Meeting& right)
        {
            const int dayComparison =
                dayOrder(left.day) - dayOrder(right.day);

            if (dayComparison != 0)
            {
                return dayComparison < 0;
            }

            return left.time < right.time;
        }
        );

    struct TimeGroup
    {
        QTime time;
        QStringList days;
        int firstDay = UnknownOrder;
    };

    QList<TimeGroup> groups;

    for (const Meeting& meeting : meetings)
    {
        auto group =
            std::find_if(
                groups.begin(),
                groups.end(),
                [&meeting](const TimeGroup& candidate)
                {
                    return candidate.time == meeting.time;
                }
                );

        if (group == groups.end())
        {
            groups.append(
                {
                    meeting.time,
                    QStringList{meeting.day},
                    dayOrder(meeting.day)
                }
                );
        }
        else if (!group->days.contains(meeting.day))
        {
            group->days.append(meeting.day);
        }
    }

    std::sort(
        groups.begin(),
        groups.end(),
        [](const TimeGroup& left, const TimeGroup& right)
        {
            if (left.firstDay != right.firstDay)
            {
                return left.firstDay < right.firstDay;
            }

            return left.time < right.time;
        }
        );

    QStringList labels;

    for (const TimeGroup& group : groups)
    {
        QString days;

        for (const QString& day : group.days)
        {
            days += dayAbbreviation(day);
        }

        labels.append(
            QStringLiteral("%1 %2")
                .arg(
                    days,
                    compactTime(group.time)
                    )
            );
    }

    return labels.isEmpty()
        ? QStringLiteral("N/A")
        : labels.join(QStringLiteral(" & "));
}

QList<SubPrepClassInformation::TeacherGroup>
SubPrepClassInformation::build(
    const QList<SourceClass>& sourceClasses,
    const BuildOptions& options
    )
{
    QHash<int, QList<ClassDetails>> classesByTeacher;
    QHash<int, Teacher> teachersById;
    QSet<int> processedClassIds;

    for (const SourceClass& source : sourceClasses)
    {
        const int classId =
            source.classroom.id;
        const int teacherId =
            source.info.teacherId;

        if (
            classId <= 0
            || teacherId <= 0
            || source.teacher.id <= 0
            || !options.visibleClassIds.contains(classId)
            || processedClassIds.contains(classId)
            )
        {
            continue;
        }

        processedClassIds.insert(classId);
        teachersById.insert(
            teacherId,
            source.teacher
            );

        const QList<ClassTime>& selectedTimes =
            options.useIntensive
                ? source.info.intensiveTimes
                : source.info.classTimes;

        ClassDetails details;
        details.classId = classId;
        details.info = source.info;
        details.studentCount = source.studentCount;
        details.classLabel = classLabel(source.info);
        details.timeText =
            formatMeetingTimes(
                selectedTimes,
                options.visibleDays
                );

        classesByTeacher[teacherId].append(details);
    }

    QList<Teacher> teachers =
        teachersById.values();

    std::sort(
        teachers.begin(),
        teachers.end(),
        SidebarNodeNaming::teacherDisplayLessThan
        );

    QList<TeacherGroup> groups;

    for (const Teacher& teacher : teachers)
    {
        QList<ClassDetails> classes =
            classesByTeacher.value(teacher.id);

        std::sort(
            classes.begin(),
            classes.end(),
            [&options](const ClassDetails& left, const ClassDetails& right)
            {
                const int leftGrade =
                    gradeOrder(left.info.classGrade);
                const int rightGrade =
                    gradeOrder(right.info.classGrade);

                if (leftGrade != rightGrade)
                {
                    return leftGrade < rightGrade;
                }

                const int leftLevel =
                    levelOrder(left.info);
                const int rightLevel =
                    levelOrder(right.info);

                if (leftLevel != rightLevel)
                {
                    return leftLevel < rightLevel;
                }

                const QList<ClassTime>& leftTimes =
                    options.useIntensive
                        ? left.info.intensiveTimes
                        : left.info.classTimes;
                const QList<ClassTime>& rightTimes =
                    options.useIntensive
                        ? right.info.intensiveTimes
                        : right.info.classTimes;

                const auto leftMeeting =
                    firstMeetingOrder(
                        leftTimes,
                        options.visibleDays
                        );
                const auto rightMeeting =
                    firstMeetingOrder(
                        rightTimes,
                        options.visibleDays
                        );

                if (leftMeeting != rightMeeting)
                {
                    return leftMeeting < rightMeeting;
                }

                return left.classId < right.classId;
            }
            );

        QStringList classLabels;
        QSet<QString> seenClassLabels;

        for (const ClassDetails& details : classes)
        {
            if (seenClassLabels.contains(details.classLabel))
            {
                continue;
            }

            seenClassLabels.insert(details.classLabel);
            classLabels.append(details.classLabel);
        }

        TeacherGroup group;
        group.teacher = teacher;
        group.displayName = teacherName(teacher);
        group.classListText =
            classLabels.join(QStringLiteral(" / "));
        group.classes = classes;

        groups.append(group);
    }

    return groups;
}
