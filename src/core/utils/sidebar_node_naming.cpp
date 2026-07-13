#include "sidebar_node_naming.h"

#include "domain/models/class_info.h"
#include "domain/models/teacher.h"

#include <QMap>
#include <QStringList>

namespace
{

const QMap<QString, QString> DayAbbreviations =
    {
        {"Monday",    "Mon"},
        {"Tuesday",   "Tues"},
        {"Wednesday", "Wed"},
        {"Thursday",  "Thurs"},
        {"Friday",    "Fri"},
        {"Saturday",  "Sat"},
        {"Sunday",    "Sun"}
};

const QMap<QStringList, QString> SpecialDayPatterns =
    {
        {QStringList{"Mon", "Wed"},              "M/W"},
        {QStringList{"Mon", "Fri"},              "M/F"},
        {QStringList{"Wed", "Fri"},              "W/F"},
        {QStringList{"Mon", "Wed", "Fri"},       "M/W/F"},
        {QStringList{"Tues", "Thurs"},           "T/Th"}
};

} // namespace



QString SidebarNodeNaming::formatClassDisplayName(
    const ClassInfo& classInfo,
    const Teacher& teacher
    )
{
    // ============================================
    // Level
    // ============================================

    const QString grade =
        classInfo.classGrade.trimmed();

    const QString level =
        classInfo.classLevel.trimmed();

    QString levelText;

    if (!grade.isEmpty() &&
        !level.isEmpty())
    {
        levelText =
            QString("%1 %2")
                .arg(grade)
                .arg(level);
    }
    else if (!grade.isEmpty())
    {
        levelText = grade;
    }
    else if (!level.isEmpty())
    {
        levelText = level;
    }
    else
    {
        levelText = "Unknown Class";
    }

    // ============================================
    // Teacher
    // ============================================

    QString teacherName =
        teacher.teacherEn.trimmed();

    if (teacherName.isEmpty())
    {
        teacherName =
            teacher.teacherKr.trimmed();
    }

    if (teacherName.isEmpty())
    {
        teacherName = "No Teacher";
    }

    // ============================================
    // Times
    // ============================================

    if (classInfo.classTimes.isEmpty())
    {
        return QString("%1 • %2")
            .arg(levelText)
            .arg(teacherName);
    }

    QStringList dayLabels;
    QStringList timeLabels;

    for (const auto& row : classInfo.classTimes)
    {
        const QString day =
            DayAbbreviations.value(
                row.day,
                row.day
                );

        QString startTime =
            row.startTime;

        startTime.replace(" AM", "");
        startTime.replace(" PM", "");

        dayLabels.append(day);
        timeLabels.append(startTime);
    }

    // ============================================
    // Compress common day patterns
    // ============================================

    QString daysText;

    if (SpecialDayPatterns.contains(dayLabels))
    {
        daysText =
            SpecialDayPatterns.value(
                dayLabels
                );
    }
    else
    {
        daysText =
            dayLabels.join("/");
    }

    // ============================================
    // Compress duplicate times
    // ============================================

    QStringList uniqueTimes;

    for (const auto& time : timeLabels)
    {
        if (!uniqueTimes.contains(time))
        {
            uniqueTimes.append(time);
        }
    }

    QString timeText;

    if (uniqueTimes.size() == 1)
    {
        timeText =
            uniqueTimes.first();
    }
    else
    {
        timeText =
            timeLabels.join(" / ");
    }

    // ============================================
    // Final
    // ============================================

    return QString(
               "%1 • %2 • %3 (%4)"
               )
        .arg(levelText)
        .arg(teacherName)
        .arg(daysText)
        .arg(timeText);
}




QString SidebarNodeNaming::formatTeacherDisplayName(
    const Teacher& teacher
    )
{
    const QString teacherEn =
        teacher.teacherEn.trimmed();

    const QString teacherKr =
        teacher.teacherKr.trimmed();

    // ============================================
    // Both names
    // ============================================

    if (!teacherEn.isEmpty() &&
        !teacherKr.isEmpty())
    {
        return QString("%1 (%2)")
        .arg(teacherEn)
            .arg(teacherKr);
    }

    // ============================================
    // Korean only
    // ============================================

    if (!teacherKr.isEmpty())
    {
        return teacherKr;
    }

    // ============================================
    // English only
    // ============================================

    if (!teacherEn.isEmpty())
    {
        return teacherEn;
    }

    // ============================================
    // Empty
    // ============================================

    return "New Teacher";
}

bool SidebarNodeNaming::teacherDisplayLessThan(
    const Teacher& left,
    const Teacher& right
    )
{
    const QString leftEnglish =
        left.teacherEn.trimmed();
    const QString rightEnglish =
        right.teacherEn.trimmed();

    const bool leftHasEnglish =
        !leftEnglish.isEmpty();
    const bool rightHasEnglish =
        !rightEnglish.isEmpty();

    if (leftHasEnglish != rightHasEnglish)
    {
        return leftHasEnglish;
    }

    const int englishComparison =
        QString::localeAwareCompare(
            leftEnglish,
            rightEnglish
            );

    if (englishComparison != 0)
    {
        return englishComparison < 0;
    }

    const int koreanComparison =
        QString::localeAwareCompare(
            left.teacherKr.trimmed(),
            right.teacherKr.trimmed()
            );

    if (koreanComparison != 0)
    {
        return koreanComparison < 0;
    }

    return left.id < right.id;
}
