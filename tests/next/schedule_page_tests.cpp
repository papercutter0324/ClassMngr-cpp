#include "data/database/workspace_store.h"
#include "next/application/class_details_save_workflow.h"
#include "next/application/schedule_slot_state_save.h"
#include "next/platform/workspace_store_class_editor_actions.h"
#include "next/platform/workspace_store_schedule_actions.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_loader.h"
#include "next/ui/page_host.h"
#include "next/ui/schedule_page.h"
#include "preferences_test_ports.h"
#include "sql_script_runner.h"

#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QSqlError>
#include <QSqlQuery>
#include <QTableView>
#include <QToolButton>
#include <QTemporaryDir>
#include <QTranslator>
#include <QtTest/QtTest>

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

using namespace ClassMngr::Next;

namespace
{

Domain::ClassId classId(const int value)
{
    return *Domain::ClassId::fromString(std::to_string(value));
}

bool createScheduleFixture(const QString& path, const QString& teacherName,
                           QString* error)
{
    WorkspaceStore store;
    const RepositoryStatus opened = store.open(path);
    if (!opened)
    {
        *error = opened.error().diagnostic;
        return false;
    }

    QFile fixture(QStringLiteral(CLASSMNGR_SOURCE_DIR)
        + QStringLiteral("/tests/fixtures/workspaces/large_startup.sql"));
    if (!fixture.open(QIODevice::ReadOnly))
    {
        *error = QStringLiteral("Could not read large_startup.sql.");
        return false;
    }
    QString fixtureError;
    if (!Test::executeSqlScript(store.database(),
                                QString::fromUtf8(fixture.readAll()),
                                &fixtureError))
    {
        *error = QStringLiteral("Could not seed large_startup.sql: %1")
            .arg(fixtureError);
        return false;
    }

    QSqlQuery teacher(store.database());
    teacher.prepare(QStringLiteral(
        "UPDATE teachers SET teacher_en=?, preferred_name=? WHERE id=1"));
    teacher.addBindValue(teacherName);
    teacher.addBindValue(teacherName);
    if (!teacher.exec())
    {
        *error = teacher.lastError().text();
        return false;
    }

    QSqlQuery assignment(store.database());
    if (!assignment.exec(QStringLiteral(R"(
            INSERT INTO schedule_testing_blocks (day, start_time, room, class_id)
            VALUES ('Monday', '17:00', 'Testing Room', NULL)
        )")))
    {
        *error = assignment.lastError().text();
        return false;
    }
    return true;
}

bool setIntensiveSlotsEmpty(const QString& path)
{
    WorkspaceStore store;
    if (!store.openExisting(path))
    {
        return false;
    }
    QSqlQuery clear(store.database());
    if (!clear.exec(QStringLiteral("DELETE FROM intensive_slot_states")))
    {
        return false;
    }
    QSqlQuery insert(store.database());
    insert.prepare(QStringLiteral(R"(
        INSERT INTO intensive_slot_states (day, start_time, state)
        VALUES (?, ?, 'empty')
    )"));
    const QStringList days{
        QStringLiteral("Monday"), QStringLiteral("Tuesday"),
        QStringLiteral("Wednesday"), QStringLiteral("Thursday"),
        QStringLiteral("Friday")
    };
    for (const QString& day : days)
    {
        for (int hour = 9; hour <= 21; ++hour)
        {
            insert.bindValue(0, day);
            insert.bindValue(1, QStringLiteral("%1:00")
                .arg(hour, 2, 10, QLatin1Char('0')));
            if (!insert.exec())
            {
                return false;
            }
        }
    }
    return true;
}

bool hasClass(const Ui::SchedulePageModel* model, const int row,
              const int column, const Domain::ClassId& id)
{
    const Ui::SchedulePageCell* const cell = model->cellAt(row, column);
    if (!cell)
    {
        return false;
    }
    for (const Ui::SchedulePageEntry& entry : cell->entries)
    {
        if (entry.classId == id)
        {
            return true;
        }
    }
    return false;
}

std::optional<QModelIndex> findCell(
    const Ui::SchedulePageModel* model,
    const std::function<bool(const Ui::SchedulePageCell&)>& predicate)
{
    for (int row = 0; row < model->rowCount(); ++row)
    {
        for (int column = 1; column < model->columnCount(); ++column)
        {
            const Ui::SchedulePageCell* const cell = model->cellAt(row, column);
            if (cell && predicate(*cell))
            {
                return model->index(row, column);
            }
        }
    }
    return std::nullopt;
}

class OtherPage final : public Ui::Page
{
public:
    using Ui::Page::Page;
};

class ClassDetailsRoutePage final : public Ui::Page
{
public:
    ClassDetailsRoutePage(const Ui::PageContext& context,
                          std::function<void()> saveAndReturn)
        : Ui::Page(context)
        , m_saveAndReturn(std::move(saveAndReturn))
    {
        m_save = new QPushButton(QStringLiteral("Save Class Details"), this);
        m_save->setObjectName(QStringLiteral("scheduleTestClassDetailsSave"));
        connect(m_save, &QPushButton::clicked, this, [this] {
            if (m_saveAndReturn)
            {
                m_saveAndReturn();
            }
        });
    }

    QPushButton* saveButton() const noexcept { return m_save; }

private:
    std::function<void()> m_saveAndReturn;
    QPushButton* m_save = nullptr;
};

struct PageFixture final
{
    Test::MemoryPreferences preferences;
    std::unique_ptr<Resources::ResourceCatalog> catalog;
    std::unique_ptr<Resources::ResourceLoader> loader;
    std::unique_ptr<Ui::PageHost> host;
    QString path;
    QString identity;
    std::optional<Domain::ClassId> selectedClass;
    QString lastSavedPath;
    QString classSaveError;
    std::optional<Application::SchedulePageReadSnapshot> scheduleReadOverride;
    bool forceReadFailure = false;
    bool forcePreferencesSaveFailure = false;
    bool forceModeSaveFailure = false;
    bool forceSlotSaveFailure = false;
    bool classSaveSucceeded = false;
    Ui::SchedulePage* schedule = nullptr;
    ClassDetailsRoutePage* detailsRoute = nullptr;

    bool initialize(const QString& initialPath)
    {
        path = initialPath;
        identity = QStringLiteral("same-workspace-id|") + path;
        const auto loadedCatalog = Resources::ResourceCatalog::load(
            QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG),
            QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT));
        if (!loadedCatalog)
        {
            return false;
        }
        catalog = std::make_unique<Resources::ResourceCatalog>(
            std::move(*loadedCatalog));
        loader = std::make_unique<Resources::ResourceLoader>(*catalog);
        host = std::make_unique<Ui::PageHost>(*loader, preferences.model);

        Ui::PageDescriptor scheduleDescriptor{
            .id = *Ui::PageId::fromString(QStringLiteral("schedule")),
            .label = QStringLiteral("Schedule"),
            .factory = [this](const Ui::PageContext& context)
                -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
                Ui::SchedulePageActions actions;
                actions.activeWorkspacePath = [this] { return path; };
                actions.workspaceIdentity = [this] { return identity; };
                actions.readSchedule = [this](const QString& databasePath) {
                    if (forceReadFailure)
                    {
                        return Application::SchedulePageReadResult::failure({
                            .code = Domain::ErrorCode::Technical,
                            .message = "schedule fixture read failed",
                            .recoverable = true
                        });
                    }
                    if (scheduleReadOverride)
                    {
                        return Application::SchedulePageReadResult::success(
                            *scheduleReadOverride);
                    }
                    return Platform::readSchedulePageAtPath(databasePath);
                };
                actions.saveDisplayPreferences = [this](
                    const QString& databasePath,
                    const Application::ScheduleDisplayPreferencesSaveRequest& request) {
                    if (forcePreferencesSaveFailure)
                    {
                        return Application::ScheduleDisplayPreferencesSaveResult::failure({
                            .code = Domain::ErrorCode::Technical,
                            .message = "schedule preferences save failed",
                            .recoverable = true
                        });
                    }
                    return Platform::saveScheduleDisplayPreferencesAtPath(
                        databasePath, request);
                };
                actions.saveDisplayMode = [this](
                    const QString& databasePath,
                    const Application::ScheduleDisplayMode mode) {
                    if (forceModeSaveFailure)
                    {
                        return Application::ScheduleDisplayModePreferencesSaveResult::failure({
                            .code = Domain::ErrorCode::Technical,
                            .message = "schedule display mode save failed",
                            .recoverable = true
                        });
                    }
                    return Platform::saveScheduleDisplayModeAtPath(databasePath, mode);
                };
                actions.saveSlotState = [this](
                    const QString& databasePath,
                    const Application::ScheduleSlotStateSaveRequest& request) {
                    if (forceSlotSaveFailure)
                    {
                        return Application::ScheduleSlotStateSaveResult::failure({
                            .code = Domain::ErrorCode::Technical,
                            .message = "schedule slot save failed",
                            .recoverable = true
                        });
                    }
                    return Platform::saveScheduleSlotStateAtPath(
                        databasePath, request);
                };
                actions.openClassDetails = [this](const Domain::ClassId& id) {
                    selectedClass = id;
                    static_cast<void>(host->activatePage(QStringLiteral("classes")));
                };
                auto page = std::make_unique<Ui::SchedulePage>(
                    context, std::move(actions));
                schedule = page.get();
                return std::unique_ptr<Ui::Page>(std::move(page));
            }
        };
        Ui::PageDescriptor classesDescriptor{
            .id = *Ui::PageId::fromString(QStringLiteral("classes")),
            .label = QStringLiteral("Classes"),
            .factory = [this](const Ui::PageContext& context)
                -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
                auto page = std::make_unique<ClassDetailsRoutePage>(context, [this] {
                    if (!selectedClass)
                    {
                        classSaveError = QStringLiteral("No class was selected.");
                        return;
                    }
                    Application::ClassDetailsSaveWorkflowRequest request;
                    request.input.classId = *selectedClass;
                    request.input.classGrade = u"E4";
                    request.input.classLevel = u"Theseus";
                    request.input.classColor = u"#DDEBFF";
                    request.input.fontColor = u"#172B4D";
                    request.input.regularTimes.push_back({
                        .day = u"Monday",
                        .startTime = u"8:00 PM",
                        .endTime = u"8:50 PM"
                    });
                    request.saveRegularTimes = true;
                    request.saveIntensiveTimes = false;
                    const auto saved = Platform::saveClassDetailsAtPath(path, request);
                    classSaveSucceeded = std::holds_alternative<
                        Application::ClassDetailsSaveWorkflowSuccess>(saved);
                    if (!classSaveSucceeded)
                    {
                        classSaveError = QStringLiteral(
                            "Class Details could not save the selected class.");
                        return;
                    }
                    lastSavedPath = path;
                    static_cast<void>(host->activatePage(QStringLiteral("schedule")));
                });
                detailsRoute = page.get();
                return std::unique_ptr<Ui::Page>(std::move(page));
            }
        };
        Ui::PageDescriptor otherDescriptor{
            .id = *Ui::PageId::fromString(QStringLiteral("other")),
            .label = QStringLiteral("Other"),
            .factory = [](const Ui::PageContext& context)
                -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
                return std::unique_ptr<Ui::Page>(
                    std::make_unique<OtherPage>(context));
            }
        };
        if (!host->registerPage(std::move(scheduleDescriptor))
            || !host->registerPage(std::move(classesDescriptor))
            || !host->registerPage(std::move(otherDescriptor)))
        {
            return false;
        }
        host->resize(1280, 900);
        host->show();
        if (!host->activatePage(QStringLiteral("schedule")) || !schedule)
        {
            return false;
        }
        QApplication::processEvents();
        return true;
    }

    void suspendSchedule()
    {
        static_cast<void>(host->activatePage(QStringLiteral("other")));
        QApplication::processEvents();
    }

    bool clickCell(const QModelIndex& index)
    {
        const QRect cellRect = schedule->scheduleTable()->visualRect(index);
        if (!cellRect.isValid() || cellRect.isEmpty())
        {
            return false;
        }
        QTest::mouseClick(schedule->scheduleTable()->viewport(), Qt::LeftButton,
                          Qt::NoModifier, cellRect.center());
        QApplication::processEvents();
        return true;
    }
};

