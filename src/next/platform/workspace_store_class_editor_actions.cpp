#include "next/platform/workspace_store_class_editor_actions.h"

#include "core/enums/schedule_type.h"
#include "data/database/workspace_store.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/class_repository.h"
#include "data/repositories/roster_repository.h"
#include "data/repositories/teacher_repository.h"
#include "domain/config/class_details_catalog.h"
#include "domain/models/class_info.h"
#include "domain/models/teacher.h"
#include "next/application/class_co_teacher_page_read_query.h"
#include "next/application/class_co_teacher_teacher_choices_read_query.h"
#include "next/application/classes_list_read_snapshot.h"
#include "next/application/class_create_use_case.h"
#include "next/application/class_delete_use_case.h"
#include "next/application/class_details_page_query.h"
#include "next/application/class_notes_page_read_query.h"
#include "next/application/class_notes_save_use_case.h"
#include "next/application/positive_decimal_id_validation.h"
#include "next/platform/repository_error_code.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QTime>

#include <charconv>
#include <exception>
#include <limits>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Platform
{
namespace
{

constexpr int kMaximumCoTeacherChoices = 4'096;
constexpr int kMaximumCoTeacherNameBytes = 4'096;
constexpr qint64 kMaximumTotalCoTeacherNameBytes = 4 * 1024 * 1024;
constexpr int kMaximumCoTeacherProfileFieldBytes = 16 * 1024;
constexpr qint64 kMaximumCoTeacherProfileBytes = 64 * 1024;
constexpr int kMaximumClassNotesMetadataFieldCodeUnits = 256;
constexpr int kMaximumClassNotesScheduleTokenCodeUnits = 64;
constexpr int kMaximumClassNotesTeacherNameFieldCodeUnits = 4'096;

[[nodiscard]] std::optional<int> legacyClassId(const Domain::ClassId& id)
{
    const std::string& value = id.value();
    int parsed = 0;
    const auto [end, error] = std::from_chars(
        value.data(), value.data() + value.size(), parsed, 10);
    if (error != std::errc{} || end != value.data() + value.size()
        || parsed <= 0 || std::to_string(parsed) != value)
    {
        return std::nullopt;
    }
    return parsed;
}

[[nodiscard]] Domain::OperationError repositoryError(
    const RepositoryError& error,
    const char* fallback
    )
{
    const QByteArray diagnostic = toLegacyDiagnostic(error).toUtf8();
    return {
        .code = repositoryDomainErrorCode(error.code),
        .message = diagnostic.isEmpty()
            ? std::string(fallback)
            : std::string(diagnostic.constData(),
                static_cast<std::size_t>(diagnostic.size())),
        .recoverable = error.code != RepositoryErrorCode::Technical
    };
}

[[nodiscard]] Domain::OperationError error(
    const Domain::ErrorCode code,
    const char* message,
    const bool recoverable = true
    )
{
    return {.code = code, .message = message, .recoverable = recoverable};
}

[[nodiscard]] Domain::OperationError openedError(
    const RepositoryStatus& opened,
    const char* fallback
    )
{
    return repositoryError(opened.error(), fallback);
}

[[nodiscard]] QString fromUtf16(const std::u16string& value)
{
    return QString::fromUtf16(value.data(), static_cast<qsizetype>(value.size()));
}

[[nodiscard]] std::string utf8(const QString& value)
{
    const QByteArray bytes = value.toUtf8();
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

[[nodiscard]] std::string displayName(const TeacherDisplayNameReadRecord& record);

[[nodiscard]] Domain::Result<Application::ClassDetailsPageFields>
classFieldsFailure(const Domain::OperationError& source)
{
    return Domain::Result<Application::ClassDetailsPageFields>::failure(source);
}

[[nodiscard]] Domain::Result<std::string> teacherFailure(
    const Domain::OperationError& source
    )
{
    return Domain::Result<std::string>::failure(source);
}

[[nodiscard]] Domain::Result<Application::ClassDetailsStudentCount>
studentCountFailure(const Domain::OperationError& source)
{
    return Domain::Result<Application::ClassDetailsStudentCount>::failure(source);
}

class SnapshotClassDetailsReadPort final
    : public Application::ClassDetailsPageReadPort
{
public:
    explicit SnapshotClassDetailsReadPort(
        Application::ClassDetailsPageReadSnapshot value
        )
        : m_value(std::move(value))
    {
    }

    [[nodiscard]] Application::ClassDetailsPageReadResult readClassDetailsPage(
        const Domain::ClassId& requestedClassId
        ) override
    {
        if (m_value.classId != requestedClassId)
        {
            return Application::ClassDetailsPageReadResult::failure(error(
                Domain::ErrorCode::Validation,
                "The selected class details returned a different class identifier.",
                false));
        }
        return Application::ClassDetailsPageReadResult::success(std::move(m_value));
    }

private:
    Application::ClassDetailsPageReadSnapshot m_value;
};

class WorkspaceStoreClassNotesReadPort final
    : public Application::ClassNotesPageReadPort
{
public:
    explicit WorkspaceStoreClassNotesReadPort(QString databasePath)
        : m_databasePath(std::move(databasePath))
    {
    }

    [[nodiscard]] Application::ClassNotesPageReadResult readClassNotesPage(
        const Domain::ClassId& classId
        ) const override
    {
        const auto id = legacyClassId(classId);
        if (!id)
        {
            return Application::ClassNotesPageReadResult::failure(error(
                Domain::ErrorCode::InvalidInput,
                "Class ID must be a canonical positive integer.", false));
        }
        WorkspaceStore store;
        const RepositoryStatus opened = store.openExisting(m_databasePath);
        if (!opened)
        {
            return Application::ClassNotesPageReadResult::failure(openedError(
                opened, "The active workspace could not be opened for class notes."));
        }
        ClassInfoRepository* const infoRepository = store.classInfoRepository();
        if (!infoRepository)
        {
            return Application::ClassNotesPageReadResult::failure(error(
                Domain::ErrorCode::NotFound,
                "The class notes repository is unavailable."));
        }

        RepositoryResult<ClassNotesPageReadOutcome> loaded = [&]() {
            try
            {
                return infoRepository->loadBoundedClassNotesPageRecordDetailed(
                    *id,
                    static_cast<int>(Application::kClassNotesSaveMaxTextCodeUnits),
                    kMaximumClassNotesMetadataFieldCodeUnits,
                    kMaximumClassNotesScheduleTokenCodeUnits);
            }
            catch (const std::exception&)
            {
                return RepositoryResult<ClassNotesPageReadOutcome>(
                    std::unexpected(RepositoryError(
                        RepositoryErrorCode::Technical,
                        QStringLiteral("Class notes could not be loaded."))));
            }
            catch (...)
            {
                return RepositoryResult<ClassNotesPageReadOutcome>(
                    std::unexpected(RepositoryError(
                        RepositoryErrorCode::Technical,
                        QStringLiteral("Class notes could not be loaded."))));
            }
        }();
        if (!loaded)
        {
            return Application::ClassNotesPageReadResult::failure(repositoryError(
                loaded.error(), "Class notes could not be loaded."));
        }
        if (loaded->parentMissing)
        {
            return Application::ClassNotesPageReadResult::failure(error(
                Domain::ErrorCode::NotFound,
                "The selected class no longer exists."));
        }
        if (loaded->metadataLimitExceeded
            || loaded->notesLimitExceeded
            || loaded->timeFillerActivitiesLimitExceeded
            || loaded->regularScheduleTokenLimitExceeded)
        {
            return Application::ClassNotesPageReadResult::failure(error(
                Domain::ErrorCode::Validation,
                "Stored class notes exceed the supported text or summary limit.",
                false));
        }

        Application::ClassNotesPageFields fields{
            .classGrade = loaded->record.classGrade.toStdU16String(),
            .classLevel = loaded->record.classLevel.toStdU16String(),
            .notes = loaded->record.notes.toStdU16String(),
            .timeFillerActivities =
                loaded->record.timeFillerActivities.toStdU16String()
        };
        fields.regularSchedule.reserve(
            static_cast<std::size_t>(loaded->record.regularTimes.size()));
        for (const ClassNotesPageRegularTime& row : loaded->record.regularTimes)
        {
            fields.regularSchedule.push_back({
                .day = row.day.toStdU16String(),
                .startTime = row.startTime.toStdU16String()
            });
        }

        Domain::Result<std::u16string> teacherDisplayName =
            Domain::Result<std::u16string>::success({});
        if (loaded->record.teacherId > 0)
        {
            TeacherRepository* const teacherRepository = store.teacherRepository();
            if (!teacherRepository)
            {
                teacherDisplayName = Domain::Result<std::u16string>::failure(error(
                    Domain::ErrorCode::NotFound,
                    "The class teacher display name is unavailable."));
            }
            else
            {
                const auto teacher = teacherRepository->lookupBoundedTeacherDisplayName(
                    loaded->record.teacherId,
                    kMaximumClassNotesTeacherNameFieldCodeUnits);
                if (!teacher)
                {
                    teacherDisplayName = Domain::Result<std::u16string>::failure(
                        repositoryError(teacher.error(),
                            "The class teacher display name is unavailable."));
                }
                else
                {
                    const std::string displayNameUtf8 =
                        displayName(teacher.value());
                    const QString selectedName = QString::fromUtf8(
                        displayNameUtf8.data(),
                        static_cast<qsizetype>(displayNameUtf8.size()));
                    teacherDisplayName = Domain::Result<std::u16string>::success(
                        selectedName.toStdU16String());
                }
            }
        }

        return Application::ClassNotesPageReadResult::success({
            .classId = classId,
            .classFields = Domain::Result<Application::ClassNotesPageFields>::success(
                std::move(fields)),
            .teacherDisplayName = std::move(teacherDisplayName)
        });
    }

private:
    QString m_databasePath;
};

class WorkspaceStoreClassNotesSavePort final
    : public Application::ClassNotesSavePort
{
public:
    explicit WorkspaceStoreClassNotesSavePort(QString databasePath)
        : m_databasePath(std::move(databasePath))
    {
    }

    [[nodiscard]] Application::ClassNotesSaveResult saveClassNotes(
        const Application::ClassNotesSaveRequest& request
        ) const override
    {
        const auto id = legacyClassId(request.classId);
        if (!id)
        {
            return Application::ClassNotesSaveResult::failure(error(
                Domain::ErrorCode::InvalidInput,
                "Class ID must be a canonical positive integer.", false));
        }
        WorkspaceStore store;
        const RepositoryStatus opened = store.openExisting(m_databasePath);
        if (!opened)
        {
            return Application::ClassNotesSaveResult::failure(openedError(
                opened, "The active workspace could not be opened to save class notes."));
        }
        ClassInfoRepository* const repository = store.classInfoRepository();
        if (!repository)
        {
            return Application::ClassNotesSaveResult::failure(error(
                Domain::ErrorCode::NotFound,
                "The class notes repository is unavailable."));
        }

        const QString notes = fromUtf16(request.notes).trimmed();
        const QString activities = fromUtf16(request.timeFillerActivities).trimmed();
        try
        {
            const RepositoryStatus saved = repository->saveClassNotesDetailed(
                *id, notes, activities);
            if (!saved)
            {
                return Application::ClassNotesSaveResult::failure(repositoryError(
                    saved.error(), "Class notes could not be saved."));
            }
            return Application::ClassNotesSaveResult::success();
        }
        catch (const std::exception&)
        {
            return Application::ClassNotesSaveResult::failure(error(
                Domain::ErrorCode::Technical, "Class notes could not be saved."));
        }
        catch (...)
        {
            return Application::ClassNotesSaveResult::failure(error(
                Domain::ErrorCode::Technical, "Class notes could not be saved."));
        }
    }

private:
    QString m_databasePath;
};

[[nodiscard]] Application::ClassDetailsPageFields fields(
    const ClassDetailsPageReadRecord& record
    )
{
    Application::ClassDetailsPageFields result{
        .classGrade = utf8(record.classGrade),
        .classLevel = utf8(record.classLevel),
        .readingBook = utf8(record.readingBook),
        .essayBook = utf8(record.essayBook),
        .classColor = utf8(record.classColor),
        .fontColor = utf8(record.fontColor)
    };
    result.regularSchedule.reserve(static_cast<std::size_t>(record.regularTimes.size()));
    for (const ClassTime& time : record.regularTimes)
    {
        result.regularSchedule.push_back({
            .day = utf8(time.day),
            .startTime = utf8(time.startTime),
            .endTime = utf8(time.endTime)
        });
    }
    result.intensiveSchedule.reserve(static_cast<std::size_t>(record.intensiveTimes.size()));
    for (const ClassTime& time : record.intensiveTimes)
    {
        result.intensiveSchedule.push_back({
            .day = utf8(time.day),
            .startTime = utf8(time.startTime),
            .endTime = utf8(time.endTime)
        });
    }
    return result;
}

[[nodiscard]] std::string displayName(const TeacherDisplayNameReadRecord& record)
{
    Teacher teacher;
    teacher.teacherKr = record.teacherKr;
    teacher.teacherEn = record.teacherEn;
    teacher.preferredRomanization = record.preferredRomanization;
    teacher.preferredName = record.preferredName;
    return utf8(teacher.preferredDisplayName());
}

[[nodiscard]] std::optional<ScheduleType> scheduleType(
    const Application::ClassDetailsScheduleMode mode
    )
{
    switch (mode)
    {
    case Application::ClassDetailsScheduleMode::Regular:
        return ScheduleType::Regular;
    case Application::ClassDetailsScheduleMode::Intensive:
        return ScheduleType::Intensive;
    }
    return std::nullopt;
}

[[nodiscard]] QString weekdayName(const Domain::Weekday weekday)
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
    return {};
}

[[nodiscard]] QList<ClassTime> legacyTimes(
    const std::vector<Domain::ScheduleTime>& values
    )
{
    QList<ClassTime> result;
    result.reserve(static_cast<qsizetype>(values.size()));
    for (const Domain::ScheduleTime& value : values)
    {
        const QTime start(
            value.startMinute().rawValue() / 60,
            value.startMinute().rawValue() % 60);
        const QTime end(
            value.endMinute().rawValue() / 60,
            value.endMinute().rawValue() % 60);
        result.append({
            weekdayName(value.weekday()),
            start.toString(QStringLiteral("h:mm AP")),
            end.toString(QStringLiteral("h:mm AP"))
        });
    }
    return result;
}

class DetailsContextPort final
    : public Application::ClassDetailsValidationContextPort
{
public:
    explicit DetailsContextPort(WorkspaceStore& store) : m_store(&store) {}

    [[nodiscard]] Application::ClassDetailsValidationContextPortResult
    loadClassDetailsValidationContext(const Domain::ClassId& id) const override
    {
        const auto numericId = legacyClassId(id);
        ClassInfoRepository* const repository = m_store
            ? m_store->classInfoRepository() : nullptr;
        if (!numericId || !repository)
        {
            return Application::ClassDetailsValidationContextPortResult::failure(
                error(Domain::ErrorCode::NotFound,
                    "The class details validation source is unavailable."));
        }
        try
        {
            const auto loaded = repository->loadClassDetailsSaveInfo(*numericId);
            if (!loaded)
            {
                return Application::ClassDetailsValidationContextPortResult::failure(
                    repositoryError(loaded.error(),
                        "The class details context could not be loaded."));
            }
            if (loaded->info.classId != *numericId)
            {
                return Application::ClassDetailsValidationContextPortResult::failure(
                    error(Domain::ErrorCode::Validation,
                        "The class details context returned a different class."));
            }
            return Application::ClassDetailsValidationContextPortResult::success({
                .matchedClassId = id,
                .teacherId = Application::ClassDetailsValidationContextTeacherId(
                    loaded->teacherId),
                .notes = loaded->info.notes.toStdU16String(),
                .timeFillerActivities = loaded->info.timeFillerActivities.toStdU16String()
            });
        }
        catch (const std::exception&)
        {
            return Application::ClassDetailsValidationContextPortResult::failure(
                error(Domain::ErrorCode::Technical,
                    "The class details context could not be loaded."));
        }
        catch (...)
        {
            return Application::ClassDetailsValidationContextPortResult::failure(
                error(Domain::ErrorCode::Technical,
                    "The class details context could not be loaded."));
        }
    }

private:
    WorkspaceStore* m_store;
};

class DetailsConflictPort final
    : public Application::ClassDetailsScheduleConflictPort
{
public:
    explicit DetailsConflictPort(WorkspaceStore& store) : m_store(&store) {}

    [[nodiscard]] Application::ClassDetailsScheduleConflictResult
    classDetailsScheduleConflicts(
        const Application::ClassDetailsScheduleConflictRequest& request
        ) const override
    {
        const auto id = legacyClassId(request.classId);
        const auto type = scheduleType(request.mode);
        ClassInfoRepository* const repository = m_store
            ? m_store->classInfoRepository() : nullptr;
        if (!id || !type || !repository)
        {
            return Application::ClassDetailsScheduleConflictResult::failure(
                error(Domain::ErrorCode::InvalidInput,
                    "Class schedule conflict inputs are invalid."));
        }
        try
        {
            const auto loaded = repository->getClassTimeConflicts(
                *id, legacyTimes(request.candidateTimes), *type);
            if (!loaded)
            {
                return Application::ClassDetailsScheduleConflictResult::failure(
                    repositoryError(loaded.error(),
                        "Class schedule conflicts could not be checked."));
            }
            std::vector<Application::ClassDetailsScheduleConflict> result;
            result.reserve(static_cast<std::size_t>(loaded->size()));
            for (const ClassConflict& conflict : *loaded)
            {
                result.push_back({
                    .className = conflict.className.toStdU16String(),
                    .day = conflict.day.toStdU16String(),
                    .startTime = conflict.startTime.toStdU16String(),
                    .endTime = conflict.endTime.toStdU16String(),
                    .conflictingClassName = conflict.conflictingClassName.toStdU16String()
                });
            }
            return Application::ClassDetailsScheduleConflictResult::success(
                std::move(result));
        }
        catch (const std::exception&)
        {
            return Application::ClassDetailsScheduleConflictResult::failure(
                error(Domain::ErrorCode::Technical,
                    "Class schedule conflicts could not be checked."));
        }
        catch (...)
        {
            return Application::ClassDetailsScheduleConflictResult::failure(
                error(Domain::ErrorCode::Technical,
                    "Class schedule conflicts could not be checked."));
        }
    }

private:
    WorkspaceStore* m_store;
};

class DetailsSavePort final : public Application::ClassDetailsSavePort
{
public:
    explicit DetailsSavePort(WorkspaceStore& store) : m_store(&store) {}

    [[nodiscard]] Domain::Result<void> saveClassDetails(
        const Application::ClassDetailsSaveRequest& request
        ) const override
    {
        const auto id = legacyClassId(request.classId);
        ClassInfoRepository* const repository = m_store
            ? m_store->classInfoRepository() : nullptr;
        if (!id || !repository)
        {
            return Domain::Result<void>::failure(error(
                Domain::ErrorCode::NotFound,
                "The class details save source is unavailable."));
        }
        try
        {
            const auto parent = repository->checkClassDetailsParentDetailed(*id);
            if (!parent)
            {
                return Domain::Result<void>::failure(repositoryError(
                    parent.error(), "The selected class could not be found."));
            }
            const auto loaded = repository->loadClassDetailsSaveInfo(*id);
            if (!loaded)
            {
                return Domain::Result<void>::failure(repositoryError(
                    loaded.error(), "The class details could not be loaded for saving."));
            }
            qint64 teacherId = loaded->teacherId;
            ClassInfo info = loaded->info;
            info.classGrade = fromUtf16(request.classGrade);
            info.classLevel = fromUtf16(request.classLevel);
            info.readingBook = fromUtf16(request.readingBook);
            info.essayBook = fromUtf16(request.essayBook);
            info.classColor = fromUtf16(request.classColor);
            info.fontColor = fromUtf16(request.fontColor);
            if (request.teacherId)
            {
                const auto parsed = Application::parseCanonicalPositiveDecimalId64(
                    request.teacherId->value());
                if (!parsed)
                {
                    return Domain::Result<void>::failure(error(
                        Domain::ErrorCode::InvalidInput,
                        "Teacher ID must be a canonical positive integer."));
                }
                teacherId = *parsed;
            }
            if (request.regularTimes)
            {
                info.classTimes = legacyTimes(*request.regularTimes);
            }
            if (request.intensiveTimes)
            {
                info.intensiveTimes = legacyTimes(*request.intensiveTimes);
            }
            info.teacherId = teacherId <= std::numeric_limits<int>::max()
                ? static_cast<int>(teacherId) : -1;
            const RepositoryStatus saved = repository->saveClassDetailsDetailed(
                info, teacherId);
            if (!saved)
            {
                return Domain::Result<void>::failure(repositoryError(
                    saved.error(), "The class details could not be saved."));
            }
            return Domain::Result<void>::success();
        }
        catch (const std::exception&)
        {
            return Domain::Result<void>::failure(error(
                Domain::ErrorCode::Technical, "The class details could not be saved."));
        }
        catch (...)
        {
            return Domain::Result<void>::failure(error(
                Domain::ErrorCode::Technical, "The class details could not be saved."));
        }
    }

private:
    WorkspaceStore* m_store;
};

class CoTeacherAssignmentPort final
    : public Application::ClassCoTeacherAssignmentPort
{
public:
    explicit CoTeacherAssignmentPort(WorkspaceStore& store) : m_store(&store) {}

    [[nodiscard]] Domain::Result<void> assignClassCoTeacher(
        const Application::ClassCoTeacherAssignmentRequest& request
        ) const override
    {
        const auto classId = legacyClassId(request.classId);
        ClassInfoRepository* const repository = m_store
            ? m_store->classInfoRepository() : nullptr;
        if (!classId || !repository)
        {
            return Domain::Result<void>::failure(error(
                Domain::ErrorCode::NotFound,
                "The co-teacher assignment source is unavailable."));
        }
        std::optional<qint64> teacherId;
        if (request.teacherId)
        {
            const auto parsed = Application::parseCanonicalPositiveDecimalId64(
                request.teacherId->value());
            if (!parsed)
            {
                return Domain::Result<void>::failure(error(
                    Domain::ErrorCode::InvalidInput,
                    "Teacher ID must be a canonical positive integer."));
            }
            teacherId = *parsed;
        }
        try
        {
            const RepositoryStatus saved =
                repository->saveClassCoTeacherAssignmentDetailed(*classId, teacherId);
            return saved
                ? Domain::Result<void>::success()
                : Domain::Result<void>::failure(repositoryError(
                    saved.error(), "The co-teacher assignment could not be saved."));
        }
        catch (const std::exception&)
        {
            return Domain::Result<void>::failure(error(
                Domain::ErrorCode::Technical,
                "The co-teacher assignment could not be saved."));
        }
        catch (...)
        {
            return Domain::Result<void>::failure(error(
                Domain::ErrorCode::Technical,
                "The co-teacher assignment could not be saved."));
        }
    }

private:
    WorkspaceStore* m_store;
};

class ClassCreatePort final : public Application::ClassCreatePort
{
public:
    explicit ClassCreatePort(WorkspaceStore& store) : m_store(&store) {}
    [[nodiscard]] Application::ClassCreateResult createClass(
        const Application::ClassCreateRequest& request
        ) const override
    {
        Q_UNUSED(request);
        ClassRepository* const repository = m_store
            ? m_store->classRepository() : nullptr;
        if (!m_store || !m_store->isOpen() || !repository)
        {
            return Application::ClassCreateResult::failure(error(
                Domain::ErrorCode::NotFound,
                "The active workspace is unavailable."));
        }
        try
        {
            const auto created = repository->createClass(QString{});
            if (!created)
            {
                return Application::ClassCreateResult::failure(repositoryError(
                    created.error(), "The class could not be created."));
            }
            if (*created <= 0)
            {
                return Application::ClassCreateResult::failure(error(
                    Domain::ErrorCode::Technical,
                    "Creating the class did not return a valid ID.", false));
            }
            const auto typedId = Domain::ClassId::fromString(std::to_string(*created));
            return typedId
                ? Application::ClassCreateResult::success(*typedId)
                : Application::ClassCreateResult::failure(error(
                    Domain::ErrorCode::Technical,
                    "Creating the class did not return a valid ID.", false));
        }
        catch (const std::exception&)
        {
            return Application::ClassCreateResult::failure(error(
                Domain::ErrorCode::Technical, "The class could not be created."));
        }
        catch (...)
        {
            return Application::ClassCreateResult::failure(error(
                Domain::ErrorCode::Technical, "The class could not be created."));
        }
    }

private:
    WorkspaceStore* m_store;
};

class ClassDeletePort final : public Application::ClassDeletePort
{
public:
    explicit ClassDeletePort(WorkspaceStore& store) : m_store(&store) {}
    [[nodiscard]] Application::ClassDeleteResult deleteClass(
        const Application::ClassDeleteRequest& request
        ) const override
    {
        const auto id = legacyClassId(request.classId);
        ClassRepository* const repository = m_store
            ? m_store->classRepository() : nullptr;
        if (!id || !repository)
        {
            return Application::ClassDeleteResult::failure(error(
                Domain::ErrorCode::NotFound,
                "The selected class could not be found."));
        }
        try
        {
            const RepositoryStatus deleted = repository->deleteClass(*id);
            return deleted
                ? Application::ClassDeleteResult::success()
                : Application::ClassDeleteResult::failure(repositoryError(
                    deleted.error(), "The class could not be deleted."));
        }
        catch (const std::exception&)
        {
            return Application::ClassDeleteResult::failure(error(
                Domain::ErrorCode::Technical, "The class could not be deleted."));
        }
        catch (...)
        {
            return Application::ClassDeleteResult::failure(error(
                Domain::ErrorCode::Technical, "The class could not be deleted."));
        }
    }

private:
    WorkspaceStore* m_store;
};

[[nodiscard]] Application::ClassDetailsValidationCatalog validationCatalog()
{
    Application::ClassDetailsValidationCatalog catalog;
    for (const std::string_view grade : ::ClassMngr::Domain::ClassDetailsCatalog::grades())
    {
        Application::ClassDetailsValidationGradeCatalog gradeCatalog;
        gradeCatalog.name = QString::fromUtf8(grade.data(),
            static_cast<qsizetype>(grade.size())).toStdU16String();
        for (const std::string_view level :
             ::ClassMngr::Domain::ClassDetailsCatalog::levelsForGrade(grade))
        {
            Application::ClassDetailsValidationLevelCatalog levelCatalog;
            levelCatalog.name = QString::fromUtf8(level.data(),
                static_cast<qsizetype>(level.size())).toStdU16String();
            for (const std::string_view book :
                 ::ClassMngr::Domain::ClassDetailsCatalog::readingBooks(grade, level))
            {
                levelCatalog.readingBooks.push_back(QString::fromUtf8(
                    book.data(), static_cast<qsizetype>(book.size())).toStdU16String());
            }
            for (const std::string_view book :
                 ::ClassMngr::Domain::ClassDetailsCatalog::essayBooks(grade, level))
            {
                levelCatalog.essayBooks.push_back(QString::fromUtf8(
                    book.data(), static_cast<qsizetype>(book.size())).toStdU16String());
            }
            gradeCatalog.levels.push_back(std::move(levelCatalog));
        }
        catalog.grades.push_back(std::move(gradeCatalog));
    }
    return catalog;
}

[[nodiscard]] Application::ClassDetailsCatalogCaseInsensitiveEquals catalogEquals()
{
    return [](const std::u16string_view left, const std::u16string_view right)
    {
        return QString::compare(
            QString::fromUtf16(left.data(), static_cast<qsizetype>(left.size())),
            QString::fromUtf16(right.data(), static_cast<qsizetype>(right.size())),
            Qt::CaseInsensitive) == 0;
    };
}

} // namespace

Application::ClassNotesPageReadResult readClassNotesAtPath(
    const QString& databasePath,
    const Domain::ClassId& classId
    )
{
    WorkspaceStoreClassNotesReadPort port(databasePath);
    return Application::ClassNotesPageReadQuery(port).execute(classId);
}

Application::ClassNotesSaveResult saveClassNotesAtPath(
    const QString& databasePath,
    const Application::ClassNotesSaveRequest& request
    )
{
    WorkspaceStoreClassNotesSavePort port(databasePath);
    return Application::ClassNotesSaveUseCase::execute(request, port);
}

Application::ClassDetailsPageReadResult readClassDetailsAtPath(
    const QString& databasePath,
    const Domain::ClassId& classId
    )
{
    const Domain::OperationError unavailable = error(
        Domain::ErrorCode::NotFound,
        "The class details page source is unavailable.");
    Application::ClassDetailsPageReadSnapshot snapshot{
        classId,
        classFieldsFailure(unavailable),
        teacherFailure(unavailable),
        studentCountFailure(unavailable)
    };
    const auto id = legacyClassId(classId);
    if (!id)
    {
        return Application::ClassDetailsPageReadResult::success(std::move(snapshot));
    }
    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        return Application::ClassDetailsPageReadResult::failure(openedError(
            opened, "The active workspace could not be opened for class details."));
    }
    ClassInfoRepository* const infoRepository = store.classInfoRepository();
    TeacherRepository* const teacherRepository = store.teacherRepository();
    RosterRepository* const rosterRepository = store.rosterRepository();
    std::optional<ClassDetailsPageReadRecord> record;
    bool parentMissing = false;
    if (!infoRepository)
    {
        snapshot.classFields = classFieldsFailure(error(
            Domain::ErrorCode::NotFound, "The class details repository is unavailable."));
    }
    else
    {
        try
        {
            const auto loaded = infoRepository->loadBoundedClassDetailsPageRecordDetailed(
                *id,
                static_cast<int>(Application::kClassesNavigationMaximumScheduleRows),
                static_cast<int>(
                    Application::kClassesNavigationMaximumScheduleTokenLength));
            if (!loaded)
            {
                snapshot.classFields = classFieldsFailure(repositoryError(
                    loaded.error(), "The selected class details could not be loaded."));
            }
            else if (loaded->parentMissing)
            {
                parentMissing = true;
                snapshot.classFields = classFieldsFailure(error(
                    Domain::ErrorCode::NotFound, "The selected class no longer exists."));
            }
            else if (loaded->regularScheduleLimitExceeded
                || loaded->intensiveScheduleLimitExceeded
                || loaded->scheduleTokenLimitExceeded)
            {
                return Application::ClassDetailsPageReadResult::failure(error(
                    Domain::ErrorCode::Validation,
                    "The selected class exceeds the supported schedule row or token limit."));
            }
            else
            {
                record = loaded->record;
                snapshot.classFields = Domain::Result<
                    Application::ClassDetailsPageFields>::success(fields(*record));
            }
        }
        catch (const std::exception&)
        {
            snapshot.classFields = classFieldsFailure(error(
                Domain::ErrorCode::Technical, "The selected class details could not be loaded."));
        }
        catch (...)
        {
            snapshot.classFields = classFieldsFailure(error(
                Domain::ErrorCode::Technical, "The selected class details could not be loaded."));
        }
    }
    if (record)
    {
        if (record->teacherId <= 0)
        {
            snapshot.teacherDisplayName = Domain::Result<std::string>::success({});
        }
        else if (!teacherRepository)
        {
            snapshot.teacherDisplayName = teacherFailure(error(
                Domain::ErrorCode::NotFound, "The teacher repository is unavailable."));
        }
        else
        {
            const auto loaded = teacherRepository->lookupTeacherDisplayName(record->teacherId);
            snapshot.teacherDisplayName = loaded
                ? Domain::Result<std::string>::success(displayName(*loaded))
                : teacherFailure(repositoryError(loaded.error(),
                    "The class teacher display name could not be loaded."));
        }
    }
    if (parentMissing)
    {
        snapshot.studentCount = studentCountFailure(error(
            Domain::ErrorCode::NotFound, "The selected class no longer exists."));
    }
    else if (!rosterRepository)
    {
        snapshot.studentCount = studentCountFailure(error(
            Domain::ErrorCode::NotFound, "The class roster repository is unavailable."));
    }
    else
    {
        const auto count = rosterRepository->getRosterStudentCount(*id);
        snapshot.studentCount = count
            ? Domain::Result<Application::ClassDetailsStudentCount>::success(
                  Application::ClassDetailsStudentCount{*count})
            : studentCountFailure(repositoryError(count.error(),
                  "The class student count could not be loaded."));
    }
    SnapshotClassDetailsReadPort port(std::move(snapshot));
    return Application::ClassDetailsPageQuery(port).execute(classId);
}

