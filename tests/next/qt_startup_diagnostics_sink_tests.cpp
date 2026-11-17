#include "core/process_memory_snapshot.h"
#include "next/application/startup_coordinator.h"
#include "next/bootstrap/qt_startup_diagnostics_sink.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QtTest/QtTest>

#include <memory>
#include <optional>
#include <utility>

using namespace ClassMngr::Next;

namespace
{

QList<QByteArray>* activeQtMessages = nullptr;

void captureQtMessage(
    QtMsgType,
    const QMessageLogContext&,
    const QString& message
    )
{
    if (activeQtMessages)
    {
        activeQtMessages->append(message.toUtf8());
    }
}

class ScopedQtMessageCapture final
{
public:
    ScopedQtMessageCapture()
    {
        activeQtMessages = &m_messages;
        m_previousHandler = qInstallMessageHandler(captureQtMessage);
    }

    ~ScopedQtMessageCapture()
    {
        qInstallMessageHandler(m_previousHandler);
        activeQtMessages = nullptr;
    }

    [[nodiscard]] const QList<QByteArray>& messages() const noexcept
    {
        return m_messages;
    }

private:
    QList<QByteArray> m_messages;
    QtMessageHandler m_previousHandler = nullptr;
};

class FixedProcessMemorySnapshotProvider final
    : public ProcessMemorySnapshotProvider
{
public:
    explicit FixedProcessMemorySnapshotProvider(ProcessMemorySnapshot sample)
        : m_sample(std::move(sample))
    {
    }

    [[nodiscard]] ProcessMemorySnapshot snapshot() const override
    {
        return m_sample;
    }

private:
    ProcessMemorySnapshot m_sample;
};

void makeAllCountersAvailable(ProcessMemorySnapshot& sample)
{
    sample.workingSetBytesAvailable = true;
    sample.peakWorkingSetBytesAvailable = true;
    sample.privateUsageBytesAvailable = true;
    sample.pagefileUsageBytesAvailable = true;
    sample.handleCountAvailable = true;
    sample.threadCountAvailable = true;
    sample.privateWorkingSetBytesAvailable = true;
    sample.privateDirtyBytesAvailable = true;
}

[[nodiscard]] QByteArray recordStartupEvent(
    ProcessMemorySnapshot sample,
    const Application::StartupEvent& event
    )
{
    ScopedQtMessageCapture capture;
    std::unique_ptr<Bootstrap::StartupDiagnosticsSink> sink =
        Bootstrap::makeQtStartupDiagnosticsSink(
            std::make_unique<FixedProcessMemorySnapshotProvider>(
                std::move(sample)
                )
            );
    sink->record(event);
    return capture.messages().value(0);
}

[[nodiscard]] std::optional<QJsonObject> checkpointFromRecord(
    const QByteArray& record
    )
{
    static const QByteArray checkpointMarker(" checkpoint=");
    const qsizetype markerIndex = record.indexOf(checkpointMarker);
    if (markerIndex < 0)
    {
        return std::nullopt;
    }

    const QByteArray serialized = record.mid(
        markerIndex + checkpointMarker.size()
        );
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(
        serialized,
        &parseError
        );
    if (parseError.error != QJsonParseError::NoError
        || !document.isObject())
    {
        return std::nullopt;
    }

    return document.object();
}

}  // namespace

class StartupDiagnosticsSinkTests final : public QObject
{
    Q_OBJECT

private slots:
    void serializesMacosResidentMemoryWithDeterministicFields();
    void serializesWindowsProcessMemoryCounterSemantics();
    void serializesUnavailableWindowsOptionalCountersAsNull();
    void serializesUnavailableMacThreadCounterAsNull();
    void serializesLinuxProcessMemoryCounterSemantics();
    void serializesLinuxMissingPssAsNullWithoutUsingAnonymousResident();
    void unavailableSampleContainsNoMeasuredCounterValues();
    void nativeProviderSmokeTest();
};