QString createTemporaryWorkspace(QTemporaryDir& directory,
                                 const QString& name,
                                 const QString& teacherName)
{
    const QString path = directory.filePath(name);
    QString error;
    return createScheduleFixture(path, teacherName, &error) ? path : QString{};
}

} // namespace

class SchedulePageTests final : public QObject
{
    Q_OBJECT

private slots:
    void controlsPersistModesPreferencesAndTestingDisplay();
    void saveFailuresKeepVisibleAndPersistedDisplayState();
    void modelCellEntryLimitIsVisibleAndRejectsPartialProjection();
    void slotCyclesPersistDeleteDefaultsAndShowFailures();
    void classSelectionRoutesToClassDetailsAndSaveRefreshesSchedule();
    void retryReplacementSuspendReentryAndCloseReleaseRows();
    void languageChangeRetranslatesScheduleControls();
};

void SchedulePageTests::controlsPersistModesPreferencesAndTestingDisplay()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString path = createTemporaryWorkspace(
        temporary, QStringLiteral("controls.tps"), QStringLiteral("Control Teacher"));
    QVERIFY(!path.isEmpty());
    QVERIFY(Platform::saveScheduleDisplayPreferencesAtPath(path, {}));
    QVERIFY(setIntensiveSlotsEmpty(path));
    PageFixture fixture;
    QVERIFY(fixture.initialize(path));
    Ui::SchedulePage* const page = fixture.schedule;
    QVERIFY(page);
    QVERIFY(page->statusLabel()->parentWidget()->isHidden());
    QVERIFY(page->retryButton()->parentWidget()->isHidden());
    auto* const preferencesButton = page->findChild<QToolButton*>(
        QStringLiteral("nextSchedulePreferences"));
    QVERIFY(preferencesButton);
    QVERIFY(preferencesButton->menu());
    QCOMPARE(preferencesButton->menu()->actions().size(), 7);
    QVERIFY(page->findChild<QAction*>(QStringLiteral("nextScheduleImportAction")));
    for (int index = 0; index < 5; ++index)
    {
        QVERIFY(page->preferenceCheckBox(index));
    }
    QCOMPARE(page->model()->displayMode(), Application::ScheduleDisplayMode::Regular);
    QCOMPARE(page->visualMetrics().model.sourceClassCount, 96);

    QTest::mouseClick(page->modeButton(Application::ScheduleDisplayMode::Intensive),
                      Qt::LeftButton);
    QApplication::processEvents();
    QCOMPARE(page->model()->displayMode(), Application::ScheduleDisplayMode::Intensive);
    QVERIFY(page->modeButton(Application::ScheduleDisplayMode::Intensive)->isChecked());
    QCOMPARE(page->model()->rowCount(), 3);

    for (int index = 0; index < 5; ++index)
    {
        QCheckBox* const checkbox = page->preferenceCheckBox(index);
        QVERIFY(checkbox);
        QVERIFY(checkbox->isEnabled());
        checkbox->click();
        QApplication::processEvents();
        QVERIFY(checkbox->isChecked());
    }
    QVERIFY(page->statusLabel()->parentWidget()->isHidden());
    QVERIFY(page->retryButton()->parentWidget()->isHidden());
    const auto saved = Platform::readSchedulePageAtPath(path);
    QVERIFY(saved);
    QCOMPARE(saved.value().displayPreferences,
             (Application::ScheduleDisplayPreferences{
                 .use24HourTime = true,
                 .showEnglishNames = true,
                 .showWeekends = true,
                 .showAllIntensiveHours = true,
                 .testingAffectsM1 = true
             }));
    QCOMPARE(page->scheduleTable()->model()->columnCount(), 8);
    QCOMPARE(page->model()->rowCount(), 13);
    QVERIFY(page->model()->rowAt(0));
    QCOMPARE(page->model()->rowAt(0)->timeRangeLabel,
             QStringLiteral("09:00 - 09:50"));
    QTest::mouseClick(page->modeButton(Application::ScheduleDisplayMode::Regular),
                      Qt::LeftButton);
    QApplication::processEvents();
    const auto englishTeacherCell = findCell(page->model(), [](const Ui::SchedulePageCell& cell) {
        return std::any_of(cell.entries.cbegin(), cell.entries.cend(), [](const auto& entry) {
            return entry.classId == classId(1);
        });
    });
    QVERIFY(englishTeacherCell.has_value());
    const QModelIndex englishTeacherIndex = page->model()->index(
        englishTeacherCell->row(), englishTeacherCell->column());
    const QString accessibleTeacherText = page->model()->data(
        englishTeacherIndex, Qt::AccessibleTextRole).toString();
    QVERIFY(accessibleTeacherText.contains(QStringLiteral("Control Teacher")));
    QCOMPARE(page->model()->data(englishTeacherIndex, Qt::ToolTipRole).toString(),
             accessibleTeacherText);
    QTest::mouseClick(page->modeButton(Application::ScheduleDisplayMode::Intensive),
                      Qt::LeftButton);
    QApplication::processEvents();

    QTest::mouseClick(page->modeButton(Application::ScheduleDisplayMode::Testing),
                      Qt::LeftButton);
    QApplication::processEvents();
    QCOMPARE(page->model()->displayMode(), Application::ScheduleDisplayMode::Testing);
    QLabel* const testingBanner = page->findChild<QLabel*>(
        QStringLiteral("nextScheduleTestingBanner"));
    QVERIFY(testingBanner);
    QCOMPARE(testingBanner->text(), QStringLiteral(
        "Testing View — M1, M2, and M3 classes are hidden"));
    const auto testingCell = findCell(page->model(), [](const Ui::SchedulePageCell& cell) {
        return cell.day == QStringLiteral("Monday")
            && cell.startMinute == 17 * 60
            && cell.plainTestingAssignment;
    });
    QVERIFY(testingCell.has_value());
    const Ui::SchedulePageCell* const assignment = page->model()->cellAt(
        testingCell->row(), testingCell->column());
    QVERIFY(assignment);
    QVERIFY(assignment->entries.empty());
    QCOMPARE(assignment->testingRoom, QStringLiteral("Testing Room"));

    const auto hasGrade = [page](const QString& grade) {
        for (int row = 0; row < page->model()->rowCount(); ++row)
        {
            for (int column = 1; column < page->model()->columnCount(); ++column)
            {
                const Ui::SchedulePageCell* const cell =
                    page->model()->cellAt(row, column);
                if (cell && std::any_of(cell->entries.cbegin(), cell->entries.cend(),
                        [&grade](const Ui::SchedulePageEntry& entry) {
                            return entry.grade.trimmed() == grade;
                        }))
                {
                    return true;
                }
            }
        }
        return false;
    };
    QVERIFY(!hasGrade(QStringLiteral("M1")));
    QVERIFY(!hasGrade(QStringLiteral("M2")));

    page->preferenceCheckBox(4)->click();
    QApplication::processEvents();
    QVERIFY(!page->preferenceCheckBox(4)->isChecked());
    QCOMPARE(testingBanner->text(), QStringLiteral(
        "Testing View — M2 and M3 classes are hidden; M1 classes remain"));
    QVERIFY(hasGrade(QStringLiteral("M1")));
    QVERIFY(!hasGrade(QStringLiteral("M2")));
    const auto m1PreferenceSaved = Platform::readSchedulePageAtPath(path);
    QVERIFY(m1PreferenceSaved);
    QVERIFY(!m1PreferenceSaved.value().displayPreferences.testingAffectsM1);
}

