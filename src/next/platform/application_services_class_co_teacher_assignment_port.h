#pragma once

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "next/application/class_co_teacher_assignment_use_case.h"

#include <QByteArray>

#include <charconv>
#include <exception>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesClassCoTeacherAssignmentPort final
    : public Application::ClassCoTeacherAssignmentPort
{
public:
    explicit ApplicationServicesClassCoTeacherAssignmentPort(
        ApplicationServices& services
        ) noexcept
        : m_services(&services)
    {
    }

    explicit ApplicationServicesClassCoTeacherAssignmentPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesClassCoTeacherAssignmentPort(
        const ApplicationServicesClassCoTeacherAssignmentPort&
        ) = delete;
    ApplicationServicesClassCoTeacherAssignmentPort& operator=(
        const ApplicationServicesClassCoTeacherAssignmentPort&
        ) = delete;
    ApplicationServicesClassCoTeacherAssignmentPort(
        ApplicationServicesClassCoTeacherAssignmentPort&&
        ) = delete;
    ApplicationServicesClassCoTeacherAssignmentPort& operator=(
        ApplicationServicesClassCoTeacherAssignmentPort&&
        ) = delete;

    [[nodiscard]] Domain::Result<void> assignClassCoTeacher(
        const Application::ClassCoTeacherAssignmentRequest& request
        ) const override
    {
        const std::optional<int> classId = legacyId(request.classId.value());
        const std::optional<int> teacherId = request.teacherId
            ? legacyId(request.teacherId->value())
            : std::optional<int>{-1};
        if (!classId || !teacherId)
        {
            return failure(
                Domain::ErrorCode::InvalidInput,
                "Class and teacher IDs must be positive integers."
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
            info.teacherId = *teacherId;

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
                "Co-teacher assignment could not be saved."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Co-teacher assignment could not be saved."
                );
        }
    }

private:
    [[nodiscard]] static std::optional<int> legacyId(
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
            message = "Co-teacher assignment could not be saved.";
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
