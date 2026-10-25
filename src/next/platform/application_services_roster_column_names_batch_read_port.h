#pragma once

#include "next/platform/repository_error_code.h"

#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/roster_repository.h"
#include "next/application/roster_column_names_batch_read_port.h"

#include <QByteArray>

#include <charconv>
#include <exception>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesRosterColumnNamesBatchReadPort final
    : public Application::RosterColumnNamesBatchReadPort
{
public:
    explicit ApplicationServicesRosterColumnNamesBatchReadPort(
        ApplicationServices& services
        ) noexcept
        : m_services(&services)
    {
    }

    explicit ApplicationServicesRosterColumnNamesBatchReadPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    [[nodiscard]] Application::RosterColumnNamesBatchReadResult
    readRosterColumnNames(
        const std::vector<Domain::ClassId>& classIds
        ) const override
    {
        if (classIds.empty())
        {
            return Application::RosterColumnNamesBatchReadResult::success({});
        }

        QList<int> legacyClassIds;
        legacyClassIds.reserve(static_cast<qsizetype>(classIds.size()));
        for (const Domain::ClassId& classId : classIds)
        {
            const std::optional<int> legacyClassId =
                legacyClassIdFrom(classId.value());
            if (!legacyClassId)
            {
                return failure(
                    Domain::ErrorCode::InvalidInput,
                    "Class IDs must be canonical positive integers."
                    );
            }
            legacyClassIds.append(*legacyClassId);
        }

        WorkspaceStore* const store =
            m_services ? m_services->workspaceStore() : nullptr;
        if (!store || !store->isOpen())
        {
            return failure(
                Domain::ErrorCode::NotFound,
                "The active database session for roster columns is unavailable."
                );
        }

        RosterRepository* const repository = store->rosterRepository();
        if (!repository)
        {
            return failure(
                Domain::ErrorCode::NotFound,
                "The active roster column repository is unavailable."
                );
        }

        try
        {
            const RepositoryResult<QList<RosterRepository::ParentAwareColumnNamesForClass>> loaded =
                repository->loadRosterColumnNamesForClassesWithParentProvenance(
                    legacyClassIds
                    );
            if (!loaded)
            {
                return failure(
                    repositoryDomainErrorCode(loaded.error().code),
                    toStdString(toLegacyDiagnostic(loaded.error()))
                    );
            }

            if (loaded->size() != legacyClassIds.size())
            {
                return failure(
                    Domain::ErrorCode::Validation,
                    "The roster column repository returned an incomplete class list."
                    );
            }

            std::vector<Application::RosterColumnNamesReadSnapshot> snapshots;
            snapshots.reserve(classIds.size());
            for (std::size_t index = 0; index < classIds.size(); ++index)
            {
                const RosterRepository::ParentAwareColumnNamesForClass& record =
                    loaded->at(static_cast<qsizetype>(index));
                if (record.classId != legacyClassIds.at(
                        static_cast<qsizetype>(index)))
                {
                    return failure(
                        Domain::ErrorCode::Validation,
                        "The roster column repository returned a different class identifier order."
                        );
                }

                if (record.parentMissing)
                {
                    snapshots.push_back({
                        .classId = classIds[index],
                        .columnsResult =
                            Domain::Result<std::vector<std::u16string>>::failure({
                                .code = Domain::ErrorCode::NotFound,
                                .message = "The parent class for roster columns was not found.",
                                .recoverable = true
                            })
                    });
                    continue;
                }

                std::vector<std::u16string> columns;
                columns.reserve(static_cast<std::size_t>(record.columns.size()));
                for (const QString& column : record.columns)
                {
                    columns.push_back(column.toStdU16String());
                }
                snapshots.push_back({
                    .classId = classIds[index],
                    .columnsResult =
                        Domain::Result<std::vector<std::u16string>>::success(
                            std::move(columns)
                            )
                });
            }

            return Application::RosterColumnNamesBatchReadResult::success(
                std::move(snapshots)
                );
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Roster column names could not be loaded."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Roster column names could not be loaded."
                );
        }
    }

private:
    [[nodiscard]] static std::optional<int> legacyClassIdFrom(
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
        return {bytes.constData(), static_cast<std::size_t>(bytes.size())};
    }

    [[nodiscard]] static Application::RosterColumnNamesBatchReadResult
    failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        if (message.empty())
        {
            message = "Roster column names could not be loaded.";
        }
        return Application::RosterColumnNamesBatchReadResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = code == Domain::ErrorCode::NotFound
        });
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