Application::ClassCoTeacherPageReadResult readClassCoTeacherAtPath(
    const QString& databasePath,
    const Domain::ClassId& classId
    )
{
    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        return Application::ClassCoTeacherPageReadResult::failure(openedError(
            opened, "The active workspace could not be opened for co-teachers."));
    }
    const auto id = legacyClassId(classId);
    ClassInfoRepository* const infoRepository = store.classInfoRepository();
    TeacherRepository* const teacherRepository = store.teacherRepository();
    if (!id || !infoRepository)
    {
        return Application::ClassCoTeacherPageReadResult::failure(error(
            Domain::ErrorCode::NotFound, "The co-teacher source is unavailable."));
    }
    try
    {
        const auto loaded = infoRepository->loadBoundedClassCoTeacherPageDetailsDetailed(
            *id,
            static_cast<int>(Application::kClassesNavigationMaximumScheduleRows),
            static_cast<int>(
                Application::kClassesNavigationMaximumScheduleTokenLength));
        if (!loaded)
        {
            return Application::ClassCoTeacherPageReadResult::failure(repositoryError(
                loaded.error(), "The co-teacher assignment could not be loaded."));
        }
        if (loaded->parentMissing)
        {
            return Application::ClassCoTeacherPageReadResult::failure(error(
                Domain::ErrorCode::NotFound, "The selected class no longer exists."));
        }
        if (loaded->regularScheduleLimitExceeded
            || loaded->scheduleTokenLimitExceeded)
        {
            return Application::ClassCoTeacherPageReadResult::failure(error(
                Domain::ErrorCode::Validation,
                "The selected class exceeds the supported schedule row or token limit."));
        }
        const auto& record = loaded->record;
        std::optional<Domain::TeacherId> teacherId;
        if (record.teacherId > 0)
        {
            teacherId = Domain::TeacherId::fromString(std::to_string(record.teacherId));
            if (!teacherId)
            {
                return Application::ClassCoTeacherPageReadResult::failure(error(
                    Domain::ErrorCode::Validation,
                    "The saved co-teacher identifier is invalid."));
            }
        }
        Application::ClassCoTeacherPageFields coTeacherFields{
            .selectedTeacherId = std::move(teacherId),
            .classGrade = record.classGrade.toStdU16String(),
            .classLevel = record.classLevel.toStdU16String()
        };
        coTeacherFields.regularSchedule.reserve(
            static_cast<std::size_t>(record.regularTimes.size()));
        for (const ClassPageDetailsRegularTime& slot : record.regularTimes)
        {
            coTeacherFields.regularSchedule.emplace_back(
                slot.day.toStdU16String(), slot.startTime.toStdU16String());
        }
        std::string teacherName;
        if (record.teacherId > 0)
        {
            if (!teacherRepository)
            {
                return Application::ClassCoTeacherPageReadResult::failure(error(
                    Domain::ErrorCode::NotFound, "The teacher repository is unavailable."));
            }
            const auto loadedTeacher = teacherRepository->lookupTeacherDisplayName(
                record.teacherId);
            if (!loadedTeacher)
            {
                return Application::ClassCoTeacherPageReadResult::failure(repositoryError(
                    loadedTeacher.error(), "The co-teacher display name could not be loaded."));
            }
            teacherName = displayName(*loadedTeacher);
        }
        Application::ClassCoTeacherPageReadSnapshot snapshot{
            .classId = classId,
            .classFields = Domain::Result<Application::ClassCoTeacherPageFields>::success(
                std::move(coTeacherFields)),
            .teacherDisplayName = Domain::Result<std::u16string>::success(
                QString::fromUtf8(teacherName).toStdU16String())
        };
        class Port final : public Application::ClassCoTeacherPageReadPort
        {
        public:
            explicit Port(Application::ClassCoTeacherPageReadSnapshot input)
                : value(std::move(input)) {}
            Application::ClassCoTeacherPageReadResult readClassCoTeacherPage(
                const Domain::ClassId&) const override
            {
                return Application::ClassCoTeacherPageReadResult::success(value);
            }
            Application::ClassCoTeacherPageReadSnapshot value;
        } port(std::move(snapshot));
        return Application::ClassCoTeacherPageReadQuery(port).execute(classId);
    }
    catch (const std::exception&)
    {
        return Application::ClassCoTeacherPageReadResult::failure(error(
            Domain::ErrorCode::Technical, "The co-teacher assignment could not be loaded."));
    }
    catch (...)
    {
        return Application::ClassCoTeacherPageReadResult::failure(error(
            Domain::ErrorCode::Technical, "The co-teacher assignment could not be loaded."));
    }
}

