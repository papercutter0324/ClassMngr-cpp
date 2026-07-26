#include "ui/shared/widgets/uniform_width_tab_bar.h"

#include <QApplication>
#include <QFile>
#include <QImage>
#include <QToolButton>
#include <QtTest>

class UniformWidthTabBarTests : public QObject
{
    Q_OBJECT

private slots:
    void tabSizeHintsUseWidestTabWidth();
    void tabWidgetCentersTabBarWhenTabsFit();
    void scrollButtonsBracketOverflowingTabs();
    void scrollButtonsAreHiddenWhenTabsFit();
    void resizingDoesNotLeaveTrailingEmptySpace();
    void tabWidgetAppliesReusableKindProperties();
    void tabWidgetAppliesSectionKindProperties();
    void navigationTabsUseSharedTheme_data();
    void navigationTabsUseSharedTheme();
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

void UniformWidthTabBarTests::scrollButtonsBracketOverflowingTabs()
{
    UniformWidthTabWidget tabs(
        UniformWidthTabKind::Class,
        QStringLiteral("overflowingTabBar")
        );
    tabs.resize(420, 200);

    for (int index = 0; index < 8; ++index)
    {
        tabs.addTab(
            new QWidget(&tabs),
            QStringLiteral("Class level %1").arg(index)
            );
    }

    tabs.show();
    QCoreApplication::processEvents();

    auto* tabBar =
        tabs.findChild<UniformWidthTabBar*>(
            QStringLiteral("overflowingTabBar")
            );
    auto* leftButton =
        tabBar
            ? tabBar->findChild<QToolButton*>(
                QStringLiteral("ScrollLeftButton")
                )
            : nullptr;
    auto* rightButton =
        tabBar
            ? tabBar->findChild<QToolButton*>(
                QStringLiteral("ScrollRightButton")
                )
            : nullptr;

    QVERIFY(tabBar);
    QVERIFY(leftButton);
    QVERIFY(rightButton);
    QVERIFY(leftButton->isVisible());
    QVERIFY(rightButton->isVisible());
    QCOMPARE(leftButton->x(), 0);
    QCOMPARE(
        rightButton->geometry().right(),
        tabBar->width() - 1
        );
    QVERIFY(
        tabBar->tabRect(0).left()
        > leftButton->geometry().right()
        );
}

void UniformWidthTabBarTests::scrollButtonsAreHiddenWhenTabsFit()
{
    UniformWidthTabWidget tabs(
        UniformWidthTabKind::Class,
        QStringLiteral("fittingTabBar")
        );
    tabs.resize(900, 200);

    for (int index = 0; index < 2; ++index)
    {
        tabs.addTab(
            new QWidget(&tabs),
            QStringLiteral("Class %1").arg(index)
            );
    }

    tabs.show();
    QCoreApplication::processEvents();

    auto* tabBar =
        tabs.findChild<UniformWidthTabBar*>(
            QStringLiteral("fittingTabBar")
            );
    auto* leftButton =
        tabBar
            ? tabBar->findChild<QToolButton*>(
                QStringLiteral("ScrollLeftButton")
                )
            : nullptr;
    auto* rightButton =
        tabBar
            ? tabBar->findChild<QToolButton*>(
                QStringLiteral("ScrollRightButton")
                )
            : nullptr;

    QVERIFY(tabBar);
    QVERIFY(leftButton);
    QVERIFY(rightButton);
    QVERIFY(!leftButton->isVisible());
    QVERIFY(!rightButton->isVisible());
}

void UniformWidthTabBarTests::resizingDoesNotLeaveTrailingEmptySpace()
{
    UniformWidthTabWidget tabs(
        UniformWidthTabKind::Class,
        QStringLiteral("resizedTabBar")
        );
    tabs.resize(360, 200);

    for (int index = 0; index < 8; ++index)
    {
        tabs.addTab(
            new QWidget(&tabs),
            QStringLiteral("A wide class level %1").arg(index)
            );
    }

    tabs.setCurrentIndex(6);
    tabs.show();
    QCoreApplication::processEvents();
    tabs.resize(900, 200);
    QCoreApplication::processEvents();

    auto* tabBar =
        tabs.findChild<UniformWidthTabBar*>(
            QStringLiteral("resizedTabBar")
            );
    auto* rightButton =
        tabBar
            ? tabBar->findChild<QToolButton*>(
                QStringLiteral("ScrollRightButton")
                )
            : nullptr;

    QVERIFY(tabBar);
    QVERIFY(rightButton);
    QVERIFY(rightButton->isVisible());
    QVERIFY(
        tabBar->tabRect(tabBar->count() - 1).right()
        >= rightButton->x() - 1
        );
}

void UniformWidthTabBarTests::tabWidgetAppliesReusableKindProperties()
{
    UniformWidthTabWidget tabs(
        UniformWidthTabKind::Class,
        QStringLiteral("testClassTabBar")
        );

    auto* tabBar =
        tabs.findChild<UniformWidthTabBar*>(
            QStringLiteral("testClassTabBar")
            );

    QVERIFY(tabBar);
    QCOMPARE(
        tabs.tabKind(),
        UniformWidthTabKind::Class
        );
    QCOMPARE(
        tabs.property("uniformTabKind").toString(),
        QStringLiteral("class")
        );
    QCOMPARE(
        tabBar->property("uniformTabKind").toString(),
        QStringLiteral("class")
        );
    QCOMPARE(
        tabs.tabShape(),
        QTabWidget::Rounded
        );
    QVERIFY(tabs.documentMode());
    QCOMPARE(
        tabs.elideMode(),
        Qt::ElideRight
        );
    QVERIFY(tabs.usesScrollButtons());
}

void UniformWidthTabBarTests::tabWidgetAppliesSectionKindProperties()
{
    UniformWidthTabWidget tabs(
        UniformWidthTabKind::Section,
        QStringLiteral("testSectionTabBar")
        );

    auto* tabBar =
        tabs.findChild<UniformWidthTabBar*>(
            QStringLiteral("testSectionTabBar")
            );

    QVERIFY(tabBar);
    QCOMPARE(
        tabs.tabKind(),
        UniformWidthTabKind::Section
        );
    QCOMPARE(
        tabs.property("uniformTabKind").toString(),
        QStringLiteral("section")
        );
    QCOMPARE(
        tabBar->property("uniformTabKind").toString(),
        QStringLiteral("section")
        );
    QCOMPARE(
        tabs.tabShape(),
        QTabWidget::Rounded
        );
    QVERIFY(tabs.documentMode());
    QCOMPARE(
        tabs.elideMode(),
        Qt::ElideRight
        );
    QVERIFY(tabs.usesScrollButtons());
}

void UniformWidthTabBarTests::navigationTabsUseSharedTheme_data()
{
    QTest::addColumn<QString>("stylesheetPath");
    QTest::addColumn<QColor>("expectedColor");

    QTest::newRow("light")
        << QStringLiteral("../resources/assets/styles/light.qss")
        << QColor(QStringLiteral("#deded8"));
    QTest::newRow("dark")
        << QStringLiteral("../resources/assets/styles/dark.qss")
        << QColor(QStringLiteral("#303030"));
}

void UniformWidthTabBarTests::navigationTabsUseSharedTheme()
{
    QFETCH(QString, stylesheetPath);
    QFETCH(QColor, expectedColor);

    QFile stylesheet(
        QFINDTESTDATA(stylesheetPath)
        );

    QVERIFY(stylesheet.open(QIODevice::ReadOnly | QIODevice::Text));
    qApp->setStyleSheet(
        QString::fromUtf8(stylesheet.readAll())
        );

    UniformWidthTabWidget gradeTabs(
        UniformWidthTabKind::Grade,
        QStringLiteral("testGradeTabBar")
        );
    UniformWidthTabWidget classTabs(
        UniformWidthTabKind::Class,
        QStringLiteral("testClassTabBar")
        );
    UniformWidthTabWidget sectionTabs(
        UniformWidthTabKind::Section,
        QStringLiteral("testSectionTabBar")
        );

    const auto addNavigationTabs =
        [](UniformWidthTabWidget& tabs)
        {
            tabs.addTab(
                new QWidget(&tabs),
                QStringLiteral("Current Month")
                );
            tabs.addTab(
                new QWidget(&tabs),
                QStringLiteral("Next 30 Days")
                );
            tabs.resize(640, 200);
            tabs.show();
        };

    addNavigationTabs(gradeTabs);
    addNavigationTabs(classTabs);
    addNavigationTabs(sectionTabs);
    QCoreApplication::processEvents();

    auto* gradeTabBar =
        gradeTabs.findChild<UniformWidthTabBar*>(
            QStringLiteral("testGradeTabBar")
            );
    auto* classTabBar =
        classTabs.findChild<UniformWidthTabBar*>(
            QStringLiteral("testClassTabBar")
            );
    auto* sectionTabBar =
        sectionTabs.findChild<UniformWidthTabBar*>(
            QStringLiteral("testSectionTabBar")
            );

    QVERIFY(gradeTabBar);
    QVERIFY(classTabBar);
    QVERIFY(sectionTabBar);

    const auto tabColor =
        [](UniformWidthTabBar* tabBar)
        {
            const QRect secondTab =
                tabBar->tabRect(1);
            const QImage image =
                tabBar->grab().toImage();

            return image.pixelColor(
                secondTab.left() + 10,
                secondTab.top() + 10
                );
        };

    QCOMPARE(gradeTabBar->font(), classTabBar->font());
    QCOMPARE(gradeTabBar->font(), sectionTabBar->font());
    QCOMPARE(
        gradeTabBar->tabSizeHint(1),
        classTabBar->tabSizeHint(1)
        );
    QCOMPARE(
        gradeTabBar->tabSizeHint(1),
        sectionTabBar->tabSizeHint(1)
        );
    QCOMPARE(tabColor(gradeTabBar), expectedColor);
    QCOMPARE(tabColor(classTabBar), expectedColor);
    QCOMPARE(tabColor(sectionTabBar), expectedColor);
    qApp->setStyleSheet({});
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
