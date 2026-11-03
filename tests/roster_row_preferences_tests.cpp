#include "core/settingsmanager.h"
#include "next/application/roster_row_display_policy.h"

#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace ClassMngr::Next::Application;

class RosterRowPreferencesTests final : public QObject
{
    Q_OBJECT

private slots:
    void settingsDefaultRangeAndInvalidFallback();
    void largeMaximumConfirmationUsesSavedValue();
    void displayPoliciesCalculateAndExtendWithinMaximum();
};

void RosterRowPreferencesTests::settingsDefaultRangeAndInvalidFallback()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    qputenv("CLASSMNGR_SETTINGS_ROOT", directory.path().toUtf8());
    auto& settings = SettingsManager::instance();
    settings.clear();
    settings.sync();

    QCOMPARE(settings.maximumRosterRows(), DefaultMaximumRosterRows);
    QCOMPARE(settings.rosterRowDisplayMode(),
        static_cast<int>(RosterRowDisplayMode::AllRows));
    QVERIFY(settings.writeMaximumRosterRowsPreference(100));
    QCOMPARE(settings.maximumRosterRows(), 100);
    QVERIFY(settings.writeRosterRowDisplayModePreference(
        static_cast<int>(RosterRowDisplayMode::Extra20)));
    QCOMPARE(settings.rosterRowDisplayMode(),
        static_cast<int>(RosterRowDisplayMode::Extra20));
    QVERIFY(!settings.writeMaximumRosterRowsPreference(0));
    QVERIFY(!settings.writeMaximumRosterRowsPreference(101));

    settings.set(QString::fromUtf8(
        SettingsManager::Keys::MAXIMUM_ROSTER_ROWS), 0);
    settings.set(QString::fromUtf8(
        SettingsManager::Keys::ROSTER_ROW_DISPLAY_MODE), 99);
    settings.sync();
    QCOMPARE(settings.maximumRosterRows(), DefaultMaximumRosterRows);
    QCOMPARE(settings.rosterRowDisplayMode(),
        static_cast<int>(RosterRowDisplayMode::AllRows));

    settings.clear();
    settings.sync();
}

void RosterRowPreferencesTests::largeMaximumConfirmationUsesSavedValue()
{
    QVERIFY(!shouldConfirmMaximumRosterRowsChange(25, 40));
    QVERIFY(shouldConfirmMaximumRosterRowsChange(25, 41));
    QVERIFY(shouldConfirmMaximumRosterRowsChange(1, 100));
    QVERIFY(!shouldConfirmMaximumRosterRowsChange(26, 100));
    QVERIFY(!shouldConfirmMaximumRosterRowsChange(50, 100));
    QVERIFY(!shouldConfirmMaximumRosterRowsChange(25, 0));
    QVERIFY(!shouldConfirmMaximumRosterRowsChange(101, 100));
}

void RosterRowPreferencesTests::displayPoliciesCalculateAndExtendWithinMaximum()
{
    QCOMPARE(visibleRosterRowCount(25, RosterRowDisplayMode::AllRows, 0), 25);
    QCOMPARE(visibleRosterRowCount(25, RosterRowDisplayMode::Extra5, 0), 5);
    QCOMPARE(visibleRosterRowCount(25, RosterRowDisplayMode::Extra5, 5), 10);
    QCOMPARE(visibleRosterRowCount(25, RosterRowDisplayMode::Extra5, 10), 15);
    QCOMPARE(visibleRosterRowCount(12, RosterRowDisplayMode::Extra25, 0), 12);
    QCOMPARE(visibleRosterRowCount(100, RosterRowDisplayMode::Extra25, 90), 100);
    QCOMPARE(visibleRosterRowCount(1, RosterRowDisplayMode::Extra5, 0), 1);
}

QTEST_GUILESS_MAIN(RosterRowPreferencesTests)
#include "roster_row_preferences_tests.moc"
