#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
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

        DatabaseSession* const session =
            m_services ? m_services->databaseSession() : nullptr;
        if (!session || !session->isOpen())
        {
            return Application::RosterPrintClassInfoReadResult::failure(
                error(
                    Domain::ErrorCode::NotFound,
                    "The active database session for roster print class information is unavailable."
                    )
                );
        }

        ClassInfoRepository* const classRepository =
            session->classInfoRepository();
        if (!classRepository)
        {
            return Application::RosterPrintClassInfoReadResult::failure(error(
                Domain::ErrorCode::NotFound,
                "The roster print class information repository is unavailable."
                ));
        }

        Result<RosterPrintClassInfoReadRecord> loaded = [&]()
            -> Result<RosterPrintClassInfoReadRecord>
        {
            try
            {
                return classRepository->loadRosterPrintClassInfoRecord(
                    *legacyClassId
                    );
            }
            catch (const std::exception&)
            {
                return std::unexpected(
                    QStringLiteral("Roster print class information could not be loaded.")
                    );
            }
            catch (...)
            {
                return std::unexpected(
                    QStringLiteral("Roster print class information could not be loaded.")
                    );
            }
        }();

        if (!loaded)
        {
            return Application::RosterPrintClassInfoReadResult::failure(
                legacyError(loaded.error())
                );
        }

        const std::string returnedId = std::to_string(loaded->classId);
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
        snapshot.classGrade = loaded->classGrade.toStdU16String();
        snapshot.classLevel = loaded->classLevel.toStdU16String();
        snapshot.teacherEn = loaded->teacherEnglishName.toStdU16String();
        snapshot.teacherKr = loaded->teacherKoreanName.toStdU16String();
        snapshot.roomNumber = loaded->roomNumber.toStdU16String();
        snapshot.wifiName = loaded->wifiName.toStdU16String();
        snapshot.wifiPassword = loaded->wifiPassword.toStdU16String();
        snapshot.zoomId = loaded->zoomId.toStdU16String();
        snapshot.zoomPassword = loaded->zoomPassword.toStdU16String();
        snapshot.regularSchedule.reserve(
            static_cast<std::size_t>(loaded->regularTimes.size())
            );
        for (const ClassTime& row : loaded->regularTimes)
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

    [[nodiscard]] static Domain::OperationError legacyError(
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
        else if (normalized.contains(QStringLiteral("validation")))
        {
            code = Domain::ErrorCode::Validation;
        }

        const QByteArray bytes = message.toUtf8();
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
