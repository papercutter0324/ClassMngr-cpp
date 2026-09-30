#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/class_info_repository.h"
#include "domain/validation/class_info_validator.h"
#include "next/application/class_notes_save_port.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <charconv>
#include <exception>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace ClassMngr::Next::Platform
{

// Qt conversion stays at this boundary. ApplicationServices remains
// caller-owned, matching the rest of the application-service ports.
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
        const Domain::Result<void> requestValidation = request.validate();
        if (!requestValidation)
        {
            return Application::ClassNotesSaveResult::failure(
                requestValidation.error()
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
                "Class notes repository is unavailable."
                );
        }

        try
        {
            const QString notes = legacyText(request.notes).trimmed();
            const QString activities =
                legacyText(request.timeFillerActivities).trimmed();
            const ValidationResult validation = ClassInfoValidator::validateNotes(
                *classId,
                notes,
                activities
                );
            if (validation.hasErrors())
            {
                return failure(
                    Domain::ErrorCode::Technical,
                    toStdString(validationError(
                        QStringLiteral("Class notes"),
                        validation
                        ))
                    );
            }

            const Status saved = repository->saveClassNotes(
                *classId,
                notes,
                activities
                );
            if (!saved)
            {
                if (!session->isOpen())
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
                detail.prepend(QStringLiteral("row %1, ").arg(issue.row + 1));
            }
            details.append(detail);
        }

        return QStringLiteral("%1 validation failed: %2")
            .arg(subject, details.join(QStringLiteral("; ")));
    }

    [[nodiscard]] static std::string toStdString(const QString& value)
    {
        const QByteArray bytes = value.toUtf8();
        return bytes.toStdString();
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
