#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QHash>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSet>
#include <QTemporaryDir>
#include <QtTest>

#include <cstdio>

namespace
{
constexpr int StartupTimeoutMs = 60000;

struct FixtureTableExpectation
{
    const char* name;
    int expectedRows;
};

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

bool writeRepresentativeStartupSettings(
    const QString& settingsRoot
    )
{
    QDir directory(settingsRoot);
    if (!directory.mkpath(QStringLiteral("PaperCloud")))
    {
        return false;
    }

    QSettings settings(
        directory.filePath(QStringLiteral("PaperCloud/ClassMngr.ini")),
        QSettings::IniFormat
        );

    // Keep this profile explicit rather than inheriting a developer's local
    // defaults. The data fixture stores schedule settings; these are the
    // application-wide settings read before the window is constructed.
    settings.setValue(QStringLiteral("options/theme"), 1);
    settings.setValue(QStringLiteral("options/fontSize"), 2);
    settings.setValue(QStringLiteral("options/language"), 1);
    settings.setValue(QStringLiteral("options/saveMode"), 0);
    settings.setValue(QStringLiteral("options/documentPageSpacing"), 2);
    settings.setValue(QStringLiteral("options/documentViewerBackground"), 1);
    settings.setValue(QStringLiteral("options/sidebarTooltipsEnabled"), true);
    settings.setValue(QStringLiteral("options/sidebarMarqueeEnabled"), false);
    settings.setValue(
        QStringLiteral("updates/automaticChecksEnabled"),
        false
        );
    settings.sync();

    return settings.status() == QSettings::NoError;
}

void printRepresentativeCheckpoint(
    const QJsonObject& checkpoint
    )
{
    const QJsonObject metrics =
        checkpoint.value(QStringLiteral("metrics")).toObject();
    const QJsonObject memory =
        checkpoint.value(QStringLiteral("memory")).toObject();
    const QByteArray name =
        checkpoint.value(QStringLiteral("name")).toString().toLocal8Bit();
    const QByteArray platform =
        memory.value(QStringLiteral("platform")).toString().toLocal8Bit();

    std::printf(
        "Representative startup: checkpoint=%s, elapsed=%.0f ms, platform=%s, "
        "workingSet=%.0f, peakWorkingSet=%.0f, private=%.0f, widgets=%d, "
        "pages=%d/%d, schedules=%d, renders=%.0f\n",
        name.constData(),
        checkpoint.value(QStringLiteral("elapsedMs")).toDouble(),
        platform.constData(),
        memory.value(QStringLiteral("workingSetBytes")).toDouble(),
        memory.value(QStringLiteral("peakWorkingSetBytes")).toDouble(),
        memory.value(QStringLiteral("privateUsageBytes")).toDouble(),
        metrics.value(QStringLiteral("widgetCount")).toInt(),
        metrics.value(QStringLiteral("instantiatedPageCount")).toInt(),
        metrics.value(QStringLiteral("registeredPageCount")).toInt(),
        metrics.value(QStringLiteral("liveScheduleWidgetCount")).toInt(),
        metrics.value(QStringLiteral("scheduleRenderCount")).toDouble()
        );
}
}

class StartupPerformanceTests : public QObject
{
    Q_OBJECT

private slots:
    void representativeStartupFixtureIsCompleteAndDeterministic();
    void reportsStartupMetricsAndHonorsThresholds();
};