Application::ClassCoTeacherTeacherChoicesResult readClassCoTeacherChoicesAtPath(
    const QString& databasePath
    )
{
    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        return Application::ClassCoTeacherTeacherChoicesResult::failure(openedError(
            opened, "The active workspace could not be opened for teacher choices."));
    }
    TeacherRepository* const repository = store.teacherRepository();
    if (!repository)
    {
        return Application::ClassCoTeacherTeacherChoicesResult::failure(error(
            Domain::ErrorCode::NotFound, "The teacher choices source is unavailable."));
    }
    try
    {
        const auto loaded = repository->loadBoundedClassCoTeacherTeacherChoiceRecords(
            kMaximumCoTeacherChoices + 1,
            kMaximumCoTeacherNameBytes,
            kMaximumTotalCoTeacherNameBytes);
        if (!loaded)
        {
            return Application::ClassCoTeacherTeacherChoicesResult::failure(
                repositoryError(loaded.error(), "Teacher choices could not be loaded."));
        }
        if (loaded->size() > kMaximumCoTeacherChoices)
        {
            return Application::ClassCoTeacherTeacherChoicesResult::failure(error(
                Domain::ErrorCode::Validation,
                "Teacher choices exceeded the supported page limit.", false));
        }
        if (std::any_of(loaded->cbegin(), loaded->cend(), [](const auto& row) {
                return row.textExceedsBound;
            }))
        {
            return Application::ClassCoTeacherTeacherChoicesResult::failure(error(
                Domain::ErrorCode::Validation,
                "Teacher choice names exceed the supported text byte limit.", false));
        }
        Application::ClassCoTeacherTeacherChoicesSnapshot snapshot;
        snapshot.teachers.reserve(static_cast<std::size_t>(loaded->size()));
        for (const ClassCoTeacherTeacherChoiceReadRecord& row : *loaded)
        {
            const auto id = Domain::TeacherId::fromString(std::to_string(row.teacherId));
            if (!id)
            {
                return Application::ClassCoTeacherTeacherChoicesResult::failure(error(
                    Domain::ErrorCode::Validation,
                    "Teacher choices contain an invalid identifier.", false));
            }
            snapshot.teachers.push_back({
                .teacherId = *id,
                .teacherKr = row.teacherKr.toStdU16String(),
                .teacherEn = row.teacherEn.toStdU16String(),
                .roomNumber = {},
                .internetType = {},
                .wifiName = {},
                .wifiPassword = {},
                .projectionType = {},
                .zoomId = {},
                .zoomPassword = {}
            });
        }
        class Port final : public Application::ClassCoTeacherTeacherChoicesReadPort
        {
        public:
            explicit Port(Application::ClassCoTeacherTeacherChoicesSnapshot input)
                : value(std::move(input)) {}
            Application::ClassCoTeacherTeacherChoicesResult
            readClassCoTeacherTeacherChoices() const override
            {
                return Application::ClassCoTeacherTeacherChoicesResult::success(value);
            }
            Application::ClassCoTeacherTeacherChoicesSnapshot value;
        } port(std::move(snapshot));
        return Application::ClassCoTeacherTeacherChoicesReadQuery(port).execute();
    }
    catch (const std::exception&)
    {
        return Application::ClassCoTeacherTeacherChoicesResult::failure(error(
            Domain::ErrorCode::Technical, "Teacher choices could not be loaded."));
    }
    catch (...)
    {
        return Application::ClassCoTeacherTeacherChoicesResult::failure(error(
            Domain::ErrorCode::Technical, "Teacher choices could not be loaded."));
    }
}

