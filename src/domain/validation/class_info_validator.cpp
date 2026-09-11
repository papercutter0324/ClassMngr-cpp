#include "class_info_validator.h"

#include "classmngr/engine/class_info_validator.h"

#include <QByteArray>

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace
{
constexpr qsizetype NotesMaximumLength = 10000;

std::string toUtf8(const QString& value)
{
    const QByteArray encoded = value.toUtf8();
    return {
        encoded.constData(),
        static_cast<std::size_t>(encoded.size())
    };
}

QString fromUtf8(std::string_view value)
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

classmngr::engine::ClassTime toEngine(const ClassTime& time)
{
    return {
        .day = toUtf8(time.day),
        .startTime = toUtf8(time.startTime),
        .endTime = toUtf8(time.endTime)
    };
}

ClassTime fromEngine(const classmngr::engine::ClassTime& time)
{
    return {
        .day = fromUtf8(time.day),
        .startTime = fromUtf8(time.startTime),
        .endTime = fromUtf8(time.endTime)
    };
}

classmngr::engine::ClassInfo toEngine(const ClassInfo& info)
{
    classmngr::engine::ClassInfo result;
    result.classId = info.classId;
    result.teacherId = info.teacherId;
    result.teacherKr = toUtf8(info.teacherKr);
    result.teacherEn = toUtf8(info.teacherEn);
    result.teacherPreferredName = toUtf8(info.teacherPreferredName);
    result.roomNumber = toUtf8(info.roomNumber);
    result.wifiName = toUtf8(info.wifiName);
    result.wifiPassword = toUtf8(info.wifiPassword);
    result.internetType = toUtf8(info.internetType);
    result.zoomId = toUtf8(info.zoomId);
    result.zoomPassword = toUtf8(info.zoomPassword);
    result.projectionType = toUtf8(info.projectionType);
    result.classGrade = toUtf8(info.classGrade);
    result.classLevel = toUtf8(info.classLevel);
    result.readingBook = toUtf8(info.readingBook);
    result.essayBook = toUtf8(info.essayBook);
    result.classColor = toUtf8(info.classColor);
    result.fontColor = toUtf8(info.fontColor);

    result.classTimes.reserve(
        static_cast<std::size_t>(info.classTimes.size())
        );
    for (const ClassTime& time : info.classTimes)
    {
        result.classTimes.push_back(toEngine(time));
    }

    result.intensiveTimes.reserve(
        static_cast<std::size_t>(info.intensiveTimes.size())
        );
    for (const ClassTime& time : info.intensiveTimes)
    {
        result.intensiveTimes.push_back(toEngine(time));
    }

    result.notes = toUtf8(info.notes);
    result.timeFillerActivities = toUtf8(info.timeFillerActivities);
    return result;
}

ClassInfo fromEngine(const classmngr::engine::ClassInfo& info)
{
    ClassInfo result;
    result.classId = info.classId;
    result.teacherId = info.teacherId;
    result.teacherKr = fromUtf8(info.teacherKr);
    result.teacherEn = fromUtf8(info.teacherEn);
    result.teacherPreferredName = fromUtf8(info.teacherPreferredName);
    result.roomNumber = fromUtf8(info.roomNumber);
    result.wifiName = fromUtf8(info.wifiName);
    result.wifiPassword = fromUtf8(info.wifiPassword);
    result.internetType = fromUtf8(info.internetType);
    result.zoomId = fromUtf8(info.zoomId);
    result.zoomPassword = fromUtf8(info.zoomPassword);
    result.projectionType = fromUtf8(info.projectionType);
    result.classGrade = fromUtf8(info.classGrade);
    result.classLevel = fromUtf8(info.classLevel);
    result.readingBook = fromUtf8(info.readingBook);
    result.essayBook = fromUtf8(info.essayBook);
    result.classColor = fromUtf8(info.classColor);
    result.fontColor = fromUtf8(info.fontColor);

    for (const classmngr::engine::ClassTime& time : info.classTimes)
    {
        result.classTimes.append(fromEngine(time));
    }
    for (const classmngr::engine::ClassTime& time : info.intensiveTimes)
    {
        result.intensiveTimes.append(fromEngine(time));
    }

    result.notes = fromUtf8(info.notes);
    result.timeFillerActivities = fromUtf8(info.timeFillerActivities);
    return result;
}

void restoreLengthArguments(
    ValidationIssue& issue,
    const ClassInfo& info
    )
{
    if (issue.code != QStringLiteral("validation.length.out_of_bounds"))
    {
        return;
    }

    const QString* value = nullptr;
    if (issue.field == QStringLiteral("notes"))
    {
        value = &info.notes;
    }
    else if (issue.field == QStringLiteral("timeFillerActivities"))
    {
        value = &info.timeFillerActivities;
    }

    if (value == nullptr)
    {
        return;
    }

    issue.arguments = {
        {QStringLiteral("length"), static_cast<qlonglong>(value->size())},
        {QStringLiteral("minimum"), 0},
        {QStringLiteral("maximum"), static_cast<qlonglong>(NotesMaximumLength)}
    };
}

ValidationResult fromEngine(
    const classmngr::engine::ValidationResult& validation,
    const ClassInfo* info = nullptr
    )
{
    ValidationResult result;
    for (const classmngr::engine::ValidationIssue& source :
         validation.issues())
    {
        ValidationIssue issue{
            .code = fromUtf8(source.code),
            .field = fromUtf8(source.field),
            .row = source.row,
            .column = source.column,
            .severity = source.isWarning()
                ? ValidationSeverity::Warning
                : ValidationSeverity::Error
        };
        if (info != nullptr)
        {
            restoreLengthArguments(issue, *info);
        }
        result.add(std::move(issue));
    }

    return result;
}
} // namespace

ClassInfo ClassInfoValidator::normalized(const ClassInfo& info)
{
    return fromEngine(
        classmngr::engine::ClassInfoValidator::normalized(toEngine(info))
        );
}

ValidationResult ClassInfoValidator::validate(const ClassInfo& info)
{
    return fromEngine(
        classmngr::engine::ClassInfoValidator::validate(toEngine(info)),
        &info
        );
}

ValidationResult ClassInfoValidator::validateNotes(
    int classId,
    const QString& notes,
    const QString& timeFillerActivities
    )
{
    ClassInfo info;
    info.classId = classId;
    info.notes = notes;
    info.timeFillerActivities = timeFillerActivities;

    return fromEngine(
        classmngr::engine::ClassInfoValidator::validateNotes(
            classId,
            toUtf8(notes),
            toUtf8(timeFillerActivities)
            ),
        &info
        );
}
