#include "classmngr/engine/open_database.h"
#include "classmngr/engine/teacher_service.h"
#include "classmngr/engine/teacher_validator.h"

#include <cstdint>
#include <iostream>
#include <string_view>

namespace
{
using classmngr::engine::ErrorCode;
using classmngr::engine::OpenDatabase;
using classmngr::engine::SqliteParameters;
using classmngr::engine::SqliteValue;
using classmngr::engine::Teacher;
using classmngr::engine::TeacherService;
using classmngr::engine::TeacherValidator;
using classmngr::engine::ValidationSeverity;

bool expect(
    bool condition,
    std::string_view message
    )
{
    if (condition)
    {
        return true;
    }

    std::cerr << "ClassMngrEngineTeacherServiceTests: "
              << message
              << '\n';
    return false;
}

bool hasIssue(
    const classmngr::engine::ValidationResult& validation,
    std::string_view code,
    ValidationSeverity severity
    )
{
    for (const auto& issue : validation.issues())
    {
        if (issue.code == code && issue.severity == severity)
        {
            return true;
        }
    }

    return false;
}

Teacher validTeacher()
{
    Teacher teacher;
    teacher.teacherKr = " 홍 길동 ";
    teacher.teacherEn = " jane  doe ";
    teacher.preferredRomanization = "Jane Doe";
    teacher.preferredName = "JANE DOE";
    teacher.roomNumber = " Room 1 ";
    teacher.birthday = "02-29";
    teacher.phoneNumber = "010 1234 5678";
    teacher.wifiName = " Wifi ";
    teacher.wifiPassword = " secret ";
    teacher.internetType = "wifi";
    teacher.zoomId = " zoom-id ";
    teacher.zoomPassword = " zoom-secret ";
    teacher.projectionType = "zoom";
    teacher.notes = " notes ";
    return teacher;
}

bool isNullValue(
    const classmngr::engine::SqliteValue& value
    )
{
    return std::holds_alternative<std::monostate>(value);
}
} // namespace

