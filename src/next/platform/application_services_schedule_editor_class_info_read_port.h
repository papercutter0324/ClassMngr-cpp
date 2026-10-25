#pragma once

#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/class_info_repository.h"
#include "next/application/schedule_editor_class_info_query.h"

#include <QByteArray>
#include <QString>

#include <charconv>
#include <exception>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesScheduleEditorClassInfoReadPort final
    : public Application::ScheduleEditorClassInfoReadPort
{
public:
    explicit ApplicationServicesScheduleEditorClassInfoReadPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    [[nodiscard]] Application::ScheduleEditorClassInfoReadResult
    readScheduleEditorClassInfo(
        const Domain::ClassId& classId
        ) const override
    {
        const std::optional<int> legacyClassId =
            legacyId(classId.value());
        if (!legacyClassId)
        {
            return failure(
                Domain::ErrorCode::InvalidInput,
                "Selected class identifier must be a canonical positive integer."
                );
        }

        WorkspaceStore* const store =
            m_services ? m_services->workspaceStore() : nullptr;
        if (!store || !store->isOpen())
        {
            return unavailableFailure();
        }

        ClassInfoRepository* const repository =
            store->classInfoRepository();
        if (!repository)
        {
            return unavailableFailure();
        }

        try
        {
            const RepositoryResult<ScheduleEditorClassInfoReadOutcome> loaded =
                repository->loadScheduleEditorClassInfoRecordDetailed(
                    *legacyClassId
                    );
            if (!loaded)
            {
                return failure(
                    domainErrorCode(loaded.error().code),
                    toStdString(toLegacyDiagnostic(loaded.error()))
                    );
            }

            if (loaded->parentMissing)
            {
                return failure(
                    Domain::ErrorCode::NotFound,
                    "The selected class does not exist."
                    );
            }

            const ScheduleEditorClassInfoReadRecord& record = loaded->record;

            const auto snapshotClassId = Domain::ClassId::fromString(
                std::to_string(record.classId)
                );
            if (!snapshotClassId)
            {
                return failure(
                    Domain::ErrorCode::Validation,
                    "Loaded class details have an invalid class identifier."
                    );
            }

            return Application::ScheduleEditorClassInfoReadResult::success({
                .classId = *snapshotClassId,
                .classGrade = record.classGrade.toStdU16String(),
                .classLevel = record.classLevel.toStdU16String(),
                .readingBook = record.readingBook.toStdU16String(),
                .essayBook = record.essayBook.toStdU16String(),
                .classColor = record.classColor.toStdU16String(),
                .fontColor = record.fontColor.toStdU16String(),
                .teacherKoreanName =
                    record.teacherKoreanName.toStdU16String(),
                .roomNumber = record.roomNumber.toStdU16String()
            });
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Class details could not be loaded."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Class details could not be loaded."
                );
        }
    }

private:
    [[nodiscard]] static Domain::ErrorCode domainErrorCode(
        const RepositoryErrorCode code
        ) noexcept
    {
        switch (code)
        {
        case RepositoryErrorCode::InvalidInput:
            return Domain::ErrorCode::InvalidInput;
        case RepositoryErrorCode::NotFound:
            return Domain::ErrorCode::NotFound;
        case RepositoryErrorCode::Conflict:
            return Domain::ErrorCode::Conflict;
        case RepositoryErrorCode::Validation:
            return Domain::ErrorCode::Validation;
        case RepositoryErrorCode::Technical:
            return Domain::ErrorCode::Technical;
        }
        return Domain::ErrorCode::Technical;
    }

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

    [[nodiscard]] static std::string toStdString(const QString& value)
    {
        const QByteArray bytes = value.toUtf8();
        return std::string(
            bytes.constData(),
            static_cast<std::size_t>(bytes.size())
            );
    }

    [[nodiscard]] static Application::ScheduleEditorClassInfoReadResult
    failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        if (message.empty())
        {
            message = "Class details could not be loaded.";
        }

        return Application::ScheduleEditorClassInfoReadResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = code != Domain::ErrorCode::Validation
        });
    }

    [[nodiscard]] static Application::ScheduleEditorClassInfoReadResult
    unavailableFailure()
    {
        return failure(
            Domain::ErrorCode::NotFound,
            "The active database session for class details is unavailable."
            );
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