Domain::Result<Application::ClassCoTeacherTeacherChoice>
readClassCoTeacherTeacherDetailsAtPath(
    const QString& databasePath,
    const Domain::TeacherId& teacherId
    )
{
    std::int64_t parsedTeacherId = 0;
    const std::string& value = teacherId.value();
    const auto [end, parseError] = std::from_chars(
        value.data(), value.data() + value.size(), parsedTeacherId, 10);
    if (parseError != std::errc{}
        || end != value.data() + value.size()
        || parsedTeacherId <= 0
        || std::to_string(parsedTeacherId) != value)
    {
        return Domain::Result<Application::ClassCoTeacherTeacherChoice>::failure(error(
            Domain::ErrorCode::InvalidInput,
            "The selected teacher identifier is invalid."));
    }
    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        return Domain::Result<Application::ClassCoTeacherTeacherChoice>::failure(
            openedError(opened, "The active workspace could not be opened for teacher details."));
    }
    TeacherRepository* const repository = store.teacherRepository();
    if (!repository)
    {
        return Domain::Result<Application::ClassCoTeacherTeacherChoice>::failure(error(
            Domain::ErrorCode::NotFound,
            "The teacher details source is unavailable."));
    }
    try
    {
        const auto loaded = repository->loadBoundedClassCoTeacherTeacherProfile(
            parsedTeacherId,
            kMaximumCoTeacherProfileFieldBytes,
            kMaximumCoTeacherProfileBytes);
        if (!loaded)
        {
            return Domain::Result<Application::ClassCoTeacherTeacherChoice>::failure(
                repositoryError(loaded.error(), "Teacher details could not be loaded."));
        }
        if (loaded->textExceedsBound)
        {
            return Domain::Result<Application::ClassCoTeacherTeacherChoice>::failure(error(
                Domain::ErrorCode::Validation,
                "Selected teacher details exceed the supported text byte limit.", false));
        }
        return Domain::Result<Application::ClassCoTeacherTeacherChoice>::success({
            .teacherId = teacherId,
            .teacherKr = {},
            .teacherEn = {},
            .roomNumber = loaded->roomNumber.toStdU16String(),
            .internetType = loaded->internetType.toStdU16String(),
            .wifiName = loaded->wifiName.toStdU16String(),
            .wifiPassword = loaded->wifiPassword.toStdU16String(),
            .projectionType = loaded->projectionType.toStdU16String(),
            .zoomId = loaded->zoomId.toStdU16String(),
            .zoomPassword = loaded->zoomPassword.toStdU16String()
        });
    }
    catch (const std::exception&)
    {
        return Domain::Result<Application::ClassCoTeacherTeacherChoice>::failure(error(
            Domain::ErrorCode::Technical,
            "Teacher details could not be loaded."));
    }
    catch (...)
    {
        return Domain::Result<Application::ClassCoTeacherTeacherChoice>::failure(error(
            Domain::ErrorCode::Technical,
            "Teacher details could not be loaded."));
    }
}

