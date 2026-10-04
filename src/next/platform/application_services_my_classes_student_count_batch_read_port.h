#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/roster_repository.h"
#include "next/application/my_classes_student_count_batch_read_port.h"

#include <QByteArray>
#include <QString>

#include <charconv>
#include <exception>
#include <optional>
#include <string>
#include <system_error>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesMyClassesStudentCountBatchReadPort final
    : public Application::MyClassesStudentCountBatchReadPort
{
public:
    explicit ApplicationServicesMyClassesStudentCountBatchReadPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    explicit ApplicationServicesMyClassesStudentCountBatchReadPort(
        ApplicationServices& services
        ) noexcept
        : m_services(&services)
    {
    }

    ApplicationServicesMyClassesStudentCountBatchReadPort(
        const ApplicationServicesMyClassesStudentCountBatchReadPort&
        ) = delete;
    ApplicationServicesMyClassesStudentCountBatchReadPort& operator=(
        const ApplicationServicesMyClassesStudentCountBatchReadPort&
        ) = delete;
    ApplicationServicesMyClassesStudentCountBatchReadPort(
        ApplicationServicesMyClassesStudentCountBatchReadPort&&
        ) = delete;
    ApplicationServicesMyClassesStudentCountBatchReadPort& operator=(
        ApplicationServicesMyClassesStudentCountBatchReadPort&&
        ) = delete;

    [[nodiscard]] Application::MyClassesStudentCountBatchReadResult
    readMyClassesStudentCounts(
        const std::vector<Domain::ClassId>& classIds
        ) const override
    {
        if (classIds.empty())
        {
            return Application::MyClassesStudentCountBatchReadResult::success(
                {}
                );
        }

        QList<int> legacyClassIds;
        legacyClassIds.reserve(static_cast<qsizetype>(classIds.size()));
        std::unordered_set<int> seenClassIds;
        seenClassIds.reserve(classIds.size());
        for (const Domain::ClassId& classId : classIds)
        {
            const std::optional<int> legacyId = legacyClassId(classId);
            if (!legacyId || !seenClassIds.insert(*legacyId).second)
            {
                return Application::MyClassesStudentCountBatchReadResult::failure(
                    error(
                        Domain::ErrorCode::InvalidInput,
                        "My Classes class IDs must be canonical positive integers and unique.",
                        false
                        )
                    );
            }
            legacyClassIds.append(*legacyId);
        }

        DatabaseSession* const session =
            m_services ? m_services->databaseSession() : nullptr;
        if (!session || !session->isOpen())
        {
            return failedEntries(
                classIds,
                error(
                    Domain::ErrorCode::NotFound,
                    "No active My Classes roster database session is available.",
                    true
                    )
                );
        }

        RosterRepository* const repository = session->rosterRepository();
        if (!repository)
        {
            return failedEntries(
                classIds,
                error(
                    Domain::ErrorCode::NotFound,
                    "The My Classes roster repository is unavailable.",
                    true
                    )
                );
        }

        Result<QList<RosterRepository::MyClassesStudentCountReadEntry>> loaded =
            [&]() -> Result<
                QList<RosterRepository::MyClassesStudentCountReadEntry>
                >
        {
            try
            {
                return repository->loadMyClassesStudentCountRecords(
                    legacyClassIds
                    );
            }
            catch (const std::exception&)
            {
                return std::unexpected(QStringLiteral(
                    "My Classes student counts could not be loaded."
                    ));
            }
            catch (...)
            {
                return std::unexpected(QStringLiteral(
                    "My Classes student counts could not be loaded."
                    ));
            }
        }();

        if (!loaded)
        {
            return failedEntries(classIds, repositoryError(loaded.error()));
        }

        if (loaded->size() != classIds.size())
        {
            return failedEntries(
                classIds,
                error(
                    Domain::ErrorCode::Validation,
                    "The My Classes roster repository returned an incomplete class list.",
                    false
                    )
                );
        }

        Application::MyClassesStudentCountBatchReadSnapshot entries;
        entries.reserve(classIds.size());
        for (std::size_t index = 0; index < classIds.size(); ++index)
        {
            const RosterRepository::MyClassesStudentCountReadEntry& source =
                loaded->at(static_cast<qsizetype>(index));
            if (source.classId != legacyClassIds.at(
                    static_cast<qsizetype>(index)))
            {
                entries.push_back({
                    .classId = classIds[index],
                    .studentCount = Domain::Result<int>::failure(error(
                        Domain::ErrorCode::Validation,
                        "The My Classes roster repository returned entries in a different identifier order.",
                        false
                        ))
                });
                continue;
            }

            if (!source.studentCount)
            {
                entries.push_back({
                    .classId = classIds[index],
                    .studentCount = Domain::Result<int>::failure(
                        repositoryError(source.studentCount.error())
                        )
                });
                continue;
            }

            entries.push_back({
                .classId = classIds[index],
                .studentCount = Domain::Result<int>::success(
                    source.studentCount.value()
                    )
            });
        }

        return Application::MyClassesStudentCountBatchReadResult::success(
            std::move(entries)
            );
    }

private:
    [[nodiscard]] static std::optional<int> legacyClassId(
        const Domain::ClassId& classId
        )
    {
        const std::string& value = classId.value();
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
        std::string message,
        const bool recoverable
        )
    {
        return {
            .code = code,
            .message = std::move(message),
            .recoverable = recoverable
        };
    }

    [[nodiscard]] static Domain::OperationError repositoryError(
        const QString& message
        )
    {
        const QByteArray bytes = message.toUtf8();
        return error(
            Domain::ErrorCode::Technical,
            bytes.isEmpty()
                ? "My Classes student count could not be loaded."
                : bytes.toStdString(),
            true
            );
    }

    [[nodiscard]] static Application::MyClassesStudentCountBatchReadResult
    failedEntries(
        const std::vector<Domain::ClassId>& classIds,
        const Domain::OperationError& failure
        )
    {
        Application::MyClassesStudentCountBatchReadSnapshot entries;
        entries.reserve(classIds.size());
        for (const Domain::ClassId& classId : classIds)
        {
            entries.push_back({
                .classId = classId,
                .studentCount = Domain::Result<int>::failure(failure)
            });
        }
        return Application::MyClassesStudentCountBatchReadResult::success(
            std::move(entries)
            );
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