void SchedulePageTests::saveFailuresKeepVisibleAndPersistedDisplayState()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString path = createTemporaryWorkspace(
        temporary, QStringLiteral("save-failures.tps"),
        QStringLiteral("Failure Teacher"));
    QVERIFY(!path.isEmpty());
    QVERIFY(Platform::saveScheduleDisplayPreferencesAtPath(path, {}));
    QVERIFY(Platform::saveScheduleDisplayModeAtPath(
        path, Application::ScheduleDisplayMode::Regular));

    PageFixture fixture;
    QVERIFY(fixture.initialize(path));
    Ui::SchedulePage* const page = fixture.schedule;
    QVERIFY(page);

    fixture.forceModeSaveFailure = true;
    QTest::mouseClick(
        page->modeButton(Application::ScheduleDisplayMode::Intensive),
        Qt::LeftButton);
    QApplication::processEvents();
    QVERIFY(page->statusLabel()->property("error").toBool());
    QVERIFY(page->statusLabel()->text().contains(
        QStringLiteral("schedule display mode save failed")));
    QCOMPARE(page->model()->displayMode(), Application::ScheduleDisplayMode::Regular);
    QVERIFY(page->modeButton(Application::ScheduleDisplayMode::Regular)->isChecked());
    QVERIFY(!page->modeButton(Application::ScheduleDisplayMode::Intensive)->isChecked());
    auto persisted = Platform::readSchedulePageAtPath(path);
    QVERIFY(persisted);
    QCOMPARE(persisted.value().displayMode, Application::ScheduleDisplayMode::Regular);

    fixture.forcePreferencesSaveFailure = true;
    QCheckBox* const use24Hour = page->preferenceCheckBox(0);
    QVERIFY(use24Hour);
    QVERIFY(!use24Hour->isChecked());
    use24Hour->click();
    QApplication::processEvents();
    QVERIFY(page->statusLabel()->property("error").toBool());
    QVERIFY(page->statusLabel()->text().contains(
        QStringLiteral("schedule preferences save failed")));
    QVERIFY(!use24Hour->isChecked());
    persisted = Platform::readSchedulePageAtPath(path);
    QVERIFY(persisted);
    QCOMPARE(persisted.value().displayPreferences,
             Application::ScheduleDisplayPreferences{});
    QCOMPARE(page->model()->preferences(),
             Application::ScheduleDisplayPreferences{});
}

