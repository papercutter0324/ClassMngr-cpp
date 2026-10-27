#pragma once

#include "data/database/workspace_store.h"
#include "data/repositories/class_info_repository.h"
#include "data/repositories/class_repository.h"
#include "domain/config/class_details_catalog.h"
#include "domain/config/class_schedule_catalog.h"
#include "domain/models/class_info.h"
#include "next/application/class_create.h"
#include "next/application/class_details_save_use_case.h"
#include "next/application/initial_setup_class_times_requirement.h"

#include <QByteArray>
#include <QTime>

#include <exception>
#include <array>
#include <charconv>
#include <cstddef>
#include <limits>
#include <ranges>
#include <string>
#include <system_error>
#include <utility>

namespace ClassMngr::Next::Platform
{

class WorkspaceStoreInitialSetupClassCreatePort final
    : public Application::ClassCreatePort
{
public:
    explicit WorkspaceStoreInitialSetupClassCreatePort(
        WorkspaceStore& store
        ) noexcept
        : m_store(&store)
    {
    }

    [[nodiscard]] Application::ClassCreateResult createClass(
        const Application::ClassCreateRequest& request
        ) const override
    {
        Q_UNUSED(request);
        if (!m_store || !m_store->isOpen())
        {
            return Application::ClassCreateResult::failure(unavailableError());
        }

        ClassRepository* const repository = m_store->classRepository();
        if (!repository)
        {
            return Application::ClassCreateResult::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "The class repository is unavailable.",
                .recoverable = true
            });
        }

        try
        {
            const RepositoryResult<int> created = repository->createClass(QString{});
            if (!created)
            {
                return Application::ClassCreateResult::failure(
                    repositoryError(created.error())
                    );
            }
            if (*created <= 0)
            {
                return Application::ClassCreateResult::failure({
                    .code = Domain::ErrorCode::Technical,
                    .message = "Creating the class did not return a valid ID.",
                    .recoverable = false
                });
            }
            const auto classId = Domain::ClassId::fromString(
                std::to_string(*created)
                );
            if (!classId)
            {
                return Application::ClassCreateResult::failure({
                    .code = Domain::ErrorCode::Technical,
                    .message = "Creating the class did not return a valid ID.",
                    .recoverable = false
                });
            }
            return Application::ClassCreateResult::success(*classId);
        }
        catch (const std::exception&)
        {
            return Application::ClassCreateResult::failure(technicalCreateError());
        }
        catch (...)
        {
            return Application::ClassCreateResult::failure(technicalCreateError());
        }
    }

private:
    [[nodiscard]] static Domain::OperationError repositoryError(
        const RepositoryError& error
        )
    {
        const QByteArray message = toLegacyDiagnostic(error).toUtf8();
        return {
            .code = domainErrorCode(error.code),
            .message = message.toStdString(),
            .recoverable = error.code != RepositoryErrorCode::Technical
        };
    }

    [[nodiscard]] static Domain::OperationError unavailableError()
    {
        return {
            .code = Domain::ErrorCode::NotFound,
            .message = "The active setup profile is unavailable.",
            .recoverable = true
        };
    }

    [[nodiscard]] static Domain::OperationError technicalCreateError()
    {
        return {
            .code = Domain::ErrorCode::Technical,
            .message = "The class could not be created.",
            .recoverable = true
        };
    }

    [[nodiscard]] static Domain::ErrorCode domainErrorCode(
        const RepositoryErrorCode code
        ) noexcept
    {
        switch (code)
        {
        case RepositoryErrorCode::InvalidInput:
            return Domain::ErrorCode::InvalidInput;
        case RepositoryErrorCode::NotFound:
            return Domain::ErrorCode::NotFound;
        case RepositoryErrorCode::Conflict:
            return Domain::ErrorCode::Conflict;
        case RepositoryErrorCode::Validation:
            return Domain::ErrorCode::Validation;
        case RepositoryErrorCode::Technical:
            return Domain::ErrorCode::Technical;
        }
        return Domain::ErrorCode::Technical;
    }

    WorkspaceStore* m_store = nullptr;
};

