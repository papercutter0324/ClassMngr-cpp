#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/data_service.h"
#include "fakes/fake_user_prompt_service.h"
#include "features/schedule/ui/schedule_page.h"
#include "features/schedule/schedule_settings_preferences.h"
#include "features/schedule/ui/schedule_widget.h"
#include "features/schedule/ui/testing_assignment_dialog.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/widgets/text_fit_push_button.h"
#include "domain/models/testing_class.h"

#include <QtTest>

#include <QAbstractButton>
#include <QAbstractItemDelegate>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QImage>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
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
void setIncludeMiddleSchoolClasses(bool include);
void setTestingBlock(
    const QString& day,
    const QString& startTime,
    const QString& room
    );
void setTestingClassAssignment(
    const QString& day,
    const QString& startTime,
    const TestingClass& testingClass
    );
QString settingValue(const QString& key);
}

class ScheduleWidgetTests : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void persistsAndMirrorsEveryViewOption();
    void clearTestingLayoutUsesScheduleService();
    void printUsesSelectedTeacherNameLanguage();
    void importButtonRequestsScheduleImport();
    void controlsUseTextFitButtons();
    void legacyHourSettingsDoNotCarryForward();
    void testingModeFiltersClassesAndDisplaysSavedBlock();
    void testingModeDisplaysAssignedTestingClassCard();
    void testingAssignmentDialogSupportsEveryAction();
    void readOnlyPresentationHidesControlsAndIgnoresClicks();
    void timeColumnAndHeaderAreNonInteractive();
    void clearDatabaseStateRemovesLoadedDataAndSettings();
    void schedulePageScrollsWithoutResizingSchedule();
};

void ScheduleWidgetTests::init()
{
    ScheduleWidgetTestStubs::reset();
}

