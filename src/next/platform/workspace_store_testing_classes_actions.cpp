#include "next/platform/workspace_store_testing_classes_actions.h"

#include "data/database/workspace_store.h"
#include "data/repositories/teacher_repository.h"
#include "data/repositories/testing_class_repository.h"
#include "domain/models/testing_class.h"
#include "next/application/positive_decimal_id_validation.h"
#include "next/application/testing_class_create_use_case.h"
#include "next/application/testing_class_delete_use_case.h"
#include "next/application/testing_class_details_update_use_case.h"
#include "next/platform/repository_error_code.h"

#include <QByteArray>
#include <QString>

#include <charconv>
#include <exception>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace ClassMngr::Next::Platform
{
namespace
{

[[nodiscard]] std::string utf8(const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    return {bytes.constData(), static_cast<std::size_t>(bytes.size())};
}

[[nodiscard]] Domain::OperationError repositoryError(
    const RepositoryError& source
    )
{
    const std::string message = utf8(source.diagnostic);
    return {
        .code = repositoryDomainErrorCode(source.code),
        .message = message.empty()
            ? std::string("The testing classes data could not be accessed.")
            : message,
        .recoverable = source.recoverableOverride.value_or(
            source.code != RepositoryErrorCode::Technical)
    };
}

[[nodiscard]] Domain::OperationError genericError(
    const Domain::ErrorCode code,
    std::string message,
    const bool recoverable = true
    )
{
    return {
        .code = code,
        .message = std::move(message),
        .recoverable = recoverable
    };
}

[[nodiscard]] std::optional<int> legacyClassId(const Domain::ClassId& id)
{
    return Application::parseCanonicalPositiveDecimalId(id.value());
}

[[nodiscard]] std::optional<qint64> legacyTeacherId(
    const std::optional<Domain::TeacherId>& id
    )
{
    if (!id)
    {
        return std::nullopt;
    }
    const auto parsed = Application::parseCanonicalPositiveDecimalId64(
        id->value());
    return parsed ? std::optional<qint64>(static_cast<qint64>(*parsed))
                  : std::nullopt;
}

[[nodiscard]] QString text(const std::u16string& value)
{
    return QString::fromUtf16(value.data(), static_cast<qsizetype>(value.size()));
}

[[nodiscard]] std::optional<QString> assignmentWeekday(
    const Domain::Weekday weekday
    )
{
    switch (weekday)
    {
    case Domain::Weekday::Monday: return QStringLiteral("Monday");
    case Domain::Weekday::Tuesday: return QStringLiteral("Tuesday");
    case Domain::Weekday::Wednesday: return QStringLiteral("Wednesday");
    case Domain::Weekday::Thursday: return QStringLiteral("Thursday");
    case Domain::Weekday::Friday: return QStringLiteral("Friday");
    case Domain::Weekday::Saturday: return QStringLiteral("Saturday");
    case Domain::Weekday::Sunday: return QStringLiteral("Sunday");
    }
    return std::nullopt;
}

[[nodiscard]] QString assignmentStartTime(const int minute)
{
    return QStringLiteral("%1:%2")
        .arg(minute / 60, 2, 10, QLatin1Char('0'))
        .arg(minute % 60, 2, 10, QLatin1Char('0'));
}

[[nodiscard]] bool weekdayCaseInsensitiveEquals(
    const std::u16string_view left,
    const std::u16string_view right
    ) noexcept
{
    if (left.size() != right.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index)
    {
        const auto lowerAscii = [](char16_t value) {
            return value >= u'A' && value <= u'Z'
                ? static_cast<char16_t>(value + (u'a' - u'A'))
                : value;
        };
        if (lowerAscii(left[index]) != lowerAscii(right[index]))
        {
            return false;
        }
    }
    return true;
}

class TestingClassChoicesPort final
    : public Application::ScheduleTestingClassChoicesReadPort
{
public:
    explicit TestingClassChoicesPort(TestingClassRepository* repository)
        : m_repository(repository)
    {
    }

    [[nodiscard]] Application::ScheduleTestingClassChoicesReadResult
    readTestingClassChoices(
        const Application::ScheduleTestingClassChoicesReadQuery&
        ) const override
    {
        if (!m_repository)
        {
            return Application::ScheduleTestingClassChoicesReadResult::failure(
                genericError(Domain::ErrorCode::NotFound,
                    "The testing classes repository is unavailable."));
        }
        try
        {
            const auto loaded = m_repository->loadTestingClassChoices();
            if (!loaded)
            {
                return Application::ScheduleTestingClassChoicesReadResult::failure(
                    repositoryError(loaded.error()));
            }
            Application::ScheduleTestingClassChoicesSnapshot snapshot;
            snapshot.choices.reserve(static_cast<std::size_t>(loaded->size()));
            for (const TestingClassChoiceRecord& choice : *loaded)
            {
                const auto classId = Domain::ClassId::fromString(
                    std::to_string(choice.classId));
                if (!classId || choice.classId <= 0)
                {
                    return Application::ScheduleTestingClassChoicesReadResult::failure(
                        genericError(Domain::ErrorCode::Validation,
                            "A testing class has an invalid identifier.", false));
                }
                snapshot.choices.push_back({
                    .classId = *classId,
                    .name = choice.name.toStdU16String(),
                    .grade = choice.grade.toStdU16String(),
                    .level = choice.level.toStdU16String(),
                    .room = choice.room.toStdU16String()
                });
            }
            return Application::ScheduleTestingClassChoicesReadResult::success(
                std::move(snapshot));
        }
        catch (const std::exception&)
        {
            return Application::ScheduleTestingClassChoicesReadResult::failure(
                genericError(Domain::ErrorCode::Technical,
                    "Testing classes could not be loaded."));
        }
        catch (...)
        {
            return Application::ScheduleTestingClassChoicesReadResult::failure(
                genericError(Domain::ErrorCode::Technical,
                    "Testing classes could not be loaded."));
        }
    }

private:
    TestingClassRepository* m_repository = nullptr;
};

class TestingClassDetailsPort final
    : public Application::TestingClassDetailsReadPort
{
public:
    explicit TestingClassDetailsPort(TestingClassRepository* repository)
        : m_repository(repository)
    {
    }

    [[nodiscard]] Application::TestingClassDetailsReadResult
    readTestingClassDetails(
        const Application::TestingClassDetailsReadQuery& request
        ) const override
    {
        const auto classId = legacyClassId(request.classId);
        if (!classId || !m_repository)
        {
            return Application::TestingClassDetailsReadResult::failure(
                genericError(classId ? Domain::ErrorCode::NotFound
                                     : Domain::ErrorCode::InvalidInput,
                    "Testing class details could not be loaded."));
        }
        try
        {
            const auto loaded = m_repository->loadTestingClassDetailsReadRecordDetailed(
                *classId);
            if (!loaded)
            {
                return Application::TestingClassDetailsReadResult::failure(
                    repositoryError(loaded.error()));
            }
            const auto teacherId = loaded->teacherId > 0
                ? Domain::TeacherId::fromString(std::to_string(loaded->teacherId))
                : std::optional<Domain::TeacherId>{};
            if (loaded->teacherId > 0 && !teacherId)
            {
                return Application::TestingClassDetailsReadResult::failure(
                    genericError(Domain::ErrorCode::Validation,
                        "Testing class details contain an invalid teacher identifier.",
                        false));
            }
            return Application::TestingClassDetailsReadResult::success({
                .classId = request.classId,
                .name = loaded->name.toStdU16String(),
                .grade = loaded->grade.toStdU16String(),
                .level = loaded->level.toStdU16String(),
                .room = loaded->room.toStdU16String(),
                .teacherId = teacherId,
                .classColor = loaded->classColor.toStdU16String(),
                .fontColor = loaded->fontColor.toStdU16String(),
                .notes = loaded->notes.toStdU16String()
            });
        }
        catch (const std::exception&)
        {
            return Application::TestingClassDetailsReadResult::failure(
                genericError(Domain::ErrorCode::Technical,
                    "Testing class details could not be loaded."));
        }
        catch (...)
        {
            return Application::TestingClassDetailsReadResult::failure(
                genericError(Domain::ErrorCode::Technical,
                    "Testing class details could not be loaded."));
        }
    }

private:
    TestingClassRepository* m_repository = nullptr;
};

class TestingTeacherChoicesPort final
    : public Application::TestingTeacherChoicesReadPort
{
public:
    explicit TestingTeacherChoicesPort(TeacherRepository* repository)
        : m_repository(repository)
    {
    }

    [[nodiscard]] Application::TestingTeacherChoicesReadResult
    readTestingTeacherChoices(
        const Application::TestingTeacherChoicesReadQuery&
        ) const override
    {
        if (!m_repository)
        {
            return Application::TestingTeacherChoicesReadResult::failure(
                genericError(Domain::ErrorCode::NotFound,
                    "The teachers repository is unavailable."));
        }
        try
        {
            const auto loaded = m_repository->loadTestingTeacherChoiceRecords();
            if (!loaded)
            {
                return Application::TestingTeacherChoicesReadResult::failure(
                    repositoryError(loaded.error()));
            }
            Application::TestingTeacherChoicesSnapshot snapshot;
            snapshot.choices.reserve(static_cast<std::size_t>(loaded->size()));
            for (const TestingTeacherChoiceReadRecord& teacher : *loaded)
            {
                if (teacher.teacherId <= 0)
                {
                    return Application::TestingTeacherChoicesReadResult::failure(
                        genericError(Domain::ErrorCode::Validation,
                            "A teacher has an invalid identifier.", false));
                }
                const auto teacherId = Domain::TeacherId::fromString(
                    std::to_string(teacher.teacherId));
                if (!teacherId)
                {
                    return Application::TestingTeacherChoicesReadResult::failure(
                        genericError(Domain::ErrorCode::Validation,
                            "A teacher has an invalid identifier.", false));
                }
                snapshot.choices.push_back({
                    .teacherId = *teacherId,
                    .name = teacher.teacherKr.toStdU16String(),
                    .room = teacher.room.toStdU16String()
                });
            }
            return Application::TestingTeacherChoicesReadResult::success(
                std::move(snapshot));
        }
        catch (const std::exception&)
        {
            return Application::TestingTeacherChoicesReadResult::failure(
                genericError(Domain::ErrorCode::Technical,
                    "Teachers could not be loaded."));
        }
        catch (...)
        {
            return Application::TestingTeacherChoicesReadResult::failure(
                genericError(Domain::ErrorCode::Technical,
                    "Teachers could not be loaded."));
        }
    }

private:
    TeacherRepository* m_repository = nullptr;
};

class TestingClassDetailsUpdatePort final
    : public Application::TestingClassDetailsUpdatePort
{
public:
    explicit TestingClassDetailsUpdatePort(TestingClassRepository* repository)
        : m_repository(repository)
    {
    }

    [[nodiscard]] Application::TestingClassDetailsUpdateResult
    updateTestingClassDetails(
        const Application::TestingClassDetailsUpdateRequest& request
        ) const override
    {
        const auto classId = legacyClassId(request.classId);
        const auto teacherId = legacyTeacherId(request.teacherId);
        if (!classId || (request.teacherId && !teacherId) || !m_repository)
        {
            return Application::TestingClassDetailsUpdateResult::failure(
                genericError(!classId || (request.teacherId && !teacherId)
                        ? Domain::ErrorCode::InvalidInput
                        : Domain::ErrorCode::NotFound,
                    "Testing class details could not be saved."));
        }
        try
        {
            TestingClass value;
            value.classId = *classId;
            value.name = text(request.name);
            value.grade = text(request.grade);
            value.level = text(request.level);
            value.room = text(request.room);
            value.teacherId = teacherId.value_or(-1);
            value.classColor = text(request.classColor);
            value.fontColor = text(request.fontColor);
            value.notes = text(request.notes);
            const RepositoryStatus saved =
                m_repository->updateTestingClassDetailed(value);
            if (!saved)
            {
                return Application::TestingClassDetailsUpdateResult::failure(
                    repositoryError(saved.error()));
            }
            return Application::TestingClassDetailsUpdateResult::success();
        }
        catch (const std::exception&)
        {
            return Application::TestingClassDetailsUpdateResult::failure(
                genericError(Domain::ErrorCode::Technical,
                    "Testing class details could not be saved."));
        }
        catch (...)
        {
            return Application::TestingClassDetailsUpdateResult::failure(
                genericError(Domain::ErrorCode::Technical,
                    "Testing class details could not be saved."));
        }
    }

private:
    TestingClassRepository* m_repository = nullptr;
};

class TestingClassCreatePort final : public Application::TestingClassCreatePort
{
public:
    explicit TestingClassCreatePort(TestingClassRepository* repository)
        : m_repository(repository)
    {
    }

    [[nodiscard]] Application::TestingClassCreateResult createTestingClass(
        const Application::TestingClassCreateRequest& request
        ) const override
    {
        const auto teacherId = legacyTeacherId(request.teacherId);
        if ((request.teacherId && !teacherId) || !m_repository)
        {
            return Application::TestingClassCreateResult::failure(
                genericError(request.teacherId && !teacherId
                        ? Domain::ErrorCode::InvalidInput
                        : Domain::ErrorCode::NotFound,
                    "Testing class could not be created."));
        }
        try
        {
            TestingClass value;
            value.name = text(request.name);
            value.grade = text(request.grade);
            value.level = text(request.level);
            value.room = text(request.room);
            value.teacherId = teacherId.value_or(-1);
            value.classColor = text(request.classColor);
            value.fontColor = text(request.fontColor);
            value.notes = text(request.notes);

            QString assignmentDay;
            QString assignmentStart;
            if (request.pendingScheduleSlot)
            {
                const auto weekday = assignmentWeekday(
                    request.pendingScheduleSlot->weekday);
                if (!weekday)
                {
                    return Application::TestingClassCreateResult::failure(
                        genericError(Domain::ErrorCode::InvalidInput,
                            "A testing assignment requires a valid weekday."));
                }
                assignmentDay = *weekday;
                assignmentStart = assignmentStartTime(
                    request.pendingScheduleSlot->startMinute.rawValue());
            }
            else
            {
                assignmentDay = request.assignmentDay
                    ? text(*request.assignmentDay) : QString{};
                assignmentStart = request.assignmentStartTime
                    ? text(*request.assignmentStartTime) : QString{};
            }
            const auto created = m_repository->createTestingClass(
                value, assignmentDay, assignmentStart);
            if (!created)
            {
                return Application::TestingClassCreateResult::failure(
                    repositoryError(created.error()));
            }
            const auto classId = Domain::ClassId::fromString(
                std::to_string(*created));
            if (!classId || *created <= 0)
            {
                return Application::TestingClassCreateResult::failure(
                    genericError(Domain::ErrorCode::Technical,
                        "Creating the testing class returned an invalid identifier.",
                        false));
            }
            return Application::TestingClassCreateResult::success(*classId);
        }
        catch (const std::exception&)
        {
            return Application::TestingClassCreateResult::failure(
                genericError(Domain::ErrorCode::Technical,
                    "Testing class could not be created."));
        }
        catch (...)
        {
            return Application::TestingClassCreateResult::failure(
                genericError(Domain::ErrorCode::Technical,
                    "Testing class could not be created."));
        }
    }

private:
    TestingClassRepository* m_repository = nullptr;
};

class TestingClassDeletePort final : public Application::TestingClassDeletePort
{
public:
    explicit TestingClassDeletePort(TestingClassRepository* repository)
        : m_repository(repository)
    {
    }

    [[nodiscard]] Application::TestingClassDeleteResult deleteTestingClass(
        const Application::TestingClassDeleteRequest& request
        ) const override
    {
        const auto classId = legacyClassId(request.classId);
        if (!classId || !m_repository)
        {
            return Application::TestingClassDeleteResult::failure(
                genericError(classId ? Domain::ErrorCode::NotFound
                                     : Domain::ErrorCode::InvalidInput,
                    "Testing class could not be deleted."));
        }
        try
        {
            const RepositoryStatus deleted =
                m_repository->deleteTestingClassDetailed(*classId);
            if (!deleted)
            {
                return Application::TestingClassDeleteResult::failure(
                    repositoryError(deleted.error()));
            }
            return Application::TestingClassDeleteResult::success();
        }
        catch (const std::exception&)
        {
            return Application::TestingClassDeleteResult::failure(
                genericError(Domain::ErrorCode::Technical,
                    "Testing class could not be deleted."));
        }
        catch (...)
        {
            return Application::TestingClassDeleteResult::failure(
                genericError(Domain::ErrorCode::Technical,
                    "Testing class could not be deleted."));
        }
    }

private:
    TestingClassRepository* m_repository = nullptr;
};

template <typename Result>
[[nodiscard]] Result openedFailure(const RepositoryStatus& opened)
{
    return Result::failure(repositoryError(opened.error()));
}

} // namespace

Application::ScheduleTestingClassChoicesReadResult
readTestingClassesAtPath(const QString& databasePath)
{
    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        return openedFailure<Application::ScheduleTestingClassChoicesReadResult>(
            opened);
    }
    TestingClassChoicesPort port(store.testingClassRepository());
    return Application::ScheduleTestingClassChoicesReadQueryHandler::execute(
        {}, port);
}

