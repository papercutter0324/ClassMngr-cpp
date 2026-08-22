#include "core/memory_usage_diagnostics.h"
#include "core/process_memory_snapshot.h"
#include "ui/shared/dialogs/memory_usage_dialog.h"

#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QtTest>

#include <algorithm>
#include <memory>

namespace
{
class FakeSnapshotProvider final : public ProcessMemorySnapshotProvider
{
public:
    ProcessMemorySnapshot snapshot() const override
    {
        ++callCount;
        return {
            true,
            3 * 1024 * 1024,
            4 * 1024 * 1024,
            2 * 1024 * 1024,
            2 * 1024 * 1024,
            12,
            3,
            QDateTime::currentDateTime()
        };
    }

    mutable int callCount = 0;
};

class FakeMemoryBreakdownProvider final
    : public QObject,
      public MemoryBreakdownProvider
{
public:
    [[nodiscard]] QList<MemoryBreakdownEntry>
        memoryBreakdown() const override
    {
        return {
            {
                QStringLiteral("Test cache"),
                QStringLiteral("Test feature"),
                4096,
                2,
                QStringLiteral("fixed test payload"),
                true
            }
        };
    }
};
}

class MemoryUsageTests : public QObject
{
    Q_OBJECT

private slots:
    void formatsBytesAndCalculatesBaselineDelta();
    void capsHistoryAndRedactsExportedPaths();
    void boundsLargeEventPayloads();
    void collectsFeatureOwnedAttribution();
    void tracksActiveBackgroundTasks();
    void recordsSlowOperations();
    void monitorDisplaysAttributionAsAPartialEstimate();
    void monitorDisplaysApplicationHealth();
    void monitorDoesNotTakeEditorFocus();
    void platformSnapshotReportsMemoryOnWindows();
};

void MemoryUsageTests::formatsBytesAndCalculatesBaselineDelta()
{
    QCOMPARE(MemoryUsageMetrics::formatBytes(0), QStringLiteral("0 B"));
    QCOMPARE(MemoryUsageMetrics::formatBytes(1024), QStringLiteral("1.00 KiB"));
    QCOMPARE(MemoryUsageMetrics::formatBytes(1536), QStringLiteral("1.50 KiB"));

    const MemoryUsageDelta growth =
        MemoryUsageMetrics::calculateDelta(1536, 1024);
    QCOMPARE(growth.absoluteBytes, qint64(512));
    QVERIFY(growth.percentageAvailable);
    QCOMPARE(growth.percentage, 50.0);

    const MemoryUsageDelta noBaseline =
        MemoryUsageMetrics::calculateDelta(1024, 0);
    QCOMPARE(noBaseline.absoluteBytes, qint64(1024));
    QVERIFY(!noBaseline.percentageAvailable);
}

void MemoryUsageTests::capsHistoryAndRedactsExportedPaths()
{
    MemoryUsageHistory history(2);
    ProcessMemorySnapshot sample;
    sample.isAvailable = true;
    sample.workingSetBytes = 1024;
    sample.capturedAt = QDateTime::currentDateTime();

    history.addSnapshot(sample);
    history.addEvent(
        QStringLiteral("marker"),
        QStringLiteral("before C:\\private\\student.db and /home/user/private.pdf")
        );
    history.addEvent(
        QStringLiteral("pdf-loaded"),
        QStringLiteral("bytes=4096")
        );

    QCOMPARE(history.size(), 2);

    const QByteArray exported = history.toJson().toJson();
    QVERIFY(!exported.contains("student.db"));
    QVERIFY(!exported.contains("private.pdf"));
    QVERIFY(exported.contains("[redacted path]"));
    QVERIFY(exported.contains("pdf-loaded"));
    QVERIFY(exported.contains("bytes=4096"));
}

void MemoryUsageTests::boundsLargeEventPayloads()
{
    MemoryUsageHistory history;
    history.addEvent(
        QStringLiteral("marker"),
        QString(100000, QLatin1Char('x'))
        );

    QCOMPARE(history.size(), 1);
    const QByteArray exported = history.toJson().toJson();
    QVERIFY(exported.size() < 12000);
    QVERIFY(exported.contains("[truncated]"));
}

void MemoryUsageTests::collectsFeatureOwnedAttribution()
{
    FakeMemoryBreakdownProvider provider;
    MemoryUsageDiagnostics::registerMemoryBreakdownProvider(
        &provider,
        &provider
        );

    const QList<MemoryBreakdownEntry> entries =
        MemoryUsageDiagnostics::collectMemoryBreakdown();
    const auto entry = std::find_if(
        entries.cbegin(),
        entries.cend(),
        [](const MemoryBreakdownEntry& value)
        {
            return value.owner == QStringLiteral("Test feature");
        }
        );

    QVERIFY(entry != entries.cend());
    QCOMPARE(entry->retainedBytes, quint64(4096));
    QCOMPARE(entry->itemCount, quint64(2));
    QVERIFY(entry->isEstimated);
}

