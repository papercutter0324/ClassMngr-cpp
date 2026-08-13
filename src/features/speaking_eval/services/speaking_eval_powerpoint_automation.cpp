#include "speaking_eval_powerpoint_automation.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QObject>
#include <QProcess>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>

#include <algorithm>

namespace
{
bool requestCancellation(const QString& path)
{
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.commit();
}
}

namespace SpeakingEvalPowerPointAutomation
{
QString executable()
{
#ifdef Q_OS_WIN
    for (const QString& candidate : {
             QStringLiteral("powershell.exe"),
             QStringLiteral("powershell"),
             QStringLiteral("pwsh.exe"),
             QStringLiteral("pwsh")
             })
    {
        const QString path = QStandardPaths::findExecutable(candidate);
        if (!path.isEmpty())
        {
            return path;
        }
    }
    return {};
#elif defined(Q_OS_MACOS)
    return QStandardPaths::findExecutable(QStringLiteral("osascript"));
#else
    return {};
#endif
}

bool isAvailable()
{
#ifdef Q_OS_WIN
    QSettings registry(
        QStringLiteral("HKEY_CLASSES_ROOT\\PowerPoint.Application"),
        QSettings::NativeFormat
        );
    return !executable().isEmpty()
        && (!registry.allKeys().isEmpty()
            || !registry.childGroups().isEmpty());
#elif defined(Q_OS_MACOS)
    return QFileInfo::exists(
        QStringLiteral("/Applications/Microsoft PowerPoint.app")
        ) && !executable().isEmpty();
#else
    return false;
#endif
}

QString availabilityMessage()
{
#ifdef Q_OS_WIN
    return QObject::tr(
        "PowerPoint export requires the installed desktop Microsoft PowerPoint application."
        );
#elif defined(Q_OS_MACOS)
    return QObject::tr(
        "PowerPoint export requires Microsoft PowerPoint in /Applications and macOS Automation permission."
        );
#else
    return QObject::tr(
        "PowerPoint template export is available only on Windows and macOS. Use the internal report on this platform."
        );
#endif
}

Status run(
    const Request& request,
    QString* errorMessage
    )
{
    if (request.executable.isEmpty() || request.reports.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = availabilityMessage();
        }
        return Status::Failed;
    }

    const int total = request.reports.size();
    if (request.progressCallback
        && !request.progressCallback(
            0,
            total,
            request.reports.constFirst().displayName
            ))
    {
        return Status::Canceled;
    }

    QProcess process;
    process.setWorkingDirectory(request.workingDirectory);
    process.start(request.executable, request.arguments);
    if (!process.waitForStarted())
    {
        if (errorMessage)
        {
            *errorMessage = QObject::tr("PowerPoint could not be started.");
        }
        return Status::Failed;
    }

    bool cancellationRequested = false;
    int completedCount = 0;
    const auto cancel = [&]()
    {
        if (!cancellationRequested)
        {
            cancellationRequested = true;
            requestCancellation(request.cancelPath);
        }
    };
    const auto updateProgress = [&]()
    {
        while (completedCount < total
               && QFileInfo::exists(
                   request.reports.at(completedCount).completionPath
                   ))
        {
            ++completedCount;
            const QString nextStudent = completedCount < total
                ? request.reports.at(completedCount).displayName
                : QString();
            if (request.progressCallback
                && !request.progressCallback(
                    completedCount,
                    total,
                    nextStudent
                    ))
            {
                cancel();
            }
        }
    };

    QElapsedTimer timer;
    timer.start();
    const qint64 timeout = static_cast<qint64>(request.timeoutPerReportMs)
        * std::max(1, total);
    while (process.state() != QProcess::NotRunning)
    {
        process.waitForFinished(100);
        updateProgress();
        if (timer.elapsed() <= timeout)
        {
            continue;
        }

        requestCancellation(request.cancelPath);
        process.terminate();
        if (!process.waitForFinished(5000))
        {
            process.kill();
            process.waitForFinished();
        }
        if (errorMessage)
        {
            *errorMessage = QObject::tr(
                "PowerPoint did not finish exporting the reports."
                );
        }
        return Status::Failed;
    }
    updateProgress();

    if (cancellationRequested)
    {
        return Status::Canceled;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
    {
        if (errorMessage)
        {
            const QString details =
                (QString::fromUtf8(process.readAllStandardError())
                 + QLatin1Char('\n')
                 + QString::fromUtf8(process.readAllStandardOutput()))
                    .trimmed();
            *errorMessage = details.isEmpty()
                ? QObject::tr("PowerPoint could not export the reports.")
                : details;
        }
        return Status::Failed;
    }

    for (const ReportMarker& report : request.reports)
    {
        if (!QFileInfo::exists(report.pdfPath)
            || QFileInfo(report.pdfPath).size() <= 0)
        {
            if (errorMessage)
            {
                *errorMessage = QObject::tr(
                    "PowerPoint did not create a PDF report for %1."
                    ).arg(report.displayName);
            }
            return Status::Failed;
        }
    }

    return Status::Completed;
}
}
