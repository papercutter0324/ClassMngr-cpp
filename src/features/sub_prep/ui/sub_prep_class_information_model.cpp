#include "sub_prep_class_information_model.h"

#include "classmngr/engine/sub_prep_class_information.h"

#include <QCoreApplication>

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace
{
using PortableClassInfo = classmngr::engine::ClassInfo;
using PortableClassTime = classmngr::engine::ClassTime;
using PortableSourceClass = classmngr::engine::SubPrepSourceClass;
using PortableTeacher = classmngr::engine::Teacher;

QString fromUtf8(const std::string& value)
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

QString localizeMeetingTimeLabels(QString value)
{
    const std::array<std::pair<QString, QString>, 7> labels{{
        {
            QStringLiteral("Thurs"),
            QCoreApplication::translate(
                "SubPrepClassInformation",
                "Thurs"
                )
        },
        {
            QStringLiteral("Tues"),
            QCoreApplication::translate(
                "SubPrepClassInformation",
                "Tues"
                )
        },
        {
            QStringLiteral("Mon"),
            QCoreApplication::translate(
                "SubPrepClassInformation",
                "Mon"
                )
        },
        {
            QStringLiteral("Wed"),
            QCoreApplication::translate(
                "SubPrepClassInformation",
                "Wed"
                )
        },
        {
            QStringLiteral("Fri"),
            QCoreApplication::translate(
                "SubPrepClassInformation",
                "Fri"
                )
        },
        {
            QStringLiteral("Sat"),
            QCoreApplication::translate(
                "SubPrepClassInformation",
                "Sat"
                )
        },
        {
            QStringLiteral("Sun"),
            QCoreApplication::translate(
                "SubPrepClassInformation",
                "Sun"
                )
        }
    }};

    QStringList groups = value.split(
        QStringLiteral(" & "),
        Qt::KeepEmptyParts
        );
    for (QString& group : groups)
    {
        const qsizetype separator = group.indexOf(QChar(u' '));
        if (separator <= 0)
        {
            continue;
        }

        const QString dayPart = group.left(separator);
        QString localizedDayPart;
        qsizetype offset = 0;
        bool recognized = true;
        while (offset < dayPart.size())
        {
            bool matched = false;
            for (const auto& [source, translation] : labels)
            {
                if (dayPart.mid(offset, source.size()) == source)
                {
                    localizedDayPart += translation;
                    offset += source.size();
                    matched = true;
                    break;
                }
            }
            if (!matched)
            {
                recognized = false;
                break;
            }
        }

        if (recognized)
        {
            group = localizedDayPart + group.mid(separator);
        }
    }
    return groups.join(QStringLiteral(" & "));
}

PortableClassTime toPortable(const ClassTime& source)
{
    return {
        source.day.toStdString(),
        source.startTime.toStdString(),
        source.endTime.toStdString()
    };
}

ClassTime fromPortable(const PortableClassTime& source)
{
    return {
        fromUtf8(source.day),
        fromUtf8(source.startTime),
        fromUtf8(source.endTime)
    };
}

PortableTeacher toPortable(const Teacher& source)
{
    PortableTeacher result;
    result.id = source.id;
    result.teacherKr = source.teacherKr.toStdString();
    result.teacherEn = source.teacherEn.toStdString();
    result.preferredRomanization = source.preferredRomanization.toStdString();
    result.preferredName = source.preferredName.toStdString();
    result.roomNumber = source.roomNumber.toStdString();
    result.birthday = source.birthday.toStdString();
    result.phoneNumber = source.phoneNumber.toStdString();
    result.wifiName = source.wifiName.toStdString();
    result.wifiPassword = source.wifiPassword.toStdString();
    result.internetType = source.internetType.toStdString();
    result.zoomId = source.zoomId.toStdString();
    result.zoomPassword = source.zoomPassword.toStdString();
    result.projectionType = source.projectionType.toStdString();
    result.notes = source.notes.toStdString();
    return result;
}

Teacher fromPortable(const PortableTeacher& source)
{
    Teacher result;
    result.id = source.id;
    result.teacherKr = fromUtf8(source.teacherKr);
    result.teacherEn = fromUtf8(source.teacherEn);
    result.preferredRomanization = fromUtf8(source.preferredRomanization);
    result.preferredName = fromUtf8(source.preferredName);
    result.roomNumber = fromUtf8(source.roomNumber);
    result.birthday = fromUtf8(source.birthday);
    result.phoneNumber = fromUtf8(source.phoneNumber);
    result.wifiName = fromUtf8(source.wifiName);
    result.wifiPassword = fromUtf8(source.wifiPassword);
    result.internetType = fromUtf8(source.internetType);
    result.zoomId = fromUtf8(source.zoomId);
    result.zoomPassword = fromUtf8(source.zoomPassword);
    result.projectionType = fromUtf8(source.projectionType);
    result.notes = fromUtf8(source.notes);
    return result;
}

PortableClassInfo toPortable(const ClassInfo& source)
{
    PortableClassInfo result;
    result.classId = source.classId;
    result.teacherId = source.teacherId;
    result.teacherKr = source.teacherKr.toStdString();
    result.teacherEn = source.teacherEn.toStdString();
    result.teacherPreferredName = source.teacherPreferredName.toStdString();
    result.roomNumber = source.roomNumber.toStdString();
    result.wifiName = source.wifiName.toStdString();
    result.wifiPassword = source.wifiPassword.toStdString();
    result.internetType = source.internetType.toStdString();
    result.zoomId = source.zoomId.toStdString();
    result.zoomPassword = source.zoomPassword.toStdString();
    result.projectionType = source.projectionType.toStdString();
    result.classGrade = source.classGrade.toStdString();
    result.classLevel = source.classLevel.toStdString();
    result.readingBook = source.readingBook.toStdString();
    result.essayBook = source.essayBook.toStdString();
    result.classColor = source.classColor.toStdString();
    result.fontColor = source.fontColor.toStdString();
    result.notes = source.notes.toStdString();
    result.timeFillerActivities = source.timeFillerActivities.toStdString();

    result.classTimes.reserve(source.classTimes.size());
    for (const ClassTime& time : source.classTimes)
    {
        result.classTimes.push_back(toPortable(time));
    }
    result.intensiveTimes.reserve(source.intensiveTimes.size());
    for (const ClassTime& time : source.intensiveTimes)
    {
        result.intensiveTimes.push_back(toPortable(time));
    }
    return result;
}

ClassInfo fromPortable(const PortableClassInfo& source)
{
    ClassInfo result;
    result.classId = source.classId;
    result.teacherId = source.teacherId;
    result.teacherKr = fromUtf8(source.teacherKr);
    result.teacherEn = fromUtf8(source.teacherEn);
    result.teacherPreferredName = fromUtf8(source.teacherPreferredName);
    result.roomNumber = fromUtf8(source.roomNumber);
    result.wifiName = fromUtf8(source.wifiName);
    result.wifiPassword = fromUtf8(source.wifiPassword);
    result.internetType = fromUtf8(source.internetType);
    result.zoomId = fromUtf8(source.zoomId);
    result.zoomPassword = fromUtf8(source.zoomPassword);
    result.projectionType = fromUtf8(source.projectionType);
    result.classGrade = fromUtf8(source.classGrade);
    result.classLevel = fromUtf8(source.classLevel);
    result.readingBook = fromUtf8(source.readingBook);
    result.essayBook = fromUtf8(source.essayBook);
    result.classColor = fromUtf8(source.classColor);
    result.fontColor = fromUtf8(source.fontColor);
    result.notes = fromUtf8(source.notes);
    result.timeFillerActivities = fromUtf8(source.timeFillerActivities);

    result.classTimes.reserve(source.classTimes.size());
    for (const PortableClassTime& time : source.classTimes)
    {
        result.classTimes.append(fromPortable(time));
    }
    result.intensiveTimes.reserve(source.intensiveTimes.size());
    for (const PortableClassTime& time : source.intensiveTimes)
    {
        result.intensiveTimes.append(fromPortable(time));
    }
    return result;
}

PortableSourceClass toPortable(
    const SubPrepClassInformation::SourceClass& source
    )
{
    PortableSourceClass result;
    result.classroom.id = source.classroom.id;
    result.classroom.name = source.classroom.name.toStdString();
    result.info = toPortable(source.info);
    result.teacher = toPortable(source.teacher);
    result.studentCount = source.studentCount;
    return result;
}

classmngr::engine::SubPrepBuildOptions toPortable(
    const SubPrepClassInformation::BuildOptions& source
    )
{
    classmngr::engine::SubPrepBuildOptions result;
    result.visibleClassIds.reserve(source.visibleClassIds.size());
    for (const int classId : source.visibleClassIds)
    {
        result.visibleClassIds.push_back(classId);
    }
    result.visibleDays.reserve(source.visibleDays.size());
    for (const QString& day : source.visibleDays)
    {
        result.visibleDays.push_back(day.toStdString());
    }
    result.useIntensive = source.useIntensive;
    return result;
}
} // namespace

