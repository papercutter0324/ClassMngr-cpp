#include "ui/shared/widgets/navigation_pill_button.h"
#include "ui/shared/widgets/navigation_pill_style.h"
#include "ui/shared/widgets/navigation_settings_button.h"
#include "ui/shared/widgets/navigation_tab_widget.h"

#include <QtTest>

#include <algorithm>

#include <QApplication>
#include <QFile>
#include <QHBoxLayout>
#include <QImage>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalSpy>
#include <QToolButton>

namespace
{
void addRepresentativeTabs(
    NavigationTabWidget* tabs,
    int count = 6
    )
{
    const QStringList labels{
        QStringLiteral("E5 Apollo • T/Th 5:00"),
        QStringLiteral("E5 Zeus"),
        QStringLiteral("M1 Athena"),
        QStringLiteral("M2 Poseidon"),
        QStringLiteral("E4 Artemis"),
        QStringLiteral("E6 Hermes")
    };

    for (int index = 0; index < count; ++index)
    {
        tabs->addTab(
            new QWidget(tabs),
            labels.at(index % labels.size())
            );
    }
}

QToolButton* leftButton(NavigationTabStrip* strip)
{
    return strip->findChild<QToolButton*>(
        QStringLiteral("NavigationTabScrollLeftButton")
        );
}

QToolButton* rightButton(NavigationTabStrip* strip)
{
    return strip->findChild<QToolButton*>(
        QStringLiteral("NavigationTabScrollRightButton")
        );
}

QScrollArea* scrollArea(NavigationTabStrip* strip)
{
    return strip->findChild<QScrollArea*>(
        QStringLiteral("navigationTabViewport")
        );
}

}

class NavigationTabWidgetTests : public QObject
{
    Q_OBJECT

private slots:
    void equalWidthTabsUseWidestLabel();
    void fitTabsUseConfiguredAlignment();
    void overflowButtonsBracketViewportAndTrailingControls();
    void resizeTransitionsAndInsertionStayConsistent();
    void programmaticSelectionRevealsActiveTab();
    void arrowsScrollByTabBoundary();
    void draggingScrollsWithoutChangingSelection();
    void clickingSelectsAndKeyboardNavigates();
    void hiddenSelectionPreservesCurrentPage();
    void removeAndClearHandleEmptyState();
    void fontChangeRecalculatesEqualWidths();
    void largestRequiredLabelHeightAppliesToEveryButton();
    void buttonsShareTopAndHeight();
    void fractionalScalePaintsCompleteBorder();
    void settingsButtonIsBorderlessAtRestAndUsesStateFill_data();
    void settingsButtonIsBorderlessAtRestAndUsesStateFill();
};

void NavigationTabWidgetTests::equalWidthTabsUseWidestLabel()
{
    NavigationTabWidget tabs(
        NavigationTabKind::Class,
        QStringLiteral("testTabStrip")
        );
    addRepresentativeTabs(&tabs, 3);

    NavigationTabStrip* strip = tabs.tabStrip();
    QVERIFY(strip);
    const int width = strip->tabButton(0)->width();
    QVERIFY(width > 0);
    QCOMPARE(strip->tabButton(1)->width(), width);
    QCOMPARE(strip->tabButton(2)->width(), width);
    QCOMPARE(strip->tabButton(0)->height(), NavigationPillStyle::ControlHeight);
}

void NavigationTabWidgetTests::fitTabsUseConfiguredAlignment()
{
    NavigationTabWidget centered(
        NavigationTabKind::Class,
        QStringLiteral("centeredStrip")
        );
    centered.resize(1000, 200);
    addRepresentativeTabs(&centered, 2);
    centered.show();
    QApplication::processEvents();

    auto* centeredArea = scrollArea(centered.tabStrip());
    QVERIFY(centeredArea);
    QVERIFY(!centered.tabStrip()->hasOverflow());
    QVERIFY(centeredArea->widget()->geometry().left() > 0);

    NavigationTabWidget leading(
        NavigationTabKind::Grade,
        QStringLiteral("leadingStrip")
        );
    leading.resize(1000, 200);
    addRepresentativeTabs(&leading, 2);
    leading.show();
    QApplication::processEvents();

    auto* leadingArea = scrollArea(leading.tabStrip());
    QVERIFY(leadingArea);
    QVERIFY(!leading.tabStrip()->hasOverflow());
    QCOMPARE(leadingArea->widget()->geometry().left(), 0);
}

