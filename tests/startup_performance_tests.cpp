#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSet>
#include <QTemporaryDir>
#include <QtTest>

#include <cstdio>

namespace
{
constexpr int StartupTimeoutMs = 60000;

QString processOutput(
    QProcess& process
    )
{
    return QStringLiteral("stdout:\n%1\nstderr:\n%2")
        .arg(
            QString::fromLocal8Bit(process.readAllStandardOutput()),
            QString::fromLocal8Bit(process.readAllStandardError())
            );
}

bool thresholdExceeded(
    const char* environmentVariable,
    double value,
    QString* message
    )
{
    bool ok = false;

    const int threshold =
        qEnvironmentVariableIntValue(
            environmentVariable,
            &ok
            );

    if (!ok || threshold <= 0 || value <= threshold)
    {
        return false;
    }

    *message =
        QStringLiteral("%1 was %2 ms, exceeding %3 ms")
            .arg(
                QString::fromLatin1(environmentVariable),
                QString::number(value, 'f', 0),
                QString::number(threshold)
                );

    return true;
}
}

class StartupPerformanceTests : public QObject
{
    Q_OBJECT

private slots:
    void reportsStartupMetricsAndHonorsThresholds();
};

void StartupPerformanceTests::reportsStartupMetricsAndHonorsThresholds()
{
    const QString appPath =
        qEnvironmentVariable("CLASSMNGR_TEST_APP_PATH");

    QVERIFY2(
        !appPath.trimmed().isEmpty(),
        "CLASSMNGR_TEST_APP_PATH was not provided."
        );
    QVERIFY2(
        QFile::exists(appPath),
        qPrintable(
            QStringLiteral("ClassMngr executable does not exist: %1")
                .arg(appPath)
            )
        );

    QTemporaryDir directory;

    QVERIFY(directory.isValid());

    const QString metricsPath =
        directory.filePath(
            QStringLiteral("startup-metrics.json")
            );

    QProcess process;
    QProcessEnvironment environment =
        QProcessEnvironment::systemEnvironment();

    environment.insert(
        QStringLiteral("CLASSMNGR_SETTINGS_ROOT"),
        directory.filePath(QStringLiteral("settings"))
        );

    if (!environment.contains(QStringLiteral("QT_QPA_PLATFORM")))
    {
        environment.insert(
            QStringLiteral("QT_QPA_PLATFORM"),
            QStringLiteral("offscreen")
            );
    }

    process.setProcessEnvironment(environment);

    QElapsedTimer processTimer;

    processTimer.start();

    process.start(
        appPath,
        {
            QStringLiteral("--startup-performance-test"),
            QStringLiteral("--startup-performance-output"),
            metricsPath
        }
        );

    QVERIFY2(
        process.waitForStarted(StartupTimeoutMs),
        qPrintable(process.errorString())
        );

    if (!process.waitForFinished(StartupTimeoutMs))
    {
        process.kill();
        process.waitForFinished();

        QFAIL(
            qPrintable(
                QStringLiteral("ClassMngr startup performance run timed out.\n%1")
                    .arg(processOutput(process))
                )
            );
    }

    const qint64 processStartToExitMs =
        processTimer.elapsed();

    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QVERIFY2(
        process.exitCode() == 0,
        qPrintable(
            QStringLiteral("ClassMngr exited with code %1.\n%2")
                .arg(process.exitCode())
                .arg(processOutput(process))
            )
        );

    QFile metricsFile(metricsPath);

    QVERIFY2(
        metricsFile.open(QIODevice::ReadOnly),
        qPrintable(
            QStringLiteral("Unable to open startup metrics file: %1")
                .arg(metricsFile.errorString())
            )
        );

    QJsonParseError parseError;

    const QJsonDocument document =
        QJsonDocument::fromJson(
            metricsFile.readAll(),
            &parseError
            );

    QVERIFY2(
        parseError.error == QJsonParseError::NoError,
        qPrintable(parseError.errorString())
        );
    QVERIFY(document.isObject());

    const QJsonObject metrics =
        document.object();

    QCOMPARE(
        metrics.value(QStringLiteral("format")).toString(),
        QStringLiteral("classmngr-startup-profile-v2")
        );
    const QJsonObject scenario =
        metrics.value(QStringLiteral("scenario")).toObject();
    QCOMPARE(
        scenario.value(QStringLiteral("name")).toString(),
        QStringLiteral("minimal-startup")
        );
    QCOMPARE(
        scenario.value(QStringLiteral("actions")).toArray().size(),
        4
        );
    QCOMPARE(
        scenario.value(QStringLiteral("settleMilliseconds")).toInt(),
        0
        );
    const QJsonArray checkpoints =
        metrics.value(QStringLiteral("checkpoints")).toArray();
    QVERIFY(checkpoints.size() >= 17);

    QSet<QString> checkpointNames;
    QJsonObject startupComplete;
    for (const QJsonValue& value : checkpoints)
    {
        const QJsonObject checkpoint = value.toObject();
        const QString name = checkpoint.value(QStringLiteral("name")).toString();
        checkpointNames.insert(name);
        QVERIFY(checkpoint.value(QStringLiteral("elapsedMs")).toDouble(-1.0) >= 0.0);
        QVERIFY(checkpoint.value(QStringLiteral("memory")).toObject().contains(
            QStringLiteral("workingSetBytes")
            ));
        QVERIFY(checkpoint.value(QStringLiteral("metrics")).toObject().contains(
            QStringLiteral("widgetCount")
            ));

        if (name == QStringLiteral("startup-complete"))
        {
            startupComplete = checkpoint;
        }
    }

    const QSet<QString> requiredCheckpoints{
        QStringLiteral("process-start"),
        QStringLiteral("qapplication-created"),
        QStringLiteral("preferences-resolved"),
        QStringLiteral("locale-applied"),
        QStringLiteral("font-applied"),
        QStringLiteral("theme-applied"),
        QStringLiteral("resource-system-initialized"),
        QStringLiteral("splash-shown"),
        QStringLiteral("services-created"),
        QStringLiteral("main-window-shell-created"),
        QStringLiteral("page-manager-initialized"),
        QStringLiteral("controllers-connected"),
        QStringLiteral("database-opened"),
        QStringLiteral("navigation-data-loaded"),
        QStringLiteral("startup-page-created"),
        QStringLiteral("startup-page-loaded"),
        QStringLiteral("window-shown"),
        QStringLiteral("startup-complete")
    };
    for (const QString& requiredCheckpoint : requiredCheckpoints)
    {
        QVERIFY(checkpointNames.contains(requiredCheckpoint));
    }
    QVERIFY(!startupComplete.isEmpty());

    const QJsonObject startupMetrics =
        startupComplete.value(QStringLiteral("metrics")).toObject();
    QVERIFY(startupMetrics.value(QStringLiteral("widgetCount")).toInt() > 0);
    QVERIFY(startupMetrics.value(QStringLiteral("registeredPageCount")).toInt() > 0);
    QVERIFY(startupMetrics.value(QStringLiteral("instantiatedPageCount")).toInt() > 0);
    QCOMPARE(
        startupMetrics.value(QStringLiteral("liveScheduleWidgetCount")).toInt(),
        1
        );
    QCOMPARE(
        startupMetrics.value(QStringLiteral("scheduleWidgetsCreated")).toDouble(),
        1.0
        );
    QCOMPARE(
        startupMetrics.value(QStringLiteral("scheduleRenderCount")).toDouble(),
        0.0
        );

    const QJsonArray events = metrics.value(QStringLiteral("events")).toArray();
    QSet<QString> eventNames;
    bool startupScheduleDiagnosticPassed = false;
    for (const QJsonValue& value : events)
    {
        const QJsonObject event = value.toObject();
        const QString eventName =
            event.value(QStringLiteral("name")).toString();
        eventNames.insert(eventName);

        if (
            eventName == QStringLiteral("schedule-widget-startup-diagnostic")
            && event.value(QStringLiteral("detail")).toString()
                == QStringLiteral("expected=1; live=1; created=1; passed=true")
            )
        {
            startupScheduleDiagnosticPassed = true;
        }
    }
    QVERIFY(eventNames.contains(QStringLiteral("page-created")));
    QVERIFY(eventNames.contains(QStringLiteral("schedule-widget-created")));
    QVERIFY(!eventNames.contains(QStringLiteral("schedule-render-start")));
    QVERIFY(!eventNames.contains(QStringLiteral("schedule-render-end")));
    QVERIFY(startupScheduleDiagnosticPassed);

    const QString representativeDatabasePath =
        QStringLiteral(CLASSMNGR_SOURCE_DIR)
        + QStringLiteral(
            "/plans/startup-sequence-optimization-plan/Testing-copy.tps"
            );
    QVERIFY2(
        QFile::exists(representativeDatabasePath),
        qPrintable(
            QStringLiteral("Representative database does not exist: %1")
                .arg(representativeDatabasePath)
            )
        );

    const QString representativeMetricsPath =
        directory.filePath(
            QStringLiteral("representative-startup-metrics.json")
            );
    QProcess representativeProcess;
    representativeProcess.setProcessEnvironment(environment);
    representativeProcess.start(
        appPath,
        {
            QStringLiteral("--startup-performance-test"),
            QStringLiteral("--startup-performance-scenario"),
            QStringLiteral("representative"),
            QStringLiteral("--startup-performance-settle-ms"),
            QStringLiteral("5000"),
            QStringLiteral("--startup-performance-output"),
            representativeMetricsPath,
            representativeDatabasePath
        }
        );
    QVERIFY2(
        representativeProcess.waitForStarted(StartupTimeoutMs),
        qPrintable(representativeProcess.errorString())
        );
    QVERIFY2(
        representativeProcess.waitForFinished(StartupTimeoutMs),
        qPrintable(processOutput(representativeProcess))
        );
    QCOMPARE(representativeProcess.exitStatus(), QProcess::NormalExit);
    QVERIFY2(
        representativeProcess.exitCode() == 0,
        qPrintable(processOutput(representativeProcess))
        );

    QFile representativeMetricsFile(representativeMetricsPath);
    QVERIFY(representativeMetricsFile.open(QIODevice::ReadOnly));
    const QJsonDocument representativeDocument =
        QJsonDocument::fromJson(representativeMetricsFile.readAll());
    QVERIFY(representativeDocument.isObject());

    QJsonObject representativeStartupComplete;
    QJsonObject representativeSettledOneSecond;
    QJsonObject representativeSettledFiveSeconds;
    for (const QJsonValue& value : representativeDocument.object()
             .value(QStringLiteral("checkpoints"))
             .toArray())
    {
        const QJsonObject checkpoint = value.toObject();
        if (checkpoint.value(QStringLiteral("name")).toString()
            == QStringLiteral("startup-complete"))
        {
            representativeStartupComplete = checkpoint;
        }
        else if (checkpoint.value(QStringLiteral("name")).toString()
                 == QStringLiteral("settled-1s"))
        {
            representativeSettledOneSecond = checkpoint;
        }
        else if (checkpoint.value(QStringLiteral("name")).toString()
                 == QStringLiteral("settled-5s"))
        {
            representativeSettledFiveSeconds = checkpoint;
        }
    }
    QVERIFY(!representativeStartupComplete.isEmpty());
    QVERIFY(!representativeSettledOneSecond.isEmpty());
    QVERIFY(!representativeSettledFiveSeconds.isEmpty());

    const QJsonObject representativeStartupMetrics =
        representativeStartupComplete.value(QStringLiteral("metrics")).toObject();
    QCOMPARE(
        representativeStartupMetrics
            .value(QStringLiteral("scheduleRenderCount"))
            .toDouble(),
        1.0
        );

    for (const QJsonObject& settledCheckpoint : {
            representativeSettledOneSecond,
            representativeSettledFiveSeconds
        })
    {
        const QJsonObject settledMetrics =
            settledCheckpoint.value(QStringLiteral("metrics")).toObject();
        QCOMPARE(
            settledMetrics.value(QStringLiteral("widgetCount")).toInt(),
            representativeStartupMetrics
                .value(QStringLiteral("widgetCount")).toInt()
            );
        QCOMPARE(
            settledMetrics.value(QStringLiteral("instantiatedPageCount")).toInt(),
            representativeStartupMetrics
                .value(QStringLiteral("instantiatedPageCount")).toInt()
            );
        QCOMPARE(
            settledMetrics.value(QStringLiteral("registeredPageCount")).toInt(),
            representativeStartupMetrics
                .value(QStringLiteral("registeredPageCount")).toInt()
            );
        QCOMPARE(
            settledMetrics.value(QStringLiteral("scheduleRenderCount")).toDouble(),
            representativeStartupMetrics
                .value(QStringLiteral("scheduleRenderCount")).toDouble()
            );
    }

    QSet<QString> representativeEventNames;
    for (const QJsonValue& value : representativeDocument.object()
             .value(QStringLiteral("events"))
             .toArray())
    {
        representativeEventNames.insert(
            value.toObject().value(QStringLiteral("name")).toString()
            );
    }
    QVERIFY(
        representativeEventNames.contains(
            QStringLiteral("schedule-render-start")
            )
        );
    QVERIFY(
        representativeEventNames.contains(
            QStringLiteral("schedule-render-end")
            )
        );

    const double startupCompleteMs =
        startupComplete.value(QStringLiteral("elapsedMs")).toDouble(-1.0);
    const double progressUpdates =
        metrics.value(QStringLiteral("progressUpdates")).toDouble(-1.0);
    const double finalProgress =
        metrics.value(QStringLiteral("finalProgress")).toDouble(-1.0);

    QVERIFY(startupCompleteMs >= 0.0);
    QVERIFY(progressUpdates > 0.0);
    QCOMPARE(finalProgress, 100.0);
    QVERIFY(processStartToExitMs >= startupCompleteMs);

#if defined(Q_OS_WIN)
    const QJsonObject startupMemory =
        startupComplete.value(QStringLiteral("memory")).toObject();
    QVERIFY(startupMemory.value(QStringLiteral("workingSetBytes")).toDouble() > 0.0);
    QVERIFY(startupMemory.value(QStringLiteral("privateUsageBytes")).toDouble() > 0.0);
#endif

    std::printf(
        "Startup performance: startupComplete=%.0f ms, process=%lld ms, widgets=%d, pages=%d/%d, schedules=%d, renders=%.0f, progressUpdates=%.0f, finalProgress=%.0f\n",
        startupCompleteMs,
        static_cast<long long>(processStartToExitMs),
        startupMetrics.value(QStringLiteral("widgetCount")).toInt(),
        startupMetrics.value(QStringLiteral("instantiatedPageCount")).toInt(),
        startupMetrics.value(QStringLiteral("registeredPageCount")).toInt(),
        startupMetrics.value(QStringLiteral("liveScheduleWidgetCount")).toInt(),
        startupMetrics.value(QStringLiteral("scheduleRenderCount")).toDouble(),
        progressUpdates,
        finalProgress
        );
    std::fflush(stdout);

    QString thresholdMessage;

    if (
        thresholdExceeded(
            "CLASSMNGR_STARTUP_WINDOW_MAX_MS",
            startupCompleteMs,
            &thresholdMessage
            )
        )
    {
        const QByteArray message =
            thresholdMessage.toLocal8Bit();

        std::printf(
            "Startup performance threshold failed: %s\n",
            message.constData()
            );
        std::fflush(stdout);

        QFAIL(message.constData());
    }

    if (
        thresholdExceeded(
            "CLASSMNGR_STARTUP_READY_MAX_MS",
            startupCompleteMs,
            &thresholdMessage
            )
        )
    {
        const QByteArray message =
            thresholdMessage.toLocal8Bit();

        std::printf(
            "Startup performance threshold failed: %s\n",
            message.constData()
            );
        std::fflush(stdout);

        QFAIL(message.constData());
    }

    if (
        thresholdExceeded(
            "CLASSMNGR_STARTUP_PROCESS_MAX_MS",
            static_cast<double>(processStartToExitMs),
            &thresholdMessage
            )
        )
    {
        const QByteArray message =
            thresholdMessage.toLocal8Bit();

        std::printf(
            "Startup performance threshold failed: %s\n",
            message.constData()
            );
        std::fflush(stdout);

        QFAIL(message.constData());
    }
}

QTEST_MAIN(StartupPerformanceTests)

#include "startup_performance_tests.moc"