void StartupDiagnosticsSinkTests
    ::serializesMacosResidentMemoryWithDeterministicFields()
{
    ProcessMemorySnapshot sample;
    sample.isAvailable = true;
    sample.platform = QStringLiteral("macos");
    sample.workingSetBytes = 1001;
    sample.peakWorkingSetBytes = 1002;
    sample.privateUsageBytes = 1003;
    sample.privateWorkingSetBytes = 1004;
    sample.pagefileUsageBytes = 1005;
    sample.threadCount = 1006;
    sample.handleCount = 1099;
    sample.privateDirtyBytes = 1098;
    makeAllCountersAvailable(sample);

    const Application::StartupEvent event{
        Application::StartupStage::SettingsLoaded,
        Application::StartupStageOutcome::Warning,
        Application::StartupDiagnostic{
            Application::StartupDiagnosticCode::StageWarning,
            Application::StartupStage::SettingsLoaded
        }
    };
    const QByteArray record = recordStartupEvent(sample, event);

    QCOMPARE(
        record,
        QByteArrayLiteral(
            "ClassMngrNext startup stage=settings-loaded outcome=warning "
            "diagnostic=stage-warning diagnostic_stage=settings-loaded "
            "checkpoint={\"metrics\":{\"compressed_bytes\":1005,"
            "\"internal_bytes\":1004,\"peak_resident_bytes\":1002,"
            "\"physical_footprint_bytes\":1003,\"resident_bytes\":1001,"
            "\"thread_count\":1006},\"outcome\":\"warning\","
            "\"platform\":\"macos\",\"sample_available\":true,"
            "\"stage\":\"settings-loaded\"}"
            )
        );

    const std::optional<QJsonObject> checkpoint = checkpointFromRecord(record);
    QVERIFY(checkpoint.has_value());
    const QJsonObject metrics = checkpoint->value(
        QStringLiteral("metrics")
        ).toObject();
    QCOMPARE(metrics.size(), 6);
    QVERIFY(!metrics.contains(QStringLiteral("working_set_bytes")));
    QVERIFY(!metrics.contains(QStringLiteral("rss_bytes")));
    QVERIFY(!metrics.contains(QStringLiteral("handle_count")));
    QVERIFY(!metrics.contains(QStringLiteral("private_dirty_bytes")));
}

void StartupDiagnosticsSinkTests
    ::serializesWindowsProcessMemoryCounterSemantics()
{
    ProcessMemorySnapshot sample;
    sample.isAvailable = true;
    sample.platform = QStringLiteral("windows");
    sample.workingSetBytes = 2001;
    sample.peakWorkingSetBytes = 2002;
    sample.privateUsageBytes = 2003;
    sample.pagefileUsageBytes = 2004;
    sample.handleCount = 21;
    sample.threadCount = 22;
    sample.privateWorkingSetBytes = 2005;
    sample.privateDirtyBytes = 2099;
    makeAllCountersAvailable(sample);

    const Application::StartupEvent event{
        Application::StartupStage::ShellCreated
    };
    const std::optional<QJsonObject> checkpoint = checkpointFromRecord(
        recordStartupEvent(sample, event)
        );
    QVERIFY(checkpoint.has_value());
    QCOMPARE(checkpoint->value(QStringLiteral("stage")).toString(),
        QStringLiteral("shell-created"));
    QCOMPARE(checkpoint->value(QStringLiteral("outcome")).toString(),
        QStringLiteral("completed"));
    QCOMPARE(checkpoint->value(QStringLiteral("sample_available")).toBool(),
        true);
    QCOMPARE(checkpoint->value(QStringLiteral("platform")).toString(),
        QStringLiteral("windows"));

    const QJsonObject metrics = checkpoint->value(
        QStringLiteral("metrics")
        ).toObject();
    QCOMPARE(metrics.size(), 7);
    QCOMPARE(metrics.value(QStringLiteral("working_set_bytes")).toInteger(),
        qint64{2001});
    QCOMPARE(metrics.value(QStringLiteral("peak_working_set_bytes")).toInteger(),
        qint64{2002});
    QCOMPARE(metrics.value(QStringLiteral("private_usage_bytes")).toInteger(),
        qint64{2003});
    QCOMPARE(metrics.value(QStringLiteral("pagefile_usage_bytes")).toInteger(),
        qint64{2004});
    QCOMPARE(metrics.value(QStringLiteral("handle_count")).toInteger(), qint64{21});
    QCOMPARE(metrics.value(QStringLiteral("thread_count")).toInteger(), qint64{22});
    QCOMPARE(metrics.value(QStringLiteral("private_working_set_bytes")).toInteger(),
        qint64{2005});
    QVERIFY(!metrics.contains(QStringLiteral("private_dirty_bytes")));
}

