#pragma once

#include "features/speaking_eval/services/speaking_eval_powerpoint_automation_port.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

namespace QtSpeakingEvalPowerPoint
{
enum class Host
{
    Windows,
    MacOS,
    Unsupported
};

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

struct ProcessRequest
{
    QString executable;
    QStringList arguments;
    QString workingDirectory;
    QString cancelPath;
    QList<ReportMarker> reports;
    int timeoutPerReportMs = 0;
    std::function<bool(int, int, const QString&)> progressCallback;
};

[[nodiscard]] Host nativeHost() noexcept;
[[nodiscard]] QString executable();
[[nodiscard]] SpeakingEvalPowerPointAvailability availability();
[[nodiscard]] QString availabilityMessage();
[[nodiscard]] Status run(const ProcessRequest& request, QString* errorMessage);
}
