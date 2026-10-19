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
    void failedWriteRestoresExactPreviousValuePreservesOtherSettingsAndRetries();
    void failedWriteRestoresAbsentKeyAndAllowsRetry();

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
        fromUtf8(port.read().value()),
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
        "profiles/\xED\x95\x99\xEA\xB5\x90/../\xEC\x9E\x90\xEB\xA3\x8C//"
        "\xF0\x9F\x93\x9A/database/"
        );

    const SettingsManagerLastDatabaseDirectoryPort port;
    const auto written = port.write(DirectoryPreferencePath(utf8(expectedPath)));
    QVERIFY(written);

    QCOMPARE(fromUtf8(port.read().value()), expectedPath);
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
    const auto written = port.write(DirectoryPreferencePath(utf8(expectedPath)));
    QVERIFY(written);

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

void NextPlatformSettingsManagerLastDatabaseDirectoryPortTests::
failedWriteRestoresExactPreviousValuePreservesOtherSettingsAndRetries()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString key = directoryKey();
    const QString unrelatedKey = QStringLiteral("files/unrelatedSetting");
    const QString recentFilesKey = QString::fromUtf8(
        SettingsManager::Keys::RECENT_FILES
        );
    const QString lastFileKey = QString::fromUtf8(
        SettingsManager::Keys::LAST_FILE
        );
    const QVariant previousValue = QVariant::fromValue(
        QByteArray("prior\0value", 11)
        );
    const QVariant unrelatedValue = QStringLiteral("leave-unchanged");
    const QVariant recentFilesValue = QStringList{
        QStringLiteral("legacy-first.tps"),
        QStringLiteral("legacy-second.tps")
    };
    const QVariant lastFileValue = QStringLiteral("legacy-last.tps");
    settings.set(key, previousValue);
    settings.set(unrelatedKey, unrelatedValue);
    settings.set(recentFilesKey, recentFilesValue);
    settings.set(lastFileKey, lastFileValue);
    settings.sync();

    bool injectAccessError = true;
    int synchronizationAttempts = 0;
    const SettingsManagerLastDatabaseDirectoryPort faultablePort(
        [&injectAccessError, &synchronizationAttempts](QSettings& target)
        {
            ++synchronizationAttempts;
            if (injectAccessError)
            {
                return QSettings::AccessError;
            }

            target.sync();
            return target.status();
        }
        );

    const QString replacementPath =
        QStringLiteral("C:/replacement/database-directory");
    const auto failedWrite = faultablePort.write(
        DirectoryPreferencePath(utf8(replacementPath))
        );
    QVERIFY(!failedWrite);
    QCOMPARE(failedWrite.error().code, Domain::ErrorCode::Technical);
    QVERIFY(failedWrite.error().recoverable);
    QVERIFY(!failedWrite.error().message.empty());
    QCOMPARE(synchronizationAttempts, 1);
    QCOMPARE(settings.get(key), previousValue);
    QCOMPARE(
        settings.get(key).metaType().id(),
        previousValue.metaType().id()
        );
    QCOMPARE(settings.get(unrelatedKey), unrelatedValue);
    QCOMPARE(settings.get(recentFilesKey), recentFilesValue);
    QCOMPARE(settings.get(lastFileKey), lastFileValue);

    QSettings persistedSettings(
        m_settingsRoot.filePath(QStringLiteral("PaperCloud/ClassMngr.ini")),
        QSettings::IniFormat
        );
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QCOMPARE(persistedSettings.value(key), previousValue);
    QCOMPARE(
        persistedSettings.value(key).metaType().id(),
        previousValue.metaType().id()
        );
    QCOMPARE(persistedSettings.value(unrelatedKey), unrelatedValue);
    QCOMPARE(persistedSettings.value(recentFilesKey), recentFilesValue);
    QCOMPARE(persistedSettings.value(lastFileKey), lastFileValue);

    injectAccessError = false;
    const auto retry = faultablePort.write(
        DirectoryPreferencePath(utf8(replacementPath))
        );
    QVERIFY(retry);
    QCOMPARE(synchronizationAttempts, 2);
    QCOMPARE(settings.get(key), QVariant(replacementPath));
    QCOMPARE(settings.get(unrelatedKey), unrelatedValue);
    QCOMPARE(settings.get(recentFilesKey), recentFilesValue);
    QCOMPARE(settings.get(lastFileKey), lastFileValue);

    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QCOMPARE(persistedSettings.value(key), QVariant(replacementPath));
    QCOMPARE(persistedSettings.value(unrelatedKey), unrelatedValue);
    QCOMPARE(persistedSettings.value(recentFilesKey), recentFilesValue);
    QCOMPARE(persistedSettings.value(lastFileKey), lastFileValue);
}

