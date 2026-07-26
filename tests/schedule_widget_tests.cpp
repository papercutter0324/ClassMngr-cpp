#include "core/application_services.h"
#include "data/data_service.h"
#include "features/schedule/ui/schedule_page.h"
#include "features/schedule/ui/schedule_widget.h"

#include <QtTest>

#include <QAbstractItemDelegate>
#include <QCheckBox>
#include <QHeaderView>
#include <QImage>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyleOptionViewItem>
#include <QTableWidget>

#include <algorithm>

namespace ScheduleWidgetTestStubs
{
extern int savedSlotStates;
extern int printRequestCount;
extern bool lastPrintRequestShowsEnglishNames;
void reset();
void setDatabaseOpen(bool open);
QString settingValue(const QString& key);
}

class ScheduleWidgetTests : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void persistsAndMirrorsEveryViewOption();
    void exportUsesSelectedTeacherNameLanguage();
    void importButtonRequestsScheduleImport();
    void legacyHourSettingsDoNotCarryForward();
    void readOnlyPresentationHidesControlsAndIgnoresClicks();
    void timeColumnAndHeaderAreNonInteractive();
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
    QVERIFY(use24Hour);
    QVERIFY(showWeekends);
    QVERIFY(showEnglishNames);
    QVERIFY(showIntensive);
    QVERIFY(showAllHours);
    QVERIFY(
        !interactive.findChild<QCheckBox*>(
            QStringLiteral("scheduleHideEmptyRowsCheckBox")
            )
        );
    QCOMPARE(use24Hour->text(), QStringLiteral("Use 24-Hour Time"));
    QCOMPARE(showWeekends->text(), QStringLiteral("Show Weekends"));
    QCOMPARE(showEnglishNames->text(), QStringLiteral("Show English Names"));
    QCOMPARE(showAllHours->text(), QStringLiteral("Show All Hours"));
    QVERIFY(!showAllHours->isChecked());
    QVERIFY(!showAllHours->isEnabled());

    use24Hour->setChecked(true);
    showWeekends->setChecked(true);
    showEnglishNames->setChecked(true);
    showIntensive->setChecked(true);
    QVERIFY(showAllHours->isEnabled());
    showAllHours->setChecked(true);
    QCOMPARE(
        ScheduleWidgetTestStubs::settingValue(
            QStringLiteral("schedule_show_all_hours_v2")
            ),
        QStringLiteral("true")
        );

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
    QCOMPARE(
        mirrored.visibleClassIds(),
        QSet<int>{42}
        );
}

void ScheduleWidgetTests::exportUsesSelectedTeacherNameLanguage()
{
    ApplicationServices services;
    ScheduleWidget widget(&services);

    auto* showEnglishNames =
        widget.findChild<QCheckBox*>(
            QStringLiteral("scheduleShowKoreanTeacherEnglishNamesCheckBox")
            );
    QVERIFY(showEnglishNames);

    showEnglishNames->setChecked(true);
    QVERIFY(
        QMetaObject::invokeMethod(
            &widget,
            "exportSchedule",
            Qt::DirectConnection
            )
        );
    QCOMPARE(ScheduleWidgetTestStubs::printRequestCount, 1);
    QVERIFY(ScheduleWidgetTestStubs::lastPrintRequestShowsEnglishNames);

    showEnglishNames->setChecked(false);
    QVERIFY(
        QMetaObject::invokeMethod(
            &widget,
            "exportSchedule",
            Qt::DirectConnection
            )
        );
    QCOMPARE(ScheduleWidgetTestStubs::printRequestCount, 2);
    QVERIFY(!ScheduleWidgetTestStubs::lastPrintRequestShowsEnglishNames);
}

void ScheduleWidgetTests::importButtonRequestsScheduleImport()
{
    ApplicationServices services;
    ScheduleWidget widget(&services);
    auto* importButton =
        widget.findChild<QPushButton*>(
            QStringLiteral("scheduleImportButton")
            );
    QVERIFY(importButton);
    QCOMPARE(
        importButton->text(),
        QStringLiteral("Import Schedule...")
        );

    QSignalSpy spy(
        &widget,
        &ScheduleWidget::scheduleImportRequested
        );
    importButton->click();
    QCOMPARE(spy.count(), 1);
}

void ScheduleWidgetTests
    ::legacyHourSettingsDoNotCarryForward()
{
    ApplicationServices services;
    services.dataService()->saveSetting(
        QStringLiteral("schedule_show_all_hours"),
        QStringLiteral("true")
        );
    services.dataService()->saveSetting(
        QStringLiteral("schedule_hide_empty_rows"),
        QStringLiteral("false")
        );

    ScheduleWidget widget(&services);

    auto* showAllHours =
        widget.findChild<QCheckBox*>(
            QStringLiteral("scheduleShowAllHoursCheckBox")
            );

    QVERIFY(showAllHours);
    QVERIFY(!showAllHours->isChecked());
    QVERIFY(!widget.displayState().showAllHours);
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
    QCOMPARE(checkBoxes.size(), 5);
    QCOMPARE(buttons.size(), 2);
    QVERIFY(
        std::any_of(
            buttons.cbegin(),
            buttons.cend(),
            [](const QPushButton* button)
            {
                return button->text() == QStringLiteral("Export");
            }
            )
        );
    QVERIFY(
        std::any_of(
            buttons.cbegin(),
            buttons.cend(),
            [](const QPushButton* button)
            {
                return button->text()
                    == QStringLiteral("Import Schedule...");
            }
            )
        );

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
    ::timeColumnAndHeaderAreNonInteractive()
{
    ApplicationServices services;
    ScheduleWidget widget(&services);

    auto* table =
        widget.findChild<QTableWidget*>(
            QStringLiteral("scheduleTable")
            );
    QVERIFY(table);
    QCOMPARE(table->selectionMode(), QAbstractItemView::NoSelection);

    QHeaderView* header = table->horizontalHeader();
    QVERIFY(header);
    QVERIFY(!header->sectionsClickable());
    QVERIFY(!header->highlightSections());
    QCOMPARE(header->focusPolicy(), Qt::NoFocus);

    QTableWidgetItem* timeItem = table->item(0, 0);
    QVERIFY(timeItem);
    QVERIFY(!(timeItem->flags() & Qt::ItemIsSelectable));

    QAbstractItemDelegate* timeDelegate =
        table->itemDelegateForColumn(0);
    QVERIFY(timeDelegate);
    QCOMPARE(
        timeDelegate->objectName(),
        QStringLiteral("scheduleTimeColumnDelegate")
        );

    const auto renderTimeItem =
        [table, timeDelegate](
            QStyle::State state
            )
        {
            QImage image(
                QSize(90, 60),
                QImage::Format_ARGB32_Premultiplied
                );
            image.fill(Qt::transparent);

            QStyleOptionViewItem option;
            option.rect = image.rect();
            option.state = state;
            option.palette = table->palette();
            option.widget = table;

            QPainter painter(&image);
            timeDelegate->paint(
                &painter,
                option,
                table->model()->index(0, 0)
                );

            return image;
        };

    const QStyle::State baseState =
        QStyle::State_Enabled | QStyle::State_Active;
    const QImage defaultAppearance =
        renderTimeItem(baseState);

    QVERIFY(
        defaultAppearance
        == renderTimeItem(baseState | QStyle::State_MouseOver)
        );
    QVERIFY(
        defaultAppearance
        == renderTimeItem(baseState | QStyle::State_Selected)
        );
    QVERIFY(
        defaultAppearance
        == renderTimeItem(baseState | QStyle::State_HasFocus)
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
