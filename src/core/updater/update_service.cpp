#include "update_service.h"

#include <QCoreApplication>
#include <QNetworkReply>
#include <QNetworkRequest>

#include <utility>

namespace
{
constexpr qint64 MaximumResponseBytes = 4 * 1024 * 1024;
constexpr int CheckTransferTimeoutMs = 15000;

bool isLocalHttpUrl(
    const QUrl& url
    )
{
    if (url.scheme() != QStringLiteral("http"))
    {
        return false;
    }

    const QString host =
        url.host().toLower();

    return host == QStringLiteral("localhost")
        || host == QStringLiteral("127.0.0.1")
        || host == QStringLiteral("::1");
}

bool isSuccessfulHttpStatus(
    const QVariant& statusCode
    )
{
    if (!statusCode.isValid())
    {
        return true;
    }

    const int status =
        statusCode.toInt();

    return status >= 200 && status < 300;
}
}

UpdateService::UpdateService(
    UpdateConfiguration configuration,
    QObject* parent
    )
    : QObject(parent)
    , m_configuration(std::move(configuration))
{
}

bool UpdateService::isBusy() const
{
    return m_busy;
}

UpdateConfiguration UpdateService::configuration() const
{
    return m_configuration;
}

bool UpdateService::hasResult() const
{
    return m_lastResult.has_value();
}

std::optional<UpdateCheckResult> UpdateService::lastResult() const
{
    return m_lastResult;
}

QDateTime UpdateService::lastSuccessfulCheckUtc() const
{
    return m_lastSuccessfulCheckUtc;
}

bool UpdateService::isResultFresh(
    const QDateTime& nowUtc
    ) const
{
    return m_lastResult.has_value()
        && isTimestampFresh(
            m_lastSuccessfulCheckUtc,
            nowUtc
            );
}

bool UpdateService::isTimestampFresh(
    const QDateTime& checkedAtUtc,
    const QDateTime& nowUtc
    )
{
    if (!checkedAtUtc.isValid() || !nowUtc.isValid())
    {
        return false;
    }

    const qint64 ageMilliseconds =
        checkedAtUtc.msecsTo(nowUtc);

    if (ageMilliseconds < 0)
    {
        return true;
    }

    return ageMilliseconds
        <= std::chrono::duration_cast<std::chrono::milliseconds>(
            ResultFreshness
            ).count();
}

bool UpdateService::checkForUpdates(
    CheckPolicy policy
    )
{
    if (m_busy)
    {
        return false;
    }

    if (
        policy == CheckPolicy::IfStale
        && isResultFresh()
        )
    {
        return false;
    }

    if (!m_configuration.hasReleasesApiUrl())
    {
        fail(
            tr("GitHub releases API URL is not configured.")
            );
        return false;
    }

    if (!isAllowedApiUrl(m_configuration.releasesApiUrl))
    {
        fail(
            tr("GitHub releases API URL must use HTTPS.")
            );
        return false;
    }

    m_busy =
        true;

    emit checkStarted();

    QNetworkRequest request(
        m_configuration.releasesApiUrl
        );
    request.setAttribute(
        QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::NoLessSafeRedirectPolicy
        );
    request.setTransferTimeout(
        CheckTransferTimeoutMs
        );
    request.setRawHeader(
        QByteArrayLiteral("Accept"),
        QByteArrayLiteral("application/vnd.github+json")
        );
    request.setRawHeader(
        QByteArrayLiteral("X-GitHub-Api-Version"),
        QByteArrayLiteral("2022-11-28")
        );
    request.setRawHeader(
        QByteArrayLiteral("User-Agent"),
        QByteArrayLiteral("ClassMngr/")
            + QCoreApplication::applicationVersion().toUtf8()
        );

    QNetworkReply* reply =
        m_network.get(
            request
            );

    connect(
        reply,
        &QNetworkReply::finished,
        this,
        [this, reply]()
        {
            reply->deleteLater();

            if (reply->error() != QNetworkReply::NoError)
            {
                fail(
                    tr("Unable to check GitHub releases: %1")
                        .arg(reply->errorString())
                    );
                return;
            }

            const QVariant statusCode =
                reply->attribute(
                    QNetworkRequest::HttpStatusCodeAttribute
                    );

            if (!isSuccessfulHttpStatus(statusCode))
            {
                fail(
                    tr("Unable to check GitHub releases: HTTP %1")
                        .arg(statusCode.toInt())
                    );
                return;
            }

            const QByteArray data =
                reply->readAll();

            if (data.size() > MaximumResponseBytes)
            {
                fail(
                    tr("GitHub releases response is too large.")
                    );
                return;
            }

            completeCheck(data);
        }
        );

    return true;
}

void UpdateService::completeCheck(
    const QByteArray& data
    )
{
    const auto release =
        GitHubRelease::latestCompatibleFromJson(
            data
            );

    if (!release)
    {
        fail(
            release.error()
            );
        return;
    }

    const auto currentVersion =
        Version::parse(
            QCoreApplication::applicationVersion()
            );

    if (!currentVersion)
    {
        fail(
            tr("Current application version is invalid: %1")
                .arg(currentVersion.error())
            );
        return;
    }

    const QDateTime checkedAtUtc =
        QDateTime::currentDateTimeUtc();

    UpdateCheckResult result;
    result.currentVersion =
        *currentVersion;
    result.latestVersion =
        release->version();
    result.artifact =
        release->artifact();
    result.releaseDate =
        release->releaseDate();
    result.releaseUrl =
        release->releaseUrl();
    result.checkedAtUtc =
        checkedAtUtc;
    result.updateAvailable =
        release->version() > *currentVersion;

    m_busy =
        false;
    m_lastResult =
        result;
    m_lastSuccessfulCheckUtc =
        checkedAtUtc;

    emit checkSucceeded(result);
}

void UpdateService::fail(
    const QString& message
    )
{
    m_busy =
        false;

    emit checkFailed(message);
}

bool UpdateService::isAllowedApiUrl(
    const QUrl& url
    ) const
{
    return url.isValid()
        && !url.isRelative()
        && (
            url.scheme() == QStringLiteral("https")
            || isLocalHttpUrl(url)
            );
}
