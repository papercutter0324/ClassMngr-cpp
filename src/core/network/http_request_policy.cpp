#include "http_request_policy.h"

#include <QNetworkRequest>
#include <QUrl>

namespace HttpRequestPolicy
{

bool isLocalHttpUrl(const QUrl& url)
{
    if (url.scheme().compare(QStringLiteral("http"), Qt::CaseInsensitive) != 0)
    {
        return false;
    }

    const QString host = url.host().toLower();
    return host == QStringLiteral("localhost")
        || host == QStringLiteral("127.0.0.1")
        || host == QStringLiteral("::1");
}

bool isAllowedSecureUrl(const QUrl& url)
{
    return url.isValid()
        && !url.isRelative()
        && (url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0
            || isLocalHttpUrl(url));
}

bool isSuccessfulStatus(const QVariant& statusCode)
{
    if (!statusCode.isValid())
    {
        return true;
    }

    return isSuccessfulStatus(statusCode.toInt());
}

bool isSuccessfulStatus(int statusCode)
{
    return statusCode >= 200 && statusCode < 300;
}

void applySafeRedirectPolicy(QNetworkRequest& request)
{
    request.setAttribute(
        QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::NoLessSafeRedirectPolicy
        );
}

} // namespace HttpRequestPolicy
