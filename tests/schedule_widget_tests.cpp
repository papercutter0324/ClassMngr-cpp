#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "core/theme_service.h"
#include "data/data_service.h"
#include "fakes/fake_user_prompt_service.h"
#include "features/schedule/ui/schedule_page.h"
#include "features/schedule/ui/schedule_cell_hit_test.h"
#include "features/schedule/ui/schedule_table_renderer.h"
#include "features/schedule/ui/schedule_widget.h"
#include "features/schedule/services/schedule_output_controller.h"
#include "features/schedule/ui/testing_assignment_dialog.h"
#include "data/database/database_session.h"
#include "data/repositories/testing_class_repository.h"
#include "next/application/schedule_testing_class_choices_query.h"
#include "next/application/schedule_testing_assignment_save.h"
#include "next/platform/application_services_schedule_testing_class_choices_read_port.h"
#include "next/platform/application_services_schedule_display_preferences_port.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/widgets/text_fit_push_button.h"
#include "domain/models/testing_class.h"

#include <QtTest>

#include <QAbstractButton>
#include <QAbstractItemDelegate>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QHeaderView>
#include <QImage>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStringList>
#include <QStyleOptionViewItem>
#include <QTableWidget>
#include <QTimer>
#include <QTemporaryDir>
#include <QUuid>

#include <algorithm>
#include <functional>
#include <string>
#include <utility>

namespace ScheduleWidgetTestStubs
{
extern int savedSlotStates;
extern int scheduleClassInfoReadCount;
extern int slotStateReadCount;
extern int testingAssignmentsReadCount;
extern int testingAssignmentWriteCount;
extern ClassMngr::Next::Application::
    ScheduleTestingAssignmentMutation lastTestingAssignmentMutation;
extern QString lastTestingAssignmentWriteDay;
extern QString lastTestingAssignmentWriteStartTime;
extern QString lastTestingAssignmentWriteRoom;
extern int lastTestingAssignmentWriteClassId;
extern bool lastTestingAssignmentWriteReplaceExisting;
extern QString lastSavedSlotDay;
extern QString lastSavedSlotStartTime;
extern QString lastSavedSlotState;
extern QString lastSavedSlotDefaultState;
extern int printRequestCount;
extern bool lastPrintRequestShowsEnglishNames;
extern Theme lastPrintRequestTheme;
extern QString lastPrintRequestUserName;
void reset();
void setCurrentTheme(Theme theme);
void setDatabaseOpen(bool open);
void setIntensiveSlotStates(QList<IntensiveSlotState> states);
void setIntensiveSlotStateReadFailure(const QString& error);
void setIntensiveSlotStateRepositoryAvailable(bool available);
void setTestingAssignmentReadFailure(const QString& error);
void setTestingAssignmentRepositoryAvailable(bool available);
void setTestingAssignmentWriteFailure(const QString& error);
void setSlotSaveFailure(const QString& error);
void setScheduleClassInfoReadFailure(bool fail);
void setExistingIntensiveHours(bool exists);
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
void setTestingClass(const TestingClass& testingClass);
void setUnresolvedTestingClassAssignment(
    const QString& day,
    const QString& startTime,
    int classId
    );
QString settingValue(const QString& key);
}

namespace
{
class ScopedQtWarningCapture final
{
public:
    explicit ScopedQtWarningCapture(QStringList& messages)
        : m_messages(messages)
        , m_previousHandler(qInstallMessageHandler(&handleMessage))
    {
        s_activeCapture = this;
    }

    ~ScopedQtWarningCapture()
    {
        s_activeCapture = nullptr;
        qInstallMessageHandler(m_previousHandler);
    }

    ScopedQtWarningCapture(const ScopedQtWarningCapture&) = delete;
    ScopedQtWarningCapture& operator=(const ScopedQtWarningCapture&) = delete;

private:
    static void handleMessage(
        const QtMsgType type,
        const QMessageLogContext& context,
        const QString& message
        )
    {
        if (s_activeCapture && type == QtWarningMsg)
        {
            s_activeCapture->m_messages.append(message);
            return;
        }

        if (s_activeCapture && s_activeCapture->m_previousHandler)
        {
            s_activeCapture->m_previousHandler(type, context, message);
        }
    }

    QStringList& m_messages;
    QtMessageHandler m_previousHandler = nullptr;
    static ScopedQtWarningCapture* s_activeCapture;
};

ScopedQtWarningCapture* ScopedQtWarningCapture::s_activeCapture = nullptr;

void saveSettingOrFail(
    DataService* dataService,
    const QString& key,
    const QVariant& value
    )
{
    QVERIFY(dataService);
    QVERIFY(dataService->saveSetting(key, value).has_value());
}

class ScheduleOutputDisplayNameUpdate final : public QObject
{
public:
    explicit ScheduleOutputDisplayNameUpdate(
        ApplicationServices& services
        )
        : m_services(services)
    {
    }

    [[nodiscard]] bool updated() const
    {
        return m_updated;
    }

protected:
    bool eventFilter(
        QObject* watched,
        QEvent* event
        ) override
    {
        auto* dialog = qobject_cast<QDialog*>(watched);
        if (
            event
            && event->type() == QEvent::Show
            && dialog
            && dialog->objectName()
                == QStringLiteral("schedulePrintDialog")
            && !m_connected
            )
        {
            m_connected = true;
            QObject::connect(
                dialog,
                &QDialog::accepted,
                [this]
                {
                    const auto saved =
                        m_services.dataService()->saveSetting(
                            QStringLiteral("myInfo/name"),
                            QStringLiteral("  Updated After Acceptance  ")
                            );
                    m_updated = saved.has_value();
                }
                );
        }

        return QObject::eventFilter(watched, event);
    }

private:
    ApplicationServices& m_services;
    bool m_updated = false;
    bool m_connected = false;
};

ScheduleViewModel rendererTestModel(
    const QString& mondaySlotState = scheduleEssaySlotState()
    )
{
    ScheduleViewModel model;
    model.days = {
        QStringLiteral("Monday"),
        QStringLiteral("Tuesday")
    };

    ScheduleRowView row;
    row.timeLabel = QStringLiteral("16:00");
    row.timeRangeLabel = QStringLiteral("4:00 PM - 4:50 PM");

    ScheduleCellView monday;
    monday.day = QStringLiteral("Monday");
    monday.timeLabel = row.timeLabel;
    monday.defaultSlotState = scheduleEssaySlotState();
    monday.slotState = mondaySlotState;
    monday.slotTogglingEnabled = true;

    ScheduleCellView tuesday;
    tuesday.day = QStringLiteral("Tuesday");
    tuesday.timeLabel = row.timeLabel;
    tuesday.defaultSlotState = scheduleEmptySlotState();
    tuesday.slotState = scheduleEmptySlotState();
    ScheduleEntry entry;
    entry.classId = 42;
    entry.teacherKr = QStringLiteral("김선생");
    entry.teacherEn = QStringLiteral("Susan");
    entry.roomNumber = QStringLiteral("413");
    entry.classGrade = QStringLiteral("E4");
    entry.classLevel = QStringLiteral("Hercules");
    tuesday.entries.append(entry);

    row.cells = {monday, tuesday};
    model.rows.append(row);
    return model;
}

const ScheduleCellView* findScheduleCell(
    const ScheduleViewModel& model,
    const QString& day,
    const QString& timeLabel
    )
{
    for (const ScheduleRowView& row : model.rows)
    {
        if (row.timeLabel != timeLabel)
        {
            continue;
        }
        for (const ScheduleCellView& cell : row.cells)
        {
            if (cell.day == day)
            {
                return &cell;
            }
        }
    }
    return nullptr;
}

using TestingAssignmentDialogScript =
    std::function<bool(TestingAssignmentDialog*)>;

bool interactWithTestingAssignmentCell(
    ScheduleWidget& widget,
    const QString& day,
    const QString& timeLabel,
    const TestingAssignmentDialogScript& script
    )
{
    auto* table = widget.findChild<QTableWidget*>(
        QStringLiteral("scheduleTable")
        );
    if (!table)
    {
        return false;
    }

    const ScheduleViewModel model = widget.scheduleModel();
    const int column = model.days.indexOf(day) + 1;
    int row = -1;
    for (qsizetype index = 0; index < model.rows.size(); ++index)
    {
        if (model.rows.at(index).timeLabel == timeLabel)
        {
            row = static_cast<int>(index);
            break;
        }
    }
    if (column <= 0 || row < 0)
    {
        return false;
    }

    QWidget* const cell = table->cellWidget(row, column);
    if (
        ScheduleCellHitTest::hit(cell).command
        != ScheduleCellCommand::EditTestingAssignment
        )
    {
        return false;
    }

    bool dialogFound = false;
    bool scriptSucceeded = false;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(
        &timer,
        &QTimer::timeout,
        &widget,
        [&]
        {
            for (QWidget* topLevel : QApplication::topLevelWidgets())
            {
                auto* dialog = qobject_cast<TestingAssignmentDialog*>(
                    topLevel
                    );
                if (!dialog)
                {
                    continue;
                }

                dialogFound = true;
                scriptSucceeded = script(dialog);
                if (!scriptSucceeded)
                {
                    dialog->reject();
                }
                return;
            }

            if (auto* modal = qobject_cast<TestingAssignmentDialog*>(
                    QApplication::activeModalWidget()
                    ))
            {
                modal->reject();
            }
        }
        );
    timer.start(0);
    const bool invoked = QMetaObject::invokeMethod(
        &widget,
        "onCellClicked",
        Qt::DirectConnection,
        Q_ARG(int, row),
        Q_ARG(int, column)
        );
    timer.stop();

    return invoked && dialogFound && scriptSucceeded;
}

bool clickTestingAssignmentDialogButton(
    TestingAssignmentDialog* dialog,
    const QDialogButtonBox::StandardButton standardButton
    )
{
    auto* buttons = dialog
        ? dialog->findChild<QDialogButtonBox*>()
        : nullptr;
    auto* button = buttons ? buttons->button(standardButton) : nullptr;
    if (!button)
    {
        return false;
    }
    button->click();
    return true;
}

bool chooseTestingClass(
    TestingAssignmentDialog* dialog,
    const int classId
    )
{
    auto* mode = dialog
        ? dialog->findChild<QComboBox*>(
            QStringLiteral("testingAssignmentModeCombo")
            )
        : nullptr;
    auto* classes = dialog
        ? dialog->findChild<QComboBox*>(
            QStringLiteral("testingAssignmentClassCombo")
            )
        : nullptr;
    auto* manage = dialog
        ? dialog->findChild<QPushButton*>(
            QStringLiteral("testingAssignmentManageClassesButton")
            )
        : nullptr;
    if (!mode || !classes || !manage)
    {
        return false;
    }

    mode->setCurrentIndex(1);
    const int classIndex = classes->findData(classId);
    if (classIndex < 0)
    {
        return false;
    }
    classes->setCurrentIndex(classIndex);
    return true;
}

ClassMngr::Next::Application::ScheduleTestingClassChoice testingClassChoice(
    const int classId,
    const QString& name,
    const QString& grade,
    const QString& level,
    const QString& room
    )
{
    const auto typedClassId = ClassMngr::Next::Domain::ClassId::fromString(
        std::to_string(classId)
        );
    if (!typedClassId)
    {
        qFatal("Test class ID must have a typed representation.");
    }

    return {
        .classId = *typedClassId,
        .name = name.toStdU16String(),
        .grade = grade.toStdU16String(),
        .level = level.toStdU16String(),
        .room = room.toStdU16String()
    };
}

QString scheduleWidgetDatabasePath(QTemporaryDir& directory)
{
    return directory.filePath(
        QStringLiteral("schedule-widget-testing-classes-%1.tps").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)
            )
        );
}

