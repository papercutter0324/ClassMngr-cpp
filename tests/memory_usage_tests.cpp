#include "core/memory_usage_diagnostics.h"
#include "core/process_memory_snapshot.h"
#include "ui/shared/dialogs/memory_usage_dialog.h"

#include <QApplication>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QtTest>

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
}

class MemoryUsageTests : public QObject
{
    Q_OBJECT

private slots:
    void formatsBytesAndCalculatesBaselineDelta();
    void capsHistoryAndRedactsExportedPaths();
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
