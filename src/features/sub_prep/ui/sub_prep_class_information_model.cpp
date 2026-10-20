#include "sub_prep_class_information_model.h"

#include "domain/rules/schedule_value_parser.h"
#include "features/classes/config/class_info_config.h"
#include "next/application/sub_prep_print_order_policy.h"
#include "next/domain/domain_types.h"

#include <algorithm>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <QHash>
#include <QCoreApplication>
#include <QTime>

namespace
{
namespace PrintOrder = ClassMngr::Next::Application;
namespace Domain = ClassMngr::Next::Domain;

constexpr int UnknownOrder =
    std::numeric_limits<int>::max();

Domain::TeacherId typedTeacherId(const int teacherId)
{
    return Domain::TeacherId::fromString(std::to_string(teacherId)).value();
}

Domain::ClassId typedClassId(const int classId)
{
    return Domain::ClassId::fromString(std::to_string(classId)).value();
}

int dayOrder(
    const QString& day
    )
{
    const QString trimmed = day.trimmed();
    const auto parsed = ScheduleValueParser::parseWeekday(trimmed);
    if (!parsed || parsed->text != trimmed)
    {
        return UnknownOrder;
    }

    const int index =
        ClassInfoConfig::Days.indexOf(
            parsed->text
            );

    return index >= 0
        ? index
        : UnknownOrder;
}

QString dayAbbreviation(
    const QString& day
    )
{
    const auto parsed = ScheduleValueParser::parseWeekday(day);
    if (!parsed || parsed->text != day)
    {
        return day.trimmed();
    }

    if (parsed->text == QStringLiteral("Monday"))
    {
        return QCoreApplication::translate(
            "SubPrepClassInformation",
            "Mon"
            );
    }
    if (parsed->text == QStringLiteral("Tuesday"))
    {
        return QCoreApplication::translate(
            "SubPrepClassInformation",
            "Tues"
            );
    }
    if (parsed->text == QStringLiteral("Wednesday"))
    {
        return QCoreApplication::translate(
            "SubPrepClassInformation",
            "Wed"
            );
    }
    if (parsed->text == QStringLiteral("Thursday"))
    {
        return QCoreApplication::translate(
            "SubPrepClassInformation",
            "Thurs"
            );
    }
    if (parsed->text == QStringLiteral("Friday"))
    {
        return QCoreApplication::translate(
            "SubPrepClassInformation",
            "Fri"
            );
    }
    if (parsed->text == QStringLiteral("Saturday"))
    {
        return QCoreApplication::translate(
            "SubPrepClassInformation",
            "Sat"
            );
    }
    if (parsed->text == QStringLiteral("Sunday"))
    {
        return QCoreApplication::translate(
            "SubPrepClassInformation",
            "Sun"
            );
    }

    return day.trimmed();
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

        const auto parsedTime =
            ScheduleValueParser::parseDisplayTime(meeting.startTime);

        if (!parsedTime)
        {
            continue;
        }

        const QTime time =
            *parsedTime;

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

        const auto parsedTime =
            ScheduleValueParser::parseDisplayTime(source.startTime);

        if (!parsedTime)
        {
            continue;
        }

        const QTime time =
            *parsedTime;

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
    QHash<int, Teacher> teachersById;
    QSet<int> processedClassIds;
    QList<ClassDetails> classes;
    std::vector<PrintOrder::SubPrepPrintClassOrderFact> classOrderFacts;
    classes.reserve(sourceClasses.size());
    classOrderFacts.reserve(static_cast<std::size_t>(sourceClasses.size()));

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

        const auto meetingOrder =
            firstMeetingOrder(
                selectedTimes,
                options.visibleDays
                );

        classes.append(std::move(details));
        classOrderFacts.push_back(
            {
                typedTeacherId(teacherId),
                PrintOrder::SubPrepPrintGradeOrder{
                    gradeOrder(source.info.classGrade)
                },
                PrintOrder::SubPrepPrintLevelOrder{
                    levelOrder(source.info)
                },
                PrintOrder::SubPrepPrintWeekdayOrder{meetingOrder.first},
                PrintOrder::SubPrepPrintStartMinute{meetingOrder.second},
                typedClassId(classId)
            }
            );
    }

    QList<Teacher> teachers =
        teachersById.values();

    std::vector<PrintOrder::SubPrepPrintTeacherOrderFact> teacherOrderFacts;
    QList<QString> trimmedEnglishNames;
    QList<QString> trimmedKoreanNames;
    teacherOrderFacts.reserve(static_cast<std::size_t>(teachers.size()));
    trimmedEnglishNames.reserve(teachers.size());
    trimmedKoreanNames.reserve(teachers.size());

    for (const Teacher& teacher : teachers)
    {
        const QString englishName =
            teacher.teacherEn.trimmed();
        teacherOrderFacts.push_back(
            {
                typedTeacherId(teacher.id),
                !englishName.isEmpty()
            }
            );
        trimmedEnglishNames.append(englishName);
        trimmedKoreanNames.append(teacher.teacherKr.trimmed());
    }

    const std::vector<PrintOrder::SubPrepPrintGroupOrder> groupOrder =
        PrintOrder::orderSubPrepPrintGroups(
            teacherOrderFacts,
            classOrderFacts,
            [&trimmedEnglishNames, &trimmedKoreanNames](
                const PrintOrder::SubPrepPrintTeacherIndex leftIndex,
                const PrintOrder::SubPrepPrintTeacherIndex rightIndex,
                const PrintOrder::SubPrepPrintTeacherNameField field
            )
            {
                const QList<QString>& names =
                    field == PrintOrder::SubPrepPrintTeacherNameField::English
                        ? trimmedEnglishNames
                        : trimmedKoreanNames;
                return QString::localeAwareCompare(
                    names.at(
                        static_cast<qsizetype>(leftIndex.rawValue())
                        ),
                    names.at(
                        static_cast<qsizetype>(rightIndex.rawValue())
                        )
                    );
            }
            );

    QList<TeacherGroup> groups;

    for (const PrintOrder::SubPrepPrintGroupOrder& orderedGroup : groupOrder)
    {
        const Teacher& teacher =
            teachers.at(
                static_cast<qsizetype>(orderedGroup.teacherIndex.rawValue())
                );
        QList<ClassDetails> orderedClasses;
        orderedClasses.reserve(
            static_cast<qsizetype>(orderedGroup.classIndexes.size())
            );
        for (
            const PrintOrder::SubPrepPrintClassIndex classIndex :
            orderedGroup.classIndexes
            )
        {
            orderedClasses.append(
                classes.at(static_cast<qsizetype>(classIndex.rawValue()))
                );
        }

        QStringList classLabels;
        QSet<QString> seenClassLabels;

        for (const ClassDetails& details : orderedClasses)
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
        group.classes = std::move(orderedClasses);

        groups.append(group);
    }

    return groups;
}
