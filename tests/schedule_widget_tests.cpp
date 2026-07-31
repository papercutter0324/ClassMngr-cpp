#include "core/application_services.h"
#include "data/data_service.h"
#include "features/schedule/ui/schedule_page.h"
#include "features/schedule/ui/schedule_settings_dialog.h"
#include "features/schedule/ui/schedule_widget.h"
#include "features/schedule/ui/testing_block_dialog.h"

#include <QtTest>

#include <QAbstractItemDelegate>
#include <QApplication>
#include <QCheckBox>
#include <QHeaderView>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyleOptionViewItem>
#include <QTableWidget>
#include <QTimer>

#include <algorithm>

namespace ScheduleWidgetTestStubs
{
extern int savedSlotStates;
extern int printRequestCount;
extern bool lastPrintRequestShowsEnglishNames;
void reset();
void setDatabaseOpen(bool open);
void setIncludeMiddleSchoolClasses(bool include);
void setTestingBlock(
    const QString& day,
    const QString& startTime,
    const QString& room
    );
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
    void testingModeFiltersClassesAndDisplaysSavedBlock();
    void testingBlockDialogTrimsRoomAndSupportsRemoval();
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

    auto* intensive =
        interactive.findChild<QPushButton*>(
            QStringLiteral("scheduleIntensiveModeButton")
            );
    auto* settings =
        interactive.findChild<QPushButton*>(
            QStringLiteral("scheduleSettingsButton")
            );
    QVERIFY(intensive);
    QVERIFY(settings);

    intensive->click();
    QCOMPARE(
        ScheduleWidgetTestStubs::settingValue(
            QStringLiteral("schedule_display_mode")
            ),
        QStringLiteral("intensive")
        );

    QTimer::singleShot(
        0,
        []()
        {
            auto* dialog =
                qobject_cast<ScheduleSettingsDialog*>(
                    QApplication::activeModalWidget()
                    );
            QVERIFY(dialog);

            const QStringList settingNames{
                QStringLiteral("scheduleSettingsUse24HourTime"),
                QStringLiteral("scheduleSettingsShowEnglishNames"),
                QStringLiteral("scheduleSettingsShowWeekends"),
                QStringLiteral("scheduleSettingsShowAllIntensiveHours"),
                QStringLiteral("scheduleSettingsTestingAffectsM1")
            };

            for (const QString& name : settingNames)
            {
                auto* check =
                    dialog->findChild<QCheckBox*>(name);
                QVERIFY(check);
                check->setChecked(true);
            }

            dialog->accept();
        }
        );
    settings->click();

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
    QVERIFY(state.showAllHours);
    QVERIFY(state.testingAffectsM1);
    QCOMPARE(
        state.displayMode,
        ScheduleDisplayMode::Intensive
        );
    QCOMPARE(
        mirrored.visibleClassIds(),
        QSet<int>{42}
        );
}

void ScheduleWidgetTests::exportUsesSelectedTeacherNameLanguage()
{
    ApplicationServices services;
    services.dataService()->saveSetting(
        QStringLiteral("schedule_show_korean_teacher_english_names"),
        QStringLiteral("true")
        );
    ScheduleWidget widget(&services);

    QVERIFY(
        QMetaObject::invokeMethod(
            &widget,
            "exportSchedule",
            Qt::DirectConnection
            )
        );
    QCOMPARE(ScheduleWidgetTestStubs::printRequestCount, 1);
    QVERIFY(ScheduleWidgetTestStubs::lastPrintRequestShowsEnglishNames);

    services.dataService()->saveSetting(
        QStringLiteral("schedule_show_korean_teacher_english_names"),
        QStringLiteral("false")
        );
    widget.refreshSchedule();
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
        QStringLiteral("Import")
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

    QVERIFY(!widget.displayState().showAllHours);
}

