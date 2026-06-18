#include "update_service.h"

#include "core/updater/update_signature_verifier.h"

#include <QCoreApplication>
#include <QNetworkReply>
#include <QNetworkRequest>

#include <utility>

namespace
{
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

QString fetchKindName(
    UpdateService::FetchKind kind
    )
{
    switch (kind)
    {
    case UpdateService::FetchKind::Manifest:
        return QObject::tr("update manifest");

    case UpdateService::FetchKind::Signature:
        return QObject::tr("update manifest signature");
    }

    return QObject::tr("update data");
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

void UpdateService::checkForUpdates()
{
    if (m_busy)
    {
        return;
    }

    if (!m_configuration.hasManifestUrl())
    {
        fail(
            tr("Update manifest URL is not configured.")
            );
        return;
    }

    if (!isAllowedManifestUrl(m_configuration.manifestUrl))
    {
        fail(
            tr("Update manifest URL must use HTTPS.")
            );
        return;
    }

    if (
        m_configuration.requireSignature
        && m_configuration.publicKeyPem.trimmed().isEmpty()
        )
    {
        fail(
            tr("Update public key is not configured.")
            );
        return;
    }

    m_busy = true;
    m_manifestData.clear();
    m_signatureData.clear();

    emit checkStarted();

    fetch(
        m_configuration.manifestUrl,
        FetchKind::Manifest
        );
}

void UpdateService::fetch(
    const QUrl& url,
    FetchKind kind
    )
{
    QNetworkRequest request(url);
    request.setAttribute(
        QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::NoLessSafeRedirectPolicy
        );

    auto* reply =
        m_network.get(
            request
            );

    connect(
        reply,
        &QNetworkReply::finished,
        this,
        [this, reply, kind]()
        {
            reply->deleteLater();

            if (reply->error() != QNetworkReply::NoError)
            {
                fail(
                    tr("Unable to download %1: %2")
                        .arg(
                            fetchKindName(kind),
                            reply->errorString()
                            )
                    );
                return;
            }

            if (
                !isSuccessfulHttpStatus(
                    reply->attribute(
                        QNetworkRequest::HttpStatusCodeAttribute
                        )
                    )
                )
            {
                fail(
                    tr("Unable to download %1: HTTP %2")
                        .arg(
                            fetchKindName(kind)
                            )
                        .arg(
                            reply->attribute(
                                QNetworkRequest::HttpStatusCodeAttribute
                                ).toInt()
                            )
                    );
                return;
            }

            handleFetched(
                kind,
                reply->readAll()
                );
        }
        );
}

void UpdateService::handleFetched(
    FetchKind kind,
    const QByteArray& data
    )
{
    switch (kind)
    {
    case FetchKind::Manifest:
        m_manifestData =
            data;

        if (m_configuration.requireSignature)
        {
            const QUrl signatureUrl =
                resolvedSignatureUrl();

            if (!isAllowedManifestUrl(signatureUrl))
            {
                fail(
                    tr("Update signature URL is not configured or is not HTTPS.")
                    );
                return;
            }

            fetch(
                signatureUrl,
                FetchKind::Signature
                );
            return;
        }

        completeCheck();
        return;

    case FetchKind::Signature:
        m_signatureData =
            data;
        completeCheck();
        return;
    }
}

void UpdateService::completeCheck()
{
    if (m_configuration.requireSignature)
    {
        if (
            auto status =
                UpdateSignatureVerifier::verifyDetachedSignature(
                    m_manifestData,
                    m_signatureData,
                    m_configuration.publicKeyPem
                    );
            !status
            )
        {
            fail(status.error());
            return;
        }
    }

    const auto manifest =
        UpdateManifest::fromJson(
            m_manifestData
            );

    if (!manifest)
    {
        fail(
            manifest.error()
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

    const auto artifact =
        manifest->artifactForAny(
            UpdateManifest::currentPlatformKeys()
            );

    if (!artifact)
    {
        fail(
            artifact.error()
            );
        return;
    }

    UpdateCheckResult result;
    result.currentVersion =
        *currentVersion;
    result.latestVersion =
        manifest->latestVersion();
    result.minimumSupportedVersion =
        manifest->minimumSupportedVersion();
    result.artifact =
        *artifact;
    result.releaseDate =
        manifest->releaseDate();
    result.notesUrl =
        manifest->notesUrl();
    result.updateAvailable =
        manifest->latestVersion() > *currentVersion;
    result.currentVersionSupported =
        !manifest->minimumSupportedVersion().isValid()
        || *currentVersion >= manifest->minimumSupportedVersion();

    m_busy = false;

    emit checkSucceeded(result);
}

void UpdateService::fail(
    const QString& message
    )
{
    m_busy = false;

    emit checkFailed(message);
}

QUrl UpdateService::resolvedSignatureUrl() const
{
    if (
        m_configuration.signatureUrl.isValid()
        && !m_configuration.signatureUrl.isRelative()
        && !m_configuration.signatureUrl.isEmpty()
        )
    {
        return m_configuration.signatureUrl;
    }

    QUrl derived =
        m_configuration.manifestUrl;

    const QString path =
        derived.path();

    if (path.endsWith(QStringLiteral(".json")))
    {
        derived.setPath(
            path.left(path.size() - 5)
            + QStringLiteral(".sig")
            );

        return derived;
    }

    return QUrl();
}

bool UpdateService::isAllowedManifestUrl(
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