QString SubPrepClassInformation::formatMeetingTimes(
    const QList<ClassTime>& times,
    const QStringList& visibleDays
    )
{
    std::vector<classmngr::engine::ClassTime> portableTimes;
    portableTimes.reserve(times.size());
    for (const ClassTime& time : times)
    {
        portableTimes.push_back(toPortable(time));
    }

    std::vector<std::string> portableDays;
    portableDays.reserve(visibleDays.size());
    for (const QString& day : visibleDays)
    {
        portableDays.push_back(day.toStdString());
    }

    return localizeMeetingTimeLabels(fromUtf8(
        classmngr::engine::SubPrepClassInformationService::formatMeetingTimes(
            portableTimes,
            portableDays
            )
        ));
}

QList<SubPrepClassInformation::TeacherGroup>
SubPrepClassInformation::build(
    const QList<SourceClass>& sourceClasses,
    const BuildOptions& options
    )
{
    std::vector<PortableSourceClass> portableSources;
    portableSources.reserve(sourceClasses.size());
    for (const SourceClass& source : sourceClasses)
    {
        portableSources.push_back(toPortable(source));
    }

    const std::vector<classmngr::engine::SubPrepTeacherGroup> portableGroups =
        classmngr::engine::SubPrepClassInformationService::build(
            portableSources,
            toPortable(options)
            );

    QList<TeacherGroup> result;
    result.reserve(static_cast<qsizetype>(portableGroups.size()));
    for (const auto& portableGroup : portableGroups)
    {
        TeacherGroup group;
        group.teacher = fromPortable(portableGroup.teacher);
        group.displayName = fromUtf8(portableGroup.displayName);
        group.classListText = fromUtf8(portableGroup.classListText);
        group.classes.reserve(
            static_cast<qsizetype>(portableGroup.classes.size())
            );

        for (const auto& portableDetails : portableGroup.classes)
        {
            ClassDetails details;
            details.classId = portableDetails.classId;
            details.info = fromPortable(portableDetails.info);
            details.studentCount = portableDetails.studentCount;
            details.classLabel = fromUtf8(portableDetails.classLabel);
            details.timeText = fromUtf8(portableDetails.timeText);
            group.classes.append(std::move(details));
        }
        result.append(std::move(group));
    }
    return result;
}