void ScheduleWidgetTests
    ::testingModeFiltersClassesAndDisplaysSavedBlock()
{
    ScheduleWidgetTestStubs::setIncludeMiddleSchoolClasses(true);
    ScheduleWidgetTestStubs::setTestingBlock(
        QStringLiteral("Monday"),
        QStringLiteral("16:00"),
        QStringLiteral("Library")
        );

    ApplicationServices services;
    ScheduleWidget widget(&services);
    QCOMPARE(
        widget.visibleClassIds(),
        QSet<int>({42, 44, 45})
        );

    auto* testingButton =
        widget.findChild<QPushButton*>(
            QStringLiteral("scheduleTestingModeButton")
            );
    auto* banner =
        widget.findChild<QLabel*>(
            QStringLiteral("scheduleTestingBanner")
            );
    auto* table =
        widget.findChild<QTableWidget*>(
            QStringLiteral("scheduleTable")
            );
    QVERIFY(testingButton);
    QVERIFY(banner);
    QVERIFY(table);

    testingButton->click();
    QCOMPARE(
        widget.displayState().displayMode,
        ScheduleDisplayMode::Testing
        );
    QCOMPARE(
        widget.visibleClassIds(),
        QSet<int>({42, 45})
        );
    QVERIFY(banner->isVisibleTo(&widget));
    QVERIFY(banner->text().contains(QStringLiteral("M2 and M3")));

    auto* monday =
        qobject_cast<QLabel*>(
            table->cellWidget(0, 1)
            );
    QVERIFY(monday);
    QVERIFY(monday->text().contains(QStringLiteral("Testing")));
    QVERIFY(monday->text().contains(QStringLiteral("Library")));
    QCOMPARE(
        monday->property("slot_state").toString(),
        scheduleTestingSlotState()
        );

    services.dataService()->saveSetting(
        QStringLiteral("schedule_testing_affects_m1"),
        QStringLiteral("true")
        );
    widget.refreshSchedule();

    QCOMPARE(widget.visibleClassIds(), QSet<int>{42});
    QVERIFY(banner->text().contains(QStringLiteral("M1, M2, and M3")));
    auto* wednesday =
        qobject_cast<QLabel*>(
            table->cellWidget(0, 3)
            );
    QVERIFY(wednesday);
    QCOMPARE(wednesday->text(), QStringLiteral("Essay"));
    QVERIFY(
        wednesday
            ->property("testing_block_creation_enabled")
            .toBool()
        );
}

void ScheduleWidgetTests
    ::testingBlockDialogTrimsRoomAndSupportsRemoval()
{
    TestingBlockDialog addDialog(
        QString(),
        false
        );
    auto* room =
        addDialog.findChild<QLineEdit*>(
            QStringLiteral("testingBlockRoomEdit")
            );
    QVERIFY(room);
    room->setText(QStringLiteral("  Library  "));
    QCOMPARE(addDialog.room(), QStringLiteral("Library"));
    QVERIFY(!addDialog.removeRequested());
    QVERIFY(
        !addDialog.findChild<QPushButton*>(
            QStringLiteral("testingBlockRemoveButton")
            )
        );

    TestingBlockDialog editDialog(
        QStringLiteral("402"),
        true
        );
    auto* remove =
        editDialog.findChild<QPushButton*>(
            QStringLiteral("testingBlockRemoveButton")
            );
    QVERIFY(remove);
    remove->click();
    QCOMPARE(editDialog.result(), QDialog::Accepted);
    QVERIFY(editDialog.removeRequested());
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
    QCOMPARE(
        widget.displayState().displayMode,
        ScheduleDisplayMode::Intensive
        );
    QCOMPARE(
        ScheduleWidgetTestStubs::settingValue(
            QStringLiteral("schedule_display_mode")
            ),
        QStringLiteral("intensive")
        );

    const auto buttons =
        controls->findChildren<QPushButton*>();
    QVERIFY(controls->findChildren<QCheckBox*>().isEmpty());
    QCOMPARE(buttons.size(), 6);
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
                    == QStringLiteral("Import");
            }
            )
        );

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

    services.dataService()->saveSetting(
        QStringLiteral("schedule_use_24h"),
        QStringLiteral("true")
        );
    services.dataService()->saveSetting(
        QStringLiteral("schedule_show_weekends"),
        QStringLiteral("true")
        );
    services.dataService()->saveSetting(
        QStringLiteral("schedule_display_mode"),
        QStringLiteral("testing")
        );
    widget.refreshSchedule();

    ScheduleWidgetTestStubs::setDatabaseOpen(false);
    widget.clearDatabaseState();

    QVERIFY(widget.visibleClassIds().isEmpty());
    const ScheduleDisplayState cleared =
        widget.displayState();
    QVERIFY(!cleared.use24HourTime);
    QVERIFY(!cleared.showKoreanTeacherEnglishNames);
    QVERIFY(!cleared.showAllHours);
    QVERIFY(!cleared.showWeekends);
    QVERIFY(!cleared.testingAffectsM1);
    QCOMPARE(
        cleared.displayMode,
        ScheduleDisplayMode::Regular
        );
    auto* regular =
        widget.findChild<QPushButton*>(
            QStringLiteral("scheduleRegularModeButton")
            );
    QVERIFY(regular);
    QVERIFY(regular->isChecked());
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