void ScheduleWidgetTests::cleanup()
{
    DialogServices::setUserPromptServiceForTesting(nullptr);
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
    QVERIFY(intensive);

    intensive->click();
    QCOMPARE(
        ScheduleWidgetTestStubs::settingValue(
            QStringLiteral("schedule_display_mode")
            ),
        QStringLiteral("intensive")
        );

    ScheduleSettingsPreferences::save(
        services.settingsService(),
        {
            true,
            true,
            true,
            true,
            true
        }
        );
    interactive.refreshSchedule();

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

void ScheduleWidgetTests::clearTestingLayoutUsesScheduleService()
{
    ScheduleWidgetTestStubs::setTestingBlock(
        QStringLiteral("Monday"),
        QStringLiteral("16:00"),
        QStringLiteral("Library")
        );
    TestingClass testingClass;
    testingClass.classId = 100;
    testingClass.name = QStringLiteral("Writing Lab");
    ScheduleWidgetTestStubs::setTestingClassAssignment(
        QStringLiteral("Tuesday"),
        QStringLiteral("17:00"),
        testingClass
        );

    ApplicationServices services;
    auto* scheduleService = services.scheduleService();
    QVERIFY(scheduleService);
    const Result<QList<TestingAssignment>> before =
        scheduleService->testingAssignments();
    QVERIFY(before);
    QCOMPARE(before->size(), 2);

    const Status cleared = scheduleService->clearTestingAssignments();
    QVERIFY(cleared);
    const Result<QList<TestingAssignment>> after =
        scheduleService->testingAssignments();
    QVERIFY(after);
    QVERIFY(after->isEmpty());
}

void ScheduleWidgetTests::printUsesSelectedTeacherNameLanguage()
{
    ApplicationServices services;
    services.dataService()->saveSetting(
        QStringLiteral("schedule_show_korean_teacher_english_names"),
        QStringLiteral("true")
        );
    ScheduleWidget widget(&services);

    widget.printSchedule();
    QCOMPARE(ScheduleWidgetTestStubs::printRequestCount, 1);
    QVERIFY(ScheduleWidgetTestStubs::lastPrintRequestShowsEnglishNames);

    services.dataService()->saveSetting(
        QStringLiteral("schedule_show_korean_teacher_english_names"),
        QStringLiteral("false")
        );
    widget.refreshSchedule();
    widget.printSchedule();
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

void ScheduleWidgetTests::controlsUseTextFitButtons()
{
    ApplicationServices services;
    ScheduleWidget widget(&services);

    const QStringList buttonNames{
        QStringLiteral("scheduleRegularModeButton"),
        QStringLiteral("scheduleIntensiveModeButton"),
        QStringLiteral("scheduleTestingModeButton"),
        QStringLiteral("scheduleTestingClassesButton"),
        QStringLiteral("scheduleImportButton")
    };

    for (const QString& buttonName : buttonNames)
    {
        auto* button =
            widget.findChild<QPushButton*>(buttonName);
        QVERIFY(button);
        QVERIFY(dynamic_cast<TextFitPushButton*>(button));
        QVERIFY(
            button->minimumSizeHint().width()
            >= button->sizeHint().width()
            );
    }
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
    QVERIFY(monday->text().contains(QStringLiteral("Oral Testing")));
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
    ::testingModeDisplaysAssignedTestingClassCard()
{
    TestingClass testingClass;
    testingClass.classId = 100;
    testingClass.name = QStringLiteral("Writing Lab");
    testingClass.grade = QStringLiteral("M2");
    testingClass.level = QStringLiteral("Mixed (High)");
    testingClass.room = QStringLiteral("Library");
    testingClass.teacherId = 7;
    testingClass.classColor = QStringLiteral("#123456");
    testingClass.fontColor = QStringLiteral("#FFFFFF");
    ScheduleWidgetTestStubs::setTestingClassAssignment(
        QStringLiteral("Monday"),
        QStringLiteral("16:00"),
        testingClass
        );

    ApplicationServices services;
    services.dataService()->saveSetting(
        QStringLiteral("schedule_show_korean_teacher_english_names"),
        QStringLiteral("true")
        );
    ScheduleWidget widget(&services);
    auto* testingButton =
        widget.findChild<QPushButton*>(
            QStringLiteral("scheduleTestingModeButton")
            );
    auto* classesButton =
        widget.findChild<QPushButton*>(
            QStringLiteral("scheduleTestingClassesButton")
            );
    auto* table =
        widget.findChild<QTableWidget*>(
            QStringLiteral("scheduleTable")
            );
    QVERIFY(testingButton);
    QVERIFY(classesButton);
    QVERIFY(table);

    testingButton->click();
    QVERIFY(classesButton->isVisibleTo(&widget));

    auto* label =
        qobject_cast<QLabel*>(
            table->cellWidget(0, 1)
            );
    QVERIFY(label);
    QCOMPARE(label->property("class_id").toInt(), 100);
    QVERIFY(
        label->property("testing_class_assignment").toBool()
        );
    const QString accessible =
        label->accessibleName();
    QVERIFY(accessible.contains(QStringLiteral("Writing Lab")));
    QVERIFY(accessible.contains(QStringLiteral("M2")));
    QVERIFY(accessible.contains(QStringLiteral("Mixed (High)")));
    QVERIFY(accessible.contains(QStringLiteral("Library")));
    QVERIFY(
        !label->text().contains(
            QStringLiteral("Writing Lab Library")
            )
        );
    QCOMPARE(
        label->palette().color(QPalette::Highlight),
        QColor(QStringLiteral("#FFFFFF"))
        );
}

void ScheduleWidgetTests
    ::testingAssignmentDialogSupportsEveryAction()
{
    TestingClass testingClass;
    testingClass.classId = 100;
    testingClass.name = QStringLiteral("Writing Lab");
    testingClass.grade = QStringLiteral("M2");
    testingClass.level = QStringLiteral("Mixed (All)");
    testingClass.room = QStringLiteral("Library");
    ScheduleWidgetTestStubs::setTestingClassAssignment(
        QStringLiteral("Monday"),
        QStringLiteral("16:00"),
        testingClass
        );

    ApplicationServices services;
    TestingAssignmentDialog assignDialog(
        services.scheduleService(),
        nullptr
        );
    auto* mode =
        assignDialog.findChild<QComboBox*>(
            QStringLiteral("testingAssignmentModeCombo")
            );
    auto* classes =
        assignDialog.findChild<QComboBox*>(
            QStringLiteral("testingAssignmentClassCombo")
            );
    auto* classLabel =
        classes
            ? qobject_cast<QLabel*>(
                classes->parentWidget()->layout()->itemAt(0)->widget()
                )
            : nullptr;
    auto* buttons =
        assignDialog.findChild<QDialogButtonBox*>();
    QVERIFY(mode);
    QVERIFY(classes);
    QVERIFY(classLabel);
    QVERIFY(buttons);
    QCOMPARE(
        mode->itemText(0),
        QStringLiteral("Oral Testing Block")
        );
    QCOMPARE(
        mode->itemText(1),
        QStringLiteral("Testing Class")
        );
    QCOMPARE(
        mode->itemText(2),
        QStringLiteral("Essay Block")
        );
    QCOMPARE(
        assignDialog.height(),
        assignDialog.sizeHint().height()
        );
    QCOMPARE(assignDialog.minimumSize(), assignDialog.maximumSize());
    QCOMPARE(assignDialog.size(), assignDialog.minimumSize());
    QVERIFY(
        assignDialog.layout()->alignment().testFlag(
            Qt::AlignTop
            )
        );
    assignDialog.show();
    QApplication::processEvents();
    const int modeTop =
        mode->geometry().top();
    const int footerTop =
        buttons->geometry().top();
    assignDialog.resize(
        assignDialog.width(),
        assignDialog.height() + 200
        );
    QApplication::processEvents();
    QCOMPARE(assignDialog.height(), assignDialog.minimumHeight());
    QCOMPARE(mode->geometry().top(), modeTop);
    QCOMPARE(buttons->geometry().top(), footerTop);
    mode->setCurrentIndex(1);
    QApplication::processEvents();
    QCOMPARE(classes->currentData().toInt(), 100);
    QCOMPARE(
        classLabel->geometry().top(),
        8
        );
    QCOMPARE(
        classes->parentWidget()->height()
            - classes->geometry().bottom()
            - 1,
        8
        );
    auto* manage =
        assignDialog.findChild<QPushButton*>(
            QStringLiteral("testingAssignmentManageClassesButton")
            );
    QVERIFY(manage);
    QVERIFY(manage->isVisibleTo(&assignDialog));
    const QRect manageGeometry(
        manage->mapTo(&assignDialog, QPoint()),
        manage->size()
        );
    auto* saveButton =
        buttons->button(QDialogButtonBox::Save);
    QVERIFY(saveButton);
    const QRect saveGeometry(
        saveButton->mapTo(&assignDialog, QPoint()),
        saveButton->size()
        );
    QCOMPARE(manageGeometry.top(), buttons->geometry().top());
    QVERIFY(!manageGeometry.intersects(saveGeometry));
    buttons->button(QDialogButtonBox::Save)->click();
    QCOMPARE(assignDialog.result(), QDialog::Accepted);
    QCOMPARE(
        assignDialog.selectedAction(),
        TestingAssignmentDialog::Action::AssignTestingClass
        );
    QCOMPARE(assignDialog.selectedClassId(), 100);

    TestingAssignmentDialog manageDialog(
        services.scheduleService(),
        nullptr
        );
    mode =
        manageDialog.findChild<QComboBox*>(
            QStringLiteral("testingAssignmentModeCombo")
            );
    QVERIFY(mode);
    mode->setCurrentIndex(1);
    manage =
        manageDialog.findChild<QPushButton*>(
            QStringLiteral("testingAssignmentManageClassesButton")
            );
    QVERIFY(manage);
    QCOMPARE(manage->text(), QStringLiteral("Manage Classes"));
    manage->click();
    QCOMPARE(
        manageDialog.selectedAction(),
        TestingAssignmentDialog::Action::ManageTestingClasses
        );

    TestingAssignment existing;
    existing.kind = TestingAssignmentKind::SpecialClass;
    existing.classId = 100;
    TestingAssignmentDialog editDialog(
        services.scheduleService(),
        &existing
        );
    auto* removedEssayButton =
        editDialog.findChild<QPushButton*>(
            QStringLiteral("testingAssignmentRemoveButton")
            );
    QVERIFY(!removedEssayButton);
    mode =
        editDialog.findChild<QComboBox*>(
            QStringLiteral("testingAssignmentModeCombo")
            );
    buttons =
        editDialog.findChild<QDialogButtonBox*>();
    QVERIFY(mode);
    QVERIFY(buttons);
    mode->setCurrentIndex(2);
    buttons->button(QDialogButtonBox::Save)->click();
    QCOMPARE(
        editDialog.selectedAction(),
        TestingAssignmentDialog::Action::RemoveAssignment
        );

    TestingAssignmentDialog plainDialog(
        services.scheduleService(),
        nullptr
        );
    auto* room =
        plainDialog.findChild<QLineEdit*>(
            QStringLiteral("testingAssignmentRoomEdit")
            );
    buttons =
        plainDialog.findChild<QDialogButtonBox*>();
    QVERIFY(room);
    QVERIFY(buttons);
    room->setText(QStringLiteral("  402  "));
    buttons->button(QDialogButtonBox::Save)->click();
    QCOMPARE(
        plainDialog.selectedAction(),
        TestingAssignmentDialog::Action::SavePlainTesting
        );
    QCOMPARE(plainDialog.room(), QStringLiteral("402"));
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
        controls->findChildren<QAbstractButton*>();
    QVERIFY(controls->findChildren<QCheckBox*>().isEmpty());
    QCOMPARE(buttons.size(), 5);
    QVERIFY(
        std::any_of(
            buttons.cbegin(),
            buttons.cend(),
            [](const QAbstractButton* button)
            {
                return button->text()
                    == QStringLiteral("Import");
            }
            )
        );

    for (const QAbstractButton* button : buttons)
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