bool insertTestingClassRecord(
    QSqlDatabase database,
    const TestingClass& testingClass
    )
{
    QSqlQuery classQuery(database);
    classQuery.prepare(QStringLiteral(
        "INSERT INTO classes (id, name) VALUES (?, ?)"
        ));
    classQuery.addBindValue(testingClass.classId);
    classQuery.addBindValue(testingClass.name);
    if (!classQuery.exec())
    {
        return false;
    }

    QSqlQuery informationQuery(database);
    informationQuery.prepare(QStringLiteral(R"(
        INSERT INTO class_info (
            class_id, teacher_id, class_grade, class_level,
            class_color, font_color
        ) VALUES (?, NULL, ?, ?, ?, ?)
    )"));
    informationQuery.addBindValue(testingClass.classId);
    informationQuery.addBindValue(testingClass.grade);
    informationQuery.addBindValue(testingClass.level);
    informationQuery.addBindValue(testingClass.classColor.isEmpty()
        ? QStringLiteral("#FFFFFF")
        : testingClass.classColor);
    informationQuery.addBindValue(testingClass.fontColor.isEmpty()
        ? QStringLiteral("#000000")
        : testingClass.fontColor);
    if (!informationQuery.exec())
    {
        return false;
    }

    QSqlQuery testingClassQuery(database);
    testingClassQuery.prepare(QStringLiteral(
        "INSERT INTO testing_classes (class_id, room) VALUES (?, ?)"
        ));
    testingClassQuery.addBindValue(testingClass.classId);
    testingClassQuery.addBindValue(testingClass.room);
    return testingClassQuery.exec();
}

bool seedTestingClassRecords(
    ApplicationServices& services,
    const QList<TestingClass>& testingClasses
    )
{
    DatabaseSession* const session = services.databaseSession();
    if (!session || !session->isOpen())
    {
        return false;
    }

    const QSqlDatabase database = session->database();
    for (const TestingClass& testingClass : testingClasses)
    {
        if (!insertTestingClassRecord(database, testingClass))
        {
            return false;
        }
    }
    return true;
}

class ApplicationServicesDatabaseGuard final
{
public:
    explicit ApplicationServicesDatabaseGuard(
        ApplicationServices& services
        ) noexcept
        : m_services(services)
    {
    }

    ~ApplicationServicesDatabaseGuard()
    {
        m_services.closeDatabase();
    }

private:
    ApplicationServices& m_services;
};

class FixedTestingClassChoicesReadPort final
    : public ClassMngr::Next::Application::
        ScheduleTestingClassChoicesReadPort
{
public:
    [[nodiscard]] ClassMngr::Next::Application::
        ScheduleTestingClassChoicesReadResult readTestingClassChoices(
            const ClassMngr::Next::Application::
                ScheduleTestingClassChoicesReadQuery& query
            ) const override
    {
        ++callCount;
        lastQuery = query;
        return result;
    }

    mutable int callCount = 0;
    mutable ClassMngr::Next::Application::
        ScheduleTestingClassChoicesReadQuery lastQuery;
    ClassMngr::Next::Application::ScheduleTestingClassChoicesReadResult
        result = ClassMngr::Next::Application::
            ScheduleTestingClassChoicesReadResult::success({});
};

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
    void printPropagatesCurrentTheme();
    void outputReadsDisplayNameAfterDialogAcceptanceWithoutTrimming();
    void outputUsesEmptyNameWhenPreferencesAreUnavailableOrServicesAreNull();
    void importButtonRequestsScheduleImport();
    void controlsUseTextFitButtons();
    void legacyHourSettingsDoNotCarryForward();
    void testingModeFiltersClassesAndDisplaysSavedBlock();
    void testingModeDisplaysAssignedTestingClassCard();
    void testingAssignmentDialogPreservesChoiceOrderLabelsAndPreselection();
    void emptyAndUnavailableTestingClassChoicesKeepNoSelectionWarning();
    void testingClassChoiceRepositoryFailureRetainsWarning();
    void testingAssignmentDialogSupportsEveryAction();
    void testingAssignmentWritesMapActionsAndReloadSuccessfulState();
    void testingAssignmentCancelAndManageDoNotWriteOrReload();
    void failedTestingAssignmentWriteWarnsAndDoesNotReload();
    void readOnlyPresentationHidesControlsAndIgnoresClicks();
    void sourceQueryBuildsRegularIntensiveAndTestingSchedules();
    void unavailableSourceReturnsDaysOnlyWithoutReading();
    void failedSourceReadLogsAndKeepsIndependentScheduleReads();
    void previewModelBypassesSourceAndScheduleReads();
    void failedTestingAssignmentReadWarnsAndRetainsAssignments();
    void unavailableTestingAssignmentReadClearsAssignments();
    void missingTestingClassWarnsAndSkipsOnlyThatAssignment();
    void slotStateSuccessReplacesAndClearsOverrides();
    void unavailableSlotStateReadIsSilentAndRetainsOverrides();
    void failedSlotStateReadWarnsAndRetainsOverrides();
    void readOnlyScheduleStillLoadsSlotStates();
    void timeColumnAndHeaderAreNonInteractive();
    void intensiveSlotToggleMapsAndPersistsViewModelChoice();
    void failedSlotToggleWarnsWithoutReloading();
    void unavailableSlotToggleSkipsWriteAndReloads();
    void regularSlotToggleUsesSharedPersistenceHandler();
    void clearDatabaseStateRemovesLoadedDataAndSettings();
    void schedulePageScrollsWithoutResizingSchedule();
    void rendererSkipsUnchangedAndUpdatesOnlyChangedCells();
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
    ScheduleWidgetTestStubs::setExistingIntensiveHours(true);
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

    ClassMngr::Next::Platform::
        ApplicationServicesScheduleDisplayPreferencesPort preferencesPort(
            services
            );
    QVERIFY(preferencesPort.save({
        .use24HourTime = true,
        .showEnglishNames = true,
        .showWeekends = true,
        .showAllIntensiveHours = true,
        .testingAffectsM1 = true
    }));
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
    QVERIFY2(
        scheduledClass->text().contains(QStringLiteral("Susan")),
        qPrintable(scheduledClass->text())
        );
    QVERIFY(!scheduledClass->text().contains(QStringLiteral("김선생")));

    ScheduleWidget mirrored(
        &services,
        nullptr,
        ScheduleMode::ReadOnly
        );
    mirrored.refreshSchedule();

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
    saveSettingOrFail(services.dataService(),
        QStringLiteral("schedule_show_korean_teacher_english_names"),
        QStringLiteral("true")
        );
    ScheduleWidget widget(&services);
    widget.refreshSchedule();

    widget.printSchedule();
    QCOMPARE(ScheduleWidgetTestStubs::printRequestCount, 1);
    QVERIFY(ScheduleWidgetTestStubs::lastPrintRequestShowsEnglishNames);
    QCOMPARE(
        ScheduleWidgetTestStubs::lastPrintRequestTheme,
        Theme::Dark
        );

    saveSettingOrFail(services.dataService(),
        QStringLiteral("schedule_show_korean_teacher_english_names"),
        QStringLiteral("false")
        );
    widget.refreshSchedule();
    widget.printSchedule();
    QCOMPARE(ScheduleWidgetTestStubs::printRequestCount, 2);
    QVERIFY(!ScheduleWidgetTestStubs::lastPrintRequestShowsEnglishNames);
    QCOMPARE(
        ScheduleWidgetTestStubs::lastPrintRequestTheme,
        Theme::Dark
        );
}

