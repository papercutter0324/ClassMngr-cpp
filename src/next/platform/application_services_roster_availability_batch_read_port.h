#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
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

        DatabaseSession* const session =
            m_services ? m_services->databaseSession() : nullptr;
        if (!session || !session->isOpen())
        {
            return failure(
                Domain::ErrorCode::NotFound,
                "The active database session for roster availability is unavailable."
                );
        }

        RosterRepository* const repository = session->rosterRepository();
        if (!repository)
        {
            return failure(
                Domain::ErrorCode::NotFound,
                "The active roster availability repository is unavailable."
                );
        }

        try
        {
            const Result<QList<RosterRepository::ColumnNamesForClass>> loaded =
                repository->loadRosterColumnNamesForClasses(legacyClassIds);
            if (!loaded)
            {
                return failure(
                    Domain::ErrorCode::Technical,
                    toStdString(loaded.error())
                    );
            }
            if (loaded->size() != legacyClassIds.size())
            {
                return failure(
                    Domain::ErrorCode::Validation,
                    "The roster column repository returned an incomplete class list."
                    );
            }

            std::vector<std::u16string> requiredColumnNames = baseColumnNames;
            std::vector<Application::RosterAvailabilityAccumulator>
                accumulators;
            accumulators.reserve(classIds.size());
            for (std::size_t index = 0; index < classIds.size(); ++index)
            {
                const RosterRepository::ColumnNamesForClass& record =
                    loaded->at(static_cast<qsizetype>(index));
                if (record.classId != legacyClassIds.at(
                        static_cast<qsizetype>(index)
                        ))
                {
                    return failure(
                        Domain::ErrorCode::Validation,
                        "The roster column repository returned a different class identifier order."
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
            const Status streamed =
                repository->forEachRosterDataCellForClasses(
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
                        accumulators[static_cast<std::size_t>(*recordIndex)]
                            .observeCell(rowIndex, columnIndex, text);
                    }
                    );
            if (!streamed)
            {
                return failure(
                    Domain::ErrorCode::Technical,
                    toStdString(streamed.error())
                    );
            }
            if (sawUnexpectedClassId)
            {
                return failure(
                    Domain::ErrorCode::Validation,
                    "The roster cell repository returned an unexpected class id."
                    );
            }

            std::vector<Application::RosterAvailabilityReadSnapshot>
                snapshots;
            snapshots.reserve(classIds.size());
            for (std::size_t index = 0; index < classIds.size(); ++index)
            {
                snapshots.push_back({
                    .classId = classIds[index],
                    .firstEmptyRow = accumulators[index].firstEmptyRow()
                });
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
