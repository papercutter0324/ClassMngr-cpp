#pragma once

#include "core/updater/update_service.h"
#include "next/application/manual_update_check.h"

#include <QObject>
#include <QPointer>

#include <memory>
#include <utility>

namespace ClassMngr::Next::Platform
{

[[nodiscard]] inline Application::ManualUpdateCheckResult toManualUpdateCheckResult(
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
            .sha256 = Application::UpdateArtifactSha256(
                result.artifact.sha256.toUtf8().toStdString()
                ),
            .sizeBytes = Application::UpdateArtifactSizeBytes(
                result.artifact.sizeBytes
                )
        },
        .releaseDateIso = result.releaseDate.toString(Qt::ISODate).toStdString(),
        .releaseUrl = result.releaseUrl.toEncoded().toStdString(),
        .checkedAtUtcMilliseconds = Application::UpdateCheckUtcMilliseconds(
            result.checkedAtUtc.toMSecsSinceEpoch()
            ),
        .updateAvailable = result.updateAvailable
    };
}

// Adapts the shared UpdateService to a repeatable manual-check port. A request
// accepted here is completed once; service results from other flows are
// published to the observer without being adopted or canceled.
class UpdateServiceManualCheckPort final
    : public QObject, public Application::ManualUpdateCheckPort
{
public:
    explicit UpdateServiceManualCheckPort(
        UpdateService* service,
        QObject* parent = nullptr
        )
        : QObject(parent)
        , m_service(service)
    {
        if (!m_service)
        {
            return;
        }

        connect(m_service, &UpdateService::checkStarted, this,
            [this]()
            {
                if (m_activeRequest)
                {
                    return;
                }
                m_externalRequestActive = true;
                if (m_observer)
                {
                    publish(Application::ManualUpdateCheckStarted{});
                }
            });
        connect(m_service, &UpdateService::checkSucceeded, this,
            [this](const UpdateCheckResult& result)
            {
                if (m_service && m_service->isBusy())
                {
                    return;
                }
                const Application::ManualUpdateCheckResult mapped =
                    toManualUpdateCheckResult(result);
                if (m_activeRequest)
                {
                    complete(m_activeRequest, mapped);
                }
                else if (m_externalRequestActive)
                {
                    m_externalRequestActive = false;
                    publish(mapped);
                }
            });
        connect(m_service, &UpdateService::checkFailedWithCategory, this,
            [this](UpdateService::FailureCategory category, const QString& message)
            {
                if (m_service && m_service->isBusy())
                {
                    return;
                }
                const Application::ManualUpdateCheckError mapped{
                    .code = toErrorCode(category),
                    .message = message.toUtf8().toStdString()
                };
                if (m_activeRequest)
                {
                    complete(m_activeRequest, mapped);
                }
                else if (m_externalRequestActive)
                {
                    m_externalRequestActive = false;
                    publish(mapped);
                }
            });
        connect(m_service, &QObject::destroyed, this,
            [this]()
            {
                if (m_activeRequest)
                {
                    m_activeRequest->completed = true;
                    m_activeRequest->completion = {};
                    m_activeRequest.reset();
                }
                m_observer = {};
                m_externalRequestActive = false;
            });
    }

    [[nodiscard]] Application::ManualUpdateCheckSnapshot snapshot() const override
    {
        if (!m_service)
        {
            return {};
        }

        Application::ManualUpdateCheckSnapshot result;
        if (const auto cached = m_service->lastResult())
        {
            result.cachedResult = toManualUpdateCheckResult(*cached);
        }
        result.resultFresh = m_service->isResultFresh();
        result.requestInProgress = m_service->isBusy();
        return result;
    }

    [[nodiscard]] Application::ManualUpdateCheckDispatch check(
        Application::ManualUpdateCheckPolicy policy,
        Application::ManualUpdateCheckCompletion completion
        ) override
    {
        using Dispatch = Application::ManualUpdateCheckDispatch;
        if (!m_service)
        {
            if (completion)
            {
                completion(Application::ManualUpdateCheckError{
                    .code = Application::ManualUpdateCheckErrorCode::Dispatch,
                    .message = "The update service is unavailable."
                });
            }
            return Dispatch::Failed;
        }

        if (m_activeRequest || m_service->isBusy())
        {
            return Dispatch::Busy;
        }

        if (policy == Application::ManualUpdateCheckPolicy::IfStale
            && m_service->isResultFresh())
        {
            return Dispatch::FreshCache;
        }

        auto request = std::make_shared<Request>();
        request->completion = std::move(completion);
        m_activeRequest = request;

        const QPointer<UpdateServiceManualCheckPort> guard(this);
        const auto servicePolicy = policy == Application::ManualUpdateCheckPolicy::Force
            ? UpdateService::CheckPolicy::Force
            : UpdateService::CheckPolicy::IfStale;
        const bool started = m_service->checkForUpdates(servicePolicy);
        if (!guard)
        {
            return Dispatch::Failed;
        }

        if (request->completed)
        {
            return Dispatch::Failed;
        }

        if (started)
        {
            return Dispatch::Started;
        }

        if (m_service->isBusy())
        {
            clear(request);
            return Dispatch::Busy;
        }

        if (policy == Application::ManualUpdateCheckPolicy::IfStale
            && m_service->isResultFresh())
        {
            clear(request);
            return Dispatch::FreshCache;
        }

        complete(request, Application::ManualUpdateCheckError{
            .code = Application::ManualUpdateCheckErrorCode::Dispatch,
            .message = "The update check could not be dispatched."
        });
        return Dispatch::Failed;
    }

    void setObserver(Application::ManualUpdateCheckObserver observer) override
    {
        m_observer = std::move(observer);
        if (m_observer && !m_activeRequest && m_service && m_service->isBusy())
        {
            // The observer may attach after a startup request has already
            // emitted checkStarted; retain its terminal result without owning it.
            m_externalRequestActive = true;
        }
    }

private:
    struct Request final
    {
        Application::ManualUpdateCheckCompletion completion;
        bool completed = false;
    };

    [[nodiscard]] static Application::ManualUpdateCheckErrorCode toErrorCode(
        UpdateService::FailureCategory category
        )
    {
        using Code = Application::ManualUpdateCheckErrorCode;
        switch (category)
        {
        case UpdateService::FailureCategory::Configuration:
            return Code::Configuration;
        case UpdateService::FailureCategory::Network:
            return Code::Network;
        case UpdateService::FailureCategory::InvalidResponse:
            return Code::InvalidResponse;
        case UpdateService::FailureCategory::InvalidCurrentVersion:
            return Code::InvalidCurrentVersion;
        }
        return Code::Dispatch;
    }

    void complete(
        std::shared_ptr<Request> request,
        Application::ManualUpdateCheckOutcome outcome
        )
    {
        if (!request || request->completed)
        {
            return;
        }

        request->completed = true;
        if (m_activeRequest == request)
        {
            m_activeRequest.reset();
        }
        auto callback = std::move(request->completion);
        if (callback)
        {
            callback(std::move(outcome));
        }
    }

    void clear(const std::shared_ptr<Request>& request)
    {
        if (m_activeRequest == request)
        {
            m_activeRequest.reset();
        }
        request->completion = {};
    }

    void publish(Application::ManualUpdateCheckEvent event)
    {
        auto observer = m_observer;
        if (observer)
        {
            observer(std::move(event));
        }
    }

    QPointer<UpdateService> m_service;
    std::shared_ptr<Request> m_activeRequest;
    Application::ManualUpdateCheckObserver m_observer;
    bool m_externalRequestActive = false;
};

} // namespace ClassMngr::Next::Platform
