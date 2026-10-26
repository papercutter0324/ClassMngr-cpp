#include "next/platform/settings_manager_preferences_port.h"

#include <QSettings>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace ClassMngr::Next;
using namespace ClassMngr::Next::Application;
using namespace ClassMngr::Next::Platform;

class SettingsManagerPreferencesTests final : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        QVERIFY(m_root.isValid());
        QVERIFY(qputenv("CLASSMNGR_SETTINGS_ROOT", m_root.path().toUtf8()));
    }
    void cleanup()
    {
        SettingsManager::instance().clear();
        SettingsManager::instance().sync();
    }
    void missingAndInvalidValuesUseExistingFallbacks()
    {
        SettingsManagerPreferencesPort port;
        auto result = port.read();
        QVERIFY(result);
        QCOMPARE(result.value(), UserPreferencesState{});
        auto& settings = SettingsManager::instance();
        settings.set(QString::fromUtf8(OptionKeys::Theme), QStringLiteral("invalid"));
        settings.set(QString::fromUtf8(OptionKeys::FontSize), 37);
        settings.set(QString::fromUtf8(SettingsManager::Keys::LANGUAGE_PREFERENCE), 99);
        settings.set(QString::fromUtf8(SettingsManager::Keys::LAST_CAMPUS_JSON_ID), QStringLiteral("  "));
        settings.set(QString::fromUtf8(SettingsManager::Keys::EXCEL_IMPORT_TIMEOUT_SECONDS), 37);
        result = port.read();
        QVERIFY(result);
        QCOMPARE(result.value(), UserPreferencesState{});
        // Legacy language values 2/3/4 migrate to English; Korean stays 5.
        settings.set(QString::fromUtf8(SettingsManager::Keys::LANGUAGE_PREFERENCE), 3);
        result = port.read();
        QVERIFY(result);
        QCOMPARE(result.value().snapshot().languagePreference(), LanguagePreference::English);
        QCOMPARE(settings.get(QString::fromUtf8(SettingsManager::Keys::LANGUAGE_PREFERENCE)).toInt(), 1);
    }
    void laterFieldFailureLeavesEarlierSettingsVisibleAndAllowsRetry()
    {
        SettingsManagerThemePreferencesPort theme;
        SettingsManagerLanguagePreferencesPort language;
        SettingsManagerFontSizePreferencesPort font;
        SettingsManagerSidebarDisplayPreferencesPort sidebar;
        SettingsManagerShowAllTeachersPreferencesPort teachers;
        SettingsManagerPowerPointDataAccessNoticePort powerpoint;
        SettingsManagerAutomaticUpdatePreferencesPort updates;
        bool fail = true;
        SettingsManagerExcelImportTimeoutPort timeout([&](QSettings& settings) {
            if (fail) return QSettings::AccessError;
            settings.sync();
            return settings.status();
        });
        SettingsManagerLastSelectedCampusPort campus;
        PreferencesSnapshotAdapter adapter({theme, language, font, sidebar, teachers,
            powerpoint, updates, timeout, campus});
        UserPreferencesState candidate;
        QVERIFY(candidate.setThemePreference(ThemePreference::Dark));
        QVERIFY(candidate.setFontSize(FontSize::Large));
        QVERIFY(candidate.setExcelImportTimeoutSeconds(300));
        QVERIFY(candidate.setLastSelectedCampusId(Domain::CampusId::fromString("later-campus")));
        const auto failed = adapter.persist(candidate.snapshot());
        QVERIFY(!failed);
        QCOMPARE(failed.error().code, Domain::ErrorCode::Technical);
        QVERIFY(failed.error().message.find("Excel import timeout") != std::string::npos);
        QVERIFY(failed.error().recoverable);
        const auto partial = adapter.read();
        QVERIFY(partial);
        QCOMPARE(partial.value().snapshot().themePreference(), ThemePreference::Dark);
        QCOMPARE(partial.value().snapshot().fontSize(), FontSize::Large);
        QCOMPARE(partial.value().snapshot().excelImportTimeoutSeconds(), 120);
        QVERIFY(!partial.value().snapshot().lastSelectedCampusId());
        fail = false;
        QVERIFY(adapter.persist(candidate.snapshot()));
        const auto retried = adapter.read();
        QVERIFY(retried);
        QCOMPARE(retried.value(), candidate);
    }
    void fullSnapshotRoundTripsThroughCanonicalKeys()
    {
        SettingsManagerPreferencesPort port;
        UserPreferencesState state;
        QVERIFY(state.setThemePreference(ThemePreference::Dark));
        QVERIFY(state.setLanguagePreference(LanguagePreference::Korean));
        QVERIFY(state.setFontSize(FontSize::Small));
        QVERIFY(state.setSidebarTooltipsEnabled(false));
        QVERIFY(state.setSidebarMarqueeEnabled(false));
        QVERIFY(state.setShowAllTeachers(false));
        QVERIFY(state.setShowPowerPointDataAccessNotice(false));
        QVERIFY(state.setAutomaticUpdateChecksEnabled(false));
        QVERIFY(state.setExcelImportTimeoutSeconds(300));
        QVERIFY(state.setLastSelectedCampusId(Domain::CampusId::fromString("campus-two")));
        QVERIFY(port.persist(state.snapshot()));
        const auto result = port.read();
        QVERIFY(result);
        QCOMPARE(result.value(), state);
        auto& settings = SettingsManager::instance();
        QCOMPARE(settings.get(QString::fromUtf8(OptionKeys::Theme)).toInt(), 0);
        QCOMPARE(settings.get(QString::fromUtf8(OptionKeys::FontSize)).toInt(), -2);
        QCOMPARE(settings.get(QString::fromUtf8(SettingsManager::Keys::LANGUAGE_PREFERENCE)).toInt(), 5);
        QVERIFY(state.clearLastSelectedCampusId());
        QVERIFY(port.persist(state.snapshot()));
        QVERIFY(!port.read().value().snapshot().lastSelectedCampusId());
    }
private:
    QTemporaryDir m_root;
};
QTEST_GUILESS_MAIN(SettingsManagerPreferencesTests)
#include "next_platform_settings_manager_preferences_port_tests.moc"