void ScheduleWidgetTests::printPropagatesCurrentTheme()
{
    ApplicationServices services;
    ScheduleWidget widget(&services);

    ScheduleWidgetTestStubs::setCurrentTheme(Theme::Light);
    widget.printSchedule();
    QCOMPARE(
        ScheduleWidgetTestStubs::lastPrintRequestTheme,
        Theme::Light
        );

    ScheduleWidgetTestStubs::setCurrentTheme(Theme::Dark);
    widget.printSchedule();
    QCOMPARE(
        ScheduleWidgetTestStubs::lastPrintRequestTheme,
        Theme::Dark
        );
}

void ScheduleWidgetTests
    ::outputReadsDisplayNameAfterDialogAcceptanceWithoutTrimming()
{
    ApplicationServices services;
    saveSettingOrFail(
        services.dataService(),
        QStringLiteral("myInfo/name"),
        QStringLiteral("Stored Before Dialog")
        );
    ScheduleOutputDisplayNameUpdate updateNameOnDialogShow(services);
    qApp->installEventFilter(&updateNameOnDialogShow);

    ScheduleOutputController::execute(
        ScheduleOutputController::Action::Print,
        nullptr,
        &services,
        rendererTestModel(),
        Theme::Light,
        false
        );

    qApp->removeEventFilter(&updateNameOnDialogShow);
    QVERIFY(updateNameOnDialogShow.updated());
    QCOMPARE(ScheduleWidgetTestStubs::printRequestCount, 1);
    QCOMPARE(
        ScheduleWidgetTestStubs::lastPrintRequestUserName,
        QStringLiteral("  Updated After Acceptance  ")
        );
}

