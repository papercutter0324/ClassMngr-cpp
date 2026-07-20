#include "core/application_services.h"
#include "data/data_service.h"
#include "features/schedule/ui/schedule_page.h"
#include "features/schedule/ui/schedule_widget.h"

#include <QtTest>

#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTableWidget>

namespace ScheduleWidgetTestStubs
{
extern int savedSlotStates;
void reset();
void setDatabaseOpen(bool open);
}

class ScheduleWidgetTests : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void persistsAndMirrorsEveryViewOption();
    void readOnlyPresentationHidesControlsAndIgnoresClicks();
    void clearDatabaseStateRemovesLoadedDataAndSettings();
    void schedulePageScrollsWithoutResizingSchedule();
};

void ScheduleWidgetTests::init()
{
    ScheduleWidgetTestStubs::reset();
}

void ScheduleWidgetTests
    ::persistsAndMirrorsEveryViewOption()
{
    ApplicationServices services;
    ScheduleWidget interactive(&services);

    auto* use24Hour =
        interactive.findChild<QCheckBox*>(
            QStringLiteral("scheduleUse24HourTimeCheckBox")
            );
    auto* showWeekends =
        interactive.findChild<QCheckBox*>(
            QStringLiteral("scheduleShowWeekendsCheckBox")
            );
    auto* showEnglishNames =
        interactive.findChild<QCheckBox*>(
            QStringLiteral("scheduleShowKoreanTeacherEnglishNamesCheckBox")
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
    QVERIFY(showEnglishNames);
    QVERIFY(showIntensive);
    QVERIFY(showAllHours);
    QVERIFY(hideEmptyRows);
    QCOMPARE(use24Hour->text(), QStringLiteral("Use 24-Hour Time"));
    QCOMPARE(showWeekends->text(), QStringLiteral("Show Weekends"));
    QCOMPARE(showEnglishNames->text(), QStringLiteral("Show English Names"));

    use24Hour->setChecked(true);
    showWeekends->setChecked(true);
    showEnglishNames->setChecked(true);
    showIntensive->setChecked(true);
    showAllHours->setChecked(true);
    hideEmptyRows->setChecked(false);

    auto* table =
        interactive.findChild<QTableWidget*>(
            QStringLiteral("scheduleTable")
            );
    QVERIFY(table);

    auto* scheduledClass =
        qobject_cast<QLabel*>(
            table->cellWidget(0, 2)
            );
    QVERIFY(scheduledClass);
    QVERIFY(scheduledClass->text().contains(QStringLiteral("Susan")));
    QVERIFY(!scheduledClass->text().contains(QStringLiteral("김선생")));

    ScheduleWidget mirrored(
        &services,
        nullptr,
        ScheduleMode::ReadOnly
        );

    const ScheduleDisplayState state =
        mirrored.displayState();

    QVERIFY(state.use24HourTime);
    QVERIFY(state.showWeekends);
    QVERIFY(state.showKoreanTeacherEnglishNames);
    QVERIFY(state.showIntensive);
    QVERIFY(state.showAllHours);
    QVERIFY(!state.hideEmptyRows);
    QCOMPARE(
        mirrored.visibleClassIds(),
        QSet<int>{42}
        );
}

void ScheduleWidgetTests
    ::readOnlyPresentationHidesControlsAndIgnoresClicks()
{
    ApplicationServices services;
    services.dataService()->saveSetting(
        QStringLiteral("schedule_show_intensive"),
        QStringLiteral("true")
        );

    ScheduleWidget widget(
        &services,
        nullptr,
        ScheduleMode::ReadOnly
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
    QCOMPARE(checkBoxes.size(), 6);
    QCOMPARE(buttons.size(), 1);
    QCOMPARE(buttons.first()->text(), QStringLiteral("Export"));

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
        ScheduleWidgetTestStubs::savedSlotStates,
        0
        );
}

void ScheduleWidgetTests
    ::clearDatabaseStateRemovesLoadedDataAndSettings()
{
    ApplicationServices services;
    ScheduleWidget widget(&services);

    QCOMPARE(widget.visibleClassIds(), QSet<int>{42});

    auto* use24Hour =
        widget.findChild<QCheckBox*>(
            QStringLiteral("scheduleUse24HourTimeCheckBox")
            );
    auto* showWeekends =
        widget.findChild<QCheckBox*>(
            QStringLiteral("scheduleShowWeekendsCheckBox")
            );

    QVERIFY(use24Hour);
    QVERIFY(showWeekends);
    use24Hour->setChecked(true);
    showWeekends->setChecked(true);

    ScheduleWidgetTestStubs::setDatabaseOpen(false);
    widget.clearDatabaseState();

    QVERIFY(widget.visibleClassIds().isEmpty());
    const ScheduleDisplayState cleared =
        widget.displayState();
    QVERIFY(!cleared.use24HourTime);
    QVERIFY(!cleared.showKoreanTeacherEnglishNames);
    QVERIFY(!cleared.showIntensive);
    QVERIFY(!cleared.showAllHours);
    QVERIFY(cleared.hideEmptyRows);
    QVERIFY(!cleared.showWeekends);
    QVERIFY(!use24Hour->isChecked());
    QVERIFY(!showWeekends->isChecked());
}

void ScheduleWidgetTests
    ::schedulePageScrollsWithoutResizingSchedule()
{
    ApplicationServices services;
    SchedulePage page(&services);

    auto* scrollArea =
        page.findChild<QScrollArea*>();
    auto* schedule =
        page.findChild<ScheduleWidget*>();

    QVERIFY(scrollArea);
    QVERIFY(schedule);
    QCOMPARE(
        scrollArea->verticalScrollBarPolicy(),
        Qt::ScrollBarAsNeeded
        );

    page.resize(800, 900);
    page.show();
    QCoreApplication::processEvents();

    const int scheduleHeight = schedule->height();

    page.resize(800, 160);
    QCoreApplication::processEvents();

    QCOMPARE(schedule->height(), scheduleHeight);
    QVERIFY(scrollArea->verticalScrollBar()->maximum() > 0);
}

QTEST_MAIN(ScheduleWidgetTests)

#include "schedule_widget_tests.moc"