Application::TestingClassDetailsReadResult readTestingClassDetailsAtPath(
    const QString& databasePath,
    const Domain::ClassId& classId
    )
{
    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        return openedFailure<Application::TestingClassDetailsReadResult>(opened);
    }
    TestingClassDetailsPort port(store.testingClassRepository());
    return Application::TestingClassDetailsReadQueryHandler::execute(
        {.classId = classId}, port);
}

Application::TestingTeacherChoicesReadResult readTestingTeacherChoicesAtPath(
    const QString& databasePath
    )
{
    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        return openedFailure<Application::TestingTeacherChoicesReadResult>(opened);
    }
    TestingTeacherChoicesPort port(store.teacherRepository());
    return Application::TestingTeacherChoicesReadQueryHandler::execute({}, port);
}

Application::TestingClassDetailsUpdateResult updateTestingClassDetailsAtPath(
    const QString& databasePath,
    const Application::TestingClassDetailsUpdateRequest& request
    )
{
    const Domain::Result<void> valid = request.validate();
    if (!valid)
    {
        return Application::TestingClassDetailsUpdateResult::failure(valid.error());
    }
    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        return openedFailure<Application::TestingClassDetailsUpdateResult>(opened);
    }
    TestingClassDetailsUpdatePort port(store.testingClassRepository());
    return Application::TestingClassDetailsUpdateUseCase::execute(request, port);
}

Application::TestingClassCreateResult createTestingClassAtPath(
    const QString& databasePath,
    const Application::TestingClassCreateRequest& request
    )
{
    const Domain::Result<void> valid = request.validate(
        weekdayCaseInsensitiveEquals);
    if (!valid)
    {
        return Application::TestingClassCreateResult::failure(valid.error());
    }
    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        return openedFailure<Application::TestingClassCreateResult>(opened);
    }
    TestingClassCreatePort port(store.testingClassRepository());
    return Application::TestingClassCreateUseCase::execute(
        request, port, weekdayCaseInsensitiveEquals);
}

Application::TestingClassDeleteResult deleteTestingClassAtPath(
    const QString& databasePath,
    const Application::TestingClassDeleteRequest& request
    )
{
    const Domain::Result<void> valid = request.validate();
    if (!valid)
    {
        return Application::TestingClassDeleteResult::failure(valid.error());
    }
    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        return openedFailure<Application::TestingClassDeleteResult>(opened);
    }
    TestingClassDeletePort port(store.testingClassRepository());
    return Application::TestingClassDeleteUseCase::execute(request, port);
}

} // namespace ClassMngr::Next::Platform