int main()
{
    bool passed = true;

    const Teacher normalized = TeacherValidator::normalized(validTeacher());
    passed &= expect(
        normalized.teacherKr == "홍길동",
        "Korean teacher name was not normalized"
        );
    passed &= expect(
        normalized.teacherEn == "Jane Doe"
            && normalized.preferredRomanization == "Jane Doe"
            && normalized.preferredName == "Jane Doe",
        "English teacher names or preferred name were not normalized"
        );
    passed &= expect(
        normalized.phoneNumber == "010-1234-5678"
            && normalized.internetType == "WiFi"
            && normalized.projectionType == "Zoom",
        "teacher contact or enum values were not canonicalized"
        );
    passed &= expect(
        normalized.preferredNameChoices().size() == 1
            && normalized.preferredDisplayName() == "Jane Doe",
        "preferred teacher display-name choices were not derived correctly"
        );

    Teacher twoSyllable;
    twoSyllable.teacherKr = "김수";
    twoSyllable.teacherEn = "Kim";
    const auto questionable = TeacherValidator::validate(
        TeacherValidator::normalized(twoSyllable)
        );
    passed &= expect(
        questionable.isValid()
            && hasIssue(
                questionable,
                "student_name.korean.unusual_length",
                ValidationSeverity::Warning
                ),
        "questionable Korean name length was not retained as a warning"
        );

    Teacher invalid;
    invalid.teacherEn = "Bad@Name";
    invalid.birthday = "02-31";
    invalid.phoneNumber = "123";
    invalid.internetType = "ethernet";
    const auto invalidValidation = TeacherValidator::validate(
        TeacherValidator::normalized(invalid)
        );
    passed &= expect(
        !invalidValidation.isValid()
            && hasIssue(
                invalidValidation,
                "student_name.english.invalid_characters",
                ValidationSeverity::Error
                )
            && hasIssue(
                invalidValidation,
                "teacher.birthday.invalid",
                ValidationSeverity::Error
                )
            && hasIssue(
                invalidValidation,
                "teacher.phone.invalid",
                ValidationSeverity::Error
                )
            && hasIssue(
                invalidValidation,
                "validation.enum.invalid_value",
                ValidationSeverity::Error
                ),
        "invalid teacher values did not produce typed validation issues"
        );

    const auto opened = OpenDatabase::execute(":memory:");
    passed &= expect(
        opened && *opened != nullptr,
        "OpenDatabase failed for teacher service test"
        );
    if (!opened || *opened == nullptr)
    {
        return 1;
    }

    auto& database = **opened;
    TeacherService service(database);

    const auto created = service.create(validTeacher());
    passed &= expect(
        created && *created > 0,
        "validated teacher creation failed"
        );

    if (created)
    {
        const auto loaded = service.get(*created);
        passed &= expect(
            loaded
                && loaded->teacherKr == "홍길동"
                && loaded->teacherEn == "Jane Doe"
                && loaded->phoneNumber == "010-1234-5678"
                && loaded->internetType == "WiFi"
                && loaded->projectionType == "Zoom",
            "teacher round trip did not preserve normalized UTF-8 data"
            );

        const auto listed = service.list();
        passed &= expect(
            listed && listed->size() == 1 && listed->front().id == *created,
            "teacher listing did not return the created teacher"
            );

        Teacher updated = *loaded;
        updated.teacherEn = " john  smith ";
        updated.preferredRomanization.clear();
        updated.preferredName = "JOHN SMITH";
        passed &= expect(
            service.save(updated) && service.get(*created)
                && service.get(*created)->teacherEn == "John Smith"
                && service.get(*created)->preferredName == "John Smith",
            "teacher save did not use the validated update path"
            );

        passed &= expect(
            database.execute(
                "INSERT INTO classes (name) VALUES (?)",
                SqliteParameters{
                    SqliteValue{std::string("Teacher class")}
                }
                ).has_value(),
            "teacher dependency class could not be created"
            );
        const auto classId = database.query("SELECT last_insert_rowid()");
        if (classId
            && classId->rows.size() == 1
            && classId->rows.front().values.size() == 1
            && std::get_if<std::int64_t>(
                &classId->rows.front().values.front()) != nullptr)
        {
            const auto* value = std::get_if<std::int64_t>(
                &classId->rows.front().values.front()
                );
            passed &= expect(
                database.execute(
                    "INSERT INTO class_info (class_id, teacher_id) "
                    "VALUES (?, ?)",
                    SqliteParameters{
                        SqliteValue{*value},
                        SqliteValue{std::int64_t{*created}}
                    }
                    ).has_value(),
                "teacher dependency class information could not be created"
                );
        }
        else
        {
            passed &= expect(
                false,
                "teacher dependency class id was not returned"
                );
        }

        passed &= expect(
            service.remove(*created).has_value(),
            "teacher deletion failed"
            );
        const auto deleted = service.get(*created);
        passed &= expect(
            !deleted && deleted.error().code == ErrorCode::NotFound,
            "deleted teacher did not return a typed not-found error"
            );
        const auto assignments = database.query(
            "SELECT teacher_id FROM class_info"
            );
        passed &= expect(
            assignments && assignments->rows.size() == 1
                && assignments->rows.front().values.size() == 1
                && isNullValue(assignments->rows.front().values.front()),
            "teacher deletion did not clear class assignments"
            );
    }

    const auto rejected = service.create(invalid);
    passed &= expect(
        !rejected && rejected.error().code == ErrorCode::InvalidFormat,
        "invalid teacher creation was not rejected at the use-case boundary"
        );
    passed &= expect(
        !service.get(0) && service.get(0).error().code == ErrorCode::InvalidArgument,
        "invalid teacher id was not rejected"
        );

    return passed ? 0 : 1;
}
