#include "sub_prep_print_source_mapper.h"

#include "domain/models/class_info.h"
#include "domain/models/classroom.h"
#include "domain/models/teacher.h"

#include <charconv>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

namespace
{
using namespace ClassMngr::Next;

QString fromUtf8(
    const std::string& value
    )
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

std::optional<int> legacyId(
    const std::string& value
    )
{
    int parsed = -1;
    const auto [end, error] = std::from_chars(
        value.data(),
        value.data() + value.size(),
        parsed
        );
    if (
        error != std::errc{}
        || end != value.data() + value.size()
        || parsed <= 0
        || std::to_string(parsed) != value
        )
    {
        return std::nullopt;
    }

    return parsed;
}

std::optional<QString> weekdayName(
    const Application::SubPrepWeekday weekday
    )
{
    using Application::SubPrepWeekday;
    switch (weekday)
    {
    case SubPrepWeekday::Monday:
        return QStringLiteral("Monday");
    case SubPrepWeekday::Tuesday:
        return QStringLiteral("Tuesday");
    case SubPrepWeekday::Wednesday:
        return QStringLiteral("Wednesday");
    case SubPrepWeekday::Thursday:
        return QStringLiteral("Thursday");
    case SubPrepWeekday::Friday:
        return QStringLiteral("Friday");
    case SubPrepWeekday::Saturday:
        return QStringLiteral("Saturday");
    case SubPrepWeekday::Sunday:
        return QStringLiteral("Sunday");
    }

    return std::nullopt;
}

Domain::OperationError mappingError(
    const char* message
    )
{
    return {
        .code = Domain::ErrorCode::Validation,
        .message = message,
        .recoverable = false
    };
}
}

Domain::Result<QList<SubPrepClassInformation::TeacherGroup>>
SubPrepPrintSourceMapper::toClassInformation(
    const Application::SubPrepPrintSource& source,
    const Application::SubPrepPrintSourceRequest& request
    )
{
    std::unordered_map<std::string, Teacher> teachersById;
    teachersById.reserve(source.teachers().size());

    for (const Application::SubPrepPrintTeacher& record : source.teachers())
    {
        const auto id = legacyId(record.id.value());
        if (!id)
        {
            return Domain::Result<
                QList<SubPrepClassInformation::TeacherGroup>
                >::failure(
                mappingError(
                    "A Sub Prep print teacher identifier cannot be represented by the current renderer."
                    )
                );
        }

        Teacher teacher;
        teacher.id = *id;
        teacher.teacherEn = fromUtf8(record.englishName);
        teacher.teacherKr = fromUtf8(record.koreanName);
        teacher.preferredName = fromUtf8(record.preferredName);
        teacher.preferredRomanization = fromUtf8(
            record.preferredRomanization
            );
        teacher.roomNumber = fromUtf8(record.room);
        teacher.wifiName = fromUtf8(record.wifiName);
        teacher.wifiPassword = fromUtf8(record.wifiPassword);
        teacher.internetType = fromUtf8(record.internetType);
        teacher.zoomId = fromUtf8(record.zoomId);
        teacher.zoomPassword = fromUtf8(record.zoomPassword);
        teacher.projectionType = fromUtf8(record.projectionType);
        teacher.notes = fromUtf8(record.teacherNotes);
        teachersById.emplace(record.id.value(), std::move(teacher));
    }

    QList<SubPrepClassInformation::SourceClass> classes;
    classes.reserve(static_cast<qsizetype>(source.classes().size()));
    QSet<int> visibleClassIds;
    QStringList visibleDays;

    for (const Application::SubPrepWeekday weekday : request.selectedDays)
    {
        const auto day = weekdayName(weekday);
        if (!day)
        {
            return Domain::Result<
                QList<SubPrepClassInformation::TeacherGroup>
                >::failure(
                mappingError("A Sub Prep print weekday is invalid.")
                );
        }
        visibleDays.append(*day);
    }

    for (const Application::SubPrepPrintClass& record : source.classes())
    {
        const auto id = legacyId(record.id.value());
        if (!id)
        {
            return Domain::Result<
                QList<SubPrepClassInformation::TeacherGroup>
                >::failure(
                mappingError(
                    "A Sub Prep print class identifier cannot be represented by the current renderer."
                    )
                );
        }

        visibleClassIds.insert(*id);
        SubPrepClassInformation::SourceClass sourceClass;
        sourceClass.classroom = Classroom({}, *id);
        sourceClass.info.classId = *id;
        sourceClass.info.classGrade = fromUtf8(record.grade);
        sourceClass.info.classLevel = fromUtf8(record.level);
        sourceClass.info.notes = fromUtf8(record.classNotes);
        sourceClass.info.classColor = fromUtf8(record.classColor);
        sourceClass.info.fontColor = fromUtf8(record.fontColor);
        sourceClass.studentCount = static_cast<int>(record.studentCount);

        if (record.teacherId.has_value())
        {
            const auto teacherId = legacyId(record.teacherId->value());
            if (!teacherId)
            {
                return Domain::Result<
                    QList<SubPrepClassInformation::TeacherGroup>
                    >::failure(
                    mappingError(
                        "A Sub Prep print teacher identifier cannot be represented by the current renderer."
                        )
                    );
            }

            const auto teacher = teachersById.find(record.teacherId->value());
            if (teacher == teachersById.end())
            {
                return Domain::Result<
                    QList<SubPrepClassInformation::TeacherGroup>
                    >::failure(
                    mappingError(
                        "A Sub Prep print class references a missing teacher projection."
                        )
                    );
            }

            sourceClass.info.teacherId = *teacherId;
            sourceClass.teacher = teacher->second;
        }

        QList<ClassTime>& times = request.mode
                == Application::ScheduleViewMode::Intensive
            ? sourceClass.info.intensiveTimes
            : sourceClass.info.classTimes;
        times.reserve(static_cast<qsizetype>(record.meetings.size()));
        for (const Application::SubPrepPrintMeeting& meeting : record.meetings)
        {
            const auto day = weekdayName(meeting.weekday);
            if (!day)
            {
                return Domain::Result<
                    QList<SubPrepClassInformation::TeacherGroup>
                    >::failure(
                    mappingError("A Sub Prep print meeting weekday is invalid.")
                    );
            }

            times.append({
                .day = *day,
                .startTime = fromUtf8(meeting.startTime),
                .endTime = fromUtf8(meeting.endTime)
            });
        }

        classes.append(std::move(sourceClass));
    }

    for (const Domain::ClassId& classId : request.selectedClassIds)
    {
        const auto id = legacyId(classId.value());
        if (!id)
        {
            return Domain::Result<
                QList<SubPrepClassInformation::TeacherGroup>
                >::failure(
                mappingError(
                    "A selected Sub Prep class identifier cannot be represented by the current renderer."
                    )
                );
        }
        visibleClassIds.insert(*id);
    }

    SubPrepClassInformation::BuildOptions options;
    options.visibleClassIds = std::move(visibleClassIds);
    options.visibleDays = std::move(visibleDays);
    options.useIntensive = request.mode
        == Application::ScheduleViewMode::Intensive;

    return Domain::Result<
        QList<SubPrepClassInformation::TeacherGroup>
        >::success(
        SubPrepClassInformation::build(classes, options)
        );
}
