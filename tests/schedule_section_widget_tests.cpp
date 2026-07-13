#include "core/application_services.h"
#include "data/data_service.h"
#include "ui/shared/widgets/sections/schedule_section_widget.h"

#include <QtTest>

#include <QCheckBox>
#include <QPushButton>
#include <QTableWidget>

namespace ScheduleSectionWidgetTestStubs
{
extern int savedSlotStates;
void reset();
}

class ScheduleSectionWidgetTests : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void persistsAndMirrorsEveryViewOption();
    void readOnlyPresentationHidesControlsAndIgnoresClicks();
};

void ScheduleSectionWidgetTests::init()
{
    ScheduleSectionWidgetTestStubs::reset();
}

void ScheduleSectionWidgetTests
    ::persistsAndMirrorsEveryViewOption()
{
    ApplicationServices services;
    ScheduleSectionWidget interactive(&services);

    auto* use24Hour =
        interactive.findChild<QCheckBox*>(
            QStringLiteral("scheduleUse24HourTimeCheckBox")
            );
    auto* showWeekends =
        interactive.findChild<QCheckBox*>(
            QStringLiteral("scheduleShowWeekendsCheckBox")
            );
    auto* showIntensive =
        interactive.findChild<QCheckBox*>(
            QStringLiteral("scheduleShowIntensiveCheckBox")
            );
    auto* showAllHours =
        interactive.findChild<QCheckBox*>(
            QStringLiteral("scheduleShowAllHoursCheckBox")
            );
    auto* hideEmptyRows =
        interactive.findChild<QCheckBox*>(
            QStringLiteral("scheduleHideEmptyRowsCheckBox")
            );

    QVERIFY(use24Hour);
    QVERIFY(showWeekends);
    QVERIFY(showIntensive);
    QVERIFY(showAllHours);
    QVERIFY(hideEmptyRows);

    use24Hour->setChecked(true);
    showWeekends->setChecked(true);
    showIntensive->setChecked(true);
    showAllHours->setChecked(true);
    hideEmptyRows->setChecked(false);

    ScheduleSectionWidget mirrored(
        &services,
        nullptr,
        ScheduleSectionMode::ReadOnly
        );

    const ScheduleDisplayState state =
        mirrored.displayState();

    QVERIFY(state.use24HourTime);
    QVERIFY(state.showWeekends);
    QVERIFY(state.showIntensive);
    QVERIFY(state.showAllHours);
    QVERIFY(!state.hideEmptyRows);
    QCOMPARE(
        mirrored.visibleClassIds(),
        QSet<int>{42}
        );
}

void ScheduleSectionWidgetTests
    ::readOnlyPresentationHidesControlsAndIgnoresClicks()
{
    ApplicationServices services;
    services.dataService()->saveSetting(
        QStringLiteral("schedule_show_intensive"),
        QStringLiteral("true")
        );

    ScheduleSectionWidget widget(
        &services,
        nullptr,
        ScheduleSectionMode::ReadOnly
        );

    auto* controls =
        widget.findChild<QWidget*>(
            QStringLiteral("scheduleControls")
            );
    auto* table =
        widget.findChild<QTableWidget*>(
            QStringLiteral("scheduleTable")
            );

    QVERIFY(controls);
    QVERIFY(controls->isHidden());
    QVERIFY(table);
    QCOMPARE(table->rowCount(), 1);

    const auto checkBoxes =
        controls->findChildren<QCheckBox*>();
    const auto buttons =
        controls->findChildren<QPushButton*>();
    QCOMPARE(checkBoxes.size(), 5);
    QCOMPARE(buttons.size(), 2);

    for (const QCheckBox* checkBox : checkBoxes)
    {
        QVERIFY(!checkBox->isVisibleTo(&widget));
    }

    for (const QPushButton* button : buttons)
    {
        QVERIFY(!button->isVisibleTo(&widget));
    }

    QVERIFY(
        QMetaObject::invokeMethod(
            &widget,
            "onCellClicked",
            Qt::DirectConnection,
            Q_ARG(int, 0),
            Q_ARG(int, 1)
            )
        );
    QCOMPARE(
        ScheduleSectionWidgetTestStubs::savedSlotStates,
        0
        );
}

QTEST_MAIN(ScheduleSectionWidgetTests)

#include "schedule_section_widget_tests.moc"
