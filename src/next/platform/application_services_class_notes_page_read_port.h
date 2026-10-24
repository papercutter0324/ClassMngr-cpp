#pragma once

#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/teacher_repository.h"
#include "next/application/class_notes_page_read_port.h"

#include <QByteArray>

#include <charconv>
#include <exception>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesClassNotesPageReadPort final
    : public Application::ClassNotesPageReadPort
{
public:
    explicit ApplicationServicesClassNotesPageReadPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    [[nodiscard]] Application::ClassNotesPageReadResult readClassNotesPage(
        const Domain::ClassId& classId
        ) const override
    {
        const std::optional<int> legacyClassId = legacyId(classId.value());
        if (!legacyClassId)
        {
            return Application::ClassNotesPageReadResult::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message = "Class ID must be a canonical positive integer.",
                .recoverable = false
            });
        }

        WorkspaceStore* const store =
            m_services ? m_services->workspaceStore() : nullptr;
        if (!store || !store->isOpen())
        {
            return snapshotWithFailures(
                classId,
                unavailableError()
                );
        }

        ClassInfoRepository* const classInfoRepository =
            store->classInfoRepository();
        if (!classInfoRepository)
        {
            return snapshotWithFailures(
                classId,
                error(
                    Domain::ErrorCode::Technical,
                    "Class notes details could not be loaded."
                    )
                );
        }

        Result<ClassNotesPageReadOutcome> loadedInfo =
            [&]() -> Result<ClassNotesPageReadOutcome>
        {
            try
            {
                return classInfoRepository->loadClassNotesPageRecordDetailed(
                    *legacyClassId
                    );
            }
            catch (const std::exception&)
            {
                return std::unexpected(
                    QStringLiteral("Class notes details could not be loaded.")
                    );
            }
            catch (...)
            {
                return std::unexpected(
                    QStringLiteral("Class notes details could not be loaded.")
                    );
            }
        }();

        if (!loadedInfo)
        {
            Application::ClassNotesPageReadSnapshot snapshot{
                classId,
                failure<Application::ClassNotesPageFields>(
                    fromLegacyError(loadedInfo.error())
                    ),
                failure<std::u16string>(error(
                    Domain::ErrorCode::NotFound,
                    "The class teacher display name is unavailable."
                    ))
            };
            return Application::ClassNotesPageReadResult::success(
                std::move(snapshot)
                );
        }

        const std::optional<Domain::ClassId> matchedClassId =
            Domain::ClassId::fromString(
                std::to_string(loadedInfo->record.classId)
                );
        if (!matchedClassId)
        {
            return Application::ClassNotesPageReadResult::failure(error(
                Domain::ErrorCode::Validation,
                "Class notes details returned an invalid class identifier."
                ));
        }

        if (loadedInfo->parentMissing)
        {
            const Domain::OperationError missingParentError = error(
                Domain::ErrorCode::NotFound,
                "The requested class no longer exists."
                );
            Application::ClassNotesPageReadSnapshot snapshot{
                *matchedClassId,
                failure<Application::ClassNotesPageFields>(
                    missingParentError
                    ),
                failure<std::u16string>(missingParentError)
            };
            return Application::ClassNotesPageReadResult::success(
                std::move(snapshot)
                );
        }

        const ClassNotesPageReadRecord& record = loadedInfo->record;

        Application::ClassNotesPageFields fields;
        fields.classGrade = record.classGrade.toStdU16String();
        fields.classLevel = record.classLevel.toStdU16String();
        fields.regularSchedule.reserve(
            static_cast<std::size_t>(record.regularTimes.size())
            );
        for (const ClassNotesPageRegularTime& time : record.regularTimes)
        {
            fields.regularSchedule.push_back({
                .day = time.day.toStdU16String(),
                .startTime = time.startTime.toStdU16String()
            });
        }
        fields.notes = record.notes.toStdU16String();
        fields.timeFillerActivities =
            record.timeFillerActivities.toStdU16String();

        Domain::Result<std::u16string> teacherName =
            Domain::Result<std::u16string>::success({});
        if (record.teacherId > 0)
        {
            teacherName = loadTeacherDisplayName(
                store->teacherRepository(),
                record.teacherId
                );
        }

        Application::ClassNotesPageReadSnapshot snapshot{
            *matchedClassId,
            Domain::Result<Application::ClassNotesPageFields>::success(
                std::move(fields)
                ),
            std::move(teacherName)
        };
        return Application::ClassNotesPageReadResult::success(
            std::move(snapshot)
            );
    }