void ScheduleWidgetTests
    ::outputUsesEmptyNameWhenPreferencesAreUnavailableOrServicesAreNull()
{
    ApplicationServices services;
    saveSettingOrFail(
        services.dataService(),
        QStringLiteral("myInfo/name"),
        QStringLiteral("Stored Name")
        );
    ScheduleWidgetTestStubs::setDatabaseOpen(false);

    ScheduleOutputController::execute(
        ScheduleOutputController::Action::Print,
        nullptr,
        &services,
        rendererTestModel(),
        Theme::Light,
        false
        );

    QVERIFY(ScheduleWidgetTestStubs::lastPrintRequestUserName.isEmpty());

    ScheduleOutputController::execute(
        ScheduleOutputController::Action::Print,
        nullptr,
        nullptr,
        rendererTestModel(),
        Theme::Light,
        false
        );

    QVERIFY(ScheduleWidgetTestStubs::lastPrintRequestUserName.isEmpty());
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
    saveSettingOrFail(services.dataService(),
        QStringLiteral("schedule_show_all_hours"),
        QStringLiteral("true")
        );
    saveSettingOrFail(services.dataService(),
        QStringLiteral("schedule_hide_empty_rows"),
        QStringLiteral("false")
        );

    ScheduleWidget widget(&services);
    widget.refreshSchedule();

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
    widget.refreshSchedule();
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

    saveSettingOrFail(services.dataService(),
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
    saveSettingOrFail(services.dataService(),
        QStringLiteral("schedule_show_korean_teacher_english_names"),
        QStringLiteral("true")
        );
    ScheduleWidget widget(&services);
    widget.refreshSchedule();
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

void ScheduleWidgetTests::
testingAssignmentDialogPreservesChoiceOrderLabelsAndPreselection()
{
    FixedTestingClassChoicesReadPort choicesReadPort;
    choicesReadPort.result =
        ClassMngr::Next::Application::
            ScheduleTestingClassChoicesReadResult::success({
                .choices = {
                    testingClassChoice(
                        42,
                        QStringLiteral("Writing Lab"),
                        QStringLiteral("M2"),
                        QStringLiteral("Mixed (High)"),
                        QStringLiteral("Library")
                        ),
                    testingClassChoice(
                        7,
                        QStringLiteral("Oral Review"),
                        QStringLiteral("E5"),
                        QStringLiteral("Lower"),
                        QStringLiteral("204")
                        )
                }
            });

    TestingAssignmentDialog assignDialog(choicesReadPort, nullptr);
    auto* classes = assignDialog.findChild<QComboBox*>(
        QStringLiteral("testingAssignmentClassCombo")
        );
    auto* mode = assignDialog.findChild<QComboBox*>(
        QStringLiteral("testingAssignmentModeCombo")
        );
    auto* buttons = assignDialog.findChild<QDialogButtonBox*>();
    QVERIFY(classes);
    QVERIFY(mode);
    QVERIFY(buttons);
    QCOMPARE(classes->count(), 2);
    QCOMPARE(
        classes->itemText(0),
        QStringLiteral("Writing Lab \u2014 M2 Mixed (High) \u2014 Room Library")
        );
    QCOMPARE(classes->itemData(0).toInt(), 42);
    QCOMPARE(
        classes->itemText(1),
        QStringLiteral("Oral Review \u2014 E5 Lower \u2014 Room 204")
        );
    QCOMPARE(classes->itemData(1).toInt(), 7);

    mode->setCurrentIndex(1);
    classes->setCurrentIndex(1);
    buttons->button(QDialogButtonBox::Save)->click();
    QCOMPARE(
        assignDialog.selectedAction(),
        TestingAssignmentDialog::Action::AssignTestingClass
        );
    QCOMPARE(assignDialog.selectedClassId(), 7);

    TestingAssignment existing;
    existing.kind = TestingAssignmentKind::SpecialClass;
    existing.classId = 42;
    TestingAssignmentDialog editDialog(choicesReadPort, &existing);
    classes = editDialog.findChild<QComboBox*>(
        QStringLiteral("testingAssignmentClassCombo")
        );
    mode = editDialog.findChild<QComboBox*>(
        QStringLiteral("testingAssignmentModeCombo")
        );
    QVERIFY(classes);
    QVERIFY(mode);
    QCOMPARE(mode->currentIndex(), 1);
    QCOMPARE(classes->currentIndex(), 0);
    QCOMPARE(classes->currentData().toInt(), 42);
    QCOMPARE(editDialog.selectedClassId(), 42);
    QCOMPARE(choicesReadPort.callCount, 2);
}

void ScheduleWidgetTests::
emptyAndUnavailableTestingClassChoicesKeepNoSelectionWarning()
{
    FixedTestingClassChoicesReadPort emptyPort;
    emptyPort.result =
        ClassMngr::Next::Application::
            ScheduleTestingClassChoicesReadResult::success({});
    FakeUserPromptService emptyPrompts;
    DialogServices::setUserPromptServiceForTesting(&emptyPrompts);
    TestingAssignmentDialog emptyDialog(emptyPort, nullptr);
    auto* classes = emptyDialog.findChild<QComboBox*>(
        QStringLiteral("testingAssignmentClassCombo")
        );
    auto* mode = emptyDialog.findChild<QComboBox*>(
        QStringLiteral("testingAssignmentModeCombo")
        );
    auto* buttons = emptyDialog.findChild<QDialogButtonBox*>();
    QVERIFY(classes);
    QVERIFY(mode);
    QVERIFY(buttons);
    QCOMPARE(classes->count(), 0);
    QVERIFY(emptyPrompts.messages.isEmpty());
    mode->setCurrentIndex(1);
    buttons->button(QDialogButtonBox::Save)->click();
    QCOMPARE(emptyPrompts.messages.size(), 1);
    QCOMPARE(emptyPrompts.messages.constFirst().title,
             QStringLiteral("Testing Assignment"));
    QCOMPARE(emptyPrompts.messages.constFirst().severity,
             PromptSeverity::Warning);
    QCOMPARE(emptyPrompts.messages.constFirst().message,
             QStringLiteral("Choose a testing class or manage your classes."));

    FixedTestingClassChoicesReadPort unavailablePort;
    unavailablePort.result =
        ClassMngr::Next::Application::
            ScheduleTestingClassChoicesReadResult::failure({
                .code = ClassMngr::Next::Domain::ErrorCode::NotFound,
                .message = "The active database session is unavailable.",
                .recoverable = false
            });
    FakeUserPromptService unavailablePrompts;
    DialogServices::setUserPromptServiceForTesting(&unavailablePrompts);
    TestingAssignmentDialog unavailableDialog(unavailablePort, nullptr);
    classes = unavailableDialog.findChild<QComboBox*>(
        QStringLiteral("testingAssignmentClassCombo")
        );
    mode = unavailableDialog.findChild<QComboBox*>(
        QStringLiteral("testingAssignmentModeCombo")
        );
    buttons = unavailableDialog.findChild<QDialogButtonBox*>();
    QVERIFY(classes);
    QVERIFY(mode);
    QVERIFY(buttons);
    QCOMPARE(classes->count(), 0);
    QVERIFY(unavailablePrompts.messages.isEmpty());
    mode->setCurrentIndex(1);
    buttons->button(QDialogButtonBox::Save)->click();
    QCOMPARE(unavailablePrompts.messages.size(), 1);
    QCOMPARE(unavailablePrompts.messages.constFirst().title,
             QStringLiteral("Testing Assignment"));
    QCOMPARE(unavailablePrompts.messages.constFirst().message,
             QStringLiteral("Choose a testing class or manage your classes."));
}

void ScheduleWidgetTests::
testingClassChoiceRepositoryFailureRetainsWarning()
{
    FixedTestingClassChoicesReadPort failedPort;
    failedPort.result =
        ClassMngr::Next::Application::
            ScheduleTestingClassChoicesReadResult::failure({
                .code = ClassMngr::Next::Domain::ErrorCode::Technical,
                .message = "injected testing class choices read failure",
                .recoverable = true
            });
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);

    TestingAssignmentDialog dialog(failedPort, nullptr);

    auto* classes = dialog.findChild<QComboBox*>(
        QStringLiteral("testingAssignmentClassCombo")
        );
    QVERIFY(classes);
    QCOMPARE(classes->count(), 0);
    QCOMPARE(failedPort.callCount, 1);
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.constFirst().title,
             QStringLiteral("Testing Classes"));
    QCOMPARE(prompts.messages.constFirst().severity,
             PromptSeverity::Warning);
    QCOMPARE(prompts.messages.constFirst().message,
             QStringLiteral("injected testing class choices read failure"));
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
    FixedTestingClassChoicesReadPort choicesReadPort;
    choicesReadPort.result =
        ClassMngr::Next::Application::
            ScheduleTestingClassChoicesReadResult::success({
                .choices = {
                    testingClassChoice(
                        testingClass.classId,
                        testingClass.name,
                        testingClass.grade,
                        testingClass.level,
                        testingClass.room
                        )
                }
            });
    TestingAssignmentDialog assignDialog(
        choicesReadPort,
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
        choicesReadPort,
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
    QCOMPARE(manageDialog.selectedClassId(), 100);
    QCOMPARE(
        manageDialog.selectedAction(),
        TestingAssignmentDialog::Action::ManageTestingClasses
        );

    TestingAssignment existing;
    existing.kind = TestingAssignmentKind::SpecialClass;
    existing.classId = 100;
    TestingAssignmentDialog editDialog(
        choicesReadPort,
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
        choicesReadPort,
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

void ScheduleWidgetTests::
testingAssignmentWritesMapActionsAndReloadSuccessfulState()
{
    ScheduleWidgetTestStubs::setIncludeMiddleSchoolClasses(true);
    TestingClass testingClass;
    testingClass.classId = 100;
    testingClass.name = QStringLiteral("Writing Lab");
    testingClass.grade = QStringLiteral("M2");
    testingClass.level = QStringLiteral("Mixed (High)");
    testingClass.room = QStringLiteral("Library");
    ScheduleWidgetTestStubs::setTestingClass(testingClass);

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(scheduleWidgetDatabasePath(directory)));
    ApplicationServicesDatabaseGuard databaseGuard(services);
    QVERIFY(seedTestingClassRecords(services, {testingClass}));
    saveSettingOrFail(
        services.dataService(),
        QStringLiteral("schedule_display_mode"),
        QStringLiteral("testing")
        );
    saveSettingOrFail(
        services.dataService(),
        QStringLiteral("schedule_testing_affects_m1"),
        QStringLiteral("true")
        );
    ScheduleWidget widget(&services);
    widget.refreshSchedule();

    const QString day = QStringLiteral("Wednesday");
    const QString startTime = QStringLiteral("16:00");
    ScheduleViewModel model = widget.scheduleModel();
    const ScheduleCellView* initialCell = findScheduleCell(
        model,
        day,
        startTime
        );
    QVERIFY(initialCell);
    QVERIFY(initialCell->testingBlockCreationEnabled);
    int readCount = ScheduleWidgetTestStubs::testingAssignmentsReadCount;

    QVERIFY(interactWithTestingAssignmentCell(
        widget,
        day,
        startTime,
        [](TestingAssignmentDialog* dialog)
        {
            return chooseTestingClass(dialog, 100)
                && clickTestingAssignmentDialogButton(
                    dialog,
                    QDialogButtonBox::Save
                    );
        }
        ));
    QCOMPARE(ScheduleWidgetTestStubs::testingAssignmentWriteCount, 1);
    QCOMPARE(
        ScheduleWidgetTestStubs::lastTestingAssignmentMutation,
        ClassMngr::Next::Application::
            ScheduleTestingAssignmentMutation::AssignTestingClass
        );
    QCOMPARE(ScheduleWidgetTestStubs::lastTestingAssignmentWriteDay, day);
    QCOMPARE(
        ScheduleWidgetTestStubs::lastTestingAssignmentWriteStartTime,
        startTime
        );
    QCOMPARE(ScheduleWidgetTestStubs::lastTestingAssignmentWriteRoom,
             QString());
    QCOMPARE(ScheduleWidgetTestStubs::lastTestingAssignmentWriteClassId, 100);
    QVERIFY(!ScheduleWidgetTestStubs::lastTestingAssignmentWriteReplaceExisting);
    ++readCount;
    QCOMPARE(ScheduleWidgetTestStubs::testingAssignmentsReadCount, readCount);
    model = widget.scheduleModel();
    const ScheduleCellView* assignedClass = findScheduleCell(
        model,
        day,
        startTime
        );
    QVERIFY(assignedClass);
    QVERIFY(assignedClass->testingClassAssignment);
    QCOMPARE(assignedClass->testingClassId, 100);

    QVERIFY(interactWithTestingAssignmentCell(
        widget,
        day,
        startTime,
        [](TestingAssignmentDialog* dialog)
        {
            auto* mode = dialog->findChild<QComboBox*>(
                QStringLiteral("testingAssignmentModeCombo")
                );
            auto* room = dialog->findChild<QLineEdit*>(
                QStringLiteral("testingAssignmentRoomEdit")
                );
            if (!mode || !room)
            {
                return false;
            }
            mode->setCurrentIndex(0);
            room->setText(QStringLiteral("  Room 9  "));
            return clickTestingAssignmentDialogButton(
                dialog,
                QDialogButtonBox::Save
                );
        }
        ));
    QCOMPARE(ScheduleWidgetTestStubs::testingAssignmentWriteCount, 2);
    QCOMPARE(
        ScheduleWidgetTestStubs::lastTestingAssignmentMutation,
        ClassMngr::Next::Application::
            ScheduleTestingAssignmentMutation::SavePlainTesting
        );
    QCOMPARE(ScheduleWidgetTestStubs::lastTestingAssignmentWriteDay, day);
    QCOMPARE(
        ScheduleWidgetTestStubs::lastTestingAssignmentWriteStartTime,
        startTime
        );
    QCOMPARE(ScheduleWidgetTestStubs::lastTestingAssignmentWriteRoom,
             QStringLiteral("Room 9"));
    QCOMPARE(ScheduleWidgetTestStubs::lastTestingAssignmentWriteClassId, -1);
    QVERIFY(ScheduleWidgetTestStubs::lastTestingAssignmentWriteReplaceExisting);
    ++readCount;
    QCOMPARE(ScheduleWidgetTestStubs::testingAssignmentsReadCount, readCount);
    model = widget.scheduleModel();
    const ScheduleCellView* assignedPlain = findScheduleCell(
        model,
        day,
        startTime
        );
    QVERIFY(assignedPlain);
    QVERIFY(!assignedPlain->testingClassAssignment);
    QCOMPARE(assignedPlain->testingRoom, QStringLiteral("Room 9"));

    QVERIFY(interactWithTestingAssignmentCell(
        widget,
        day,
        startTime,
        [](TestingAssignmentDialog* dialog)
        {
            return chooseTestingClass(dialog, 100)
                && clickTestingAssignmentDialogButton(
                    dialog,
                    QDialogButtonBox::Save
                    );
        }
        ));
    QCOMPARE(ScheduleWidgetTestStubs::testingAssignmentWriteCount, 3);
    QCOMPARE(
        ScheduleWidgetTestStubs::lastTestingAssignmentMutation,
        ClassMngr::Next::Application::
            ScheduleTestingAssignmentMutation::AssignTestingClass
        );
    QCOMPARE(ScheduleWidgetTestStubs::lastTestingAssignmentWriteClassId, 100);
    QVERIFY(ScheduleWidgetTestStubs::lastTestingAssignmentWriteReplaceExisting);
    ++readCount;
    QCOMPARE(ScheduleWidgetTestStubs::testingAssignmentsReadCount, readCount);

    QVERIFY(interactWithTestingAssignmentCell(
        widget,
        day,
        startTime,
        [](TestingAssignmentDialog* dialog)
        {
            auto* mode = dialog->findChild<QComboBox*>(
                QStringLiteral("testingAssignmentModeCombo")
                );
            if (!mode)
            {
                return false;
            }
            mode->setCurrentIndex(2);
            return clickTestingAssignmentDialogButton(
                dialog,
                QDialogButtonBox::Save
                );
        }
        ));
    QCOMPARE(ScheduleWidgetTestStubs::testingAssignmentWriteCount, 4);
    QCOMPARE(
        ScheduleWidgetTestStubs::lastTestingAssignmentMutation,
        ClassMngr::Next::Application::
            ScheduleTestingAssignmentMutation::RemoveAssignment
        );
    QCOMPARE(ScheduleWidgetTestStubs::lastTestingAssignmentWriteDay, day);
    QCOMPARE(
        ScheduleWidgetTestStubs::lastTestingAssignmentWriteStartTime,
        startTime
        );
    QCOMPARE(ScheduleWidgetTestStubs::lastTestingAssignmentWriteClassId, -1);
    ++readCount;
    QCOMPARE(ScheduleWidgetTestStubs::testingAssignmentsReadCount, readCount);
    model = widget.scheduleModel();
    const ScheduleCellView* removed = findScheduleCell(
        model,
        day,
        startTime
        );
    QVERIFY(removed);
    QVERIFY(!removed->testingClassAssignment);
    QVERIFY(removed->testingRoom.isEmpty());

    QVERIFY(interactWithTestingAssignmentCell(
        widget,
        day,
        startTime,
        [](TestingAssignmentDialog* dialog)
        {
            auto* room = dialog->findChild<QLineEdit*>(
                QStringLiteral("testingAssignmentRoomEdit")
                );
            if (!room)
            {
                return false;
            }
            room->setText(QStringLiteral("Room 3"));
            return clickTestingAssignmentDialogButton(
                dialog,
                QDialogButtonBox::Save
                );
        }
        ));
    QCOMPARE(ScheduleWidgetTestStubs::testingAssignmentWriteCount, 5);
    QCOMPARE(
        ScheduleWidgetTestStubs::lastTestingAssignmentMutation,
        ClassMngr::Next::Application::
            ScheduleTestingAssignmentMutation::SavePlainTesting
        );
    QCOMPARE(ScheduleWidgetTestStubs::lastTestingAssignmentWriteRoom,
             QStringLiteral("Room 3"));
    QVERIFY(!ScheduleWidgetTestStubs::lastTestingAssignmentWriteReplaceExisting);
    ++readCount;
    QCOMPARE(ScheduleWidgetTestStubs::testingAssignmentsReadCount, readCount);
}

void ScheduleWidgetTests::
testingAssignmentCancelAndManageDoNotWriteOrReload()
{
    ScheduleWidgetTestStubs::setIncludeMiddleSchoolClasses(true);
    TestingClass testingClass;
    testingClass.classId = 100;
    testingClass.name = QStringLiteral("Writing Lab");
    testingClass.grade = QStringLiteral("M2");
    testingClass.level = QStringLiteral("Mixed (High)");
    testingClass.room = QStringLiteral("Library");
    ScheduleWidgetTestStubs::setTestingClass(testingClass);

    TestingClass earlierTestingClass;
    earlierTestingClass.classId = 200;
    earlierTestingClass.name = QStringLiteral("Oral Review");
    earlierTestingClass.grade = QStringLiteral("E5");
    earlierTestingClass.level = QStringLiteral("Lower");
    earlierTestingClass.room = QStringLiteral("204");

    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    ApplicationServices services;
    QVERIFY(services.openDatabase(scheduleWidgetDatabasePath(directory)));
    ApplicationServicesDatabaseGuard databaseGuard(services);
    QVERIFY(seedTestingClassRecords(
        services,
        {testingClass, earlierTestingClass}
        ));
    saveSettingOrFail(
        services.dataService(),
        QStringLiteral("schedule_display_mode"),
        QStringLiteral("testing")
        );
    saveSettingOrFail(
        services.dataService(),
        QStringLiteral("schedule_testing_affects_m1"),
        QStringLiteral("true")
        );
    ScheduleWidget widget(&services);
    widget.refreshSchedule();

    const QString day = QStringLiteral("Wednesday");
    const QString startTime = QStringLiteral("16:00");
    const int readCount = ScheduleWidgetTestStubs::testingAssignmentsReadCount;
    QVERIFY(interactWithTestingAssignmentCell(
        widget,
        day,
        startTime,
        [](TestingAssignmentDialog* dialog)
        {
            return clickTestingAssignmentDialogButton(
                dialog,
                QDialogButtonBox::Cancel
                );
        }
        ));
    QCOMPARE(ScheduleWidgetTestStubs::testingAssignmentWriteCount, 0);
    QCOMPARE(ScheduleWidgetTestStubs::testingAssignmentsReadCount, readCount);

    QSignalSpy requested(
        &widget,
        &ScheduleWidget::testingClassesRequested
        );
    QVERIFY(interactWithTestingAssignmentCell(
        widget,
        day,
        startTime,
        [](TestingAssignmentDialog* dialog)
        {
            auto* classes = dialog->findChild<QComboBox*>(
                QStringLiteral("testingAssignmentClassCombo")
                );
            if (
                !classes
                || classes->count() != 2
                || classes->itemData(0).toInt() != 200
                || classes->itemData(1).toInt() != 100
                )
            {
                return false;
            }
            if (!chooseTestingClass(dialog, 100))
            {
                return false;
            }
            auto* manage = dialog->findChild<QPushButton*>(
                    QStringLiteral("testingAssignmentManageClassesButton")
                    );
            if (!manage)
            {
                return false;
            }
            manage->click();
            return true;
        }
        ));

    QCOMPARE(ScheduleWidgetTestStubs::testingAssignmentWriteCount, 0);
    QCOMPARE(ScheduleWidgetTestStubs::testingAssignmentsReadCount, readCount);
    QCOMPARE(requested.count(), 1);
    const QList<QVariant> arguments = requested.takeFirst();
    QCOMPARE(arguments.size(), 3);
    QCOMPARE(arguments.at(0).toInt(), 100);
    QCOMPARE(arguments.at(1).toString(), day);
    QCOMPARE(arguments.at(2).toString(), startTime);
}

void ScheduleWidgetTests::
failedTestingAssignmentWriteWarnsAndDoesNotReload()
{
    ScheduleWidgetTestStubs::setIncludeMiddleSchoolClasses(true);
    ScheduleWidgetTestStubs::setTestingBlock(
        QStringLiteral("Wednesday"),
        QStringLiteral("16:00"),
        QStringLiteral("Old Room")
        );
    ScheduleWidgetTestStubs::setTestingAssignmentWriteFailure(
        QStringLiteral("injected testing assignment write failure")
        );

    ApplicationServices services;
    saveSettingOrFail(
        services.dataService(),
        QStringLiteral("schedule_display_mode"),
        QStringLiteral("testing")
        );
    ScheduleWidget widget(&services);
    widget.refreshSchedule();
    const int readCount = ScheduleWidgetTestStubs::testingAssignmentsReadCount;
    const ScheduleViewModel beforeModel = widget.scheduleModel();
    const ScheduleCellView* before = findScheduleCell(
        beforeModel,
        QStringLiteral("Wednesday"),
        QStringLiteral("16:00")
        );
    QVERIFY(before);
    QCOMPARE(before->testingRoom, QStringLiteral("Old Room"));

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    QVERIFY(interactWithTestingAssignmentCell(
        widget,
        QStringLiteral("Wednesday"),
        QStringLiteral("16:00"),
        [](TestingAssignmentDialog* dialog)
        {
            auto* room = dialog->findChild<QLineEdit*>(
                QStringLiteral("testingAssignmentRoomEdit")
                );
            if (!room)
            {
                return false;
            }
            room->setText(QStringLiteral("New Room"));
            return clickTestingAssignmentDialogButton(
                dialog,
                QDialogButtonBox::Save
                );
        }
        ));

    QCOMPARE(ScheduleWidgetTestStubs::testingAssignmentWriteCount, 1);
    QCOMPARE(ScheduleWidgetTestStubs::lastTestingAssignmentWriteRoom,
             QStringLiteral("New Room"));
    QVERIFY(ScheduleWidgetTestStubs::lastTestingAssignmentWriteReplaceExisting);
    QCOMPARE(ScheduleWidgetTestStubs::testingAssignmentsReadCount, readCount);
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.constFirst().title,
             QStringLiteral("Testing Assignment"));
    QCOMPARE(prompts.messages.constFirst().severity, PromptSeverity::Warning);
    QCOMPARE(prompts.messages.constFirst().message,
             QStringLiteral("injected testing assignment write failure"));
    const ScheduleViewModel afterModel = widget.scheduleModel();
    const ScheduleCellView* after = findScheduleCell(
        afterModel,
        QStringLiteral("Wednesday"),
        QStringLiteral("16:00")
        );
    QVERIFY(after);
    QCOMPARE(after->testingRoom, QStringLiteral("Old Room"));
}

void ScheduleWidgetTests
    ::readOnlyPresentationHidesControlsAndIgnoresClicks()
{
    ApplicationServices services;
    saveSettingOrFail(services.dataService(),
        QStringLiteral("schedule_show_intensive"),
        QStringLiteral("true")
        );

    ScheduleWidget widget(
        &services,
        nullptr,
        ScheduleMode::ReadOnly
        );
    widget.refreshSchedule();

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
    QCOMPARE(table->rowCount(), 13);
    QCOMPARE(ScheduleWidgetTestStubs::scheduleClassInfoReadCount, 1);
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

void ScheduleWidgetTests::
sourceQueryBuildsRegularIntensiveAndTestingSchedules()
{
    ScheduleWidgetTestStubs::setExistingIntensiveHours(true);

    ApplicationServices services;
    ScheduleWidget widget(&services);
    widget.refreshSchedule();

    QCOMPARE(ScheduleWidgetTestStubs::scheduleClassInfoReadCount, 1);
    const ScheduleViewModel regular = widget.scheduleModel();
    const ScheduleCellView* regularCell = findScheduleCell(
        regular,
        QStringLiteral("Tuesday"),
        QStringLiteral("16:00")
        );
    QVERIFY(regularCell);
    QCOMPARE(regularCell->entries.size(), 1);
    QCOMPARE(regularCell->entries.first().classId, 42);

    saveSettingOrFail(
        services.dataService(),
        QStringLiteral("schedule_display_mode"),
        QStringLiteral("intensive")
        );
    widget.refreshSchedule();

    QCOMPARE(ScheduleWidgetTestStubs::scheduleClassInfoReadCount, 2);
    QCOMPARE(widget.displayState().displayMode, ScheduleDisplayMode::Intensive);
    const ScheduleViewModel intensive = widget.scheduleModel();
    const ScheduleCellView* intensiveCell = findScheduleCell(
        intensive,
        QStringLiteral("Tuesday"),
        QStringLiteral("09:00")
        );
    QVERIFY(intensiveCell);
    QCOMPARE(intensiveCell->entries.size(), 1);
    QCOMPARE(intensiveCell->entries.first().classId, 42);

    saveSettingOrFail(
        services.dataService(),
        QStringLiteral("schedule_display_mode"),
        QStringLiteral("testing")
        );
    widget.refreshSchedule();

    QCOMPARE(ScheduleWidgetTestStubs::scheduleClassInfoReadCount, 3);
    QCOMPARE(widget.displayState().displayMode, ScheduleDisplayMode::Testing);
    const ScheduleViewModel testing = widget.scheduleModel();
    const ScheduleCellView* testingCell = findScheduleCell(
        testing,
        QStringLiteral("Tuesday"),
        QStringLiteral("16:00")
        );
    QVERIFY(testingCell);
    QCOMPARE(testingCell->entries.size(), 1);
    QCOMPARE(testingCell->entries.first().classId, 42);
}

void ScheduleWidgetTests::
unavailableSourceReturnsDaysOnlyWithoutReading()
{
    ScheduleWidgetTestStubs::setDatabaseOpen(false);

    ApplicationServices services;
    ScheduleWidget widget(&services);
    QStringList warnings;
    {
        const ScopedQtWarningCapture warningCapture(warnings);
        widget.refreshSchedule();
    }

    QCOMPARE(ScheduleWidgetTestStubs::scheduleClassInfoReadCount, 0);
    QCOMPARE(widget.scheduleModel().days, visibleScheduleDays(false));
    QVERIFY(widget.scheduleModel().rows.isEmpty());
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join(QLatin1Char('\n'))));
}

