#pragma once

#include "core/updater/update_configuration.h"
#include "core/updater/github_release.h"

#include <QDateTime>
#include <QNetworkAccessManager>
#include <QObject>

#include <chrono>
#include <optional>

struct UpdateCheckResult
{
    Version currentVersion;
    Version latestVersion;
    UpdateArtifact artifact;
    QDate releaseDate;
    QUrl releaseUrl;
    QDateTime checkedAtUtc;
    bool updateAvailable = false;
};

class UpdateService : public QObject
{
    Q_OBJECT

public:
    enum class CheckPolicy
    {
        IfStale,
        Force
    };

    explicit UpdateService(
        UpdateConfiguration configuration = UpdateConfiguration::fromBuild(),
        QObject* parent = nullptr
        );

    [[nodiscard]] bool isBusy() const;
    [[nodiscard]] UpdateConfiguration configuration() const;
    [[nodiscard]] bool hasResult() const;
    [[nodiscard]] std::optional<UpdateCheckResult> lastResult() const;
    [[nodiscard]] QDateTime lastSuccessfulCheckUtc() const;
    [[nodiscard]] bool isResultFresh(
        const QDateTime& nowUtc = QDateTime::currentDateTimeUtc()
        ) const;

    [[nodiscard]] static bool isTimestampFresh(
        const QDateTime& checkedAtUtc,
        const QDateTime& nowUtc
        );

public slots:
    bool checkForUpdates(
        CheckPolicy policy = CheckPolicy::Force
        );

signals:
    void checkStarted();
    void checkSucceeded(
        const UpdateCheckResult& result
        );
    void checkFailed(
        const QString& message
        );

private:
    void completeCheck(
        const QByteArray& data
        );
    void fail(
        const QString& message
        );

    [[nodiscard]] bool isAllowedApiUrl(
        const QUrl& url
        ) const;

private:
    static constexpr auto ResultFreshness =
        std::chrono::hours(6);

    UpdateConfiguration m_configuration;
    QNetworkAccessManager m_network;
    bool m_busy = false;
    std::optional<UpdateCheckResult> m_lastResult;
    QDateTime m_lastSuccessfulCheckUtc;
};
