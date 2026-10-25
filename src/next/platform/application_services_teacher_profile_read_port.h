#pragma once

#include "core/application_services.h"
#include "data/database/workspace_store.h"
#include "data/repositories/teacher_repository.h"
#include "domain/models/teacher.h"
#include "next/application/teacher_profile_read_port.h"

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

class ApplicationServicesTeacherProfileReadPort final
    : public Application::TeacherProfileReadPort
{
public:
    explicit ApplicationServicesTeacherProfileReadPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    [[nodiscard]] Application::TeacherProfileReadResult readTeacherProfile(
        const Domain::TeacherId& id
        ) const override
    {
        const std::optional<qint64> parsedTeacherId = teacherId(id);
        if (!parsedTeacherId)
        {
            return Application::TeacherProfileReadResult::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message = "Teacher ID must be a canonical positive integer.",
                .recoverable = true
            });
        }

        WorkspaceStore* const store =
            m_services ? m_services->workspaceStore() : nullptr;
        if (!store || !store->isOpen())
        {
            return Application::TeacherProfileReadResult::failure(
                unavailableError());
        }

        TeacherRepository* const repository = store->teacherRepository();
        if (!repository)
        {
            return Application::TeacherProfileReadResult::failure(
                unavailableError());
        }

        try
        {
            const RepositoryResult<TeacherProfileReadRecord> loaded =
                repository->lookupTeacherProfileById(*parsedTeacherId);
            if (!loaded)
            {
                return Application::TeacherProfileReadResult::failure(
                    repositoryError(loaded.error()));
            }
            if (loaded->teacherId != *parsedTeacherId)
            {
                return Application::TeacherProfileReadResult::failure({
                    .code = Domain::ErrorCode::Validation,
                    .message = "Teacher repository returned a different teacher identifier.",
                    .recoverable = false
                });
            }

            return Application::TeacherProfileReadResult::success({
                .teacherId = id,
                .fields = profileFieldsFromRecord(*loaded)
            });
        }
        catch (const std::exception&)
        {
            return Application::TeacherProfileReadResult::failure(
                technicalError("Teacher profile could not be loaded."));
        }
        catch (...)
        {
            return Application::TeacherProfileReadResult::failure(
                technicalError("Teacher profile could not be loaded."));
        }
    }

private:
    [[nodiscard]] static std::optional<qint64> teacherId(
        const Domain::TeacherId& id
        )
    {
        const std::string& value = id.value();
        qint64 parsed = 0;
        const auto [end, error] = std::from_chars(
            value.data(),
            value.data() + value.size(),
            parsed
            );
        if (
            error != std::errc{}
            || end != value.data() + value.size()
            || parsed <= 0
            || std::to_string(parsed) != value
            )
        {
            return std::nullopt;
        }
        return parsed;
    }

    [[nodiscard]] static Domain::TeacherProfileFields profileFieldsFromRecord(
        const TeacherProfileReadRecord& profile
        )
    {
        return {
            .teacherKr = profile.teacherKr.toStdU16String(),
            .teacherEn = profile.teacherEn.toStdU16String(),
            .preferredRomanization =
                profile.preferredRomanization.toStdU16String(),
            .preferredName = profile.preferredName.toStdU16String(),
            .roomNumber = profile.roomNumber.toStdU16String(),
            .birthday = profile.birthday.toStdU16String(),
            .phoneNumber = profile.phoneNumber.toStdU16String(),
            .wifiName = profile.wifiName.toStdU16String(),
            .wifiPassword = profile.wifiPassword.toStdU16String(),
            .internetType = profile.internetType.toStdU16String(),
            .zoomId = profile.zoomId.toStdU16String(),
            .zoomPassword = profile.zoomPassword.toStdU16String(),
            .projectionType = profile.projectionType.toStdU16String(),
            .notes = profile.notes.toStdU16String()
        };
    }

    [[nodiscard]] static std::string utf8(const QString& value)
    {
        const QByteArray bytes = value.toUtf8();
        return {bytes.constData(), static_cast<std::size_t>(bytes.size())};
    }

    [[nodiscard]] static Domain::OperationError unavailableError()
    {
        return {
            .code = Domain::ErrorCode::NotFound,
            .message = "No active Teacher Profile database session is available.",
            .recoverable = true
        };
    }

    [[nodiscard]] static Domain::OperationError repositoryError(
        const RepositoryError& error
        )
    {
        Domain::ErrorCode code = Domain::ErrorCode::Technical;
        switch (error.code)
        {
        case RepositoryErrorCode::InvalidInput:
            code = Domain::ErrorCode::InvalidInput;
            break;
        case RepositoryErrorCode::NotFound:
            code = Domain::ErrorCode::NotFound;
            break;
        case RepositoryErrorCode::Conflict:
            code = Domain::ErrorCode::Conflict;
            break;
        case RepositoryErrorCode::Validation:
            code = Domain::ErrorCode::Validation;
            break;
        case RepositoryErrorCode::Technical:
            code = Domain::ErrorCode::Technical;
            break;
        }
        const std::string detail = utf8(toLegacyDiagnostic(error));
        return {
            .code = code,
            .message = detail.empty()
                ? "Teacher profile could not be loaded."
                : detail,
            .recoverable = code == Domain::ErrorCode::NotFound
                || code == Domain::ErrorCode::InvalidInput
        };
    }

    [[nodiscard]] static Domain::OperationError technicalError(
        std::string message
        )
    {
        return {
            .code = Domain::ErrorCode::Technical,
            .message = std::move(message),
            .recoverable = false
        };
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
