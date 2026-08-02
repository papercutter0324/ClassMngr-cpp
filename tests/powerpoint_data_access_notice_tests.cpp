#include "core/settingsmanager.h"
#include "ui/shared/actions/action_registry.h"

#include <QtTest>

#include <QTemporaryDir>

class PowerPointDataAccessNoticeTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void noticeActionDefaultsToEnabledAndPersists();

private:
    QTemporaryDir m_settingsRoot;
};

void PowerPointDataAccessNoticeTests::initTestCase()
{
    QVERIFY(m_settingsRoot.isValid());
    qputenv(
        "CLASSMNGR_SETTINGS_ROOT",
        m_settingsRoot.path().toUtf8()
        );
}

void PowerPointDataAccessNoticeTests::
    noticeActionDefaultsToEnabledAndPersists()
{
    SettingsManager& settings =
        SettingsManager::instance();
    settings.remove(
        QString::fromUtf8(
            SettingsManager::Keys::
                SHOW_POWERPOINT_DATA_ACCESS_NOTICE
            )
        );
    settings.sync();

    ActionRegistry defaultActions;
    defaultActions.createActions();
    QVERIFY(defaultActions.showPowerPointDataAccessNotice);
    QVERIFY(defaultActions.showPowerPointDataAccessNotice->isChecked());
    QCOMPARE(
        defaultActions.showPowerPointDataAccessNotice->text(),
        QStringLiteral("Show Data Access Notice Before Export")
        );

    defaultActions.showPowerPointDataAccessNotice->setChecked(false);
    settings.sync();
    QVERIFY(!settings.showPowerPointDataAccessNotice());

    ActionRegistry reloadedActions;
    reloadedActions.createActions();
    QVERIFY(reloadedActions.showPowerPointDataAccessNotice);
    QVERIFY(!reloadedActions.showPowerPointDataAccessNotice->isChecked());

    reloadedActions.showPowerPointDataAccessNotice->setChecked(true);
    settings.sync();
    QVERIFY(settings.showPowerPointDataAccessNotice());
}

QTEST_MAIN(PowerPointDataAccessNoticeTests)

#include "powerpoint_data_access_notice_tests.moc"