void ScheduleWidgetTests::
failedSourceReadLogsAndKeepsIndependentScheduleReads()
{
    ScheduleWidgetTestStubs::setScheduleClassInfoReadFailure(true);
    ScheduleWidgetTestStubs::setTestingBlock(
        QStringLiteral("Monday"),
        QStringLiteral("16:00"),
        QStringLiteral("Library")
        );

    ApplicationServices services;
    ScheduleWidget widget(&services);
    QTest::ignoreMessage(
        QtWarningMsg,
        QRegularExpression(QStringLiteral(
            "^\"injected schedule read failure\"$"
            ))
        );
    widget.refreshSchedule();

    QCOMPARE(ScheduleWidgetTestStubs::scheduleClassInfoReadCount, 1);
    QCOMPARE(ScheduleWidgetTestStubs::slotStateReadCount, 1);
    QCOMPARE(ScheduleWidgetTestStubs::testingAssignmentsReadCount, 1);
    QCOMPARE(widget.scheduleModel().days, visibleScheduleDays(false));
    QVERIFY(widget.scheduleModel().rows.isEmpty());
}

void ScheduleWidgetTests::previewModelBypassesSourceAndScheduleReads()
{
    ApplicationServices services;
    ScheduleWidget widget(&services);

    ScheduleViewModel preview;
    preview.days = {QStringLiteral("Wednesday")};
    ScheduleRowView row;
    row.timeLabel = QStringLiteral("10:00");
    ScheduleCellView previewCell;
    previewCell.day = QStringLiteral("Wednesday");
    previewCell.timeLabel = QStringLiteral("10:00");
    row.cells.append(previewCell);
    preview.rows.append(std::move(row));
    widget.setPreviewModel(preview);
    widget.refreshSchedule();

    QCOMPARE(ScheduleWidgetTestStubs::scheduleClassInfoReadCount, 0);
    QCOMPARE(ScheduleWidgetTestStubs::slotStateReadCount, 0);
    QCOMPARE(ScheduleWidgetTestStubs::testingAssignmentsReadCount, 0);
    QCOMPARE(
        widget.scheduleModel().days,
        QStringList{QStringLiteral("Wednesday")}
        );
    QCOMPARE(widget.scheduleModel().rows.size(), 1);
    QCOMPARE(
        widget.scheduleModel().rows.first().timeLabel,
        QStringLiteral("10:00")
        );
}