void NavigationTabWidgetTests::
    overflowButtonsBracketViewportAndTrailingControls()
{
    NavigationTabWidget tabs(
        NavigationTabKind::Grade,
        QStringLiteral("overflowStrip")
        );
    tabs.resize(760, 220);
    addRepresentativeTabs(&tabs);

    auto* trailing = new QWidget;
    auto* trailingLayout = new QHBoxLayout(trailing);
    trailingLayout->setContentsMargins(16, 0, 0, 0);
    for (int index = 0; index < 6; ++index)
    {
        auto* filter = new NavigationPillButton(trailing);
        filter->setText(QString::number(index));
        trailingLayout->addWidget(filter);
    }
    trailingLayout->addWidget(new NavigationSettingsButton(trailing));
    tabs.setTrailingWidget(trailing);

    tabs.show();
    QApplication::processEvents();

    NavigationTabStrip* strip = tabs.tabStrip();
    QToolButton* left = leftButton(strip);
    QToolButton* right = rightButton(strip);
    QScrollArea* area = scrollArea(strip);
    QVERIFY(strip->hasOverflow());
    QVERIFY(left->isVisible());
    QVERIFY(right->isVisible());
    QCOMPARE(left->width(), 28);
    QCOMPARE(right->width(), 28);
    QVERIFY(left->geometry().right() < area->geometry().left());
    QVERIFY(area->geometry().right() < right->geometry().left());
    QVERIFY(right->geometry().right() < trailing->geometry().left());
}

void NavigationTabWidgetTests::resizeTransitionsAndInsertionStayConsistent()
{
    NavigationTabWidget tabs(
        NavigationTabKind::Class,
        QStringLiteral("resizeStrip")
        );
    tabs.resize(1000, 220);
    addRepresentativeTabs(&tabs, 3);
    tabs.show();
    QApplication::processEvents();
    QVERIFY(!tabs.tabStrip()->hasOverflow());

    tabs.resize(300, 220);
    QApplication::processEvents();
    QVERIFY(tabs.tabStrip()->hasOverflow());
    QVERIFY(leftButton(tabs.tabStrip())->isVisible());
    QVERIFY(rightButton(tabs.tabStrip())->isVisible());

    tabs.resize(1400, 220);
    QApplication::processEvents();
    QVERIFY(!tabs.tabStrip()->hasOverflow());
    QVERIFY(!leftButton(tabs.tabStrip())->isVisible());
    QVERIFY(!rightButton(tabs.tabStrip())->isVisible());

    for (int index = 0; index < 8; ++index)
    {
        tabs.addTab(new QWidget(&tabs), QStringLiteral("Additional %1").arg(index));
    }
    QApplication::processEvents();
    QVERIFY(tabs.tabStrip()->hasOverflow());
}

void NavigationTabWidgetTests::programmaticSelectionRevealsActiveTab()
{
    NavigationTabWidget tabs(
        NavigationTabKind::Class,
        QStringLiteral("revealStrip")
        );
    tabs.resize(320, 220);
    addRepresentativeTabs(&tabs);
    tabs.show();
    QApplication::processEvents();

    auto* area = scrollArea(tabs.tabStrip());
    auto* bar = area->horizontalScrollBar();
    QCOMPARE(bar->value(), bar->minimum());
    tabs.setCurrentIndex(tabs.count() - 1);
    QApplication::processEvents();
    QVERIFY(bar->value() > bar->minimum());

    NavigationPillButton* active =
        tabs.tabStrip()->tabButton(tabs.count() - 1);
    QVERIFY(active->geometry().right() <= bar->value() + area->viewport()->width());
}

