#include "features/classes/ui/classes_navigation_tabs.h"
#include "ui/shared/widgets/uniform_width_tab_bar.h"

#include <QApplication>
#include <QFile>
#include <QHoverEvent>
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
    void classesNavigationFactoryUsesPillAppearance();
    void navigationPillAppearance_data();
    void navigationPillAppearance();
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
        tabs.tabAppearance(),
        UniformWidthTabAppearance::NavigationPill
        );
    QCOMPARE(
        tabBar->tabAppearance(),
        UniformWidthTabAppearance::NavigationPill
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
        tabs.tabAppearance(),
        UniformWidthTabAppearance::Platform
        );
    QCOMPARE(
        tabBar->tabAppearance(),
        UniformWidthTabAppearance::Platform
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

void UniformWidthTabBarTests::classesNavigationFactoryUsesPillAppearance()
{
    QWidget owner;
    const QList<UniformWidthTabWidget*> tabs{
        ClassesNavigationTabs::create(
            UniformWidthTabKind::Grade,
            QStringLiteral("classesGradeTabBar"),
            &owner
            ),
        ClassesNavigationTabs::create(
            UniformWidthTabKind::Class,
            QStringLiteral("classesLevelTabBar"),
            &owner
            ),
        ClassesNavigationTabs::create(
            UniformWidthTabKind::Section,
            QStringLiteral("classesSectionTabBar"),
            &owner
            )
    };

    QSize expectedTabSize;

    for (UniformWidthTabWidget* tabWidget : tabs)
    {
        QVERIFY(tabWidget);
        tabWidget->addTab(
            new QWidget(tabWidget),
            QStringLiteral("Reference")
            );
        tabWidget->addTab(
            new QWidget(tabWidget),
            QStringLiteral("Reference")
            );

        auto* tabBar =
            qobject_cast<UniformWidthTabBar*>(
                tabWidget->tabBar()
                );

        QVERIFY(tabBar);
        QCOMPARE(
            tabWidget->tabAppearance(),
            UniformWidthTabAppearance::NavigationPill
            );
        QCOMPARE(
            tabBar->tabAppearance(),
            UniformWidthTabAppearance::NavigationPill
            );
        QCOMPARE(
            tabWidget->font(),
            QApplication::font()
            );
        QCOMPARE(
            tabBar->font(),
            QApplication::font()
            );
        QCOMPARE(
            tabWidget->currentIndex(),
            0
            );

        if (expectedTabSize.isEmpty())
        {
            expectedTabSize =
                tabBar->tabSizeHint(0);
        }

        QCOMPARE(
            tabBar->tabSizeHint(0),
            expectedTabSize
            );
        QCOMPARE(
            tabBar->tabSizeHint(1),
            expectedTabSize
            );
    }
}

void UniformWidthTabBarTests::navigationPillAppearance_data()
{
    QTest::addColumn<QColor>("windowColor");
    QTest::addColumn<QColor>("buttonColor");
    QTest::addColumn<QColor>("buttonTextColor");
    QTest::addColumn<QColor>("borderColor");
    QTest::addColumn<QColor>("hoverColor");
    QTest::addColumn<QColor>("selectedColor");
    QTest::addColumn<QColor>("selectedTextColor");

    QTest::newRow("light")
        << QColor(QStringLiteral("#eff0f1"))
        << QColor(QStringLiteral("#deded8"))
        << QColor(QStringLiteral("#27313a"))
        << QColor(QStringLiteral("#c5c7c3"))
        << QColor(QStringLiteral("#f5f5f5"))
        << QColor(QStringLiteral("#3daee9"))
        << QColor(QStringLiteral("#ff00ff"));

    QTest::newRow("dark")
        << QColor(QStringLiteral("#202326"))
        << QColor(QStringLiteral("#303030"))
        << QColor(QStringLiteral("#f0f0f0"))
        << QColor(QStringLiteral("#454545"))
        << QColor(QStringLiteral("#31363b"))
        << QColor(QStringLiteral("#3daee9"))
        << QColor(QStringLiteral("#ff00ff"));
}

void UniformWidthTabBarTests::navigationPillAppearance()
{
    QFETCH(QColor, windowColor);
    QFETCH(QColor, buttonColor);
    QFETCH(QColor, buttonTextColor);
    QFETCH(QColor, borderColor);
    QFETCH(QColor, hoverColor);
    QFETCH(QColor, selectedColor);
    QFETCH(QColor, selectedTextColor);

    QPalette navigationPalette =
        qApp->palette();

    for (const QPalette::ColorGroup group : {
             QPalette::Active,
             QPalette::Inactive
             })
    {
        navigationPalette.setColor(
            group,
            QPalette::Window,
            windowColor
            );
        navigationPalette.setColor(
            group,
            QPalette::Button,
            buttonColor
            );
        navigationPalette.setColor(
            group,
            QPalette::ButtonText,
            buttonTextColor
            );
        navigationPalette.setColor(
            group,
            QPalette::Mid,
            borderColor
            );
        navigationPalette.setColor(
            group,
            QPalette::Light,
            hoverColor
            );
        navigationPalette.setColor(
            group,
            QPalette::Highlight,
            selectedColor
            );
        navigationPalette.setColor(
            group,
            QPalette::HighlightedText,
            selectedTextColor
            );
    }

    UniformWidthTabWidget gradeTabs(
        UniformWidthTabKind::Grade,
        QStringLiteral("pillGradeTabBar")
        );
    UniformWidthTabWidget classTabs(
        UniformWidthTabKind::Class,
        QStringLiteral("pillClassTabBar")
        );
    UniformWidthTabWidget sectionTabs(
        UniformWidthTabKind::Section,
        QStringLiteral("pillSectionTabBar")
        );

    const auto configure =
        [&navigationPalette](UniformWidthTabWidget& tabs)
        {
            tabs.setPalette(
                navigationPalette
                );
            tabs.setTabAppearance(
                UniformWidthTabAppearance::NavigationPill
                );
            tabs.addTab(
                new QWidget(&tabs),
                QStringLiteral("Reference")
                );
            tabs.addTab(
                new QWidget(&tabs),
                QStringLiteral("Reference")
                );
            tabs.setCurrentIndex(0);
            tabs.resize(640, 200);
            tabs.show();
        };

    configure(gradeTabs);
    configure(classTabs);
    configure(sectionTabs);
    QCoreApplication::processEvents();

    const QList<UniformWidthTabBar*> tabBars{
        gradeTabs.findChild<UniformWidthTabBar*>(
            QStringLiteral("pillGradeTabBar")
            ),
        classTabs.findChild<UniformWidthTabBar*>(
            QStringLiteral("pillClassTabBar")
            ),
        sectionTabs.findChild<UniformWidthTabBar*>(
            QStringLiteral("pillSectionTabBar")
            )
    };

    QVERIFY(tabBars.at(0));
    QVERIFY(tabBars.at(1));
    QVERIFY(tabBars.at(2));

    const QSize expectedSize =
        tabBars.first()->tabSizeHint(0);
    const QFont expectedFont =
        tabBars.first()->font();

    for (UniformWidthTabBar* tabBar : tabBars)
    {
        QCOMPARE(
            tabBar->tabAppearance(),
            UniformWidthTabAppearance::NavigationPill
            );
        QCOMPARE(
            tabBar->property("uniformTabAppearance").toString(),
            QStringLiteral("navigationPill")
            );
        QCOMPARE(
            tabBar->tabSizeHint(0),
            expectedSize
            );
        QCOMPARE(
            tabBar->tabSizeHint(1),
            expectedSize
            );
        QCOMPARE(
            tabBar->font(),
            expectedFont
            );

        const QRect selectedTab =
            tabBar->tabRect(0);
        const QRect inactiveTab =
            tabBar->tabRect(1);
        const QImage image =
            tabBar->grab().toImage();
        const int fillSampleOffset = 8;

        QCOMPARE(
            image.pixelColor(
                selectedTab.left() + fillSampleOffset,
                selectedTab.center().y()
                ),
            selectedColor
            );

        const QColor selectedTextColor(
            QStringLiteral("#101418")
            );
        bool selectedTextIsNearBlack = false;

        for (
            int y = selectedTab.top();
            y <= selectedTab.bottom() && !selectedTextIsNearBlack;
            ++y
            )
        {
            for (int x = selectedTab.left(); x <= selectedTab.right(); ++x)
            {
                if (image.pixelColor(x, y) == selectedTextColor)
                {
                    selectedTextIsNearBlack = true;
                    break;
                }
            }
        }

        QVERIFY(selectedTextIsNearBlack);
        QCOMPARE(
            image.pixelColor(
                inactiveTab.left() + fillSampleOffset,
                inactiveTab.center().y()
                ),
            buttonColor
            );
        QCOMPARE(
            image.pixelColor(
                inactiveTab.left(),
                inactiveTab.center().y()
                ),
            borderColor
            );
        QCOMPARE(
            image.pixelColor(
                selectedTab.right() - 2,
                selectedTab.center().y()
                ).alpha(),
            0
            );
        QCOMPARE(
            image.pixelColor(
                selectedTab.left(),
                selectedTab.top()
                ).alpha(),
            0
            );

        QHoverEvent hoverEvent(
            QEvent::HoverMove,
            QPointF(inactiveTab.center()),
            QPointF(selectedTab.center())
            );
        QApplication::sendEvent(
            tabBar,
            &hoverEvent
            );
        QCoreApplication::processEvents();

        const QImage hoveredImage =
            tabBar->grab().toImage();

        QCOMPARE(
            hoveredImage.pixelColor(
                inactiveTab.left() + fillSampleOffset,
                inactiveTab.center().y()
            ),
            hoverColor
            );

        QEvent hoverLeaveEvent(
            QEvent::HoverLeave
            );
        QApplication::sendEvent(
            tabBar,
            &hoverLeaveEvent
            );
    }
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
