#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

namespace SpeakingEvalPowerPointAutomation
{
enum class Status
{
    Completed,
    Canceled,
    Failed
};

struct ReportMarker
{
    QString displayName;
    QString pdfPath;
    QString completionPath;
};

struct Request
{
    QString executable;
    QStringList arguments;
    QString workingDirectory;
    QString cancelPath;
    QList<ReportMarker> reports;
    int timeoutPerReportMs = 0;
    std::function<bool(int, int, const QString&)> progressCallback;
};

[[nodiscard]] QString executable();
[[nodiscard]] bool isAvailable();
[[nodiscard]] QString availabilityMessage();
[[nodiscard]] Status run(
    const Request& request,
    QString* errorMessage
    );
}