void NavigationTabWidgetTests::arrowsScrollByTabBoundary()
{
    NavigationTabWidget tabs(
        NavigationTabKind::Class,
        QStringLiteral("arrowStrip")
        );
    tabs.resize(320, 220);
    addRepresentativeTabs(&tabs);
    tabs.show();
    QApplication::processEvents();

    auto* bar = scrollArea(tabs.tabStrip())->horizontalScrollBar();
    const int step = tabs.tabStrip()->tabButton(0)->width()
        + NavigationPillStyle::Gap;
    rightButton(tabs.tabStrip())->click();
    QCOMPARE(bar->value(), std::min(step, bar->maximum()));
    leftButton(tabs.tabStrip())->click();
    QCOMPARE(bar->value(), 0);
}

void NavigationTabWidgetTests::draggingScrollsWithoutChangingSelection()
{
    NavigationTabWidget tabs(
        NavigationTabKind::Class,
        QStringLiteral("dragStrip")
        );
    tabs.resize(320, 220);
    addRepresentativeTabs(&tabs);
    tabs.show();
    QApplication::processEvents();

    auto* bar = scrollArea(tabs.tabStrip())->horizontalScrollBar();
    bar->setValue(bar->maximum());
    const int before = bar->value();
    const int selected = tabs.currentIndex();
    NavigationPillButton* button = tabs.tabStrip()->tabButton(5);

    QTest::mousePress(button, Qt::LeftButton, Qt::NoModifier, button->rect().center());
    QTest::mouseMove(button, button->rect().center() + QPoint(120, 0), 20);
    QTest::mouseRelease(button, Qt::LeftButton, Qt::NoModifier, button->rect().center() + QPoint(120, 0));

    QVERIFY(bar->value() < before);
    QCOMPARE(tabs.currentIndex(), selected);
}

void NavigationTabWidgetTests::clickingSelectsAndKeyboardNavigates()
{
    NavigationTabWidget tabs(
        NavigationTabKind::Class,
        QStringLiteral("keyboardStrip")
        );
    tabs.resize(900, 220);
    addRepresentativeTabs(&tabs, 4);
    tabs.show();
    QApplication::processEvents();

    QSignalSpy spy(&tabs, &NavigationTabWidget::currentChanged);
    QTest::mouseClick(tabs.tabStrip()->tabButton(2), Qt::LeftButton);
    QCOMPARE(tabs.currentIndex(), 2);
    QCOMPARE(spy.count(), 1);

    tabs.tabStrip()->tabButton(2)->setFocus();
    QTest::keyClick(tabs.tabStrip()->tabButton(2), Qt::Key_Right);
    QCOMPARE(tabs.currentIndex(), 3);
    QTest::keyClick(tabs.tabStrip()->tabButton(3), Qt::Key_Home);
    QCOMPARE(tabs.currentIndex(), 0);
    QTest::keyClick(tabs.tabStrip()->tabButton(0), Qt::Key_End);
    QCOMPARE(tabs.currentIndex(), 3);
    tabs.tabStrip()->tabButton(1)->setFocus();
    QTest::keyClick(tabs.tabStrip()->tabButton(1), Qt::Key_Space);
    QCOMPARE(tabs.currentIndex(), 1);
    tabs.tabStrip()->tabButton(2)->setFocus();
    QTest::keyClick(tabs.tabStrip()->tabButton(2), Qt::Key_Return);
    QCOMPARE(tabs.currentIndex(), 2);
}

void NavigationTabWidgetTests::hiddenSelectionPreservesCurrentPage()
{
    NavigationTabWidget tabs(
        NavigationTabKind::Section,
        QStringLiteral("selectionStrip")
        );
    addRepresentativeTabs(&tabs, 3);
    tabs.setCurrentIndex(1);
    QWidget* selectedPage = tabs.currentWidget();

    tabs.setSelectionVisible(false);
    QVERIFY(!tabs.selectionVisible());
    QCOMPARE(tabs.currentIndex(), 1);
    QCOMPARE(tabs.currentWidget(), selectedPage);
    QVERIFY(!tabs.tabStrip()->tabButton(1)->isChecked());

    tabs.setSelectionVisible(true);
    QVERIFY(tabs.tabStrip()->tabButton(1)->isChecked());
}

