#include "features/campus/ui/campus_dashboard_page.h"
#include "core/settingsmanager.h"
#include "next/platform/settings_manager_last_selected_campus_port.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QTest>

#include <optional>

class CampusDashboardPageTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void unavailableCheckboxesAreAdminOnly_data();
    void unavailableCheckboxesAreAdminOnly();
    void storedCampusSelectionIsUsedAsFallback();
    void campusSelectionIsPersistedThroughTypedPort();

private:
    QTemporaryDir m_settingsDirectory;
};

void CampusDashboardPageTests::initTestCase()
{
    QVERIFY(m_settingsDirectory.isValid());
    QVERIFY(
        qputenv(
            "CLASSMNGR_SETTINGS_ROOT",
            m_settingsDirectory.path().toUtf8()
            )
        );

    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void CampusDashboardPageTests::cleanup()
{
    SettingsManager::instance().clear();
    SettingsManager::instance().sync();
}

void CampusDashboardPageTests::unavailableCheckboxesAreAdminOnly_data()
{
    QTest::addColumn<bool>("adminMode");

    QTest::newRow("standard") << false;
    QTest::newRow("admin") << true;
}

void CampusDashboardPageTests::unavailableCheckboxesAreAdminOnly()
{
    QFETCH(bool, adminMode);

    CampusDashboardPage page(adminMode);

    const QStringList objectNames{
        QStringLiteral("printerDriverUrlUnavailableCheck"),
        QStringLiteral("photocopierCodeUnavailableCheck")
    };

    for (const QString& objectName : objectNames)
    {
        auto* checkBox =
            page.findChild<QCheckBox*>(objectName);

        QVERIFY2(checkBox, qPrintable(objectName));
        QCOMPARE(checkBox->isHidden(), !adminMode);
        QCOMPARE(checkBox->isEnabled(), adminMode);
    }
}

void CampusDashboardPageTests::storedCampusSelectionIsUsedAsFallback()
{
    using ClassMngr::Next::Domain::CampusId;
    using ClassMngr::Next::Platform::
        SettingsManagerLastSelectedCampusPort;

    SettingsManagerLastSelectedCampusPort port;
    port.write(CampusId::fromString("j"));

    CampusDashboardPage page(false);
    page.refresh();

    auto* combo = page.findChild<QComboBox*>();
    QVERIFY(combo);
    QCOMPARE(combo->currentData().toString(), QStringLiteral("j"));
}

void CampusDashboardPageTests::campusSelectionIsPersistedThroughTypedPort()
{
    using ClassMngr::Next::Platform::
        SettingsManagerLastSelectedCampusPort;

    SettingsManagerLastSelectedCampusPort port;
    port.write(std::nullopt);

    CampusDashboardPage page(false);
    page.refresh();

    auto* combo = page.findChild<QComboBox*>();
    QVERIFY(combo);
    QVERIFY(combo->count() > 1);

    combo->setCurrentIndex(combo->count() - 1);
    QCoreApplication::processEvents();

    const auto storedCampusId = port.read();
    QVERIFY(storedCampusId.has_value());
    QCOMPARE(
        QString::fromStdString(storedCampusId->value()),
        combo->currentData().toString()
        );
}

QTEST_MAIN(CampusDashboardPageTests)

#include "campus_dashboard_page_tests.moc"