private:
    [[nodiscard]] static std::optional<int> legacyId(
        const std::string& value
        )
    {
        if (value.empty() || value.front() < '1' || value.front() > '9')
        {
            return std::nullopt;
        }

        int parsed = 0;
        const auto [end, error] = std::from_chars(
            value.data(),
            value.data() + value.size(),
            parsed
            );
        if (error != std::errc{}
            || end != value.data() + value.size()
            || parsed <= 0
            || std::to_string(parsed) != value)
        {
            return std::nullopt;
        }
        return parsed;
    }

    [[nodiscard]] static Domain::OperationError error(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        return {
            .code = code,
            .message = std::move(message),
            .recoverable = code == Domain::ErrorCode::NotFound
        };
    }

    [[nodiscard]] static Domain::OperationError unavailableError()
    {
        return error(
            Domain::ErrorCode::NotFound,
            "The active database session for class notes is unavailable."
            );
    }

    [[nodiscard]] static Domain::OperationError fromLegacyError(
        const QString& message
        )
    {
        const QString normalized = message.toLower();
        Domain::ErrorCode code = Domain::ErrorCode::Technical;
        if (normalized.contains(QStringLiteral("not found"))
            || normalized.contains(QStringLiteral("no matching record"))
            || normalized.contains(QStringLiteral("unavailable")))
        {
            code = Domain::ErrorCode::NotFound;
        }
        else if (normalized.contains(QStringLiteral("invalid")))
        {
            code = Domain::ErrorCode::InvalidInput;
        }

        const QByteArray bytes = message.toUtf8();
        return error(
            code,
            bytes.isEmpty()
                ? "Class notes details could not be loaded."
                : bytes.toStdString()
            );
    }

    [[nodiscard]] static Domain::OperationError fromTeacherDisplayNameLookup(
        const TeacherDisplayNameLookupResult& lookup
        )
    {
        Domain::ErrorCode code = Domain::ErrorCode::Technical;
        switch (lookup.code)
        {
        case TeacherDisplayNameLookupCode::Missing:
            code = Domain::ErrorCode::NotFound;
            break;
        case TeacherDisplayNameLookupCode::InvalidInput:
            code = Domain::ErrorCode::InvalidInput;
            break;
        case TeacherDisplayNameLookupCode::Found:
        case TeacherDisplayNameLookupCode::Failure:
            code = Domain::ErrorCode::Technical;
            break;
        }

        const QByteArray bytes = lookup.diagnostic.toUtf8();
        return error(
            code,
            bytes.isEmpty()
                ? "The class teacher display name could not be loaded."
                : bytes.toStdString()
            );
    }

    template <typename Value>
    [[nodiscard]] static Domain::Result<Value> failure(
        Domain::OperationError operationError
        )
    {
        return Domain::Result<Value>::failure(std::move(operationError));
    }

    [[nodiscard]] static Application::ClassNotesPageReadResult
    snapshotWithFailures(
        const Domain::ClassId& classId,
        const Domain::OperationError& sourceError
        )
    {
        Application::ClassNotesPageReadSnapshot snapshot{
            classId,
            failure<Application::ClassNotesPageFields>(sourceError),
            failure<std::u16string>(sourceError)
        };
        return Application::ClassNotesPageReadResult::success(
            std::move(snapshot)
            );
    }

    [[nodiscard]] static Domain::Result<std::u16string>
    loadTeacherDisplayName(
        TeacherRepository* repository,
        const qint64 teacherId
        )
    {
        if (!repository)
        {
            return failure<std::u16string>(error(
                Domain::ErrorCode::NotFound,
                "The class teacher repository is unavailable."
                ));
        }

        try
        {
            const TeacherDisplayNameLookupResult loaded =
                repository->lookupTeacherDisplayName(teacherId);
            if (loaded.code != TeacherDisplayNameLookupCode::Found)
            {
                return failure<std::u16string>(
                    fromTeacherDisplayNameLookup(loaded)
                    );
            }
            Teacher teacher;
            teacher.teacherKr = loaded.record.teacherKr;
            teacher.teacherEn = loaded.record.teacherEn;
            teacher.preferredRomanization =
                loaded.record.preferredRomanization;
            teacher.preferredName = loaded.record.preferredName;
            return Domain::Result<std::u16string>::success(
                teacher.preferredDisplayName().toStdU16String()
                );
        }
        catch (const std::exception&)
        {
            return failure<std::u16string>(error(
                Domain::ErrorCode::Technical,
                "The class teacher display name could not be loaded."
                ));
        }
        catch (...)
        {
            return failure<std::u16string>(error(
                Domain::ErrorCode::Technical,
                "The class teacher display name could not be loaded."
                ));
        }
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