void StartupDiagnosticsSinkTests
    ::serializesUnavailableWindowsOptionalCountersAsNull()
{
    ProcessMemorySnapshot sample;
    sample.isAvailable = true;
    sample.platform = QStringLiteral("windows");
    sample.workingSetBytesAvailable = true;
    sample.peakWorkingSetBytesAvailable = true;
    sample.privateUsageBytesAvailable = true;
    sample.pagefileUsageBytesAvailable = true;
    sample.privateWorkingSetBytesAvailable = true;

    const Application::StartupEvent event{
        Application::StartupStage::ShellCreated
    };
    const std::optional<QJsonObject> checkpoint = checkpointFromRecord(
        recordStartupEvent(sample, event)
        );
    QVERIFY(checkpoint.has_value());
    const QJsonObject metrics = checkpoint->value(
        QStringLiteral("metrics")
        ).toObject();

    QCOMPARE(metrics.value(QStringLiteral("working_set_bytes")).toInteger(),
        qint64{0});
    QVERIFY(metrics.value(QStringLiteral("handle_count")).isNull());
    QVERIFY(metrics.value(QStringLiteral("thread_count")).isNull());
}

void StartupDiagnosticsSinkTests
    ::serializesUnavailableMacThreadCounterAsNull()
{
    ProcessMemorySnapshot sample;
    sample.isAvailable = true;
    sample.platform = QStringLiteral("macos");
    makeAllCountersAvailable(sample);
    sample.threadCountAvailable = false;

    const Application::StartupEvent event{
        Application::StartupStage::ShellCreated
    };
    const std::optional<QJsonObject> checkpoint = checkpointFromRecord(
        recordStartupEvent(sample, event)
        );
    QVERIFY(checkpoint.has_value());
    const QJsonObject metrics = checkpoint->value(
        QStringLiteral("metrics")
        ).toObject();

    QVERIFY(metrics.value(QStringLiteral("thread_count")).isNull());
    QCOMPARE(metrics.value(QStringLiteral("resident_bytes")).toInteger(),
        qint64{0});
}

void StartupDiagnosticsSinkTests
    ::serializesLinuxProcessMemoryCounterSemantics()
{
    ProcessMemorySnapshot sample;
    sample.isAvailable = true;
    sample.platform = QStringLiteral("linux");
    sample.workingSetBytes = 3001;
    sample.peakWorkingSetBytes = 3002;
    sample.privateUsageBytes = 3003;
    sample.pagefileUsageBytes = 3004;
    sample.handleCount = 31;
    sample.threadCount = 32;
    sample.privateWorkingSetBytes = 3005;
    sample.privateDirtyBytes = 3006;
    makeAllCountersAvailable(sample);

    const Application::StartupEvent event{
        Application::StartupStage::ThemeLoaded
    };
    const std::optional<QJsonObject> checkpoint = checkpointFromRecord(
        recordStartupEvent(sample, event)
        );
    QVERIFY(checkpoint.has_value());
    QCOMPARE(checkpoint->value(QStringLiteral("platform")).toString(),
        QStringLiteral("linux"));

    const QJsonObject metrics = checkpoint->value(
        QStringLiteral("metrics")
        ).toObject();
    QCOMPARE(metrics.size(), 8);
    QCOMPARE(metrics.value(QStringLiteral("rss_bytes")).toInteger(), qint64{3001});
    QCOMPARE(metrics.value(QStringLiteral("peak_rss_bytes")).toInteger(), qint64{3002});
    QCOMPARE(metrics.value(QStringLiteral("private_usage_bytes")).toInteger(), qint64{3003});
    QCOMPARE(
        metrics.value(QStringLiteral("private_clean_and_dirty_bytes")).toInteger(),
        qint64{3004}
        );
    QCOMPARE(metrics.value(QStringLiteral("file_descriptor_count")).toInteger(), qint64{31});
    QCOMPARE(metrics.value(QStringLiteral("thread_count")).toInteger(), qint64{32});
    QCOMPARE(
        metrics.value(QStringLiteral("anonymous_resident_bytes")).toInteger(),
        qint64{3005}
        );
    QCOMPARE(metrics.value(QStringLiteral("private_dirty_bytes")).toInteger(),
        qint64{3006});
    QVERIFY(!metrics.contains(QStringLiteral("working_set_bytes")));
}

