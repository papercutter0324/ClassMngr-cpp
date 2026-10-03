#pragma once

#include "core/application_services.h"
#include "data/database/database_session.h"
#include "data/repositories/settings_repository.h"
#include "data/repositories/teacher_import_repository.h"
#include "next/application/teacher_import_latest_source_date_read_port.h"

#include <QByteArray>
#include <QDate>
#include <QString>
#include <QVariant>

#include <exception>
#include <optional>
#include <string>
#include <utility>

namespace ClassMngr::Next::Platform
{

class ApplicationServicesTeacherImportLatestSourceDateReadPort final
    : public Application::TeacherImportLatestSourceDateReadPort
{
public:
    explicit ApplicationServicesTeacherImportLatestSourceDateReadPort(
        ApplicationServices* services
        ) noexcept
        : m_services(services)
    {
    }

    [[nodiscard]] Application::TeacherImportLatestSourceDateReadResult
        readLatestTeacherImportSourceDate() const override
    {
        DatabaseSession* const session =
            m_services ? m_services->databaseSession() : nullptr;
        if (!session || !session->isOpen())
        {
            return Application::TeacherImportLatestSourceDateReadResult::failure(
                unavailableError());
        }

        SettingsRepository* const repository = session->settingsRepository();
        if (!repository)
        {
            return Application::TeacherImportLatestSourceDateReadResult::failure(
                unavailableError());
        }

        try
        {
            const Result<QVariant> setting = repository->loadSetting(
                QString::fromLatin1(
                    TeacherImportRepository::LatestSourceDateSetting));
            if (!setting)
            {
                return Application::TeacherImportLatestSourceDateReadResult::failure(
                    repositoryError(setting.error()));
            }

            if (!setting->isValid() || setting->isNull())
            {
                return Application::TeacherImportLatestSourceDateReadResult::success(
                    std::nullopt);
            }

            const QString sourceDateText = setting->toString();
            if (sourceDateText.isEmpty())
            {
                return Application::TeacherImportLatestSourceDateReadResult::success(
                    std::nullopt);
            }

            const QDate sourceDate = QDate::fromString(
                sourceDateText,
                Qt::ISODate);
            if (!sourceDate.isValid()
                || sourceDate.toString(Qt::ISODate) != sourceDateText)
            {
                return Application::TeacherImportLatestSourceDateReadResult::success(
                    std::nullopt);
            }

            const QByteArray canonicalDate =
                sourceDate.toString(Qt::ISODate).toUtf8();
            return Application::TeacherImportLatestSourceDateReadResult::success(
                std::string(
                    canonicalDate.constData(),
                    static_cast<std::size_t>(canonicalDate.size())));
        }
        catch (const std::exception&)
        {
            return Application::TeacherImportLatestSourceDateReadResult::failure(
                technicalError(
                    "The latest teacher import source date could not be read."));
        }
        catch (...)
        {
            return Application::TeacherImportLatestSourceDateReadResult::failure(
                technicalError(
                    "The latest teacher import source date could not be read."));
        }
    }

private:
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
        const QByteArray detail = message.toUtf8();
        return {
            .code = Domain::ErrorCode::Technical,
            .message = detail.isEmpty()
                ? "The latest teacher import source date could not be read."
                : std::string(
                      detail.constData(),
                      static_cast<std::size_t>(detail.size())),
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