void ScheduleWidgetTests::
failedTestingAssignmentReadWarnsAndRetainsAssignments()
{
    TestingClass testingClass;
    testingClass.classId = 100;
    testingClass.name = QStringLiteral("Writing Lab");
    testingClass.grade = QStringLiteral("M2");
    testingClass.level = QStringLiteral("Mixed (High)");
    testingClass.room = QStringLiteral("Library");
    ScheduleWidgetTestStubs::setTestingClassAssignment(
        QStringLiteral("Monday"),
        QStringLiteral("16:00"),
        testingClass
        );

    ApplicationServices services;
    saveSettingOrFail(
        services.dataService(),
        QStringLiteral("schedule_display_mode"),
        QStringLiteral("testing")
        );
    ScheduleWidget widget(&services);
    widget.refreshSchedule();

    QCOMPARE(ScheduleWidgetTestStubs::testingAssignmentsReadCount, 1);
    ScheduleViewModel model = widget.scheduleModel();
    const ScheduleCellView* monday = findScheduleCell(
        model,
        QStringLiteral("Monday"),
        QStringLiteral("16:00")
        );
    QVERIFY(monday);
    QVERIFY(monday->testingClassAssignment);
    QCOMPARE(monday->testingClassId, 100);

    ScheduleWidgetTestStubs::setTestingAssignmentReadFailure(
        QStringLiteral("injected testing assignment read failure")
        );
    QTest::ignoreMessage(
        QtWarningMsg,
        QRegularExpression(QStringLiteral(
            ".*Failed to load testing blocks:.*"
            "injected testing assignment read failure.*"
            ))
        );
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    widget.refreshSchedule();

    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.constFirst().title,
             QStringLiteral("Testing Layout"));
    QCOMPARE(prompts.messages.constFirst().severity, PromptSeverity::Warning);
    QCOMPARE(prompts.messages.constFirst().message,
             QStringLiteral("injected testing assignment read failure"));
    QCOMPARE(ScheduleWidgetTestStubs::testingAssignmentsReadCount, 2);
    model = widget.scheduleModel();
    monday = findScheduleCell(
        model,
        QStringLiteral("Monday"),
        QStringLiteral("16:00")
        );
    QVERIFY(monday);
    QVERIFY(monday->testingClassAssignment);
    QCOMPARE(monday->testingClassId, 100);
}

void ScheduleWidgetTests::
unavailableTestingAssignmentReadClearsAssignments()
{
    TestingClass testingClass;
    testingClass.classId = 100;
    testingClass.name = QStringLiteral("Writing Lab");
    testingClass.grade = QStringLiteral("M2");
    testingClass.level = QStringLiteral("Mixed (High)");
    testingClass.room = QStringLiteral("Library");
    ScheduleWidgetTestStubs::setTestingClassAssignment(
        QStringLiteral("Monday"),
        QStringLiteral("16:00"),
        testingClass
        );

    ApplicationServices services;
    saveSettingOrFail(
        services.dataService(),
        QStringLiteral("schedule_display_mode"),
        QStringLiteral("testing")
        );
    ScheduleWidget widget(&services);
    widget.refreshSchedule();

    ScheduleViewModel model = widget.scheduleModel();
    const ScheduleCellView* monday = findScheduleCell(
        model,
        QStringLiteral("Monday"),
        QStringLiteral("16:00")
        );
    QVERIFY(monday);
    QVERIFY(monday->testingClassAssignment);

    ScheduleWidgetTestStubs::setTestingAssignmentRepositoryAvailable(false);
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    widget.refreshSchedule();

    QVERIFY(prompts.messages.isEmpty());
    QCOMPARE(ScheduleWidgetTestStubs::testingAssignmentsReadCount, 1);
    model = widget.scheduleModel();
    monday = findScheduleCell(
        model,
        QStringLiteral("Monday"),
        QStringLiteral("16:00")
        );
    QVERIFY(!monday || !monday->testingClassAssignment);
}

