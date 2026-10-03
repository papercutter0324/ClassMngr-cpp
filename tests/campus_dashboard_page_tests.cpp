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
    void campusSelectorPreservesDirectoryOrderAndLabels_data();
    void campusSelectorPreservesDirectoryOrderAndLabels();
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

void CampusDashboardPageTests::
campusSelectorPreservesDirectoryOrderAndLabels_data()
{
    QTest::addColumn<bool>("adminMode");

    QTest::newRow("standard") << false;
    QTest::newRow("admin") << true;
}

void CampusDashboardPageTests::
campusSelectorPreservesDirectoryOrderAndLabels()
{
    using ClassMngr::Next::Platform::
        SettingsManagerLastSelectedCampusPort;

    QFETCH(bool, adminMode);

    SettingsManagerLastSelectedCampusPort().write(std::nullopt);

    CampusDashboardPage page(adminMode);
    page.refresh();

    auto* combo = page.findChild<QComboBox*>();
    QVERIFY(combo);

    const QStringList expectedIds{
        QStringLiteral("bundang"),
        QStringLiteral("daechi"),
        QStringLiteral("dongtan"),
        QStringLiteral("dongtan_2"),
        QStringLiteral("hanam"),
        QStringLiteral("ilsan"),
        QStringLiteral("j"),
        QStringLiteral("juk"),
        QStringLiteral("jung"),
        QStringLiteral("pyeongchon"),
        QStringLiteral("songpa"),
        QStringLiteral("suji"),
        QStringLiteral("yongtong")
    };
    const QStringList expectedNames{
        QStringLiteral("Bundang"),
        QStringLiteral("Daechi"),
        QStringLiteral("Dongtan 1"),
        QStringLiteral("Dongtan 2"),
        QStringLiteral("Hanam"),
        QStringLiteral("Ilsan"),
        QStringLiteral("Jeongja"),
        QStringLiteral("Jukjeon"),
        QStringLiteral("Junggye"),
        QStringLiteral("Pyeongchon"),
        QStringLiteral("Songpa"),
        QStringLiteral("Suji"),
        QStringLiteral("Yeongtong")
    };
    const QStringList expectedCodes{
        QStringLiteral("BDG"),
        QStringLiteral("DAE"),
        QStringLiteral("DGT1"),
        QStringLiteral("DGT2"),
        QStringLiteral("HNM"),
        QStringLiteral("ILS"),
        QStringLiteral("JJA"),
        QStringLiteral("JJN"),
        QStringLiteral("JGY"),
        QStringLiteral("PYC"),
        QStringLiteral("SPA"),
        QStringLiteral("SUJ"),
        QStringLiteral("YTG")
    };

    QCOMPARE(combo->count(), expectedIds.size());
    for (int index = 0; index < expectedIds.size(); ++index)
    {
        const QString expectedLabel =
            adminMode
                ? expectedNames.at(index)
                : QStringLiteral("%1 (%2)")
                    .arg(expectedNames.at(index), expectedCodes.at(index));

        QCOMPARE(combo->itemData(index).toString(), expectedIds.at(index));
        QCOMPARE(combo->itemText(index), expectedLabel);
    }

    QCOMPARE(combo->currentData().toString(), QStringLiteral("bundang"));

    combo->setCurrentIndex(combo->count() - 1);
    QCOMPARE(combo->currentData().toString(), QStringLiteral("yongtong"));

    page.refresh();
    QCOMPARE(combo->currentData().toString(), QStringLiteral("yongtong"));
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
    QCOMPARE(combo->currentText(), QStringLiteral("Jeongja (JJA)"));
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
