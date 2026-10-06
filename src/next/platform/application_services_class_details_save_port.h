#pragma once

#include "core/enums/schedule_type.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "domain/models/class_conflict.h"
#include "domain/models/class_info.h"
#include "domain/validation/class_info_validator.h"
#include "domain/validation/validation_result.h"
#include "next/application/class_details_save_use_case.h"

#include <QByteArray>
#include <QStringList>
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
                "Class details repository is unavailable."
                );
        }

        try
        {
            const Result<ClassInfo> loaded =
                repository->loadClassInfo(*classId);
            if (!loaded)
            {
                return repositoryFailure(session, loaded.error());
            }

            ClassInfo info = *loaded;
            info.classGrade = legacyText(request.classGrade);
            info.classLevel = legacyText(request.classLevel);
            info.readingBook = legacyText(request.readingBook);
            info.essayBook = legacyText(request.essayBook);
            info.classColor = legacyText(request.classColor);
            info.fontColor = legacyText(request.fontColor);
            if (request.teacherId)
            {
                const std::optional<int> teacherId =
                    legacyClassId(request.teacherId->value());
                if (!teacherId)
                {
                    return failure(
                        Domain::ErrorCode::InvalidInput,
                        "Teacher ID must be a canonical positive integer."
                        );
                }
                info.teacherId = *teacherId;
            }
            if (request.regularTimes)
            {
                info.classTimes = legacyTimes(*request.regularTimes);
            }
            if (request.intensiveTimes)
            {
                info.intensiveTimes = legacyTimes(*request.intensiveTimes);
            }

            const ClassInfo normalized = ClassInfoValidator::normalized(info);
            const ValidationResult validation =
                ClassInfoValidator::validate(normalized);
            if (validation.hasErrors())
            {
                return repositoryFailure(
                    session,
                    validationError(
                        QStringLiteral("Class information"),
                        validation
                        )
                    );
            }

            const Status regularConflicts = validateScheduleConflicts(
                *repository,
                normalized,
                normalized.classTimes,
                ScheduleType::Regular
                );
            if (!regularConflicts)
            {
                return repositoryFailure(session, regularConflicts.error());
            }

            const Status intensiveConflicts = validateScheduleConflicts(
                *repository,
                normalized,
                normalized.intensiveTimes,
                ScheduleType::Intensive
                );
            if (!intensiveConflicts)
            {
                return repositoryFailure(session, intensiveConflicts.error());
            }

            const Status saved = repository->saveClassInfo(normalized);
            if (!saved)
            {
                return repositoryFailure(session, saved.error());
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

    [[nodiscard]] static QString validationError(
        const QString& subject,
        const ValidationResult& validation
        )
    {
        QStringList details;
        for (const ValidationIssue& issue : validation.errors())
        {
            QString detail = issue.field.isEmpty()
                ? issue.code
                : QStringLiteral("%1: %2").arg(issue.field, issue.code);
            if (issue.row >= 0 && !issue.field.contains(QChar(u'[')))
            {
                detail.prepend(
                    QStringLiteral("row %1, ").arg(issue.row + 1)
                    );
            }
            details.append(detail);
        }

        return QStringLiteral("%1 validation failed: %2")
            .arg(subject, details.join(QStringLiteral("; ")));
    }

    [[nodiscard]] static Status validateScheduleConflicts(
        ClassInfoRepository& repository,
        const ClassInfo& info,
        const QList<ClassTime>& times,
        const ScheduleType type
        )
    {
        if (times.isEmpty())
        {
            return {};
        }

        const Result<QList<ClassConflict>> conflicts =
            repository.getClassTimeConflicts(info.classId, times, type);
        if (!conflicts)
        {
            return std::unexpected(conflicts.error());
        }
        if (conflicts->isEmpty())
        {
            return {};
        }

        const ClassConflict& first = conflicts->first();
        return std::unexpected(
            QStringLiteral(
                "Class schedule conflict: %1 %2\u2013%3 conflicts with %4."
                ).arg(
                    first.day,
                    first.startTime,
                    first.endTime,
                    first.conflictingClassName
                    )
            );
    }

    [[nodiscard]] static Domain::Result<void> repositoryFailure(
        DatabaseSession* session,
        const QString& message
        )
    {
        if (!session || !session->isOpen())
        {
            return unavailableFailure();
        }
        return failure(Domain::ErrorCode::Technical, toStdString(message));
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
