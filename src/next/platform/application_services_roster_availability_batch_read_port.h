#pragma once

#include "next/platform/repository_error_code.h"

#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/roster_repository.h"
#include "next/application/roster_availability_accumulator.h"
#include "next/application/roster_availability_batch_read_port.h"

#include <QByteArray>
#include <QHash>
#include <QString>

#include <charconv>
#include <exception>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesRosterAvailabilityBatchReadPort final
    : public Application::RosterAvailabilityBatchReadPort
{
public:
    explicit ApplicationServicesRosterAvailabilityBatchReadPort(
        ApplicationServices& services
        ) noexcept
        : m_services(&services)
    {
    }

    explicit ApplicationServicesRosterAvailabilityBatchReadPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    [[nodiscard]] Application::RosterAvailabilityBatchReadResult
    readRosterAvailability(
        const std::vector<Domain::ClassId>& classIds,
        const std::vector<std::u16string>& baseColumnNames
        ) const override
    {
        if (classIds.empty())
        {
            return Application::RosterAvailabilityBatchReadResult::success({});
        }

        QList<int> legacyClassIds;
        legacyClassIds.reserve(static_cast<qsizetype>(classIds.size()));
        QHash<int, qsizetype> recordIndexByClassId;
        recordIndexByClassId.reserve(static_cast<qsizetype>(classIds.size()));
        for (std::size_t index = 0; index < classIds.size(); ++index)
        {
            const std::optional<int> legacyClassId =
                legacyClassIdFrom(classIds[index].value());
            if (!legacyClassId)
            {
                return failure(
                    Domain::ErrorCode::InvalidInput,
                    "Class IDs must be canonical positive integers."
                    );
            }
            if (recordIndexByClassId.contains(*legacyClassId))
            {
                return failure(
                    Domain::ErrorCode::InvalidInput,
                    "Class IDs must be unique."
                    );
            }
            recordIndexByClassId.insert(
                *legacyClassId,
                static_cast<qsizetype>(index)
                );
            legacyClassIds.append(*legacyClassId);
        }

        WorkspaceStore* const store =
            m_services ? m_services->workspaceStore() : nullptr;
        if (!store || !store->isOpen())
        {
            return failure(
                Domain::ErrorCode::NotFound,
                "The active database session for roster availability is unavailable."
                );
        }

        RosterRepository* const repository = store->rosterRepository();
        if (!repository)
        {
            return failure(
                Domain::ErrorCode::NotFound,
                "The active roster availability repository is unavailable."
                );
        }

        try
        {
            std::vector<Application::RosterAvailabilityReadSnapshot>
                snapshots;
            Domain::ErrorCode operationFailureCode =
                Domain::ErrorCode::Technical;
            const auto repositoryCodeFor = [](const Domain::ErrorCode code)
                -> RepositoryErrorCode
            {
                switch (code)
                {
                case Domain::ErrorCode::InvalidInput:
                    return RepositoryErrorCode::InvalidInput;
                case Domain::ErrorCode::NotFound:
                    return RepositoryErrorCode::NotFound;
                case Domain::ErrorCode::Conflict:
                    return RepositoryErrorCode::Conflict;
                case Domain::ErrorCode::Validation:
                    return RepositoryErrorCode::Validation;
                case Domain::ErrorCode::Canceled:
                case Domain::ErrorCode::Technical:
                    return RepositoryErrorCode::Technical;
                }
                return RepositoryErrorCode::Technical;
            };
            const auto operationFailure = [
                &operationFailureCode,
                &repositoryCodeFor
                ](
                    const Domain::ErrorCode code,
                    QString diagnostic
                    ) -> RepositoryStatus
            {
                operationFailureCode = code;
                return std::unexpected(RepositoryError{
                    repositoryCodeFor(code),
                    std::move(diagnostic)
                });
            };

            const auto transactionFailure = store->withReadTransaction(
                [&]() -> RepositoryStatus
                {
                    const RepositoryResult<
                        QList<RosterRepository::ParentAwareColumnNamesForClass>
                        > loaded = repository
                            ->loadRosterColumnNamesForClassesWithParentProvenance(
                                legacyClassIds
                                );
                    if (!loaded)
                    {
                        return operationFailure(
                            repositoryDomainErrorCode(loaded.error().code),
                            toLegacyDiagnostic(loaded.error())
                            );
                    }
                    if (loaded->size() != legacyClassIds.size())
                    {
                        return operationFailure(
                            Domain::ErrorCode::Validation,
                            QStringLiteral(
                                "The roster column repository returned an incomplete class list."
                                )
                            );
                    }

                    std::vector<std::u16string> requiredColumnNames = baseColumnNames;
                    std::vector<Application::RosterAvailabilityAccumulator>
                        accumulators;
                    accumulators.reserve(classIds.size());
                    for (std::size_t index = 0; index < classIds.size(); ++index)
                    {
                        const RosterRepository::ParentAwareColumnNamesForClass&
                            record = loaded->at(static_cast<qsizetype>(index));
                        if (record.classId != legacyClassIds.at(
                                static_cast<qsizetype>(index)
                                ))
                        {
                            return operationFailure(
                                Domain::ErrorCode::Validation,
                                QStringLiteral(
                                    "The roster column repository returned a different class identifier order."
                                    )
                                );
                        }

                        std::vector<std::u16string> storedColumns;
                        storedColumns.reserve(
                            static_cast<std::size_t>(record.columns.size())
                            );
                        for (const QString& column : record.columns)
                        {
                            storedColumns.push_back(column.toStdU16String());
                        }
                        accumulators.emplace_back(
                            storedColumns,
                            requiredColumnNames,
                            qtCaseInsensitiveEquals
                            );
                    }

                    bool sawUnexpectedClassId = false;
                    const RepositoryStatus streamed =
                        repository->forEachRosterDataCellForExistingClasses(
                            legacyClassIds,
                            static_cast<int>(Application::RosterModeledRowCount),
                            [&accumulators,
                             &recordIndexByClassId,
                             &sawUnexpectedClassId](
                                const int classId,
                                const int rowIndex,
                                const int columnIndex,
                                const QString& value
                                )
                            {
                                const auto recordIndex =
                                    recordIndexByClassId.constFind(classId);
                                if (recordIndex == recordIndexByClassId.cend())
                                {
                                    sawUnexpectedClassId = true;
                                    return;
                                }

                                const std::u16string text = value.toStdU16String();
                                const Application::RosterAvailabilityAccumulatorRowIndex
                                    applicationRowIndex{rowIndex};
                                const Application::RosterAvailabilityAccumulatorSourceColumnIndex
                                    applicationSourceColumnIndex{columnIndex};
                                accumulators[static_cast<std::size_t>(*recordIndex)]
                                    .observeCell(
                                        applicationRowIndex,
                                        applicationSourceColumnIndex,
                                        text
                                        );
                            }
                            );
                    if (!streamed)
                    {
                        return operationFailure(
                            repositoryDomainErrorCode(streamed.error().code),
                            toLegacyDiagnostic(streamed.error())
                            );
                    }
                    if (sawUnexpectedClassId)
                    {
                        return operationFailure(
                            Domain::ErrorCode::Validation,
                            QStringLiteral(
                                "The roster cell repository returned an unexpected class id."
                                )
                            );
                    }

                    snapshots.reserve(classIds.size());
                    for (std::size_t index = 0; index < classIds.size(); ++index)
                    {
                        const RosterRepository::ParentAwareColumnNamesForClass&
                            record = loaded->at(static_cast<qsizetype>(index));
                        if (record.parentMissing)
                        {
                            snapshots.push_back({
                                .classId = classIds[index],
                                .firstEmptyRowResult = Domain::Result<
                                    Application::RosterAvailabilityFirstEmptyRow
                                    >::failure({
                                        .code = Domain::ErrorCode::NotFound,
                                        .message = "The requested class does not exist.",
                                        .recoverable = true
                                    })
                            });
                        }
                        else
                        {
                            snapshots.push_back({
                                .classId = classIds[index],
                                .firstEmptyRowResult = Domain::Result<
                                    Application::RosterAvailabilityFirstEmptyRow
                                    >::success(accumulators[index].firstEmptyRow())
                            });
                        }
                    }
                    return {};
                }
                );

            if (transactionFailure)
            {
                using Stage = WorkspaceStore::ReadTransactionFailure::Stage;
                if (transactionFailure->stage == Stage::Start)
                {
                    return failure(
                        Domain::ErrorCode::Technical,
                        toStdString(QStringLiteral(
                            "Starting the roster availability read transaction failed: %1"
                            ).arg(transactionFailure->error.diagnostic))
                        );
                }
                if (transactionFailure->stage == Stage::Operation)
                {
                    return failure(
                        operationFailureCode,
                        toStdString(transactionFailure->error.diagnostic)
                        );
                }

                QString error = transactionFailure->error.diagnostic;
                if (error.isEmpty())
                {
                    error = QStringLiteral("unknown database error");
                }
                return failure(
                    Domain::ErrorCode::Technical,
                    toStdString(QStringLiteral(
                        "Committing the roster availability read transaction failed: %1"
                        ).arg(error))
                    );
            }

            return Application::RosterAvailabilityBatchReadResult::success(
                std::move(snapshots)
                );
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Roster availability could not be loaded."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Roster availability could not be loaded."
                );
        }
    }

private:
    [[nodiscard]] static bool qtCaseInsensitiveEquals(
        const std::u16string_view left,
        const std::u16string_view right
        )
    {
        return QString::fromStdU16String(std::u16string(left)).compare(
            QString::fromStdU16String(std::u16string(right)),
            Qt::CaseInsensitive
            ) == 0;
    }

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

    [[nodiscard]] static Application::RosterAvailabilityBatchReadResult
    failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        if (message.empty())
        {
            message = "Roster availability could not be loaded.";
        }
        return Application::RosterAvailabilityBatchReadResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = code == Domain::ErrorCode::NotFound
        });
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
