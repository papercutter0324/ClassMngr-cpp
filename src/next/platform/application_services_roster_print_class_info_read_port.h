#pragma once

#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/class_info_repository.h"
#include "next/application/roster_print_class_info_read_port.h"

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

class ApplicationServicesRosterPrintClassInfoReadPort final
    : public Application::RosterPrintClassInfoReadPort
{
public:
    explicit ApplicationServicesRosterPrintClassInfoReadPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    [[nodiscard]] Application::RosterPrintClassInfoReadResult
    readRosterPrintClassInfo(
        const Domain::ClassId& classId
        ) const override
    {
        const std::optional<int> legacyClassId = legacyId(classId.value());
        if (!legacyClassId)
        {
            return Application::RosterPrintClassInfoReadResult::failure(error(
                Domain::ErrorCode::InvalidInput,
                "Roster print class ID must be a canonical positive integer."
                ));
        }

        WorkspaceStore* const store =
            m_services ? m_services->workspaceStore() : nullptr;
        if (!store || !store->isOpen())
        {
            return Application::RosterPrintClassInfoReadResult::failure(
                error(
                    Domain::ErrorCode::NotFound,
                    "The active database session for roster print class information is unavailable."
                    )
                );
        }

        ClassInfoRepository* const classRepository =
            store->classInfoRepository();
        if (!classRepository)
        {
            return Application::RosterPrintClassInfoReadResult::failure(error(
                Domain::ErrorCode::NotFound,
                "The roster print class information repository is unavailable."
                ));
        }

        RepositoryResult<RosterPrintClassInfoReadOutcome> loaded = [&]()
            -> RepositoryResult<RosterPrintClassInfoReadOutcome>
        {
            try
            {
                return classRepository->loadRosterPrintClassInfoRecordDetailed(
                    *legacyClassId
                    );
            }
            catch (const std::exception&)
            {
                return std::unexpected(
                    RepositoryError(
                        RepositoryErrorCode::Technical,
                        QStringLiteral("Roster print class information could not be loaded.")
                        )
                    );
            }
            catch (...)
            {
                return std::unexpected(
                    RepositoryError(
                        RepositoryErrorCode::Technical,
                        QStringLiteral("Roster print class information could not be loaded.")
                        )
                    );
            }
        }();

        if (!loaded)
        {
            return Application::RosterPrintClassInfoReadResult::failure(
                repositoryError(loaded.error())
                );
        }

        if (loaded->parentMissing)
        {
            return Application::RosterPrintClassInfoReadResult::failure(error(
                Domain::ErrorCode::NotFound,
                "The requested class was not found."
                ));
        }

        const RosterPrintClassInfoReadRecord& record = loaded->record;
        const std::string returnedId = std::to_string(record.classId);
        const auto typedReturnedId = Domain::ClassId::fromString(returnedId);
        if (!typedReturnedId)
        {
            return Application::RosterPrintClassInfoReadResult::failure(error(
                Domain::ErrorCode::Validation,
                "The roster print class information repository returned an invalid class identifier."
                ));
        }

        Application::RosterPrintClassInfoReadSnapshot snapshot(
            *typedReturnedId
            );
        snapshot.classGrade = record.classGrade.toStdU16String();
        snapshot.classLevel = record.classLevel.toStdU16String();
        snapshot.teacherEn = record.teacherEnglishName.toStdU16String();
        snapshot.teacherKr = record.teacherKoreanName.toStdU16String();
        snapshot.roomNumber = record.roomNumber.toStdU16String();
        snapshot.wifiName = record.wifiName.toStdU16String();
        snapshot.wifiPassword = record.wifiPassword.toStdU16String();
        snapshot.zoomId = record.zoomId.toStdU16String();
        snapshot.zoomPassword = record.zoomPassword.toStdU16String();
        snapshot.regularSchedule.reserve(
            static_cast<std::size_t>(record.regularTimes.size())
            );
        for (const ClassTime& row : record.regularTimes)
        {
            snapshot.regularSchedule.push_back({
                row.day.toStdU16String(),
                row.startTime.toStdU16String(),
                row.endTime.toStdU16String()
            });
        }

        return Application::RosterPrintClassInfoReadResult::success(
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
        const auto [end, conversionError] = std::from_chars(
            value.data(),
            value.data() + value.size(),
            parsed
            );
        if (conversionError != std::errc{}
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

    [[nodiscard]] static Domain::OperationError repositoryError(
        const RepositoryError& repositoryError
        )
    {
        Domain::ErrorCode code = Domain::ErrorCode::Technical;
        switch (repositoryError.code)
        {
        case RepositoryErrorCode::InvalidInput:
            code = Domain::ErrorCode::InvalidInput;
            break;
        case RepositoryErrorCode::NotFound:
            code = Domain::ErrorCode::NotFound;
            break;
        case RepositoryErrorCode::Conflict:
            code = Domain::ErrorCode::Conflict;
            break;
        case RepositoryErrorCode::Validation:
            code = Domain::ErrorCode::Validation;
            break;
        case RepositoryErrorCode::Technical:
            code = Domain::ErrorCode::Technical;
            break;
        }

        const QByteArray bytes = toLegacyDiagnostic(repositoryError).toUtf8();
        return error(
            code,
            bytes.isEmpty()
                ? "Roster print class information could not be loaded."
                : bytes.toStdString()
            );
    }

    ApplicationServices* m_services;
};

} // namespace ClassMngr::Next::Platform
