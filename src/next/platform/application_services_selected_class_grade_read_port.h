#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "next/application/selected_class_grade_read_port.h"

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

class ApplicationServicesSelectedClassGradeReadPort final
    : public Application::SelectedClassGradeReadPort
{
public:
    explicit ApplicationServicesSelectedClassGradeReadPort(
        ApplicationServices& services
        ) noexcept
        : m_services(services)
    {
    }

    [[nodiscard]] Application::SelectedClassGradeReadResult
    readSelectedClassGrade(
        const Domain::ClassId& classId
        ) const override
    {
        const std::optional<int> legacyClassId = legacyId(classId.value());
        if (!legacyClassId)
        {
            return failure(
                Domain::ErrorCode::InvalidInput,
                "Selected class ID must be a canonical positive integer."
                );
        }

        DatabaseSession* const session = m_services.databaseSession();
        if (!session || !session->isOpen())
        {
            return failure(
                Domain::ErrorCode::NotFound,
                "The active database session for the selected class grade is unavailable."
                );
        }

        ClassInfoRepository* const repository =
            session->classInfoRepository();
        if (!repository)
        {
            return failure(
                Domain::ErrorCode::NotFound,
                "The selected class grade repository is unavailable."
                );
        }

        Result<SelectedClassGradeReadRecord> source = [&]()
            -> Result<SelectedClassGradeReadRecord>
        {
            try
            {
                return repository->loadSelectedClassGradeRecord(
                    *legacyClassId
                    );
            }
            catch (const std::exception&)
            {
                return std::unexpected(
                    QStringLiteral("The selected class grade could not be loaded.")
                    );
            }
            catch (...)
            {
                return std::unexpected(
                    QStringLiteral("The selected class grade could not be loaded.")
                    );
            }
        }();

        if (!source)
        {
            return failure(
                legacyErrorCode(source.error()),
                legacyErrorMessage(source.error())
                );
        }

        const std::optional<Domain::ClassId> loadedClassId =
            Domain::ClassId::fromString(std::to_string(source->classId));
        if (!loadedClassId)
        {
            return failure(
                Domain::ErrorCode::Validation,
                "The selected class grade returned an invalid class identifier."
                );
        }

        const QByteArray gradeBytes = source->classGrade.toUtf8();
        Application::SelectedClassGradeReadSnapshot snapshot{
            *loadedClassId,
            gradeBytes.toStdString()
        };
        return Application::SelectedClassGradeReadResult::success(
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

    [[nodiscard]] static Domain::ErrorCode legacyErrorCode(
        const QString& message
        )
    {
        const QString normalized = message.toLower();
        if (normalized.contains(QStringLiteral("not found"))
            || normalized.contains(QStringLiteral("no matching record"))
            || normalized.contains(QStringLiteral("unavailable")))
        {
            return Domain::ErrorCode::NotFound;
        }
        if (normalized.contains(QStringLiteral("invalid")))
        {
            return Domain::ErrorCode::InvalidInput;
        }
        if (normalized.contains(QStringLiteral("validation")))
        {
            return Domain::ErrorCode::Validation;
        }
        return Domain::ErrorCode::Technical;
    }

    [[nodiscard]] static std::string legacyErrorMessage(
        const QString& message
        )
    {
        const QByteArray bytes = message.toUtf8();
        return bytes.isEmpty()
            ? "The selected class grade could not be loaded."
            : bytes.toStdString();
    }

    [[nodiscard]] static Application::SelectedClassGradeReadResult failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        return Application::SelectedClassGradeReadResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = code == Domain::ErrorCode::NotFound
        });
    }

    ApplicationServices& m_services;
};

} // namespace ClassMngr::Next::Platform