void MemoryUsageTests::tracksActiveBackgroundTasks()
{
    MemoryUsageDiagnostics::enable();
    const quint64 taskId =
        MemoryUsageDiagnostics::beginBackgroundTask(
            QStringLiteral("Calendar"),
            QStringLiteral("test cache request")
            );
    QVERIFY(taskId > 0);

    const QList<DeveloperBackgroundTask> tasks =
        MemoryUsageDiagnostics::activeBackgroundTasks();
    const auto task = std::find_if(
        tasks.cbegin(),
        tasks.cend(),
        [taskId](const DeveloperBackgroundTask& value)
        {
            return value.id == taskId;
        }
        );
    QVERIFY(task != tasks.cend());
    QCOMPARE(task->category, QStringLiteral("Calendar"));
    QVERIFY(!task->cancellable);

    MemoryUsageDiagnostics::finishBackgroundTask(taskId);
    const QList<DeveloperBackgroundTask> remaining =
        MemoryUsageDiagnostics::activeBackgroundTasks();
    QVERIFY(std::none_of(
        remaining.cbegin(),
        remaining.cend(),
        [taskId](const DeveloperBackgroundTask& value)
        {
            return value.id == taskId;
        }
        ));
}

void MemoryUsageTests::recordsSlowOperations()
{
    MemoryUsageDiagnostics::enable();
    MemoryUsageDiagnostics::history().clear();
    MemoryUsageDiagnostics::recordTimedOperation(
        QStringLiteral("test-operation"),
        QStringLiteral("deterministic test"),
        500,
        500
        );

    const QList<MemoryUsageHistoryEntry>& events =
        MemoryUsageDiagnostics::history().entries();
    QCOMPARE(events.size(), 1);
    QCOMPARE(events.constFirst().eventType, QStringLiteral("slow-operation"));
    QVERIFY(events.constFirst().eventDetail.contains(
        QStringLiteral("elapsedMs=500")
        ));
}

void MemoryUsageTests::monitorDisplaysAttributionAsAPartialEstimate()
{
    FakeMemoryBreakdownProvider provider;
    MemoryUsageDiagnostics::registerMemoryBreakdownProvider(
        &provider,
        &provider
        );

    MemoryUsageDialog dialog(
        nullptr,
        nullptr,
        std::make_unique<FakeSnapshotProvider>()
        );
    dialog.refreshNow();

    const auto* summary = dialog.findChild<QLabel*>(
        QStringLiteral("memoryUsageAttributionSummary")
        );
    QVERIFY(summary);
    QVERIFY(summary->text().contains(QStringLiteral("partial")));
    QVERIFY(summary->text().contains(
        QStringLiteral("Unattributed/shared/runtime")
        ));

    const auto* entries = dialog.findChild<QPlainTextEdit*>(
        QStringLiteral("memoryUsageAttribution")
        );
    QVERIFY(entries);
    QVERIFY(entries->toPlainText().contains(QStringLiteral("Test feature")));
}

void MemoryUsageTests::monitorDisplaysApplicationHealth()
{
    MemoryUsageDialog dialog(
        nullptr,
        nullptr,
        std::make_unique<FakeSnapshotProvider>()
        );
    dialog.refreshNow();

    const auto* health = dialog.findChild<QPlainTextEdit*>(
        QStringLiteral("memoryUsageApplicationHealth")
        );
    QVERIFY(health);
    QVERIFY(health->toPlainText().contains(QStringLiteral("Application version:")));
    QVERIFY(health->toPlainText().contains(QStringLiteral("Database: unavailable")));
    QVERIFY(health->toPlainText().contains(QStringLiteral("Active background tasks:")));
}

void MemoryUsageTests::monitorDoesNotTakeEditorFocus()
{
    QWidget mainWindow;
    auto* layout = new QVBoxLayout(&mainWindow);
    auto* editor = new QLineEdit(&mainWindow);
    editor->setText(QStringLiteral("active editor"));
    layout->addWidget(editor);
    mainWindow.show();
    QTRY_VERIFY(mainWindow.isVisible());
    editor->setFocus();
    QTRY_COMPARE(QApplication::focusWidget(), static_cast<QWidget*>(editor));

    MemoryUsageDialog dialog(
        &mainWindow,
        nullptr,
        std::make_unique<FakeSnapshotProvider>()
        );
    QVERIFY(dialog.windowFlags().testFlag(Qt::Tool));
    QVERIFY(dialog.windowFlags().testFlag(Qt::WindowStaysOnTopHint));
    QVERIFY(dialog.windowFlags().testFlag(Qt::WindowDoesNotAcceptFocus));
    QVERIFY(dialog.testAttribute(Qt::WA_ShowWithoutActivating));

    dialog.show();
    QTRY_VERIFY(dialog.isVisible());
    QCOMPARE(QApplication::focusWidget(), static_cast<QWidget*>(editor));

    auto* refresh = dialog.findChild<QPushButton*>(
        QStringLiteral("memoryUsageResetPeakButton")
        );
    QVERIFY(refresh);
    QTest::mouseClick(refresh, Qt::LeftButton);
    QCOMPARE(QApplication::focusWidget(), static_cast<QWidget*>(editor));

    dialog.hide();
    QTRY_VERIFY(!dialog.isVisible());
    dialog.show();
    QTRY_VERIFY(dialog.isVisible());
    QCOMPARE(QApplication::focusWidget(), static_cast<QWidget*>(editor));
}

void MemoryUsageTests::platformSnapshotReportsMemoryOnWindows()
{
    PlatformProcessMemorySnapshotProvider provider;
    const ProcessMemorySnapshot snapshot = provider.snapshot();

#if defined(Q_OS_WIN)
    QVERIFY(snapshot.isAvailable);
    QVERIFY(snapshot.workingSetBytes > 0);
    QVERIFY(snapshot.privateUsageBytes > 0);
#else
    QVERIFY(!snapshot.isAvailable);
#endif
}

QTEST_MAIN(MemoryUsageTests)

#include "memory_usage_tests.moc"
