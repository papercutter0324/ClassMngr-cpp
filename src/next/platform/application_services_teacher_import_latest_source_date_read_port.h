#pragma once

#include "core/application_services.h"
#include "data/database/workspace_store.h"
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

    [[nodiscard]] Application::TeacherImportLatestSourceDateReadPortResult
        readLatestTeacherImportSourceDate() const override
    {
        WorkspaceStore* const store =
            m_services ? m_services->workspaceStore() : nullptr;
        if (!store || !store->isOpen())
        {
            return Application::TeacherImportLatestSourceDateReadPortResult::failure(
                unavailableError());
        }

        SettingsRepository* const repository = store->settingsRepository();
        if (!repository)
        {
            return Application::TeacherImportLatestSourceDateReadPortResult::failure(
                unavailableError());
        }

        try
        {
            const RepositoryResult<QVariant> setting = repository->loadSetting(
                QString::fromLatin1(
                    TeacherImportRepository::LatestSourceDateSetting));
            if (!setting)
            {
                return Application::TeacherImportLatestSourceDateReadPortResult::failure(
                    repositoryError(toLegacyDiagnostic(setting.error())));
            }

            if (!setting->isValid() || setting->isNull())
            {
                return Application::TeacherImportLatestSourceDateReadPortResult::success(
                    std::nullopt);
            }

            const QString sourceDateText = setting->toString();
            if (sourceDateText.isEmpty())
            {
                return Application::TeacherImportLatestSourceDateReadPortResult::success(
                    std::nullopt);
            }

            const QDate sourceDate = QDate::fromString(
                sourceDateText,
                Qt::ISODate);
            if (!sourceDate.isValid()
                || sourceDate.toString(Qt::ISODate) != sourceDateText)
            {
                return Application::TeacherImportLatestSourceDateReadPortResult::success(
                    std::nullopt);
            }

            const QByteArray canonicalDate =
                sourceDate.toString(Qt::ISODate).toUtf8();
            return Application::TeacherImportLatestSourceDateReadPortResult::success(
                std::string(
                    canonicalDate.constData(),
                    static_cast<std::size_t>(canonicalDate.size())));
        }
        catch (const std::exception&)
        {
            return Application::TeacherImportLatestSourceDateReadPortResult::failure(
                technicalError(
                    "The latest teacher import source date could not be read."));
        }
        catch (...)
        {
            return Application::TeacherImportLatestSourceDateReadPortResult::failure(
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
