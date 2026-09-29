#pragma once

#include "core/application_services.h"
#include "core/enums/schedule_type.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "next/application/class_details_schedule_conflict_query.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QTime>

#include <array>
#include <charconv>
#include <exception>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesClassDetailsScheduleConflictPort final
    : public Application::ClassDetailsScheduleConflictPort
{
public:
    explicit ApplicationServicesClassDetailsScheduleConflictPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    [[nodiscard]] Application::ClassDetailsScheduleConflictResult
    classDetailsScheduleConflicts(
        const Application::ClassDetailsScheduleConflictRequest& request
        ) const override
    {
        const std::optional<int> classId = legacyId(request.classId.value());
        if (!classId)
        {
            return failure(
                Domain::ErrorCode::InvalidInput,
                "Class ID must be a canonical positive integer."
                );
        }

        const std::optional<ScheduleType> scheduleType =
            legacyScheduleType(request.mode);
        if (!scheduleType)
        {
            return failure(
                Domain::ErrorCode::InvalidInput,
                "Class schedule mode is invalid."
                );
        }

        DatabaseSession* const session =
            m_services ? m_services->databaseSession() : nullptr;
        if (!session || !session->isOpen())
        {
            return unavailableFailure();
        }

        ClassInfoRepository* const repository =
            session->classInfoRepository();
        if (!repository)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Class schedule conflicts could not be checked."
                );
        }

        try
        {
            // Overlap and ordering stay in persistence; this adapter only maps
            // typed request values and Qt-free display fields.
            const Result<QList<ClassConflict>> loaded =
                repository->getClassTimeConflicts(
                    *classId,
                    legacyTimes(request.candidateTimes),
                    *scheduleType
                    );
            if (!loaded)
            {
                return failure(
                    Domain::ErrorCode::Technical,
                    toStdString(loaded.error())
                    );
            }

            std::vector<Application::ClassDetailsScheduleConflict> conflicts;
            conflicts.reserve(static_cast<std::size_t>(loaded->size()));
            for (const ClassConflict& conflict : *loaded)
            {
                conflicts.push_back({
                    .className = conflict.className.toStdU16String(),
                    .day = conflict.day.toStdU16String(),
                    .startTime = conflict.startTime.toStdU16String(),
                    .endTime = conflict.endTime.toStdU16String(),
                    .conflictingClassName =
                        conflict.conflictingClassName.toStdU16String()
                });
            }

            return Application::ClassDetailsScheduleConflictResult::success(
                std::move(conflicts)
                );
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Class schedule conflicts could not be checked."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Class schedule conflicts could not be checked."
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

    [[nodiscard]] static std::optional<ScheduleType> legacyScheduleType(
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

    [[nodiscard]] static QList<ClassTime> legacyTimes(
        const std::vector<Domain::ScheduleTime>& times
        )
    {
        static const std::array<QString, 7> weekdays{
            QStringLiteral("Monday"),
            QStringLiteral("Tuesday"),
            QStringLiteral("Wednesday"),
            QStringLiteral("Thursday"),
            QStringLiteral("Friday"),
            QStringLiteral("Saturday"),
            QStringLiteral("Sunday")
        };

        QList<ClassTime> converted;
        converted.reserve(static_cast<qsizetype>(times.size()));
        for (const Domain::ScheduleTime& time : times)
        {
            const int weekday = time.weekdayIndex();
            const QTime start(time.startMinute() / 60, time.startMinute() % 60);
            const QTime end(time.endMinute() / 60, time.endMinute() % 60);
            converted.append({
                weekdays.at(static_cast<std::size_t>(weekday)),
                start.toString(QStringLiteral("h:mm AP")),
                end.toString(QStringLiteral("h:mm AP"))
            });
        }

        return converted;
    }

    [[nodiscard]] static std::string toStdString(const QString& value)
    {
        const QByteArray bytes = value.toUtf8();
        return std::string(
            bytes.constData(),
            static_cast<std::size_t>(bytes.size())
            );
    }

    [[nodiscard]] static Application::ClassDetailsScheduleConflictResult
    failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        if (message.empty())
        {
            message = "Class schedule conflicts could not be checked.";
        }

        return Application::ClassDetailsScheduleConflictResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = code != Domain::ErrorCode::Validation
        });
    }

    [[nodiscard]] static Application::ClassDetailsScheduleConflictResult
    unavailableFailure()
    {
        return failure(
            Domain::ErrorCode::NotFound,
            "The active database session for schedule conflicts is unavailable."
            );
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