void SchedulePageTests::modelCellEntryLimitIsVisibleAndRejectsPartialProjection()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString path = createTemporaryWorkspace(
        temporary, QStringLiteral("cell-limit.tps"),
        QStringLiteral("Limit Teacher"));
    QVERIFY(!path.isEmpty());
    const auto loaded = Platform::readSchedulePageAtPath(path);
    QVERIFY(loaded);

    PageFixture fixture;
    QVERIFY(fixture.initialize(path));
    Ui::SchedulePage* const page = fixture.schedule;
    QVERIFY(page);

    Application::SchedulePageReadSnapshot exactlyAtLimit = loaded.value();
    exactlyAtLimit.displayMode = Application::ScheduleDisplayMode::Regular;
    const Application::ScheduleBuilderSourceScheduleRow row{
        .day = Application::ScheduleBuilderSourceScheduleText(u"Monday"),
        .startTime = Application::ScheduleBuilderSourceScheduleText(u"4:00 PM"),
        .endTime = Application::ScheduleBuilderSourceScheduleText(u"4:50 PM")
    };
    for (auto& sourceClass : exactlyAtLimit.source.classes)
    {
        sourceClass.regularSchedule = {row};
    }
    fixture.scheduleReadOverride = exactlyAtLimit;
    QVERIFY(page->reload());
    QCOMPARE(page->visualMetrics().model.entryCount, 96);
    QCOMPARE(page->model()->rowCount(), 6);
    const auto fullCell = findCell(page->model(), [](const Ui::SchedulePageCell& cell) {
        return cell.day == QStringLiteral("Monday")
            && cell.startMinute == 16 * 60
            && cell.entries.size() == 96;
    });
    QVERIFY(fullCell.has_value());

    Application::SchedulePageReadSnapshot overLimit = exactlyAtLimit;
    Application::ScheduleBuilderSourceClass extraClass = overLimit.source.classes.front();
    extraClass.classId = classId(97);
    overLimit.source.classes.push_back(std::move(extraClass));
    fixture.scheduleReadOverride = std::move(overLimit);
    QVERIFY(!page->reload());
    QVERIFY(page->statusLabel()->property("error").toBool());
    QVERIFY(page->statusLabel()->text().contains(
        QStringLiteral("96 class-entry limit")));
    QCOMPARE(page->visualMetrics().model.rowCount, 0);
    QCOMPARE(page->visualMetrics().model.cellCount, 0);
    QCOMPARE(page->visualMetrics().model.entryCount, 0);
    QCOMPARE(page->visualMetrics().model.sourceClassCount, 0);
}

