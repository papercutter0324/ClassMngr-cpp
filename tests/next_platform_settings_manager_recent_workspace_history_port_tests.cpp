#include "core/settingsmanager.h"
#include "next/application/recent_workspace_history.h"
#include "next/platform/settings_manager_recent_workspace_history_port.h"

#include <QByteArray>
#include <QTemporaryDir>
#include <QStringList>
#include <QtTest/QtTest>
#include <QVariant>

#include <cstddef>
#include <string>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Platform;

namespace
{

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

QString recentFilesKey()
{
    return QString::fromUtf8(SettingsManager::Keys::RECENT_FILES);
}

QString lastFileKey()
{
    return QString::fromUtf8(SettingsManager::Keys::LAST_FILE);
}

QString lastDirectoryKey()
{
    return QString::fromUtf8(
        SettingsManager::Keys::LAST_DATABASE_DIRECTORY
        );
}

} // namespace

class NextPlatformSettingsManagerRecentWorkspaceHistoryPortTests final
    : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void missingOrUnavailableSettingsUseEmptyFallback();
    void loadPreservesLegacyOrderingAndUnicode();
    void saveRoundTripsTypedHistoryAndLastPath();
    void mostRecentPathUsesListBeforeLastFileFallback();
    void clearClearsRecentAndLastFileWithoutTouchingDirectory();

private:
    QTemporaryDir m_settingsRoot;
};

void NextPlatformSettingsManagerRecentWorkspaceHistoryPortTests::
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

void NextPlatformSettingsManagerRecentWorkspaceHistoryPortTests::cleanup()
{
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void NextPlatformSettingsManagerRecentWorkspaceHistoryPortTests::
missingOrUnavailableSettingsUseEmptyFallback()
{
    SettingsManager& settings = SettingsManager::instance();
    settings.remove(recentFilesKey());
    settings.remove(lastFileKey());

    const SettingsManagerRecentWorkspaceHistoryPort port;
    const RecentWorkspaceHistory missing = port.load();
    QVERIFY(missing.paths.empty());
    QVERIFY(!missing.lastPath.has_value());
    QVERIFY(!missing.mostRecentDatabasePath().has_value());

    settings.set(recentFilesKey(), QVariant());
    settings.set(lastFileKey(), QVariant());
    const RecentWorkspaceHistory unavailable = port.load();
    QVERIFY(unavailable.paths.empty());
    QVERIFY(!unavailable.lastPath.has_value());
}

void NextPlatformSettingsManagerRecentWorkspaceHistoryPortTests::
loadPreservesLegacyOrderingAndUnicode()
{
    const QString unicodePath = QString::fromUtf8(
        "profiles/\xED\x95\x99\xEA\xB5\x90-\xF0\x9F\x93\x9A.tps"
        );
    const QString secondPath = QStringLiteral("profiles/second.tps");
    SettingsManager& settings = SettingsManager::instance();
    settings.setRecentFiles({unicodePath, secondPath});
    settings.setLastFile(unicodePath);

    const SettingsManagerRecentWorkspaceHistoryPort port;
    const RecentWorkspaceHistory loaded = port.load();
    const RecentWorkspaceHistory expected{
        .paths = {
            RecentWorkspacePath(utf8(unicodePath)),
            RecentWorkspacePath(utf8(secondPath))
        },
        .lastPath = RecentWorkspacePath(utf8(unicodePath))
    };

    QVERIFY(loaded == expected);
}

void NextPlatformSettingsManagerRecentWorkspaceHistoryPortTests::
saveRoundTripsTypedHistoryAndLastPath()
{
    const QString unicodePath = QString::fromUtf8(
        "profiles/\xED\x95\x99\xEA\xB5\x90-\xF0\x9F\x93\x9A.tps"
        );
    const QString secondPath = QStringLiteral("profiles/second.tps");
    const RecentWorkspaceHistory expected{
        .paths = {
            RecentWorkspacePath(utf8(unicodePath)),
            RecentWorkspacePath(utf8(secondPath))
        },
        .lastPath = RecentWorkspacePath(utf8(unicodePath))
    };

    const SettingsManagerRecentWorkspaceHistoryPort port;
    port.save(expected);

    SettingsManager& settings = SettingsManager::instance();
    QCOMPARE(
        settings.getRecentFiles(),
        (QStringList{unicodePath, secondPath})
        );
    QCOMPARE(settings.getLastFile(), unicodePath);
    QVERIFY(port.load() == expected);
}

void NextPlatformSettingsManagerRecentWorkspaceHistoryPortTests::
mostRecentPathUsesListBeforeLastFileFallback()
{
    const SettingsManagerRecentWorkspaceHistoryPort port;
    const RecentWorkspacePath listPath(
        std::string("list-first.tps")
        );
    const RecentWorkspacePath lastPath(
        std::string("last-fallback.tps")
        );

    port.save({
        .paths = {listPath},
        .lastPath = lastPath
    });
    const RecentWorkspaceHistory withList = port.load();
    QVERIFY(
        withList.mostRecentDatabasePath().has_value()
        && *withList.mostRecentDatabasePath() == listPath
        );

    port.save({
        .paths = {},
        .lastPath = lastPath
    });
    const RecentWorkspaceHistory withFallback = port.load();
    QVERIFY(
        withFallback.mostRecentDatabasePath().has_value()
        && *withFallback.mostRecentDatabasePath() == lastPath
        );
}

void NextPlatformSettingsManagerRecentWorkspaceHistoryPortTests::
clearClearsRecentAndLastFileWithoutTouchingDirectory()
{
    SettingsManager& settings = SettingsManager::instance();
    const QString directory = QStringLiteral("/kept/database-directory");
    settings.set(lastDirectoryKey(), directory);

    const SettingsManagerRecentWorkspaceHistoryPort port;
    port.save({
        .paths = {RecentWorkspacePath(std::string("recent.tps"))},
        .lastPath = RecentWorkspacePath(std::string("recent.tps"))
    });
    port.clear();

    const RecentWorkspaceHistory cleared = port.load();
    QVERIFY(cleared.paths.empty());
    QVERIFY(!cleared.lastPath.has_value());
    QVERIFY(settings.get(recentFilesKey()).toStringList().isEmpty());
    QVERIFY(settings.get(lastFileKey()).toString().isEmpty());
    QCOMPARE(settings.get(lastDirectoryKey()).toString(), directory);
}

QTEST_APPLESS_MAIN(NextPlatformSettingsManagerRecentWorkspaceHistoryPortTests)

#include "next_platform_settings_manager_recent_workspace_history_port_tests.moc"
