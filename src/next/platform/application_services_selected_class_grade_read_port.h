#pragma once

#include "core/application_services.h"
#include "data/database/workspace_store.h"
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

        WorkspaceStore* const store = m_services.workspaceStore();
        if (!store || !store->isOpen())
        {
            return failure(
                Domain::ErrorCode::NotFound,
                "The active database session for the selected class grade is unavailable."
                );
        }

        ClassInfoRepository* const repository =
            store->classInfoRepository();
        if (!repository)
        {
            return failure(
                Domain::ErrorCode::NotFound,
                "The selected class grade repository is unavailable."
                );
        }

        RepositoryResult<SelectedClassGradeReadOutcome> source = [&]()
            -> RepositoryResult<SelectedClassGradeReadOutcome>
        {
            try
            {
                return repository->loadSelectedClassGradeRecordDetailed(
                    *legacyClassId
                    );
            }
            catch (const std::exception&)
            {
                return std::unexpected(RepositoryError(
                    RepositoryErrorCode::Technical,
                    QStringLiteral("The selected class grade could not be loaded.")
                    ));
            }
            catch (...)
            {
                return std::unexpected(RepositoryError(
                    RepositoryErrorCode::Technical,
                    QStringLiteral("The selected class grade could not be loaded.")
                    ));
            }
        }();

        if (!source)
        {
            return failure(
                legacyErrorCode(source.error().code),
                legacyErrorMessage(source.error())
                );
        }

        if (source->parentMissing)
        {
            return failure(
                Domain::ErrorCode::NotFound,
                "The selected class does not exist."
                );
        }

        const std::optional<Domain::ClassId> loadedClassId =
            Domain::ClassId::fromString(
                std::to_string(source->record.classId)
                );
        if (!loadedClassId)
        {
            return failure(
                Domain::ErrorCode::Validation,
                "The selected class grade returned an invalid class identifier."
                );
        }

        const QByteArray gradeBytes = source->record.classGrade.toUtf8();
        Application::SelectedClassGradeReadSnapshot snapshot{
            *loadedClassId,
            Application::SelectedClassGradeValue(std::string(
                gradeBytes.constData(),
                static_cast<std::size_t>(gradeBytes.size())
                ))
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
        const RepositoryErrorCode code
        )
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

    [[nodiscard]] static std::string legacyErrorMessage(
        const RepositoryError& error
        )
    {
        const QByteArray bytes = toLegacyDiagnostic(error).toUtf8();
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