void SchedulePageTests::slotCyclesPersistDeleteDefaultsAndShowFailures()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString path = createTemporaryWorkspace(
        temporary, QStringLiteral("slots.tps"), QStringLiteral("Slot Teacher"));
    QVERIFY(!path.isEmpty());
    PageFixture fixture;
    QVERIFY(fixture.initialize(path));
    Ui::SchedulePage* const page = fixture.schedule;
    QTest::mouseClick(page->modeButton(Application::ScheduleDisplayMode::Intensive),
                      Qt::LeftButton);
    QApplication::processEvents();

    const auto mondayNine = findCell(page->model(), [](const Ui::SchedulePageCell& cell) {
        return cell.day == QStringLiteral("Monday")
            && cell.startMinute == 13 * 60
            && cell.entries.empty()
            && cell.slotTogglingEnabled;
    });
    QVERIFY(mondayNine.has_value());

    fixture.forceSlotSaveFailure = true;
    QVERIFY(fixture.clickCell(*mondayNine));
    QVERIFY(page->statusLabel()->property("error").toBool());
    QVERIFY(page->statusLabel()->text().contains(QStringLiteral("schedule slot save failed")));
    QCOMPARE(page->model()->cellAt(mondayNine->row(), mondayNine->column())->state,
             Application::ScheduleSlotState::Essay);

    fixture.forceSlotSaveFailure = false;
    QVERIFY(fixture.clickCell(*mondayNine));
    auto saved = Platform::readSchedulePageAtPath(path);
    QVERIFY(saved);
    QVERIFY(std::any_of(saved.value().slotStates.rows.begin(),
                        saved.value().slotStates.rows.end(), [](const auto& row) {
        return row.identity.day() == u"Monday"
            && row.identity.startTime() == u"13:00"
            && row.state == u"lunch";
    }));

    QVERIFY(fixture.clickCell(*mondayNine));
    saved = Platform::readSchedulePageAtPath(path);
    QVERIFY(saved);
    QVERIFY(std::any_of(saved.value().slotStates.rows.begin(),
                        saved.value().slotStates.rows.end(), [](const auto& row) {
        return row.identity.day() == u"Monday"
            && row.identity.startTime() == u"13:00"
            && row.state == u"empty";
    }));

    QVERIFY(fixture.clickCell(*mondayNine));
    saved = Platform::readSchedulePageAtPath(path);
    QVERIFY(saved);
    QVERIFY(std::none_of(saved.value().slotStates.rows.begin(),
                         saved.value().slotStates.rows.end(), [](const auto& row) {
        return row.identity.day() == u"Monday"
            && row.identity.startTime() == u"13:00";
    }));
}