void ScheduleWidgetTests::
missingTestingClassWarnsAndSkipsOnlyThatAssignment()
{
    TestingClass testingClass;
    testingClass.classId = 100;
    testingClass.name = QStringLiteral("Writing Lab");
    testingClass.grade = QStringLiteral("M2");
    testingClass.level = QStringLiteral("Mixed (High)");
    testingClass.room = QStringLiteral("Library");
    ScheduleWidgetTestStubs::setTestingClassAssignment(
        QStringLiteral("Monday"),
        QStringLiteral("16:00"),
        testingClass
        );
    ScheduleWidgetTestStubs::setUnresolvedTestingClassAssignment(
        QStringLiteral("Tuesday"),
        QStringLiteral("16:00"),
        101
        );
    ScheduleWidgetTestStubs::setTestingBlock(
        QStringLiteral("Wednesday"),
        QStringLiteral("16:00"),
        QStringLiteral("Room 9")
        );

    ApplicationServices services;
    saveSettingOrFail(
        services.dataService(),
        QStringLiteral("schedule_display_mode"),
        QStringLiteral("testing")
        );
    ScheduleWidget widget(&services);
    QTest::ignoreMessage(
        QtWarningMsg,
        QRegularExpression(QStringLiteral(
            ".*Failed to load assigned testing class:.*"
            "The testing class was not found\\..*"
            ))
        );
    widget.refreshSchedule();

    ScheduleViewModel model = widget.scheduleModel();
    const ScheduleCellView* monday = findScheduleCell(
        model,
        QStringLiteral("Monday"),
        QStringLiteral("16:00")
        );
    const ScheduleCellView* tuesday = findScheduleCell(
        model,
        QStringLiteral("Tuesday"),
        QStringLiteral("16:00")
        );
    const ScheduleCellView* wednesday = findScheduleCell(
        model,
        QStringLiteral("Wednesday"),
        QStringLiteral("16:00")
        );
    QVERIFY(monday);
    QVERIFY(monday->testingClassAssignment);
    QCOMPARE(monday->testingClassId, 100);
    QVERIFY(!tuesday || !tuesday->testingClassAssignment);
    QVERIFY(wednesday);
    QCOMPARE(wednesday->testingRoom, QStringLiteral("Room 9"));
}

void ScheduleWidgetTests::slotStateSuccessReplacesAndClearsOverrides()
{
    ScheduleWidgetTestStubs::setExistingIntensiveHours(true);
    ApplicationServices services;
    saveSettingOrFail(
        services.dataService(),
        QStringLiteral("schedule_display_mode"),
        QStringLiteral("intensive")
        );
    ScheduleWidgetTestStubs::setIntensiveSlotStates({
        {
            QStringLiteral("Tuesday"),
            QStringLiteral("09:00"),
            QString::fromUcs4(U" custom 🧭 ")
        },
        {
            QStringLiteral("Monday"),
            QStringLiteral("09:00"),
            QStringLiteral("legacy override")
        }
    });

    ScheduleWidget widget(&services);
    widget.refreshSchedule();

    QCOMPARE(ScheduleWidgetTestStubs::slotStateReadCount, 1);
    ScheduleViewModel model = widget.scheduleModel();
    const ScheduleCellView* tuesday = findScheduleCell(
        model,
        QStringLiteral("Tuesday"),
        QStringLiteral("09:00")
        );
    const ScheduleCellView* monday = findScheduleCell(
        model,
        QStringLiteral("Monday"),
        QStringLiteral("09:00")
        );
    QVERIFY(tuesday);
    QVERIFY(monday);
    QCOMPARE(tuesday->slotState,
             QString::fromUcs4(U" custom 🧭 "));
    QCOMPARE(monday->slotState, QStringLiteral("legacy override"));

    ScheduleWidgetTestStubs::setIntensiveSlotStates({
        {
            QStringLiteral("Monday"),
            QStringLiteral("09:00"),
            QStringLiteral("replacement")
        }
    });
    widget.refreshSchedule();

    QCOMPARE(ScheduleWidgetTestStubs::slotStateReadCount, 2);
    model = widget.scheduleModel();
    tuesday = findScheduleCell(
        model,
        QStringLiteral("Tuesday"),
        QStringLiteral("09:00")
        );
    monday = findScheduleCell(
        model,
        QStringLiteral("Monday"),
        QStringLiteral("09:00")
        );
    QVERIFY(tuesday);
    QVERIFY(monday);
    QCOMPARE(tuesday->slotState, scheduleEssaySlotState());
    QCOMPARE(monday->slotState, QStringLiteral("replacement"));

    ScheduleWidgetTestStubs::setIntensiveSlotStates({});
    widget.refreshSchedule();

    QCOMPARE(ScheduleWidgetTestStubs::slotStateReadCount, 3);
    model = widget.scheduleModel();
    tuesday = findScheduleCell(
        model,
        QStringLiteral("Tuesday"),
        QStringLiteral("09:00")
        );
    monday = findScheduleCell(
        model,
        QStringLiteral("Monday"),
        QStringLiteral("09:00")
        );
    QVERIFY(tuesday);
    QVERIFY(monday);
    QCOMPARE(tuesday->slotState, scheduleEssaySlotState());
    QCOMPARE(monday->slotState, scheduleEssaySlotState());
}

void ScheduleWidgetTests::
unavailableSlotStateReadIsSilentAndRetainsOverrides()
{
    ScheduleWidgetTestStubs::setExistingIntensiveHours(true);
    ApplicationServices services;
    saveSettingOrFail(
        services.dataService(),
        QStringLiteral("schedule_display_mode"),
        QStringLiteral("intensive")
        );
    ScheduleWidgetTestStubs::setIntensiveSlotStates({
        {
            QStringLiteral("Tuesday"),
            QStringLiteral("09:00"),
            QStringLiteral("saved override")
        }
    });

    ScheduleWidget widget(&services);
    widget.refreshSchedule();
    QCOMPARE(ScheduleWidgetTestStubs::slotStateReadCount, 1);

    ScheduleWidgetTestStubs::setIntensiveSlotStateRepositoryAvailable(false);
    ScheduleWidgetTestStubs::setIntensiveSlotStates({
        {
            QStringLiteral("Tuesday"),
            QStringLiteral("09:00"),
            QStringLiteral("must not replace")
        }
    });
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    widget.refreshSchedule();

    QVERIFY(prompts.messages.isEmpty());
    QCOMPARE(ScheduleWidgetTestStubs::slotStateReadCount, 1);
    const ScheduleViewModel model = widget.scheduleModel();
    const ScheduleCellView* tuesday = findScheduleCell(
        model,
        QStringLiteral("Tuesday"),
        QStringLiteral("09:00")
        );
    QVERIFY(tuesday);
    QCOMPARE(tuesday->slotState, QStringLiteral("saved override"));
}

void ScheduleWidgetTests::failedSlotStateReadWarnsAndRetainsOverrides()
{
    ScheduleWidgetTestStubs::setExistingIntensiveHours(true);
    ApplicationServices services;
    saveSettingOrFail(
        services.dataService(),
        QStringLiteral("schedule_display_mode"),
        QStringLiteral("intensive")
        );
    ScheduleWidgetTestStubs::setIntensiveSlotStates({
        {
            QStringLiteral("Tuesday"),
            QStringLiteral("09:00"),
            QStringLiteral("saved override")
        }
    });

    ScheduleWidget widget(&services);
    widget.refreshSchedule();
    QCOMPARE(ScheduleWidgetTestStubs::slotStateReadCount, 1);

    ScheduleWidgetTestStubs::setIntensiveSlotStateReadFailure(
        QStringLiteral("injected slot-state read failure")
        );
    QTest::ignoreMessage(
        QtWarningMsg,
        QRegularExpression(QStringLiteral(
            ".*Failed to load intensive slot states:.*"
            "injected slot-state read failure.*"
            ))
        );
    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    widget.refreshSchedule();

    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.constFirst().title, QStringLiteral("Schedule"));
    QCOMPARE(prompts.messages.constFirst().severity, PromptSeverity::Warning);
    QVERIFY(prompts.messages.constFirst().message.contains(
        QStringLiteral("injected slot-state read failure")
        ));
    QCOMPARE(ScheduleWidgetTestStubs::slotStateReadCount, 2);
    const ScheduleViewModel model = widget.scheduleModel();
    const ScheduleCellView* tuesday = findScheduleCell(
        model,
        QStringLiteral("Tuesday"),
        QStringLiteral("09:00")
        );
    QVERIFY(tuesday);
    QCOMPARE(tuesday->slotState, QStringLiteral("saved override"));
}

void ScheduleWidgetTests::readOnlyScheduleStillLoadsSlotStates()
{
    ScheduleWidgetTestStubs::setExistingIntensiveHours(true);
    ApplicationServices services;
    saveSettingOrFail(
        services.dataService(),
        QStringLiteral("schedule_display_mode"),
        QStringLiteral("intensive")
        );
    ScheduleWidgetTestStubs::setIntensiveSlotStates({
        {
            QStringLiteral("Tuesday"),
            QStringLiteral("09:00"),
            QStringLiteral("read-only override")
        }
    });

    ScheduleWidget widget(
        &services,
        nullptr,
        ScheduleMode::ReadOnly
        );
    widget.refreshSchedule();

    QCOMPARE(ScheduleWidgetTestStubs::slotStateReadCount, 1);
    const ScheduleViewModel model = widget.scheduleModel();
    const ScheduleCellView* tuesday = findScheduleCell(
        model,
        QStringLiteral("Tuesday"),
        QStringLiteral("09:00")
        );
    QVERIFY(tuesday);
    QCOMPARE(tuesday->slotState, QStringLiteral("read-only override"));
}

