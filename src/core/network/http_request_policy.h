#pragma once

#include <QVariant>

class QNetworkRequest;
class QUrl;

namespace HttpRequestPolicy
{

[[nodiscard]] bool isLocalHttpUrl(const QUrl& url);
[[nodiscard]] bool isAllowedSecureUrl(const QUrl& url);
[[nodiscard]] bool isSuccessfulStatus(int statusCode);
[[nodiscard]] bool isSuccessfulStatus(const QVariant& statusCode);
void applySafeRedirectPolicy(QNetworkRequest& request);

} // namespace HttpRequestPolicy
