#include "features/campus/ui/campus_dashboard_page.h"

#include <QCheckBox>
#include <QTemporaryDir>
#include <QTest>

class CampusDashboardPageTests : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void unavailableCheckboxesAreAdminOnly_data();
    void unavailableCheckboxesAreAdminOnly();

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

QTEST_MAIN(CampusDashboardPageTests)

#include "campus_dashboard_page_tests.moc"