void StartupDiagnosticsSinkTests
    ::serializesLinuxMissingPssAsNullWithoutUsingAnonymousResident()
{
    ProcessMemorySnapshot sample;
    sample.isAvailable = true;
    sample.platform = QStringLiteral("linux");
    sample.workingSetBytes = 4001;
    sample.workingSetBytesAvailable = true;
    sample.privateWorkingSetBytes = 4005;
    sample.privateWorkingSetBytesAvailable = true;

    const Application::StartupEvent event{
        Application::StartupStage::ThemeLoaded
    };
    const std::optional<QJsonObject> checkpoint = checkpointFromRecord(
        recordStartupEvent(sample, event)
        );
    QVERIFY(checkpoint.has_value());

    const QJsonObject metrics = checkpoint->value(
        QStringLiteral("metrics")
        ).toObject();
    QVERIFY(metrics.value(QStringLiteral("private_usage_bytes")).isNull());
    QCOMPARE(
        metrics.value(QStringLiteral("anonymous_resident_bytes")).toInteger(),
        qint64{4005}
        );
    QVERIFY(metrics.value(QStringLiteral("peak_rss_bytes")).isNull());
    QVERIFY(metrics.value(QStringLiteral("private_dirty_bytes")).isNull());
}

void StartupDiagnosticsSinkTests
    ::unavailableSampleContainsNoMeasuredCounterValues()
{
    ProcessMemorySnapshot sample;
    sample.isAvailable = false;
    sample.platform = QStringLiteral("macos");
    sample.workingSetBytes = 9001;
    sample.peakWorkingSetBytes = 9002;
    sample.privateUsageBytes = 9003;
    sample.privateWorkingSetBytes = 9004;
    sample.pagefileUsageBytes = 9005;
    sample.handleCount = 91;
    sample.threadCount = 92;
    sample.privateDirtyBytes = 9006;

    const Application::StartupEvent event{
        Application::StartupStage::StartupReady
    };
    const std::optional<QJsonObject> checkpoint = checkpointFromRecord(
        recordStartupEvent(sample, event)
        );
    QVERIFY(checkpoint.has_value());
    QCOMPARE(checkpoint->value(QStringLiteral("stage")).toString(),
        QStringLiteral("startup-ready"));
    QCOMPARE(checkpoint->value(QStringLiteral("sample_available")).toBool(),
        false);
    QCOMPARE(checkpoint->value(QStringLiteral("platform")).toString(),
        QStringLiteral("macos"));
    QVERIFY(checkpoint->value(QStringLiteral("metrics")).toObject().isEmpty());
}

void StartupDiagnosticsSinkTests::nativeProviderSmokeTest()
{
    const ProcessMemorySnapshot sample =
        PlatformProcessMemorySnapshotProvider().snapshot();
#if defined(Q_OS_WIN)
    QCOMPARE(sample.platform, QStringLiteral("windows"));
#elif defined(Q_OS_MACOS)
    QCOMPARE(sample.platform, QStringLiteral("macos"));
#elif defined(Q_OS_LINUX)
    QCOMPARE(sample.platform, QStringLiteral("linux"));
#else
    QSKIP("No native process-memory provider is implemented for this platform.");
#endif
    QVERIFY(sample.isAvailable);
    QVERIFY(sample.workingSetBytesAvailable);
    QVERIFY(sample.workingSetBytes > 0);
    QVERIFY(sample.peakWorkingSetBytesAvailable);
    QVERIFY(sample.peakWorkingSetBytes >= sample.workingSetBytes);

    const Application::StartupEvent event{
        Application::StartupStage::ProcessCreated
    };
    const std::optional<QJsonObject> checkpoint = checkpointFromRecord(
        recordStartupEvent(sample, event)
        );
    QVERIFY(checkpoint.has_value());
    QVERIFY(checkpoint->value(QStringLiteral("sample_available")).toBool());
    QVERIFY(!checkpoint->value(QStringLiteral("metrics")).toObject().isEmpty());
}

QTEST_GUILESS_MAIN(StartupDiagnosticsSinkTests)

#include "qt_startup_diagnostics_sink_tests.moc"
