#pragma once

#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "next/application/class_notes_save_port.h"

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

// Qt and legacy service conversion stay at this boundary. ApplicationServices
// remains caller-owned, matching the rest of the application-service ports.
class ApplicationServicesClassNotesSavePort final
    : public Application::ClassNotesSavePort
{
public:
    explicit ApplicationServicesClassNotesSavePort(
        ApplicationServices& services
        ) noexcept
        : m_services(&services)
    {
    }

    explicit ApplicationServicesClassNotesSavePort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    ApplicationServicesClassNotesSavePort(
        const ApplicationServicesClassNotesSavePort&
        ) = delete;
    ApplicationServicesClassNotesSavePort& operator=(
        const ApplicationServicesClassNotesSavePort&
        ) = delete;
    ApplicationServicesClassNotesSavePort(
        ApplicationServicesClassNotesSavePort&&
        ) = delete;
    ApplicationServicesClassNotesSavePort& operator=(
        ApplicationServicesClassNotesSavePort&&
        ) = delete;

    [[nodiscard]] Application::ClassNotesSaveResult saveClassNotes(
        const Application::ClassNotesSaveRequest& request
        ) const override
    {
        const Domain::Result<void> validation = request.validate();
        if (!validation)
        {
            return Application::ClassNotesSaveResult::failure(
                validation.error()
                );
        }

        const std::optional<int> classId = legacyClassId(request.classId);
        if (!classId)
        {
            return failure(
                Domain::ErrorCode::InvalidInput,
                "Class identifier must be a positive integer."
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
            const Status saved = classService->saveClassNotes(
                *classId,
                legacyText(request.notes),
                legacyText(request.timeFillerActivities)
                );
            if (!saved)
            {
                if (!classService->isAvailable())
                {
                    return unavailableFailure();
                }

                const QByteArray errorBytes = saved.error().toUtf8();
                return failure(
                    Domain::ErrorCode::Technical,
                    errorBytes.isEmpty()
                        ? "Class notes could not be saved."
                        : errorBytes.toStdString()
                    );
            }

            return Application::ClassNotesSaveResult::success();
        }
        catch (const std::exception&)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Class notes could not be saved."
                );
        }
        catch (...)
        {
            return failure(
                Domain::ErrorCode::Technical,
                "Class notes could not be saved."
                );
        }
    }

private:
    [[nodiscard]] static QString legacyText(
        const std::u16string& value
        )
    {
        return QString::fromUtf16(
            value.data(),
            static_cast<qsizetype>(value.size())
            );
    }

    [[nodiscard]] static std::optional<int> legacyClassId(
        const Domain::ClassId& id
        )
    {
        const std::string& value = id.value();
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

    [[nodiscard]] static Application::ClassNotesSaveResult failure(
        const Domain::ErrorCode code,
        std::string message
        )
    {
        return Application::ClassNotesSaveResult::failure({
            .code = code,
            .message = std::move(message),
            .recoverable = false
        });
    }

    [[nodiscard]] static Application::ClassNotesSaveResult unavailableFailure()
    {
        return failure(
            Domain::ErrorCode::NotFound,
            "The class service is unavailable."
            );
    }

    ApplicationServices* m_services = nullptr;
};

} // namespace ClassMngr::Next::Platform