void SchedulePageTests::classSelectionRoutesToClassDetailsAndSaveRefreshesSchedule()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString path = createTemporaryWorkspace(
        temporary, QStringLiteral("class-edit.tps"), QStringLiteral("Edited Schedule Teacher"));
    QVERIFY(!path.isEmpty());
    PageFixture fixture;
    QVERIFY(fixture.initialize(path));
    Ui::SchedulePage* const page = fixture.schedule;
    const auto classOneCell = findCell(page->model(), [](const Ui::SchedulePageCell& cell) {
        return std::any_of(cell.entries.begin(), cell.entries.end(), [](const auto& entry) {
            return entry.classId == classId(1);
        });
    });
    QVERIFY(classOneCell.has_value());
    QVERIFY(fixture.clickCell(*classOneCell));
    QVERIFY(fixture.host->activePageKey()
        == std::optional<QString>(QStringLiteral("classes")));
    QVERIFY(fixture.selectedClass == std::optional<Domain::ClassId>(classId(1)));
    QCOMPARE(page->visualMetrics().model.rowCount, 0);
    QVERIFY(fixture.detailsRoute);

    QTest::mouseClick(fixture.detailsRoute->saveButton(), Qt::LeftButton);
    QApplication::processEvents();
    QVERIFY2(fixture.classSaveSucceeded, qPrintable(fixture.classSaveError));
    QCOMPARE(fixture.lastSavedPath, path);
    QVERIFY(fixture.host->activePageKey()
        == std::optional<QString>(QStringLiteral("schedule")));
    const auto updatedCell = findCell(page->model(), [](const Ui::SchedulePageCell& cell) {
        return cell.day == QStringLiteral("Monday")
            && cell.startMinute == 20 * 60
            && std::any_of(cell.entries.begin(), cell.entries.end(), [](const auto& entry) {
                return entry.classId == classId(1);
            });
    });
    QVERIFY(updatedCell.has_value());
}

