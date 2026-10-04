#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/roster_repository.h"
#include "next/application/roster_template_print_source_read_port.h"

#include <QByteArray>
#include <QStringList>

#include <charconv>
#include <exception>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesRosterTemplatePrintSourceReadPort final
    : public Application::RosterTemplatePrintSourceReadPort
{
public:
    explicit ApplicationServicesRosterTemplatePrintSourceReadPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    [[nodiscard]] Application::RosterTemplatePrintSourceReadResult
    readRosterTemplatePrintSource(
        const Application::RosterTemplatePrintSourceReadRequest& query
        ) const override
    {
        if (query.classIds.empty())
        {
            return Application::RosterTemplatePrintSourceReadResult::success({});
        }

        QList<int> legacyClassIds;
        std::vector<Domain::ClassId> typedClassIds;
        legacyClassIds.reserve(
            static_cast<qsizetype>(query.classIds.size())
            );
        typedClassIds.reserve(query.classIds.size());
        for (const Domain::ClassId& typedId : query.classIds)
        {
            const std::optional<int> classId = legacyId(typedId.value());
            if (!classId)
            {
                return failure(
                    Domain::ErrorCode::InvalidInput,
                    "Roster print class IDs must be canonical positive integers."
                    );
            }
            if (legacyClassIds.contains(*classId))
            {
                return failure(
                    Domain::ErrorCode::InvalidInput,
                    "Roster print class IDs must be unique."
                    );
            }
            legacyClassIds.append(*classId);
            typedClassIds.push_back(typedId);
        }

        DatabaseSession* const session =
            m_services ? m_services->databaseSession() : nullptr;
        if (!session || !session->isOpen())
        {
            return failure(
                Domain::ErrorCode::NotFound,
                "The active database session for roster template printing is unavailable."
                );
        }

        ClassInfoRepository* const classInfoRepository =
            session->classInfoRepository();
        RosterRepository* const rosterRepository =
            session->rosterRepository();
        if (!classInfoRepository || !rosterRepository)
        {
            return failure(
                Domain::ErrorCode::NotFound,
                "A roster template print repository is unavailable."
                );
        }

        try
        {
            const Result<QList<RosterPrintClassInfoReadRecord>> classInfoRecords =
                classInfoRepository->loadRosterPrintClassInfoRecords(
                    legacyClassIds
                    );
            if (!classInfoRecords)
            {
                return failure(
                    Domain::ErrorCode::Technical,
                    toStdString(classInfoRecords.error())
                    );
            }
            if (classInfoRecords->size() != legacyClassIds.size())
            {
                return failure(
                    Domain::ErrorCode::Validation,
                    "Roster print class information batch returned a different number of classes."
                    );
            }

            const Result<QList<RosterRepository::TemplatePrintReadRecord>>
                rosterRecords = rosterRepository->loadRostersForTemplatePrint(
                    legacyClassIds
                    );
            if (!rosterRecords)
            {
                return failure(
                    Domain::ErrorCode::Technical,
                    toStdString(rosterRecords.error())
                    );
            }
            if (rosterRecords->size() != legacyClassIds.size())
            {
                return failure(
                    Domain::ErrorCode::Validation,
                    "Roster print roster batch returned a different number of classes."
                    );
            }

            Application::RosterTemplatePrintSourceReadSnapshot snapshots;
            snapshots.reserve(query.classIds.size());
            for (qsizetype index = 0;
                 index < legacyClassIds.size();
                 ++index)
            {
                const RosterPrintClassInfoReadRecord& classInfo =
                    classInfoRecords->at(index);
                const RosterRepository::TemplatePrintReadRecord& roster =
                    rosterRecords->at(index);
                if (classInfo.classId != legacyClassIds.at(index)
                    || roster.classId != legacyClassIds.at(index))
                {
                    return failure(
                        Domain::ErrorCode::Validation,
                        "A roster print batch returned a class in the wrong order."
                        );
                }

                Application::RosterPrintClassInfoReadSnapshot infoSnapshot(
                    typedClassIds[static_cast<std::size_t>(index)]
                    );
                infoSnapshot.classGrade = classInfo.classGrade.toStdU16String();
                infoSnapshot.classLevel = classInfo.classLevel.toStdU16String();
                infoSnapshot.teacherEn =
                    classInfo.teacherEnglishName.toStdU16String();
                infoSnapshot.teacherKr =
                    classInfo.teacherKoreanName.toStdU16String();
                infoSnapshot.roomNumber = classInfo.roomNumber.toStdU16String();
                infoSnapshot.wifiName = classInfo.wifiName.toStdU16String();
                infoSnapshot.wifiPassword =
                    classInfo.wifiPassword.toStdU16String();
                infoSnapshot.zoomId = classInfo.zoomId.toStdU16String();
                infoSnapshot.zoomPassword =
                    classInfo.zoomPassword.toStdU16String();
                infoSnapshot.regularSchedule.reserve(
                    static_cast<std::size_t>(classInfo.regularTimes.size())
                    );
                for (const ClassTime& time : classInfo.regularTimes)
                {
                    infoSnapshot.regularSchedule.push_back({
                        time.day.toStdU16String(),
                        time.startTime.toStdU16String(),
                        time.endTime.toStdU16String()
                    });
                }

                snapshots.emplace_back(
                    typedClassIds[static_cast<std::size_t>(index)],
                    std::move(infoSnapshot),
                    applicationSnapshot(roster.roster)
                    );
            }

            return Application::RosterTemplatePrintSourceReadResult::success(
                std::move(snapshots)
                );
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Roster template print data could not be loaded."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Roster template print data could not be loaded."
                );
        }
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

    [[nodiscard]] static Application::RosterSnapshot applicationSnapshot(
        const Roster& roster
        )
    {
        Application::RosterSnapshot snapshot;
        snapshot.columns.reserve(
            static_cast<std::size_t>(roster.columns.size())
            );
        for (const QString& column : roster.columns)
        {
            snapshot.columns.push_back(column.toStdU16String());
        }

        snapshot.columnWidths.reserve(
            static_cast<std::size_t>(roster.columnWidths.size())
            );
        for (const int width : roster.columnWidths)
        {
            snapshot.columnWidths.push_back(width);
        }

        snapshot.rows.reserve(static_cast<std::size_t>(roster.rows.size()));
        for (const QStringList& sourceRow : roster.rows)
        {
            std::vector<std::u16string> row;
            row.reserve(static_cast<std::size_t>(sourceRow.size()));
            for (const QString& cell : sourceRow)
            {
                row.push_back(cell.toStdU16String());
            }
            snapshot.rows.push_back(std::move(row));
        }

        return snapshot;
    }

    [[nodiscard]] static std::string toStdString(const QString& value)
    {
        const QByteArray bytes = value.toUtf8();
        return bytes.toStdString();
    }

    [[nodiscard]] static Application::RosterTemplatePrintSourceReadResult
    failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        if (message.empty())
        {
            message = "Roster template print data could not be loaded.";
        }

        return Application::RosterTemplatePrintSourceReadResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = code != Domain::ErrorCode::InvalidInput
        });
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