void StartupPerformanceTests
    ::representativeStartupFixtureIsCompleteAndDeterministic()
{
    const QString fixturePath =
        QStringLiteral(CLASSMNGR_SOURCE_DIR)
        + QStringLiteral(
            "/plans/startup-sequence-optimization-plan/Testing-copy.tps"
            );
    QVERIFY2(
        QFile::exists(fixturePath),
        qPrintable(
            QStringLiteral("Representative database does not exist: %1")
                .arg(fixturePath)
            )
        );

    const QString connectionName =
        QStringLiteral("startup-representative-fixture-validation");
    QSqlDatabase database =
        QSqlDatabase::addDatabase(
            QStringLiteral("QSQLITE"),
            connectionName
            );
    database.setDatabaseName(fixturePath);
    QVERIFY2(
        database.open(),
        qPrintable(database.lastError().text())
        );

    QSqlQuery query(database);
    QVERIFY2(
        query.exec(QStringLiteral("PRAGMA integrity_check")),
        qPrintable(query.lastError().text())
        );
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("ok"));

    for (const FixtureTableExpectation& expected : {
             FixtureTableExpectation{"teachers", 4},
             FixtureTableExpectation{"classes", 8},
             FixtureTableExpectation{"class_times", 10},
             FixtureTableExpectation{"class_intensive_times", 4},
             FixtureTableExpectation{"intensive_slot_states", 3},
             FixtureTableExpectation{"roster_columns", 8},
             FixtureTableExpectation{"roster_data", 24},
             FixtureTableExpectation{"app_settings", 12}
         })
    {
        QVERIFY2(
            query.exec(
                QStringLiteral("SELECT COUNT(*) FROM %1")
                    .arg(QString::fromLatin1(expected.name))
                ),
            qPrintable(query.lastError().text())
            );
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), expected.expectedRows);
    }

    QVERIFY(query.exec(QStringLiteral(R"(
        SELECT COUNT(DISTINCT teacher_id)
        FROM class_info
        WHERE teacher_id IS NOT NULL
    )")));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 4);

    QVERIFY(query.exec(QStringLiteral(
        "SELECT COUNT(DISTINCT class_id) FROM roster_data"
        )));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 2);

    query.prepare(QStringLiteral(
        "SELECT value FROM app_settings WHERE key=?"
        ));
    query.addBindValue(QStringLiteral("schedule_display_mode"));
    QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("regular"));

    query.prepare(QStringLiteral(
        "SELECT value FROM app_settings WHERE key=?"
        ));
    query.addBindValue(QStringLiteral("schedule_show_weekends"));
    QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toString(), QStringLiteral("true"));

    database.close();
    database = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);
}

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
    int startupCompleteCount = 0;
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
            ++startupCompleteCount;
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
    QCOMPARE(startupCompleteCount, 1);

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
    const double startupCompleteElapsedMilliseconds =
        startupComplete.value(QStringLiteral("elapsedMs")).toDouble();
    for (const QJsonValue& value : events)
    {
        const QJsonObject event = value.toObject();
        const QString eventName =
            event.value(QStringLiteral("name")).toString();
        eventNames.insert(eventName);

        QVERIFY(
            eventName != QStringLiteral("page-created")
            || event.value(QStringLiteral("elapsedMs")).toDouble()
                <= startupCompleteElapsedMilliseconds
            );

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

    const QString representativeFixturePath =
        QStringLiteral(CLASSMNGR_SOURCE_DIR)
        + QStringLiteral(
            "/plans/startup-sequence-optimization-plan/Testing-copy.tps"
            );
    QVERIFY2(
        QFile::exists(representativeFixturePath),
        qPrintable(
            QStringLiteral("Representative database does not exist: %1")
                .arg(representativeFixturePath)
            )
        );

    const QString representativeDatabasePath =
        directory.filePath(QStringLiteral("representative-startup.tps"));
    QVERIFY2(
        QFile::copy(
            representativeFixturePath,
            representativeDatabasePath
            ),
        qPrintable(
            QStringLiteral("Unable to copy representative database to %1")
                .arg(representativeDatabasePath)
            )
        );
    QVERIFY2(
        writeRepresentativeStartupSettings(
            directory.filePath(QStringLiteral("settings"))
            ),
        "Unable to write deterministic representative startup settings."
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
    QJsonParseError representativeParseError;
    const QJsonDocument representativeDocument =
        QJsonDocument::fromJson(
            representativeMetricsFile.readAll(),
            &representativeParseError
            );
    QVERIFY2(
        representativeParseError.error == QJsonParseError::NoError,
        qPrintable(representativeParseError.errorString())
        );
    QVERIFY(representativeDocument.isObject());

    const QJsonObject representativeReport = representativeDocument.object();
    QCOMPARE(
        representativeReport.value(QStringLiteral("format")).toString(),
        QStringLiteral("classmngr-startup-profile-v2")
        );
    const QJsonObject representativeScenario =
        representativeReport.value(QStringLiteral("scenario")).toObject();
    QCOMPARE(
        representativeScenario.value(QStringLiteral("name")).toString(),
        QStringLiteral("representative-startup")
        );
    QCOMPARE(
        representativeScenario.value(QStringLiteral("actions")).toArray().size(),
        4
        );
    QCOMPARE(
        representativeScenario.value(QStringLiteral("settleMilliseconds")).toInt(),
        5000
        );

    QHash<QString, QJsonObject> representativeCheckpoints;
    QHash<QString, int> representativeCheckpointCounts;
    QJsonObject representativeWindowShown;
    QJsonObject representativeStartupComplete;
    QJsonObject representativeSettledOneSecond;
    QJsonObject representativeSettledFiveSeconds;
    for (const QJsonValue& value : representativeReport
             .value(QStringLiteral("checkpoints"))
             .toArray())
    {
        const QJsonObject checkpoint = value.toObject();
        const QString name = checkpoint.value(QStringLiteral("name")).toString();
        representativeCheckpoints.insert(name, checkpoint);
        ++representativeCheckpointCounts[name];

        if (name == QStringLiteral("window-shown"))
        {
            representativeWindowShown = checkpoint;
        }
        else if (name == QStringLiteral("startup-complete"))
        {
            representativeStartupComplete = checkpoint;
        }
        else if (name == QStringLiteral("settled-1s"))
        {
            representativeSettledOneSecond = checkpoint;
        }
        else if (name == QStringLiteral("settled-5s"))
        {
            representativeSettledFiveSeconds = checkpoint;
        }
    }
    QVERIFY(!representativeWindowShown.isEmpty());
    QVERIFY(!representativeStartupComplete.isEmpty());
    QCOMPARE(
        representativeCheckpointCounts.value(QStringLiteral("window-shown")),
        1
        );
    QCOMPARE(
        representativeCheckpointCounts.value(QStringLiteral("startup-complete")),
        1
        );
    QCOMPARE(
        representativeCheckpointCounts.value(QStringLiteral("settled-5s")),
        1
        );
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

    // These three checkpoints are the permanent startup regression surface:
    // first visible frame, completed startup, and five-second steady state.
    // Widget counts may drop when the splash is released, but hidden pages or
    // schedules must never appear in any of the three snapshots.
    for (const QString& checkpointName : {
             QStringLiteral("window-shown"),
             QStringLiteral("startup-complete"),
             QStringLiteral("settled-5s")
         })
    {
        const QJsonObject checkpoint =
            representativeCheckpoints.value(checkpointName);
        const QJsonObject checkpointMetrics =
            checkpoint.value(QStringLiteral("metrics")).toObject();
        const QJsonObject checkpointMemory =
            checkpoint.value(QStringLiteral("memory")).toObject();

        QVERIFY(
            checkpoint.value(QStringLiteral("elapsedMs")).toDouble(-1.0)
                >= 0.0
            );
        QVERIFY(checkpointMemory.value(QStringLiteral("available")).toBool());
        QVERIFY(!checkpointMemory.value(QStringLiteral("platform")).toString().isEmpty());
        QVERIFY(
            checkpointMemory.value(QStringLiteral("workingSetBytes")).toDouble()
                > 0.0
            );
        QVERIFY(
            checkpointMemory.value(QStringLiteral("privateUsageBytes")).toDouble()
                > 0.0
            );
        QVERIFY(
            checkpointMemory.value(QStringLiteral("peakWorkingSetBytes")).toDouble()
                >= checkpointMemory.value(QStringLiteral("workingSetBytes")).toDouble()
            );
        QVERIFY(checkpointMetrics.value(QStringLiteral("widgetCount")).toInt() > 0);
        QCOMPARE(
            checkpointMetrics.value(QStringLiteral("instantiatedPageCount")).toInt(),
            1
            );
        QCOMPARE(
            checkpointMetrics.value(QStringLiteral("registeredPageCount")).toInt(),
            11
            );
        QCOMPARE(
            checkpointMetrics.value(QStringLiteral("liveScheduleWidgetCount")).toInt(),
            1
            );
        QCOMPARE(
            checkpointMetrics.value(QStringLiteral("scheduleWidgetsCreated")).toDouble(),
            1.0
            );
        QCOMPARE(
            checkpointMetrics.value(QStringLiteral("scheduleRenderCount")).toDouble(),
            1.0
            );

        printRepresentativeCheckpoint(checkpoint);
    }
    std::fflush(stdout);

    const QJsonObject representativePeakMemory =
        representativeReport.value(QStringLiteral("peakMemory")).toObject();
    QVERIFY(representativePeakMemory.value(QStringLiteral("available")).toBool());
    QCOMPARE(
        representativePeakMemory
            .value(QStringLiteral("checkpointSampleCount"))
            .toInt(),
        representativeReport.value(QStringLiteral("checkpoints")).toArray().size()
        );
    QVERIFY(
        representativePeakMemory.value(QStringLiteral("workingSetBytes")).toDouble()
            >= representativeSettledFiveSeconds
                .value(QStringLiteral("memory"))
                .toObject()
                .value(QStringLiteral("workingSetBytes"))
                .toDouble()
        );
    QVERIFY(
        representativePeakMemory
            .value(QStringLiteral("peakWorkingSetBytes"))
            .toDouble()
            >= representativePeakMemory
                .value(QStringLiteral("workingSetBytes"))
                .toDouble()
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
        QCOMPARE(
            settledMetrics.value(QStringLiteral("liveScheduleWidgetCount")).toInt(),
            representativeStartupMetrics
                .value(QStringLiteral("liveScheduleWidgetCount")).toInt()
            );
        QCOMPARE(
            settledMetrics.value(QStringLiteral("scheduleWidgetsCreated")).toDouble(),
            representativeStartupMetrics
                .value(QStringLiteral("scheduleWidgetsCreated")).toDouble()
            );
        QCOMPARE(
            settledMetrics.value(QStringLiteral("scheduleTableItemsCreated")).toDouble(),
            representativeStartupMetrics
                .value(QStringLiteral("scheduleTableItemsCreated")).toDouble()
            );
        QCOMPARE(
            settledMetrics.value(QStringLiteral("scheduleCellWidgetsCreated")).toDouble(),
            representativeStartupMetrics
                .value(QStringLiteral("scheduleCellWidgetsCreated")).toDouble()
            );
    }

    QSet<QString> representativeEventNames;
    QSet<QString> representativePageIdentifiers;
    bool representativeScheduleDiagnosticPassed = false;
    const double representativeStartupCompleteElapsedMilliseconds =
        representativeStartupComplete.value(QStringLiteral("elapsedMs")).toDouble();
    for (const QJsonValue& value : representativeReport
             .value(QStringLiteral("events"))
             .toArray())
    {
        const QJsonObject event = value.toObject();
        const QString eventName =
            event.value(QStringLiteral("name")).toString();
        representativeEventNames.insert(eventName);
        QVERIFY(
            eventName != QStringLiteral("page-created")
            || event.value(QStringLiteral("elapsedMs")).toDouble()
                <= representativeStartupCompleteElapsedMilliseconds
            );

        QVERIFY(
            eventName != QStringLiteral("schedule-render-start")
            || event.value(QStringLiteral("elapsedMs")).toDouble()
                <= representativeStartupCompleteElapsedMilliseconds
            );

        if (eventName == QStringLiteral("page-created"))
        {
            representativePageIdentifiers.insert(
                event.value(QStringLiteral("detail")).toString()
                );
        }

        if (
            eventName == QStringLiteral("schedule-widget-startup-diagnostic")
            && event.value(QStringLiteral("detail")).toString()
                == QStringLiteral("expected=1; live=1; created=1; passed=true")
            )
        {
            representativeScheduleDiagnosticPassed = true;
        }
    }
    QCOMPARE(representativePageIdentifiers.size(), 1);
    QVERIFY(representativePageIdentifiers.contains(QStringLiteral("my-workspace")));
    QVERIFY(!representativePageIdentifiers.contains(QStringLiteral("sub-prep")));
    QVERIFY(!representativePageIdentifiers.contains(QStringLiteral("pdf-viewer")));
    QVERIFY(!representativePageIdentifiers.contains(QStringLiteral("campus-dashboard")));
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
    QVERIFY(representativeScheduleDiagnosticPassed);

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