void SchedulePageTests::retryReplacementSuspendReentryAndCloseReleaseRows()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString firstPath = createTemporaryWorkspace(
        temporary, QStringLiteral("first.tps"), QStringLiteral("First Teacher"));
    const QString secondPath = createTemporaryWorkspace(
        temporary, QStringLiteral("second.tps"), QStringLiteral("Second Teacher"));
    QVERIFY(!firstPath.isEmpty());
    QVERIFY(!secondPath.isEmpty());

    PageFixture fixture;
    fixture.forceReadFailure = true;
    QVERIFY(fixture.initialize(firstPath));
    Ui::SchedulePage* const page = fixture.schedule;
    QVERIFY(page->statusLabel()->property("error").toBool());
    QVERIFY(page->retryButton()->isEnabled());
    QCOMPARE(page->visualMetrics().model.rowCount, 0);
    fixture.forceReadFailure = false;
    QTest::mouseClick(page->retryButton(), Qt::LeftButton);
    QApplication::processEvents();
    QCOMPARE(page->visualMetrics().model.sourceClassCount, 96);
    QCOMPARE(page->visualMetrics().model.entryCount, 768);
    QVERIFY(page->visualMetrics().tableViewCount == 1);
    QCOMPARE(page->visualMetrics().tableCellWidgetCount, 0);
    QVERIFY(page->visualMetrics().descendantWidgetCount < 64);
    for (int row = 0; row < page->model()->rowCount(); ++row)
    {
        int maximumEntryCount = 0;
        for (int column = 1; column < page->model()->columnCount(); ++column)
        {
            const Ui::SchedulePageCell* const cell =
                page->model()->cellAt(row, column);
            QVERIFY(cell);
            maximumEntryCount = std::max(
                maximumEntryCount, static_cast<int>(cell->entries.size()));
        }
        if (maximumEntryCount > 0)
        {
            QCOMPARE(page->scheduleTable()->rowHeight(row),
                     22 + maximumEntryCount * 38);
        }
    }

    fixture.suspendSchedule();
    QCOMPARE(page->visualMetrics().model.rowCount, 0);
    QCOMPARE(page->visualMetrics().model.cellCount, 0);
    QCOMPARE(page->visualMetrics().model.entryCount, 0);
    QCOMPARE(page->visualMetrics().model.sourceClassCount, 0);

    fixture.path = secondPath;
    fixture.identity = QStringLiteral("same-workspace-id|") + secondPath;
    QVERIFY(fixture.host->activatePage(QStringLiteral("schedule")));
    QApplication::processEvents();
    QCOMPARE(page->visualMetrics().model.sourceClassCount, 96);
    QCOMPARE(page->visualMetrics().model.entryCount, 768);
    const auto teacher = findCell(page->model(), [](const Ui::SchedulePageCell& cell) {
        return std::any_of(cell.entries.begin(), cell.entries.end(), [](const auto& entry) {
            return entry.classId == classId(1);
        });
    });
    QVERIFY(teacher.has_value());
    const Ui::SchedulePageCell* const teacherCell = page->model()->cellAt(
        teacher->row(), teacher->column());
    QCOMPARE(teacherCell->entries.front().teacherEnglishName,
             QStringLiteral("Second Teacher"));

    fixture.suspendSchedule();
    fixture.path.clear();
    fixture.identity.clear();
    QVERIFY(fixture.host->activatePage(QStringLiteral("schedule")));
    QApplication::processEvents();
    QCOMPARE(page->visualMetrics().model.rowCount, 0);
    QCOMPARE(page->visualMetrics().model.cellCount, 0);
    QCOMPARE(page->visualMetrics().model.entryCount, 0);
    QCOMPARE(page->visualMetrics().model.sourceClassCount, 0);
    QVERIFY(!page->scheduleTable()->isEnabled());
    QCOMPARE(page->statusLabel()->text(), QStringLiteral("No Teacher Profile is open."));
}