void NavigationTabWidgetTests::removeAndClearHandleEmptyState()
{
    NavigationTabWidget tabs(
        NavigationTabKind::Grade,
        QStringLiteral("removalStrip")
        );
    addRepresentativeTabs(&tabs, 3);
    tabs.setCurrentIndex(2);
    tabs.removeTab(2);
    QCOMPARE(tabs.count(), 2);
    QCOMPARE(tabs.currentIndex(), 1);
    tabs.clear();
    QCOMPARE(tabs.count(), 0);
    QCOMPARE(tabs.currentIndex(), -1);
    QCOMPARE(tabs.currentWidget(), nullptr);
}

void NavigationTabWidgetTests::fontChangeRecalculatesEqualWidths()
{
    NavigationTabWidget tabs(
        NavigationTabKind::Class,
        QStringLiteral("fontStrip")
        );
    addRepresentativeTabs(&tabs, 3);
    const int before = tabs.tabStrip()->tabButton(0)->width();
    QFont font = tabs.tabStrip()->font();
    font.setPointSize(font.pointSize() + 8);
    tabs.tabStrip()->setFont(font);
    for (int index = 0; index < tabs.count(); ++index)
    {
        tabs.tabStrip()->tabButton(index)->setFont(font);
    }
    QApplication::processEvents();
    QVERIFY(tabs.tabStrip()->tabButton(0)->width() > before);
    QCOMPARE(
        tabs.tabStrip()->tabButton(0)->width(),
        tabs.tabStrip()->tabButton(2)->width()
        );
}

void NavigationTabWidgetTests::
    largestRequiredLabelHeightAppliesToEveryButton()
{
    NavigationTabWidget tabs(
        NavigationTabKind::Section,
        QStringLiteral("dynamicHeightStrip")
        );
    tabs.resize(800, 200);
    tabs.addTab(new QWidget(&tabs), QStringLiteral("Current Month"));
    tabs.addTab(new QWidget(&tabs), QStringLiteral("Next 30 Days"));
    tabs.addTab(new QWidget(&tabs), QStringLiteral("Next 10 Events"));

    auto* trailing = new QWidget;
    auto* trailingLayout = new QHBoxLayout(trailing);
    trailingLayout->setContentsMargins(0, 0, 0, 0);
    auto* weekdayButton = new NavigationPillButton(trailing);
    weekdayButton->setText(QStringLiteral("Thurs."));
    auto* settingsButton = new NavigationSettingsButton(trailing);
    trailingLayout->addWidget(weekdayButton);
    trailingLayout->addWidget(settingsButton);
    tabs.setTrailingWidget(trailing);

    QFont largeFont = tabs.tabStrip()->tabButton(1)->font();
    largeFont.setPointSize(largeFont.pointSize() + 12);
    tabs.tabStrip()->tabButton(1)->setFont(largeFont);
    tabs.show();
    QApplication::processEvents();

    int largestRequiredHeight = 0;
    for (int index = 0; index < tabs.count(); ++index)
    {
        largestRequiredHeight = std::max(
            largestRequiredHeight,
            tabs.tabStrip()->tabButton(index)->sizeHint().height()
            );
    }
    largestRequiredHeight = std::max(
        {
            largestRequiredHeight,
            weekdayButton->sizeHint().height(),
            settingsButton->sizeHint().height()
        }
        );

    QVERIFY(largestRequiredHeight > NavigationPillStyle::ControlHeight);
    for (int index = 0; index < tabs.count(); ++index)
    {
        NavigationPillButton* button = tabs.tabStrip()->tabButton(index);
        QCOMPARE(button->height(), largestRequiredHeight);
        QCOMPARE(button->geometry().top(), 0);
    }
    QCOMPARE(weekdayButton->height(), largestRequiredHeight);
    QCOMPARE(settingsButton->height(), largestRequiredHeight);
    QCOMPARE(
        tabs.tabStrip()->height(),
        largestRequiredHeight + NavigationPillStyle::RowBottomSpacing
        );
    QCOMPARE(
        scrollArea(tabs.tabStrip())->height(),
        tabs.tabStrip()->height()
        );
}

