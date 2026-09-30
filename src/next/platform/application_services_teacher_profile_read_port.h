#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
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
        const std::optional<int> legacyId = legacyTeacherId(id);
        if (!legacyId)
        {
            return Application::TeacherProfileReadResult::failure({
                .code = Domain::ErrorCode::InvalidInput,
                .message = "Teacher ID must be a canonical positive integer.",
                .recoverable = true
            });
        }

        DatabaseSession* const session =
            m_services ? m_services->databaseSession() : nullptr;
        if (!session || !session->isOpen())
        {
            return Application::TeacherProfileReadResult::failure(
                unavailableError());
        }

        TeacherRepository* const repository = session->teacherRepository();
        if (!repository)
        {
            return Application::TeacherProfileReadResult::failure(
                unavailableError());
        }

        try
        {
            const Result<Teacher> loaded = repository->getTeacher(*legacyId);
            if (!loaded)
            {
                return Application::TeacherProfileReadResult::failure(
                    repositoryError(loaded.error()));
            }
            if (loaded->id != *legacyId)
            {
                return Application::TeacherProfileReadResult::failure({
                    .code = Domain::ErrorCode::Validation,
                    .message = "Teacher repository returned a different teacher identifier.",
                    .recoverable = false
                });
            }

            return Application::TeacherProfileReadResult::success({
                .teacherId = id,
                .fields = profileFieldsFromTeacher(*loaded)
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
    [[nodiscard]] static std::optional<int> legacyTeacherId(
        const Domain::TeacherId& id
        )
    {
        const std::string& value = id.value();
        int parsed = 0;
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

    [[nodiscard]] static Domain::TeacherProfileFields profileFieldsFromTeacher(
        const Teacher& teacher
        )
    {
        return {
            .teacherKr = teacher.teacherKr.toStdU16String(),
            .teacherEn = teacher.teacherEn.toStdU16String(),
            .preferredRomanization =
                teacher.preferredRomanization.toStdU16String(),
            .preferredName = teacher.preferredName.toStdU16String(),
            .roomNumber = teacher.roomNumber.toStdU16String(),
            .birthday = teacher.birthday.toStdU16String(),
            .phoneNumber = teacher.phoneNumber.toStdU16String(),
            .wifiName = teacher.wifiName.toStdU16String(),
            .wifiPassword = teacher.wifiPassword.toStdU16String(),
            .internetType = teacher.internetType.toStdU16String(),
            .zoomId = teacher.zoomId.toStdU16String(),
            .zoomPassword = teacher.zoomPassword.toStdU16String(),
            .projectionType = teacher.projectionType.toStdU16String(),
            .notes = teacher.notes.toStdU16String()
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
        const QString& message
        )
    {
        const std::string detail = utf8(message);
        return {
            .code = Domain::ErrorCode::Technical,
            .message = detail.empty()
                ? "Teacher profile could not be loaded."
                : detail,
            .recoverable = false
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