void SchedulePageTests::languageChangeRetranslatesScheduleControls()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString path = createTemporaryWorkspace(
        temporary, QStringLiteral("language.tps"), QStringLiteral("Language Teacher"));
    QVERIFY(!path.isEmpty());
    PageFixture fixture;
    QVERIFY(fixture.initialize(path));
    QTest::mouseClick(
        fixture.schedule->modeButton(Application::ScheduleDisplayMode::Testing),
        Qt::LeftButton);
    QApplication::processEvents();
    QTranslator translator;
    QVERIFY(translator.load(QDir(QStringLiteral(
        CLASSMNGR_NEXT_TEST_TRANSLATION_DIRECTORY)).filePath(
            QStringLiteral("ClassMngr_ko_KR.qm"))));
    QVERIFY(qApp->installTranslator(&translator));
    QEvent languageChange(QEvent::LanguageChange);
    QApplication::sendEvent(fixture.schedule, &languageChange);
    QCOMPARE(fixture.schedule->findChild<QLabel*>(
                 QStringLiteral("nextScheduleTestingBanner"))->text(),
             QStringLiteral("테스트 보기 — M2, M3 수업은 숨겨지고 M1 수업은 표시됩니다"));
    QCOMPARE(fixture.schedule->accessibleName(), QStringLiteral("주간 수업 일정"));
    QCOMPARE(fixture.schedule->modeButton(Application::ScheduleDisplayMode::Regular)
                 ->text(), QStringLiteral("정규"));
    QCOMPARE(fixture.schedule->findChild<QToolButton*>(
                 QStringLiteral("nextSchedulePreferences"))->text(),
             QStringLiteral("표시 설정"));
    qApp->removeTranslator(&translator);
}

QTEST_MAIN(SchedulePageTests)
#include "schedule_page_tests.moc"
