#pragma once

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "domain/models/class_info.h"
#include "next/application/class_details_save_use_case.h"

#include <QByteArray>
#include <QTime>

#include <charconv>
#include <exception>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesClassDetailsSavePort final
    : public Application::ClassDetailsSavePort
{
public:
    explicit ApplicationServicesClassDetailsSavePort(
        ApplicationServices& services
        ) noexcept
        : m_services(&services)
    {
    }

    explicit ApplicationServicesClassDetailsSavePort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesClassDetailsSavePort(
        const ApplicationServicesClassDetailsSavePort&
        ) = delete;
    ApplicationServicesClassDetailsSavePort& operator=(
        const ApplicationServicesClassDetailsSavePort&
        ) = delete;
    ApplicationServicesClassDetailsSavePort(
        ApplicationServicesClassDetailsSavePort&&
        ) = delete;
    ApplicationServicesClassDetailsSavePort& operator=(
        ApplicationServicesClassDetailsSavePort&&
        ) = delete;

    [[nodiscard]] Domain::Result<void> saveClassDetails(
        const Application::ClassDetailsSaveRequest& request
        ) const override
    {
        const std::optional<int> classId = legacyClassId(request.classId.value());
        if (!classId)
        {
            return failure(
                Domain::ErrorCode::InvalidInput,
                "Class ID must be a positive integer."
                );
        }

        ClassService* const classService =
            m_services ? m_services->classService() : nullptr;
        if (!classService || !classService->isAvailable())
        {
            return unavailableFailure();
        }

        try
        {
            const Result<ClassInfo> loaded = classService->classInfo(*classId);
            if (!loaded)
            {
                if (!classService->isAvailable())
                {
                    return unavailableFailure();
                }

                return failure(
                    Domain::ErrorCode::Technical,
                    toStdString(loaded.error())
                    );
            }

            ClassInfo info = *loaded;
            info.classGrade = legacyText(request.classGrade);
            info.classLevel = legacyText(request.classLevel);
            info.readingBook = legacyText(request.readingBook);
            info.essayBook = legacyText(request.essayBook);
            info.classColor = legacyText(request.classColor);
            info.fontColor = legacyText(request.fontColor);
            if (request.regularTimes)
            {
                info.classTimes = legacyTimes(*request.regularTimes);
            }
            if (request.intensiveTimes)
            {
                info.intensiveTimes = legacyTimes(*request.intensiveTimes);
            }

            const Status saved = classService->saveClassInfo(info);
            if (!saved)
            {
                if (!classService->isAvailable())
                {
                    return unavailableFailure();
                }

                return failure(
                    Domain::ErrorCode::Technical,
                    toStdString(saved.error())
                    );
            }

            return Domain::Result<void>::success();
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Class details could not be saved."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Class details could not be saved."
                );
        }
    }

private:
    [[nodiscard]] static std::optional<int> legacyClassId(
        const std::string& value
        )
    {
        int parsed = 0;
        const auto [end, error] = std::from_chars(
            value.data(),
            value.data() + value.size(),
            parsed
            );
        if (error != std::errc{}
            || end != value.data() + value.size()
            || parsed <= 0)
        {
            return std::nullopt;
        }

        return parsed;
    }

    [[nodiscard]] static QString legacyText(const std::u16string& value)
    {
        return QString::fromUtf16(
            value.data(),
            static_cast<qsizetype>(value.size())
            );
    }

    [[nodiscard]] static QString legacyDay(const Domain::Weekday day)
    {
        switch (day)
        {
        case Domain::Weekday::Monday:
            return QStringLiteral("Monday");
        case Domain::Weekday::Tuesday:
            return QStringLiteral("Tuesday");
        case Domain::Weekday::Wednesday:
            return QStringLiteral("Wednesday");
        case Domain::Weekday::Thursday:
            return QStringLiteral("Thursday");
        case Domain::Weekday::Friday:
            return QStringLiteral("Friday");
        case Domain::Weekday::Saturday:
            return QStringLiteral("Saturday");
        case Domain::Weekday::Sunday:
            return QStringLiteral("Sunday");
        }

        return {};
    }

    [[nodiscard]] static ClassTime legacyTime(
        const Domain::ScheduleTime& value
        )
    {
        const QTime start(
            value.startMinute() / 60,
            value.startMinute() % 60
            );
        const QTime end(
            value.endMinute() / 60,
            value.endMinute() % 60
            );
        return {
            .day = legacyDay(value.weekday()),
            .startTime = start.toString(QStringLiteral("h:mm AP")),
            .endTime = end.toString(QStringLiteral("h:mm AP"))
        };
    }

    [[nodiscard]] static QList<ClassTime> legacyTimes(
        const std::vector<Domain::ScheduleTime>& values
        )
    {
        QList<ClassTime> result;
        result.reserve(static_cast<qsizetype>(values.size()));
        for (const Domain::ScheduleTime& value : values)
        {
            result.append(legacyTime(value));
        }
        return result;
    }

    [[nodiscard]] static std::string toStdString(const QString& value)
    {
        const QByteArray bytes = value.toUtf8();
        return bytes.toStdString();
    }

    [[nodiscard]] static Domain::Result<void> failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        if (message.empty())
        {
            message = "Class details could not be saved.";
        }

        return Domain::Result<void>::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = false
        });
    }

    [[nodiscard]] static Domain::Result<void> unavailableFailure()
    {
        return failure(
            Domain::ErrorCode::NotFound,
            "The class service is unavailable."
            );
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