Application::ClassDetailsSaveWorkflowResult saveClassDetailsAtPath(
    const QString& databasePath,
    const Application::ClassDetailsSaveWorkflowRequest& request
    )
{
    if (request.input.regularTimes.size()
            > Application::kClassesNavigationMaximumScheduleRows
        || request.input.intensiveTimes.size()
            > Application::kClassesNavigationMaximumScheduleRows)
    {
        return Application::ClassDetailsSaveWorkflowFailure{
            Application::ClassDetailsSaveWorkflowStage::BoundedInput,
            error(Domain::ErrorCode::Validation,
                "A class schedule cannot contain more than 14 rows.")
        };
    }
    const auto overScheduleTokenLimit = [](const auto& rows) {
        return std::any_of(rows.cbegin(), rows.cend(), [](const auto& row) {
            return row.day.size() > Application::kClassesNavigationMaximumScheduleTokenLength
                || row.startTime.size()
                    > Application::kClassesNavigationMaximumScheduleTokenLength
                || row.endTime.size()
                    > Application::kClassesNavigationMaximumScheduleTokenLength;
        });
    };
    if (overScheduleTokenLimit(request.input.regularTimes)
        || overScheduleTokenLimit(request.input.intensiveTimes))
    {
        return Application::ClassDetailsSaveWorkflowFailure{
            Application::ClassDetailsSaveWorkflowStage::BoundedInput,
            error(Domain::ErrorCode::Validation,
                "A class schedule token cannot exceed 64 characters.")
        };
    }
    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        return Application::ClassDetailsSaveWorkflowFailure{
            Application::ClassDetailsSaveWorkflowStage::ValidationContext,
            openedError(opened, "The active workspace could not be opened to save class details.")
        };
    }
    DetailsContextPort context(store);
    DetailsConflictPort conflicts(store);
    DetailsSavePort save(store);
    return Application::ClassDetailsSaveWorkflow::execute(
        request, validationCatalog(), context, conflicts, save, catalogEquals());
}

