#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
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

double metricValue(
    const QJsonObject& metrics,
    const QString& key
    )
{
    return metrics.value(key).toDouble(-1.0);
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

    const double windowConstructedMs =
        metricValue(
            metrics,
            QStringLiteral("processStartToWindowConstructedMs")
            );
    const double readyMs =
        metricValue(
            metrics,
            QStringLiteral("processStartToReadyMs")
            );
    const double windowConstructedWorkingSetBytes =
        metricValue(
            metrics,
            QStringLiteral("windowConstructedWorkingSetBytes")
            );
    const double windowConstructedPrivateBytes =
        metricValue(
            metrics,
            QStringLiteral("windowConstructedPrivateBytes")
            );
    const double progressUpdates =
        metricValue(
            metrics,
            QStringLiteral("progressUpdates")
            );
    const double finalProgress =
        metricValue(
            metrics,
            QStringLiteral("finalProgress")
            );

    QVERIFY(windowConstructedMs >= 0.0);
    QVERIFY(readyMs >= windowConstructedMs);
    QVERIFY(progressUpdates > 0.0);
    QCOMPARE(finalProgress, 100.0);
    QVERIFY(processStartToExitMs >= readyMs);

#if defined(Q_OS_WIN)
    QVERIFY(windowConstructedWorkingSetBytes > 0.0);
    QVERIFY(windowConstructedPrivateBytes > 0.0);
#endif

    std::printf(
        "Startup performance: windowConstructed=%.0f ms, ready=%.0f ms, process=%lld ms, workingSet=%.1f MiB, privateBytes=%.1f MiB, progressUpdates=%.0f, finalProgress=%.0f\n",
        windowConstructedMs,
        readyMs,
        static_cast<long long>(processStartToExitMs),
        windowConstructedWorkingSetBytes / (1024.0 * 1024.0),
        windowConstructedPrivateBytes / (1024.0 * 1024.0),
        progressUpdates,
        finalProgress
        );
    std::fflush(stdout);

    QString thresholdMessage;

    if (
        thresholdExceeded(
            "CLASSMNGR_STARTUP_WINDOW_MAX_MS",
            windowConstructedMs,
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
            readyMs,
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
