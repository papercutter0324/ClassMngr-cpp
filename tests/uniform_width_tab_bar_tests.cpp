#include "ui/shared/widgets/uniform_width_tab_bar.h"

#include <QApplication>
#include <QtTest>

class UniformWidthTabBarTests : public QObject
{
    Q_OBJECT

private slots:
    void tabSizeHintsUseWidestTabWidth();
    void tabWidgetCentersTabBarWhenTabsFit();
};

void UniformWidthTabBarTests::tabSizeHintsUseWidestTabWidth()
{
    UniformWidthTabBar tabBar;

    tabBar.addTab(
        QStringLiteral("E4")
        );
    tabBar.addTab(
        QStringLiteral("A much longer class tab")
        );
    tabBar.addTab(
        QStringLiteral("M1")
        );

    const int expectedWidth =
        tabBar.tabSizeHint(1).width();

    QVERIFY(expectedWidth > 0);
    QCOMPARE(
        tabBar.tabSizeHint(0).width(),
        expectedWidth
        );
    QCOMPARE(
        tabBar.tabSizeHint(2).width(),
        expectedWidth
        );
}

void UniformWidthTabBarTests::tabWidgetCentersTabBarWhenTabsFit()
{
    UniformWidthTabWidget tabs(
        QStringLiteral("testTabBar")
        );
    tabs.resize(
        640,
        200
        );

    tabs.addTab(
        new QWidget(&tabs),
        QStringLiteral("E5")
        );
    tabs.addTab(
        new QWidget(&tabs),
        QStringLiteral("E6")
        );
    tabs.addTab(
        new QWidget(&tabs),
        QStringLiteral("M1")
        );

    tabs.show();
    QCoreApplication::processEvents();

    auto* tabBar =
        tabs.findChild<UniformWidthTabBar*>(
            QStringLiteral("testTabBar")
            );

    QVERIFY(tabBar);
    QVERIFY(tabBar->width() < tabs.width());
    QCOMPARE(
        tabBar->geometry().x(),
        (tabs.width() - tabBar->width()) / 2
        );

    tabs.setCurrentIndex(1);
    QCOMPARE(
        tabBar->geometry().x(),
        (tabs.width() - tabBar->width()) / 2
        );
}

int main(
    int argc,
    char** argv
    )
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
    {
        qputenv(
            "QT_QPA_PLATFORM",
#if defined(Q_OS_WIN)
            QByteArray("windows")
#else
            QByteArray("offscreen")
#endif
            );
    }

    QApplication app(argc, argv);

    UniformWidthTabBarTests tests;
    return QTest::qExec(
        &tests,
        argc,
        argv
        );
}

#include "uniform_width_tab_bar_tests.moc"
