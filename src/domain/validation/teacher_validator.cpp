#include "teacher_validator.h"

#include "classmngr/engine/teacher_validator.h"

#include <QByteArray>

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace
{
constexpr qsizetype RoomNumberMaximumLength = 64;
constexpr qsizetype BirthdayMaximumLength = 5;
constexpr qsizetype PhoneNumberMaximumLength = 32;
constexpr qsizetype CredentialMaximumLength = 128;
constexpr qsizetype PasswordMaximumLength = 256;
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

classmngr::engine::Teacher toEngine(const Teacher& teacher)
{
    classmngr::engine::Teacher result;
    result.id = teacher.id;
    result.teacherKr = toUtf8(teacher.teacherKr);
    result.teacherEn = toUtf8(teacher.teacherEn);
    result.preferredRomanization = toUtf8(teacher.preferredRomanization);
    result.preferredName = toUtf8(teacher.preferredName);
    result.roomNumber = toUtf8(teacher.roomNumber);
    result.birthday = toUtf8(teacher.birthday);
    result.phoneNumber = toUtf8(teacher.phoneNumber);
    result.wifiName = toUtf8(teacher.wifiName);
    result.wifiPassword = toUtf8(teacher.wifiPassword);
    result.internetType = toUtf8(teacher.internetType);
    result.zoomId = toUtf8(teacher.zoomId);
    result.zoomPassword = toUtf8(teacher.zoomPassword);
    result.projectionType = toUtf8(teacher.projectionType);
    result.notes = toUtf8(teacher.notes);
    return result;
}

Teacher fromEngine(const classmngr::engine::Teacher& teacher)
{
    Teacher result;
    result.id = teacher.id;
    result.teacherKr = fromUtf8(teacher.teacherKr);
    result.teacherEn = fromUtf8(teacher.teacherEn);
    result.preferredRomanization = fromUtf8(teacher.preferredRomanization);
    result.preferredName = fromUtf8(teacher.preferredName);
    result.roomNumber = fromUtf8(teacher.roomNumber);
    result.birthday = fromUtf8(teacher.birthday);
    result.phoneNumber = fromUtf8(teacher.phoneNumber);
    result.wifiName = fromUtf8(teacher.wifiName);
    result.wifiPassword = fromUtf8(teacher.wifiPassword);
    result.internetType = fromUtf8(teacher.internetType);
    result.zoomId = fromUtf8(teacher.zoomId);
    result.zoomPassword = fromUtf8(teacher.zoomPassword);
    result.projectionType = fromUtf8(teacher.projectionType);
    result.notes = fromUtf8(teacher.notes);
    return result;
}

void restoreLengthArguments(
    ValidationIssue& issue,
    const Teacher& teacher
    )
{
    if (issue.code != QStringLiteral("validation.length.out_of_bounds"))
    {
        return;
    }

    const QString* value = nullptr;
    qsizetype maximum = 0;
    if (issue.field == QStringLiteral("roomNumber"))
    {
        value = &teacher.roomNumber;
        maximum = RoomNumberMaximumLength;
    }
    else if (issue.field == QStringLiteral("birthday"))
    {
        value = &teacher.birthday;
        maximum = BirthdayMaximumLength;
    }
    else if (issue.field == QStringLiteral("phoneNumber"))
    {
        value = &teacher.phoneNumber;
        maximum = PhoneNumberMaximumLength;
    }
    else if (issue.field == QStringLiteral("wifiName"))
    {
        value = &teacher.wifiName;
        maximum = CredentialMaximumLength;
    }
    else if (issue.field == QStringLiteral("wifiPassword"))
    {
        value = &teacher.wifiPassword;
        maximum = PasswordMaximumLength;
    }
    else if (issue.field == QStringLiteral("zoomId"))
    {
        value = &teacher.zoomId;
        maximum = CredentialMaximumLength;
    }
    else if (issue.field == QStringLiteral("zoomPassword"))
    {
        value = &teacher.zoomPassword;
        maximum = PasswordMaximumLength;
    }
    else if (issue.field == QStringLiteral("notes"))
    {
        value = &teacher.notes;
        maximum = NotesMaximumLength;
    }

    if (value == nullptr)
    {
        return;
    }

    issue.arguments = {
        {QStringLiteral("length"), static_cast<qlonglong>(value->size())},
        {QStringLiteral("minimum"), 0},
        {QStringLiteral("maximum"), static_cast<qlonglong>(maximum)}
    };
}

ValidationResult fromEngine(
    const classmngr::engine::ValidationResult& validation,
    const Teacher* teacher = nullptr
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
        if (teacher != nullptr)
        {
            restoreLengthArguments(issue, *teacher);
        }
        result.add(std::move(issue));
    }

    return result;
}
} // namespace

Teacher TeacherValidator::normalized(const Teacher& teacher)
{
    return fromEngine(
        classmngr::engine::TeacherValidator::normalized(toEngine(teacher))
        );
}

QString TeacherValidator::normalizedPhoneNumber(const QString& value)
{
    const std::string normalized =
        classmngr::engine::TeacherValidator::normalizedPhoneNumber(
            toUtf8(value)
            );
    return fromUtf8(normalized);
}

ValidationResult TeacherValidator::validate(const Teacher& teacher)
{
    return fromEngine(
        classmngr::engine::TeacherValidator::validate(toEngine(teacher)),
        &teacher
        );
}
