#include "core/process_memory_snapshot.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

namespace
{
bool writeFile(const QString& path, const QByteArray& contents)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Text)
        && file.write(contents) == contents.size();
}
}

class ProcessMemorySnapshotTests : public QObject
{
    Q_OBJECT

private slots:
    void readsLinuxProcMetricsInBytes();
    void fallsBackWhenSmapsRollupIsUnavailable();
    void reportsUnavailableWhenRssCannotBeRead();
    void readsLiveLinuxProcfsPseudoFiles();
};

void ProcessMemorySnapshotTests::readsLinuxProcMetricsInBytes()
{
    QTemporaryDir procRoot;
    QVERIFY(procRoot.isValid());

    const QString selfDirectory =
        QDir(procRoot.path()).filePath(QStringLiteral("self"));
    QVERIFY(QDir().mkpath(selfDirectory));
    QVERIFY(writeFile(
        QDir(selfDirectory).filePath(QStringLiteral("status")),
        QByteArrayLiteral(
            "Name:\tClassMngrTest\n"
            "VmRSS:\t 42 kB\n"
            "VmHWM:\t 50 kB\n"
            "RssAnon:\t 12 kB\n"
            )
        ));
    QVERIFY(writeFile(
        QDir(selfDirectory).filePath(QStringLiteral("smaps_rollup")),
        QByteArrayLiteral(
            "Pss:\t 20 kB\n"
            "Private_Clean:\t 3 kB\n"
            "Private_Dirty:\t 5 kB\n"
            )
        ));

    const ProcessMemorySnapshot snapshot =
        PlatformProcessMemorySnapshotProvider(procRoot.path()).snapshot();

    QVERIFY(snapshot.isAvailable);
    QCOMPARE(snapshot.platform, QStringLiteral("linux"));
    QCOMPARE(snapshot.workingSetBytes, quint64(42 * 1024));
    QCOMPARE(snapshot.peakWorkingSetBytes, quint64(50 * 1024));
    QCOMPARE(snapshot.privateWorkingSetBytes, quint64(12 * 1024));
    QCOMPARE(snapshot.privateUsageBytes, quint64(20 * 1024));
    QCOMPARE(snapshot.privateDirtyBytes, quint64(5 * 1024));
    QCOMPARE(snapshot.pagefileUsageBytes, quint64(8 * 1024));
}

void ProcessMemorySnapshotTests::fallsBackWhenSmapsRollupIsUnavailable()
{
    QTemporaryDir procRoot;
    QVERIFY(procRoot.isValid());

    const QString selfDirectory =
        QDir(procRoot.path()).filePath(QStringLiteral("self"));
    QVERIFY(QDir().mkpath(selfDirectory));
    QVERIFY(writeFile(
        QDir(selfDirectory).filePath(QStringLiteral("status")),
        QByteArrayLiteral(
            "VmRSS:\t 40 kB\n"
            "VmHWM:\t 48 kB\n"
            "RssAnon:\t 14 kB\n"
            )
        ));

    const ProcessMemorySnapshot snapshot =
        PlatformProcessMemorySnapshotProvider(procRoot.path()).snapshot();

    QVERIFY(snapshot.isAvailable);
    QCOMPARE(snapshot.workingSetBytes, quint64(40 * 1024));
    QCOMPARE(snapshot.peakWorkingSetBytes, quint64(48 * 1024));
    QCOMPARE(snapshot.privateWorkingSetBytes, quint64(14 * 1024));
    QCOMPARE(snapshot.privateUsageBytes, quint64(14 * 1024));
}

void ProcessMemorySnapshotTests::reportsUnavailableWhenRssCannotBeRead()
{
    QTemporaryDir procRoot;
    QVERIFY(procRoot.isValid());

    const QString selfDirectory =
        QDir(procRoot.path()).filePath(QStringLiteral("self"));
    QVERIFY(QDir().mkpath(selfDirectory));
    QVERIFY(writeFile(
        QDir(selfDirectory).filePath(QStringLiteral("status")),
        QByteArrayLiteral("Name:\tClassMngrTest\nVmHWM:\t 50 kB\n")
        ));

    const ProcessMemorySnapshot snapshot =
        PlatformProcessMemorySnapshotProvider(procRoot.path()).snapshot();

    QVERIFY(!snapshot.isAvailable);
    QCOMPARE(snapshot.platform, QStringLiteral("linux"));
    QCOMPARE(snapshot.workingSetBytes, quint64(0));
    QCOMPARE(snapshot.peakWorkingSetBytes, quint64(0));
    QCOMPARE(snapshot.privateUsageBytes, quint64(0));
}

void ProcessMemorySnapshotTests::readsLiveLinuxProcfsPseudoFiles()
{
    const ProcessMemorySnapshot snapshot =
        PlatformProcessMemorySnapshotProvider().snapshot();

    QVERIFY(snapshot.isAvailable);
    QCOMPARE(snapshot.platform, QStringLiteral("linux"));
    QVERIFY(snapshot.workingSetBytes > 0);
    QVERIFY(snapshot.privateUsageBytes > 0);
    QVERIFY(
        snapshot.peakWorkingSetBytes >= snapshot.workingSetBytes
        );
}

QTEST_GUILESS_MAIN(ProcessMemorySnapshotTests)

#include "process_memory_snapshot_tests.moc"