void NavigationTabWidgetTests::buttonsShareTopAndHeight()
{
    NavigationTabWidget tabs(
        NavigationTabKind::Section,
        QStringLiteral("calendarUpcomingTabBar")
        );
    tabs.resize(800, 200);
    tabs.addTab(new QWidget(&tabs), QStringLiteral("Current Month"));
    tabs.addTab(new QWidget(&tabs), QStringLiteral("Next 30 Days"));
    tabs.addTab(new QWidget(&tabs), QStringLiteral("Next 10 Events"));
    tabs.setCurrentIndex(1);
    tabs.show();
    QApplication::processEvents();

    NavigationPillButton* first = tabs.tabStrip()->tabButton(0);
    QVERIFY(first);
    const QRect referenceGeometry = first->geometry();

    for (int index = 0; index < tabs.count(); ++index)
    {
        NavigationPillButton* button = tabs.tabStrip()->tabButton(index);
        QVERIFY(button);
        QCOMPARE(button->geometry().top(), referenceGeometry.top());
        QCOMPARE(button->geometry().height(), referenceGeometry.height());
    }
}

void NavigationTabWidgetTests::fractionalScalePaintsCompleteBorder()
{
    constexpr qreal Scale = 1.25;
    constexpr int LogicalWidth = 200;
    constexpr int LogicalHeight = NavigationPillStyle::ControlHeight;
    const QColor fillColor(QStringLiteral("#303030"));
    const QColor borderColor(QStringLiteral("#454545"));

    QImage image(
        qRound(LogicalWidth * Scale),
        qRound(LogicalHeight * Scale),
        QImage::Format_ARGB32_Premultiplied
        );
    image.setDevicePixelRatio(Scale);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    NavigationPillStyle::paint(
        &painter,
        QRect(0, 0, LogicalWidth, LogicalHeight),
        QString(),
        QIcon(),
        qApp->font(),
        Qt::ElideRight,
        qApp->style(),
        nullptr,
        qApp->palette(),
        {},
        {
            fillColor,
            borderColor,
            QColor(),
            QColor(),
            QColor()
        }
        );
    painter.end();

    const int centerX = image.width() / 2;
    QCOMPARE(image.pixelColor(centerX, 0), borderColor);
    QCOMPARE(image.pixelColor(centerX, image.height() - 1), borderColor);
}

void NavigationTabWidgetTests::
    settingsButtonIsBorderlessAtRestAndUsesStateFill_data()
{
    QTest::addColumn<QString>("stylePath");
    QTest::newRow("light") << QStringLiteral(
        CLASSMNGR_SOURCE_DIR "/resources/assets/styles/light.qss"
        );
    QTest::newRow("dark") << QStringLiteral(
        CLASSMNGR_SOURCE_DIR "/resources/assets/styles/dark.qss"
        );
}

void NavigationTabWidgetTests::
    settingsButtonIsBorderlessAtRestAndUsesStateFill()
{
    QFETCH(QString, stylePath);
    QFile file(stylePath);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString originalStyle = qApp->styleSheet();
    qApp->setStyleSheet(QString::fromUtf8(file.readAll()));

    QWidget host;
    host.resize(90, 70);
    auto* button = new NavigationSettingsButton(&host);
    button->move(20, 15);
    host.show();
    QApplication::processEvents();

    const QColor hostPixel = host.grab().toImage().pixelColor(2, 2);
    const QImage resting = button->grab().toImage();
    QCOMPARE(resting.pixelColor(1, 1), hostPixel);

    QTest::mouseMove(button, button->rect().center());
    QApplication::processEvents();
    const QColor hoveredCorner = button->grab().toImage().pixelColor(4, 4);
    QVERIFY(hoveredCorner != hostPixel);

    QTest::mousePress(button, Qt::LeftButton, Qt::NoModifier, button->rect().center());
    QApplication::processEvents();
    const QColor pressedCorner = button->grab().toImage().pixelColor(4, 4);
    QVERIFY(pressedCorner != hoveredCorner);
    QTest::mouseRelease(button, Qt::LeftButton);

    qApp->setStyleSheet(originalStyle);
}

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    NavigationTabWidgetTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "navigation_tab_widget_tests.moc"
