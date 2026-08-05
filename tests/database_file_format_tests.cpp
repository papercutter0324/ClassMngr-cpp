#include "app/startup_database_path.h"
#include "core/database_file_format.h"

#include <QTest>

class DatabaseFileFormatTests : public QObject
{
    Q_OBJECT

private slots:
    void nativeOutputPathAppendsExtension();
    void nativeOutputPathPreservesNativeExtension();
    void nativeOutputPathMigratesLegacyExtension();
    void supportedInputPathPreservesKnownExtensions();
    void supportedInputPathCompletesExtensionlessPath();
    void supportedInputPathPreservesUnknownExtension();
    void startupDatabasePathFindsNativeFile();
    void startupDatabasePathAcceptsLegacyFile();
    void startupDatabasePathIgnoresOptionValues();
};

void DatabaseFileFormatTests::nativeOutputPathAppendsExtension()
{
    QCOMPARE(
        DatabaseFileFormat::nativeOutputPath(
            QStringLiteral("school")
            ),
        QStringLiteral("school.tps")
        );
}

void DatabaseFileFormatTests::nativeOutputPathPreservesNativeExtension()
{
    QCOMPARE(
        DatabaseFileFormat::nativeOutputPath(
            QStringLiteral("school.TPS")
            ),
        QStringLiteral("school.TPS")
        );
}

void DatabaseFileFormatTests::nativeOutputPathMigratesLegacyExtension()
{
    QCOMPARE(
        DatabaseFileFormat::nativeOutputPath(
            QStringLiteral("school.DB")
            ),
        QStringLiteral("school.tps")
        );
}

void DatabaseFileFormatTests::supportedInputPathPreservesKnownExtensions()
{
    QCOMPARE(
        DatabaseFileFormat::supportedInputPath(
            QStringLiteral("school.tps")
            ),
        QStringLiteral("school.tps")
        );

    QCOMPARE(
        DatabaseFileFormat::supportedInputPath(
            QStringLiteral("legacy.db")
            ),
        QStringLiteral("legacy.db")
        );
}

void DatabaseFileFormatTests::supportedInputPathCompletesExtensionlessPath()
{
    QCOMPARE(
        DatabaseFileFormat::supportedInputPath(
            QStringLiteral("school")
            ),
        QStringLiteral("school.tps")
        );
}

void DatabaseFileFormatTests::supportedInputPathPreservesUnknownExtension()
{
    QCOMPARE(
        DatabaseFileFormat::supportedInputPath(
            QStringLiteral("school.sqlite")
            ),
        QStringLiteral("school.sqlite")
        );
}

void DatabaseFileFormatTests::startupDatabasePathFindsNativeFile()
{
    QCOMPARE(
        startupDatabasePath(
            {
                QStringLiteral("ClassMngr.exe"),
                QStringLiteral("C:/School Data/current.tps")
            }
            ),
        QStringLiteral("C:/School Data/current.tps")
        );
}

void DatabaseFileFormatTests::startupDatabasePathAcceptsLegacyFile()
{
    QCOMPARE(
        startupDatabasePath(
            {
                QStringLiteral("ClassMngr.exe"),
                QStringLiteral("C:/School Data/legacy.DB")
            }
            ),
        QStringLiteral("C:/School Data/legacy.DB")
        );
}

void DatabaseFileFormatTests::startupDatabasePathIgnoresOptionValues()
{
    QCOMPARE(
        startupDatabasePath(
            {
                QStringLiteral("ClassMngr.exe"),
                QStringLiteral("--startup-performance-test"),
                QStringLiteral("--startup-performance-output"),
                QStringLiteral("metrics.tps"),
                QStringLiteral("current.tps")
            }
            ),
        QStringLiteral("current.tps")
        );
}

QTEST_APPLESS_MAIN(DatabaseFileFormatTests)

#include "database_file_format_tests.moc"
