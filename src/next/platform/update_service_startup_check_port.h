#pragma once

#include "core/updater/update_service.h"
#include "next/application/startup_update_check.h"

#include <QObject>
#include <QPointer>

#include <utility>

namespace ClassMngr::Next::Platform
{

[[nodiscard]] inline Application::StartupUpdateCheckResult toStartupUpdateCheckResult(
    const UpdateCheckResult& result
    )
{
    const auto version = [](const Version& value) -> Application::UpdateVersion
    {
        return {value.majorVersion(), value.minorVersion(), value.patchVersion()};
    };
    return {
        .currentVersion = version(result.currentVersion),
        .latestVersion = version(result.latestVersion),
        .artifact = {
            .platformKey = Application::UpdateArtifactPlatformKey(
                result.artifact.platformKey.toUtf8().toStdString()
                ),
            .url = result.artifact.url.toEncoded().toStdString(),
            .fileName = result.artifact.fileName.toUtf8().toStdString(),
            .sha256 = result.artifact.sha256.toUtf8().toStdString(),
            .sizeBytes = result.artifact.sizeBytes
        },
        .releaseDateIso = result.releaseDate.toString(Qt::ISODate).toStdString(),
        .releaseUrl = result.releaseUrl.toEncoded().toStdString(),
        .checkedAtUtcMilliseconds = result.checkedAtUtc.toMSecsSinceEpoch(),
        .updateAvailable = result.updateAvailable
    };
}

// Lives on the service's thread. Context-bound connections and QPointer stop
// delivery after adapter/service destruction. No network cancellation is
// implied. Manual UpdateDialog checks continue to use the service directly.
class UpdateServiceStartupCheckPort final
    : public QObject, public Application::StartupUpdateCheckPort
{
public:
    explicit UpdateServiceStartupCheckPort(UpdateService* service)
        : m_service(service)
    {
        if (!m_service)
        {
            return;
        }
        connect(m_service, &UpdateService::checkSucceeded, this,
            [this](const UpdateCheckResult& result)
            {
                complete(toStartupUpdateCheckResult(result));
            });
        connect(m_service, &UpdateService::checkFailedWithCategory, this,
            [this](UpdateService::FailureCategory category, const QString& message)
            {
                using Code = Application::StartupUpdateCheckErrorCode;
                Code code = Code::Configuration;
                switch (category)
                {
                case UpdateService::FailureCategory::Configuration:
                    code = Code::Configuration;
                    break;
                case UpdateService::FailureCategory::Network:
                    code = Code::Network;
                    break;
                case UpdateService::FailureCategory::InvalidResponse:
                    code = Code::InvalidResponse;
                    break;
                case UpdateService::FailureCategory::InvalidCurrentVersion:
                    code = Code::InvalidCurrentVersion;
                    break;
                }
                complete(Application::StartupUpdateCheckError{
                    .code = code,
                    .message = message.toUtf8().toStdString()
                });
            });
        connect(m_service, &QObject::destroyed, this,
            [this]() { m_pending = false; m_completion = {}; });
    }

    [[nodiscard]] Application::StartupUpdateCheckDispatch check(
        Application::StartupUpdateCheckCompletion completion
        ) override
    {
        using Dispatch = Application::StartupUpdateCheckDispatch;
        if (m_pending || (m_service && m_service->isBusy()))
        {
            return Dispatch::Busy;
        }
        m_pending = true;
        m_completion = std::move(completion);
        if (!m_service)
        {
            complete(Application::StartupUpdateCheckError{
                .code = Application::StartupUpdateCheckErrorCode::Dispatch,
                .message = "The update service is unavailable."
            });
            return Dispatch::Failed;
        }

        const QPointer<UpdateServiceStartupCheckPort> guard(this);
        if (m_service->checkForUpdates(UpdateService::CheckPolicy::Force))
        {
            return Dispatch::Started;
        }
        if (!guard)
        {
            return Dispatch::Failed;
        }
        // Configuration errors complete synchronously through the typed
        // failure signal. A future dispatch refusal must still complete once.
        if (m_pending)
        {
            complete(Application::StartupUpdateCheckError{
                .code = Application::StartupUpdateCheckErrorCode::Dispatch,
                .message = "The update check could not be dispatched."
            });
        }
        return Dispatch::Failed;
    }

private:
    void complete(Application::StartupUpdateCheckOutcome outcome)
    {
        if (!m_pending)
        {
            return;
        }
        m_pending = false;
        auto callback = std::move(m_completion);
        m_completion = {};
        if (callback)
        {
            callback(std::move(outcome));
        }
    }

    QPointer<UpdateService> m_service;
    bool m_pending = false;
    Application::StartupUpdateCheckCompletion m_completion;
};

} // namespace ClassMngr::Next::Platform