void NextPlatformSettingsManagerLastDatabaseDirectoryPortTests::
failedWriteRestoresAbsentKeyAndAllowsRetry()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString key = directoryKey();
    const QString unrelatedKey = QStringLiteral("files/unrelatedSetting");
    const QString recentFilesKey = QString::fromUtf8(
        SettingsManager::Keys::RECENT_FILES
        );
    const QString lastFileKey = QString::fromUtf8(
        SettingsManager::Keys::LAST_FILE
        );
    const QVariant unrelatedValue = 73;
    const QVariant recentFilesValue = QStringList{
        QStringLiteral("legacy-first.tps"),
        QStringLiteral("legacy-second.tps")
    };
    const QVariant lastFileValue = QStringLiteral("legacy-last.tps");
    settings.remove(key);
    settings.set(unrelatedKey, unrelatedValue);
    settings.set(recentFilesKey, recentFilesValue);
    settings.set(lastFileKey, lastFileValue);
    settings.sync();

    bool injectAccessError = true;
    int synchronizationAttempts = 0;
    const SettingsManagerLastDatabaseDirectoryPort faultablePort(
        [&injectAccessError, &synchronizationAttempts](QSettings& target)
        {
            ++synchronizationAttempts;
            if (injectAccessError)
            {
                return QSettings::AccessError;
            }

            target.sync();
            return target.status();
        }
        );

    const QString replacementPath =
        QStringLiteral("C:/new/database-directory");
    const auto failedWrite = faultablePort.write(
        DirectoryPreferencePath(utf8(replacementPath))
        );
    QVERIFY(!failedWrite);
    QCOMPARE(failedWrite.error().code, Domain::ErrorCode::Technical);
    QVERIFY(failedWrite.error().recoverable);
    QVERIFY(!failedWrite.error().message.empty());
    QCOMPARE(synchronizationAttempts, 1);
    QVERIFY(!settings.get(key).isValid());
    QVERIFY(faultablePort.read().empty());
    QCOMPARE(settings.get(unrelatedKey), unrelatedValue);
    QCOMPARE(settings.get(recentFilesKey), recentFilesValue);
    QCOMPARE(settings.get(lastFileKey), lastFileValue);

    QSettings persistedSettings(
        m_settingsRoot.filePath(QStringLiteral("PaperCloud/ClassMngr.ini")),
        QSettings::IniFormat
        );
    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QVERIFY(!persistedSettings.contains(key));
    QCOMPARE(persistedSettings.value(unrelatedKey), unrelatedValue);
    QCOMPARE(persistedSettings.value(recentFilesKey), recentFilesValue);
    QCOMPARE(persistedSettings.value(lastFileKey), lastFileValue);

    injectAccessError = false;
    const auto retry = faultablePort.write(
        DirectoryPreferencePath(utf8(replacementPath))
        );
    QVERIFY(retry);
    QCOMPARE(synchronizationAttempts, 2);
    QCOMPARE(settings.get(key), QVariant(replacementPath));
    QCOMPARE(settings.get(unrelatedKey), unrelatedValue);
    QCOMPARE(settings.get(recentFilesKey), recentFilesValue);
    QCOMPARE(settings.get(lastFileKey), lastFileValue);

    persistedSettings.sync();
    QCOMPARE(persistedSettings.status(), QSettings::NoError);
    QCOMPARE(persistedSettings.value(key), QVariant(replacementPath));
    QCOMPARE(persistedSettings.value(unrelatedKey), unrelatedValue);
    QCOMPARE(persistedSettings.value(recentFilesKey), recentFilesValue);
    QCOMPARE(persistedSettings.value(lastFileKey), lastFileValue);
}

QTEST_APPLESS_MAIN(NextPlatformSettingsManagerLastDatabaseDirectoryPortTests)

#include "next_platform_settings_manager_last_database_directory_port_tests.moc"