Domain::Result<void> assignClassCoTeacherAtPath(
    const QString& databasePath,
    const Application::ClassCoTeacherAssignmentRequest& request
    )
{
    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        return Domain::Result<void>::failure(openedError(
            opened, "The active workspace could not be opened to save co-teachers."));
    }
    CoTeacherAssignmentPort port(store);
    const Domain::TeacherId teacherId = request.teacherId.value_or(
        Domain::TeacherId::fromString("-1").value());
    return Application::ClassCoTeacherAssignmentUseCase::execute(
        request.classId, teacherId, port);
}

Application::ClassCreateResult createClassAtPath(const QString& databasePath)
{
    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        return Application::ClassCreateResult::failure(openedError(
            opened, "The active workspace could not be opened to create a class."));
    }
    ClassCreatePort port(store);
    return Application::ClassCreateUseCase::execute({}, port);
}

Application::ClassDeleteResult deleteClassAtPath(
    const QString& databasePath,
    const Application::ClassDeleteRequest& request
    )
{
    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(databasePath);
    if (!opened)
    {
        return Application::ClassDeleteResult::failure(openedError(
            opened, "The active workspace could not be opened to delete a class."));
    }
    ClassDeletePort port(store);
    return Application::ClassDeleteUseCase::execute(request, port);
}

} // namespace ClassMngr::Next::Platform