class WorkspaceStoreInitialSetupClassDetailsSavePort final
    : public Application::ClassDetailsSavePort
{
public:
    explicit WorkspaceStoreInitialSetupClassDetailsSavePort(
        WorkspaceStore& store
        ) noexcept
        : m_store(&store)
    {
    }

    [[nodiscard]] Domain::Result<void> saveClassDetails(
        const Application::ClassDetailsSaveRequest& request
        ) const override
    {
        if (!m_store || !m_store->isOpen())
        {
            return failure(
                Domain::ErrorCode::NotFound,
                "The active setup profile is unavailable.",
                true
                );
        }
        const auto classId = legacyClassId(request.classId.value());
        const auto teacherId = request.teacherId
            ? Application::parseCanonicalPositiveDecimalId64(
                  request.teacherId->value()
                  )
            : std::nullopt;
        if (!classId || !teacherId)
        {
            return failure(
                Domain::ErrorCode::InvalidInput,
                "The class details request is invalid.",
                true
                );
        }

        ClassInfoRepository* const repository = m_store->classInfoRepository();
        if (!repository)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Class details repository is unavailable.",
                true
                );
        }

        const QString validationMessage = validateRequest(request);
        if (!validationMessage.isEmpty())
        {
            return failure(
                Domain::ErrorCode::Validation,
                validationMessage.toStdString(),
                true
                );
        }

        ClassInfo info;
        info.classId = *classId;
        info.teacherId = *teacherId <= std::numeric_limits<int>::max()
            ? static_cast<int>(*teacherId)
            : -1;
        info.classGrade = legacyText(request.classGrade);
        info.classLevel = legacyText(request.classLevel);
        info.readingBook = legacyText(request.readingBook);
        info.essayBook = legacyText(request.essayBook);
        info.classColor = legacyText(request.classColor);
        info.fontColor = legacyText(request.fontColor);
        info.classTimes = legacyTimes(*request.regularTimes);
        info.intensiveTimes = legacyTimes(*request.intensiveTimes);

        try
        {
            const RepositoryStatus saved = repository->saveClassDetailsDetailed(
                info,
                *teacherId
                );
            return saved
                ? Domain::Result<void>::success()
                : repositoryFailure(saved.error());
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Class details could not be saved.",
                true
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Class details could not be saved.",
                true
                );
        }
    }

