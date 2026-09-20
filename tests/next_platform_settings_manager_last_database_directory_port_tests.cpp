#include "core/settingsmanager.h"
#include "next/application/last_database_directory_port.h"
#include "next/platform/settings_manager_last_database_directory_port.h"

#include <QDir>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <cstddef>
#include <string>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Platform;

namespace
{

QString directoryKey()
{
    return QString::fromUtf8(
        SettingsManager::Keys::LAST_DATABASE_DIRECTORY
        );
}

std::string utf8(
    const QString& value
    )
{
    const QByteArray encoded = value.toUtf8();
    return std::string(
        encoded.constData(),
        static_cast<std::size_t>(encoded.size())
        );
}

QString fromUtf8(
    const std::string& value
    )
{
    return QString::fromUtf8(
        value.data(),
        static_cast<qsizetype>(value.size())
        );
}

} // namespace

class NextPlatformSettingsManagerLastDatabaseDirectoryPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void usesTheExactCanonicalLegacyKey();
    void missingAndEmptyValuesReadAsEmpty();
    void roundTripsUnicodeUtf8Paths();
    void persistsThroughTheSynchronizedLegacySetter();

private:
    QTemporaryDir m_settingsRoot;
};

void NextPlatformSettingsManagerLastDatabaseDirectoryPortTests::
initTestCase()
{
    QVERIFY(m_settingsRoot.isValid());
    QVERIFY(
        qputenv(
            "CLASSMNGR_SETTINGS_ROOT",
            m_settingsRoot.path().toUtf8()
            )
        );

    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void NextPlatformSettingsManagerLastDatabaseDirectoryPortTests::cleanup()
{
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void NextPlatformSettingsManagerLastDatabaseDirectoryPortTests::
usesTheExactCanonicalLegacyKey()
{
    QCOMPARE(
        directoryKey(),
        QStringLiteral("files/lastDirectory")
        );

    SettingsManager& settings = SettingsManager::instance();
    settings.set(directoryKey(), QStringLiteral("legacy-directory"));

    const SettingsManagerLastDatabaseDirectoryPort port;
    QCOMPARE(
        fromUtf8(port.read()),
        QStringLiteral("legacy-directory")
        );
}

void NextPlatformSettingsManagerLastDatabaseDirectoryPortTests::
missingAndEmptyValuesReadAsEmpty()
{
    SettingsManager& settings = SettingsManager::instance();
    settings.remove(directoryKey());

    const SettingsManagerLastDatabaseDirectoryPort port;
    QVERIFY(port.read().empty());

    settings.set(directoryKey(), QString());
    QVERIFY(port.read().empty());

    settings.set(directoryKey(), QVariant());
    QVERIFY(port.read().empty());
}

void NextPlatformSettingsManagerLastDatabaseDirectoryPortTests::
roundTripsUnicodeUtf8Paths()
{
    const QString expectedPath = QString::fromUtf8(
        "profiles/\xED\x95\x99\xEA\xB5\x90-\xF0\x9F\x93\x9A/database"
        );

    const SettingsManagerLastDatabaseDirectoryPort port;
    port.write(utf8(expectedPath));

    QCOMPARE(fromUtf8(port.read()), expectedPath);
    QCOMPARE(
        SettingsManager::instance()
            .get(directoryKey())
            .toString(),
        expectedPath
        );
}

void NextPlatformSettingsManagerLastDatabaseDirectoryPortTests::
persistsThroughTheSynchronizedLegacySetter()
{
    const QString expectedPath =
        QStringLiteral("C:/persisted/database-directory");

    const SettingsManagerLastDatabaseDirectoryPort port;
    port.write(utf8(expectedPath));

    QSettings persistedSettings(
        QDir(m_settingsRoot.path()).filePath(
            QStringLiteral("PaperCloud/ClassMngr.ini")
            ),
        QSettings::IniFormat
        );
    persistedSettings.sync();

    QCOMPARE(
        persistedSettings.value(directoryKey()).toString(),
        expectedPath
        );
}

QTEST_APPLESS_MAIN(NextPlatformSettingsManagerLastDatabaseDirectoryPortTests)

#include "next_platform_settings_manager_last_database_directory_port_tests.moc"