void ScheduleWidgetTests
    ::timeColumnAndHeaderAreNonInteractive()
{
    ApplicationServices services;
    ScheduleWidget widget(&services);
    widget.refreshSchedule();

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
    ::intensiveSlotToggleMapsAndPersistsViewModelChoice()
{
    ApplicationServices services;
    ScheduleWidget widget(&services);
    widget.refreshSchedule();

    auto* intensiveButton = widget.findChild<QPushButton*>(
        QStringLiteral("scheduleIntensiveModeButton")
        );
    QVERIFY(intensiveButton);
    intensiveButton->click();

    auto* table = widget.findChild<QTableWidget*>(
        QStringLiteral("scheduleTable")
        );
    QVERIFY(table);
    const ScheduleViewModel model = widget.scheduleModel();
    int regularHourRow = -1;
    for (int row = 0; row < model.rows.size(); ++row)
    {
        if (model.rows.at(row).timeLabel == QStringLiteral("16:00"))
        {
            regularHourRow = row;
            break;
        }
    }
    QVERIFY(regularHourRow >= 0);

    const int buildCountBeforeClick =
        ScheduleWidgetTestStubs::scheduleClassInfoReadCount;
    QVERIFY(
        QMetaObject::invokeMethod(
            &widget,
            "onCellClicked",
            Qt::DirectConnection,
            Q_ARG(int, regularHourRow),
            Q_ARG(int, 1)
            )
        );

    QCOMPARE(ScheduleWidgetTestStubs::savedSlotStates, 1);
    QCOMPARE(
        ScheduleWidgetTestStubs::lastSavedSlotDay,
        QStringLiteral("Monday")
        );
    QCOMPARE(
        ScheduleWidgetTestStubs::lastSavedSlotStartTime,
        QStringLiteral("16:00")
        );
    QCOMPARE(
        ScheduleWidgetTestStubs::lastSavedSlotState,
        QStringLiteral("lunch")
        );
    QCOMPARE(
        ScheduleWidgetTestStubs::lastSavedSlotDefaultState,
        QStringLiteral("essay")
        );
    QCOMPARE(
        ScheduleWidgetTestStubs::scheduleClassInfoReadCount,
        buildCountBeforeClick + 1
        );
}

void ScheduleWidgetTests::failedSlotToggleWarnsWithoutReloading()
{
    ApplicationServices services;
    ScheduleWidget widget(&services);
    widget.refreshSchedule();

    auto* intensiveButton = widget.findChild<QPushButton*>(
        QStringLiteral("scheduleIntensiveModeButton")
        );
    QVERIFY(intensiveButton);
    intensiveButton->click();

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    ScheduleWidgetTestStubs::setSlotSaveFailure(
        QStringLiteral("injected slot failure")
        );
    const int buildCountBeforeClick =
        ScheduleWidgetTestStubs::scheduleClassInfoReadCount;

    QVERIFY(
        QMetaObject::invokeMethod(
            &widget,
            "onCellClicked",
            Qt::DirectConnection,
            Q_ARG(int, 0),
            Q_ARG(int, 1)
            )
        );

    QCOMPARE(ScheduleWidgetTestStubs::savedSlotStates, 1);
    QCOMPARE(
        ScheduleWidgetTestStubs::scheduleClassInfoReadCount,
        buildCountBeforeClick
        );
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(
        prompts.messages.constFirst().title,
        QStringLiteral("Update Schedule")
        );
    QCOMPARE(
        prompts.messages.constFirst().severity,
        PromptSeverity::Warning
        );
    QVERIFY(
        prompts.messages.constFirst().message.contains(
            QStringLiteral("injected slot failure")
            )
        );
}

void ScheduleWidgetTests::unavailableSlotToggleSkipsWriteAndReloads()
{
    ApplicationServices services;
    ScheduleWidget widget(&services);
    widget.refreshSchedule();

    auto* intensiveButton = widget.findChild<QPushButton*>(
        QStringLiteral("scheduleIntensiveModeButton")
        );
    QVERIFY(intensiveButton);
    intensiveButton->click();

    const int buildCountBeforeClick =
        ScheduleWidgetTestStubs::scheduleClassInfoReadCount;
    ScheduleWidgetTestStubs::setDatabaseOpen(false);

    QVERIFY(
        QMetaObject::invokeMethod(
            &widget,
            "onCellClicked",
            Qt::DirectConnection,
            Q_ARG(int, 0),
            Q_ARG(int, 1)
            )
        );

    QCOMPARE(ScheduleWidgetTestStubs::savedSlotStates, 0);
    QCOMPARE(
        ScheduleWidgetTestStubs::scheduleClassInfoReadCount,
        buildCountBeforeClick
        );
}

void ScheduleWidgetTests::regularSlotToggleUsesSharedPersistenceHandler()
{
    ApplicationServices services;
    ScheduleWidget widget(&services);
    widget.refreshSchedule();

    auto* table = widget.findChild<QTableWidget*>(
        QStringLiteral("scheduleTable")
        );
    QVERIFY(table);
    QWidget* mondaySlot = table->cellWidget(0, 1);
    QVERIFY(mondaySlot);
    mondaySlot->setProperty("slot_toggling_enabled", true);

    const int buildCountBeforeClick =
        ScheduleWidgetTestStubs::scheduleClassInfoReadCount;
    QVERIFY(
        QMetaObject::invokeMethod(
            &widget,
            "onCellClicked",
            Qt::DirectConnection,
            Q_ARG(int, 0),
            Q_ARG(int, 1)
            )
        );

    QCOMPARE(ScheduleWidgetTestStubs::savedSlotStates, 1);
    QCOMPARE(
        ScheduleWidgetTestStubs::lastSavedSlotDay,
        QStringLiteral("Monday")
        );
    QCOMPARE(
        ScheduleWidgetTestStubs::lastSavedSlotStartTime,
        QStringLiteral("16:00")
        );
    QCOMPARE(
        ScheduleWidgetTestStubs::lastSavedSlotState,
        QStringLiteral("essay")
        );
    QCOMPARE(
        ScheduleWidgetTestStubs::lastSavedSlotDefaultState,
        QStringLiteral("empty")
        );
    QCOMPARE(
        ScheduleWidgetTestStubs::scheduleClassInfoReadCount,
        buildCountBeforeClick + 1
        );
}

void ScheduleWidgetTests
    ::clearDatabaseStateRemovesLoadedDataAndSettings()
{
    ApplicationServices services;
    ScheduleWidget widget(&services);
    widget.refreshSchedule();

    QCOMPARE(widget.visibleClassIds(), QSet<int>{42});

    saveSettingOrFail(services.dataService(),
        QStringLiteral("schedule_use_24h"),
        QStringLiteral("true")
        );
    saveSettingOrFail(services.dataService(),
        QStringLiteral("schedule_show_weekends"),
        QStringLiteral("true")
        );
    saveSettingOrFail(services.dataService(),
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
    page.setDatabaseOpen(true);
    page.activate();

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

void ScheduleWidgetTests
    ::rendererSkipsUnchangedAndUpdatesOnlyChangedCells()
{
    QTableWidget table;
    ScheduleTableRenderer::initialize(&table);

    const ScheduleTableRenderOptions options{.cellParent = &table};
    const ScheduleViewModel initial = rendererTestModel();
    const ScheduleTableRenderMetrics first =
        ScheduleTableRenderer::render(&table, initial, options);
    QVERIFY(first.fullRender);

    QTableWidgetItem* const timeItem = table.item(0, 0);
    QWidget* const mondayWidget = table.cellWidget(0, 1);
    QWidget* const tuesdayWidget = table.cellWidget(0, 2);
    QVERIFY(timeItem);
    QVERIFY(mondayWidget);
    QVERIFY(tuesdayWidget);

    ScheduleViewModel changed =
        rendererTestModel(scheduleLunchSlotState());
    const ScheduleTableRenderMetrics partial =
        ScheduleTableRenderer::render(&table, changed, options);
    QVERIFY(!partial.fullRender);
    QCOMPARE(partial.tableItemsCreated, 0);
    QCOMPARE(partial.cellWidgetsCreated, 1);
    QCOMPARE(partial.cellWidgetsRemoved, 1);
    QCOMPARE(table.item(0, 0), timeItem);
    QCOMPARE(table.cellWidget(0, 2), tuesdayWidget);

    QWidget* const updatedMondayWidget = table.cellWidget(0, 1);
    QVERIFY(updatedMondayWidget);
    QVERIFY(updatedMondayWidget != mondayWidget);

    const ScheduleTableRenderMetrics unchanged =
        ScheduleTableRenderer::render(&table, changed, options);
    QVERIFY(!unchanged.fullRender);
    QCOMPARE(unchanged.tableItemsCreated, 0);
    QCOMPARE(unchanged.cellWidgetsCreated, 0);
    QCOMPARE(unchanged.cellWidgetsRemoved, 0);
    QCOMPARE(unchanged.cellWidgetsQueuedForDeletion, 0);
    QCOMPARE(table.item(0, 0), timeItem);
    QCOMPARE(table.cellWidget(0, 1), updatedMondayWidget);
    QCOMPARE(table.cellWidget(0, 2), tuesdayWidget);
}

QTEST_MAIN(ScheduleWidgetTests)

#include "schedule_widget_tests.moc"