private:
    [[nodiscard]] static std::optional<int> legacyClassId(const std::string& value)
    {
        int parsed = 0;
        const auto [end, error] = std::from_chars(
            value.data(), value.data() + value.size(), parsed
            );
        if (error != std::errc{} || end != value.data() + value.size() || parsed <= 0)
        {
            return std::nullopt;
        }
        return parsed;
    }

    [[nodiscard]] static QString legacyText(const std::u16string& value)
    {
        return QString::fromUtf16(value.data(), static_cast<qsizetype>(value.size()));
    }

    [[nodiscard]] static QString legacyDay(const Domain::Weekday day)
    {
        const auto index = static_cast<std::size_t>(day);
        if (index >= ::ClassMngr::Domain::ClassScheduleCatalog::weekdays.size())
        {
            return {};
        }
        const std::string_view name =
            ::ClassMngr::Domain::ClassScheduleCatalog::weekdays[index];
        return QString::fromUtf8(
            name.data(),
            static_cast<qsizetype>(name.size())
            );
    }

    [[nodiscard]] static ClassTime legacyTime(const Domain::ScheduleTime& value)
    {
        const QTime start(
            value.startMinute().rawValue() / 60,
            value.startMinute().rawValue() % 60
            );
        const QTime end(
            value.endMinute().rawValue() / 60,
            value.endMinute().rawValue() % 60
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

    [[nodiscard]] static bool validColor(const std::u16string& rawValue)
    {
        const QString value = legacyText(rawValue);
        if (value.size() != 7 || value.front() != QLatin1Char('#'))
        {
            return false;
        }
        for (qsizetype index = 1; index < value.size(); ++index)
        {
            const QChar digit = value.at(index);
            if (!((digit >= QLatin1Char('0') && digit <= QLatin1Char('9'))
                  || (digit >= QLatin1Char('A') && digit <= QLatin1Char('F'))
                  || (digit >= QLatin1Char('a') && digit <= QLatin1Char('f'))))
            {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] static bool validScheduleRows(
        const std::vector<Domain::ScheduleTime>& rows,
        const bool intensive
        )
    {
        for (std::size_t index = 0; index < rows.size(); ++index)
        {
            const Domain::ScheduleTime& row = rows[index];
            const int start = row.startMinute().rawValue();
            const int end = row.endMinute().rawValue();
            if (start < 0 || start >= 24 * 60 || end <= start
                || end > ::ClassMngr::Domain::ClassScheduleCatalog::latestEndTimeMinutes)
            {
                return false;
            }
            const int hour24 = start / 60;
            const int hour12 = hour24 % 12 == 0 ? 12 : hour24 % 12;
            const std::string hour = std::to_string(hour12);
            const bool allowedHour = intensive
                ? std::ranges::find(
                      ::ClassMngr::Domain::ClassScheduleCatalog::intensiveHours,
                      hour
                      )
                    != ::ClassMngr::Domain::ClassScheduleCatalog::intensiveHours.end()
                : std::ranges::find(
                      ::ClassMngr::Domain::ClassScheduleCatalog::regularHours,
                      hour
                      )
                    != ::ClassMngr::Domain::ClassScheduleCatalog::regularHours.end();
            if (!allowedHour)
            {
                return false;
            }
            const int minute = start % 60;
            const std::string minuteChoice = QStringLiteral(":%1")
                .arg(minute, 2, 10, QLatin1Char('0'))
                .toStdString();
            const bool allowedMinute = std::ranges::find(
                ::ClassMngr::Domain::ClassScheduleCatalog::startMinutes,
                minuteChoice
                ) != ::ClassMngr::Domain::ClassScheduleCatalog::startMinutes.end();
            const int duration = end - start;
            const bool allowedDuration = std::ranges::find(
                ::ClassMngr::Domain::ClassScheduleCatalog::endTimeDurations,
                duration
                ) != ::ClassMngr::Domain::ClassScheduleCatalog::endTimeDurations.end();
            if (!allowedMinute || !allowedDuration)
            {
                return false;
            }
            for (std::size_t prior = 0; prior < index; ++prior)
            {
                if (row.overlaps(rows[prior]))
                {
                    return false;
                }
            }
        }
        return true;
    }

    [[nodiscard]] static QString validateRequest(
        const Application::ClassDetailsSaveRequest& request
        )
    {
        const QString grade = legacyText(request.classGrade);
        const QString level = legacyText(request.classLevel);
        const QByteArray gradeBytes = grade.toUtf8();
        const QByteArray levelBytes = level.toUtf8();
        const auto grades = ::ClassMngr::Domain::ClassDetailsCatalog::grades();
        const auto levels = ::ClassMngr::Domain::ClassDetailsCatalog::levelsForGrade(
            std::string_view(gradeBytes.constData(),
                             static_cast<std::size_t>(gradeBytes.size()))
            );
        if (std::ranges::find(
                grades,
                std::string_view(gradeBytes.constData(),
                                 static_cast<std::size_t>(gradeBytes.size()))
                ) == grades.end()
            || std::ranges::find(
                   levels,
                   std::string_view(levelBytes.constData(),
                                    static_cast<std::size_t>(levelBytes.size()))
                   ) == levels.end())
        {
            return QStringLiteral("The class grade or level is invalid.");
        }

        const auto readingBooks = ::ClassMngr::Domain::ClassDetailsCatalog::readingBooks(
            std::string_view(gradeBytes.constData(),
                             static_cast<std::size_t>(gradeBytes.size())),
            std::string_view(levelBytes.constData(),
                             static_cast<std::size_t>(levelBytes.size()))
            );
        const auto essayBooks = ::ClassMngr::Domain::ClassDetailsCatalog::essayBooks(
            std::string_view(gradeBytes.constData(),
                             static_cast<std::size_t>(gradeBytes.size())),
            std::string_view(levelBytes.constData(),
                             static_cast<std::size_t>(levelBytes.size()))
            );
        const auto contains = [](const auto& choices, const std::u16string& value) {
            const QByteArray bytes = legacyText(value).toUtf8();
            const std::string_view candidate(
                bytes.constData(), static_cast<std::size_t>(bytes.size())
                );
            return std::ranges::find(choices, candidate) != choices.end();
        };
        if (!contains(readingBooks, request.readingBook)
            || !contains(essayBooks, request.essayBook))
        {
            return QStringLiteral("A selected book is invalid for this class.");
        }
        if (!validColor(request.classColor) || !validColor(request.fontColor))
        {
            return QStringLiteral("The class colors are invalid.");
        }
        if (!request.regularTimes || !request.intensiveTimes
            || !validScheduleRows(*request.regularTimes, false)
            || !validScheduleRows(*request.intensiveTimes, true))
        {
            return QStringLiteral("One or more class times are invalid.");
        }
        return {};
    }

    [[nodiscard]] static Domain::Result<void> repositoryFailure(
        const RepositoryError& error
        )
    {
        const QByteArray message = toLegacyDiagnostic(error).toUtf8();
        return failure(
            domainErrorCode(error.code),
            message.toStdString(),
            error.code != RepositoryErrorCode::Technical
            );
    }

    [[nodiscard]] static Domain::ErrorCode domainErrorCode(
        const RepositoryErrorCode code
        ) noexcept
    {
        switch (code)
        {
        case RepositoryErrorCode::InvalidInput:
            return Domain::ErrorCode::InvalidInput;
        case RepositoryErrorCode::NotFound:
            return Domain::ErrorCode::NotFound;
        case RepositoryErrorCode::Conflict:
            return Domain::ErrorCode::Conflict;
        case RepositoryErrorCode::Validation:
            return Domain::ErrorCode::Validation;
        case RepositoryErrorCode::Technical:
            return Domain::ErrorCode::Technical;
        }
        return Domain::ErrorCode::Technical;
    }

    [[nodiscard]] static Domain::Result<void> failure(
        const Domain::ErrorCode code,
        std::string message,
        const bool recoverable
        )
    {
        if (message.empty())
        {
            message = "Class details could not be saved.";
        }
        return Domain::Result<void>::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = recoverable
        });
    }

    WorkspaceStore* m_store = nullptr;
};

}  // namespace ClassMngr::Next::Platform
