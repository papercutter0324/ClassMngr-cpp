#include "data/database/workspace_store.h"
#include "core/process_memory_snapshot.h"
#include "core/settingsmanager.h"
#include "features/schedule/import/schedule_workbook_parser.h"
#include "next/platform/workspace_store_schedule_actions.h"
#include "next/platform/workspace_store_schedule_import_actions.h"
#include "next/resources/resource_catalog.h"
#include "next/resources/resource_loader.h"
#include "next/ui/page_host.h"
#include "next/ui/schedule_import_workflow_dialog.h"
#include "next/ui/schedule_import_review_models.h"
#include "next/ui/schedule_page.h"
#include "preferences_test_ports.h"
#include "sql_script_runner.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QHeaderView>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPalette>
#include <QPushButton>
#include <QStandardPaths>
#include <QPointer>
#include <QRadioButton>
#include <QStyleFactory>
#include <QSet>
#include <QStackedWidget>
#include <QSqlQuery>
#include <QTableView>
#include <QTemporaryDir>
#include <QTimer>
#include <QThread>
#include <QTranslator>
#include <QVariantMap>
#include <QtTest/QtTest>

#include <algorithm>
#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace ClassMngr::Next;

namespace
{

QString scheduleFixturePath()
{
    return QDir(QStringLiteral(CLASSMNGR_SOURCE_DIR)).filePath(
        QStringLiteral("tests/fixtures/imports/schedule_review.xlsx"));
}

QString largeConflictFixturePath()
{
    return QDir(QStringLiteral(CLASSMNGR_SOURCE_DIR)).filePath(
        QStringLiteral("docs/qt-rewrite/visual-baseline/release/"
                       "normal-schedule-import-next/"
                       "generated-large-schedule-import.xlsx"));
}

QString largeMutationFixturePath()
{
    return QDir(QStringLiteral(CLASSMNGR_SOURCE_DIR)).filePath(
        QStringLiteral("docs/qt-rewrite/visual-baseline/release/"
                       "large-schedule-import-boundary/"
                       "generated-large-schedule-import.xlsx"));
}

QString nondefaultSelectionFixturePath()
{
    return QDir(QStringLiteral(CLASSMNGR_SOURCE_DIR)).filePath(
        QStringLiteral("tests/fixtures/imports/"
                       "schedule_import_nondefault_selection.xlsx"));
}

QString largeApplicableFixturePath()
{
    return QDir(QStringLiteral(CLASSMNGR_SOURCE_DIR)).filePath(
        QStringLiteral("docs/qt-rewrite/visual-baseline/release/"
                       "normal-schedule-import-next/"
                       "generated-large-schedule-import-applicable.xlsx"));
}

QString intensiveFixturePath()
{
    return QDir(QStringLiteral(CLASSMNGR_SOURCE_DIR)).filePath(
        QStringLiteral("tests/fixtures/imports/schedule_intensive_synthetic.xlsx"));
}

QString overlapConflictFixturePath()
{
    return QDir(QStringLiteral(CLASSMNGR_SOURCE_DIR)).filePath(
        QStringLiteral("tests/fixtures/imports/schedule_overlap_conflict.xlsx"));
}

std::optional<ScheduleImportUserBlock> firstVisibleUser(
    const QString& workbookPath,
    const ScheduleImportKind kind = ScheduleImportKind::Normal
    )
{
    QFile workbookFile(workbookPath);
    if (!workbookFile.open(QIODevice::ReadOnly))
    {
        return std::nullopt;
    }
    auto parsed = parseScheduleImportWorkbook(workbookFile, kind);
    if (!parsed)
    {
        return std::nullopt;
    }
    for (const ScheduleImportSheet& sheet : parsed->sheets)
    {
        if (sheet.visible && !sheet.users.isEmpty())
        {
            return sheet.users.front();
        }
    }
    return std::nullopt;
}

bool createWorkspace(const QString& path, const ScheduleImportUserBlock& user,
                     QString* error)
{
    WorkspaceStore store;
    const RepositoryStatus opened = store.open(path);
    if (!opened)
    {
        *error = opened.error().diagnostic;
        return false;
    }

    QSet<QString> teacherKeys;
    int nextId = 101;
    for (const ScheduleImportClassCandidate& candidate : user.classes)
    {
        if (teacherKeys.contains(candidate.teacherKey))
        {
            continue;
        }
        teacherKeys.insert(candidate.teacherKey);
        QSqlQuery insert(store.database());
        insert.prepare(QStringLiteral(R"(
            INSERT INTO teachers (id, teacher_kr, room_number)
            VALUES (?, ?, ?)
        )"));
        insert.addBindValue(nextId++);
        insert.addBindValue(candidate.teacherKr);
        insert.addBindValue(QStringLiteral("Original Room"));
        if (!insert.exec())
        {
            *error = insert.lastError().text();
            return false;
        }
    }
    return true;
}

bool runSql(const QString& path, const QString& sql, QString* error = nullptr)
{
    WorkspaceStore store;
    const RepositoryStatus opened = store.openExisting(path);
    if (!opened)
    {
        if (error) *error = opened.error().diagnostic;
        return false;
    }
    QSqlQuery query(store.database());
    if (!query.exec(sql))
    {
        if (error) *error = query.lastError().text();
        return false;
    }
    return true;
}

std::optional<int> scalarInt(const QString& path, const QString& sql)
{
    WorkspaceStore store;
    if (!store.openExisting(path))
    {
        return std::nullopt;
    }
    QSqlQuery query(store.database());
    if (!query.exec(sql) || !query.next())
    {
        return std::nullopt;
    }
    return query.value(0).toInt();
}

QString teacherRoom(const QString& path, const QString& id)
{
    WorkspaceStore store;
    if (!store.openExisting(path))
    {
        return {};
    }
    QSqlQuery query(store.database());
    query.prepare(QStringLiteral("SELECT room_number FROM teachers WHERE id=?"));
    query.addBindValue(id.toLongLong());
    return query.exec() && query.next() ? query.value(0).toString() : QString{};
}

QJsonObject memoryJson(const ProcessMemorySnapshot& sample)
{
    return {
        {QStringLiteral("available"), sample.isAvailable},
        {QStringLiteral("platform"), sample.platform},
        {QStringLiteral("workingSetBytes"),
            static_cast<qint64>(sample.workingSetBytes)},
        {QStringLiteral("peakWorkingSetBytes"),
            static_cast<qint64>(sample.peakWorkingSetBytes)},
        {QStringLiteral("privateUsageBytes"),
            static_cast<qint64>(sample.privateUsageBytes)},
        {QStringLiteral("privateWorkingSetBytes"),
            static_cast<qint64>(sample.privateWorkingSetBytes)},
        {QStringLiteral("capturedAt"), sample.capturedAt.toString(Qt::ISODateWithMs)}
    };
}

QJsonArray rectangleJson(const QRect& rect)
{
    return {rect.x(), rect.y(), rect.width(), rect.height()};
}

class ScheduleImportPageFixture final
{
public:
    Test::MemoryPreferences preferences;
    std::unique_ptr<Resources::ResourceCatalog> catalog;
    std::unique_ptr<Resources::ResourceLoader> loader;
    std::unique_ptr<Ui::PageHost> host;
    QString activePath;
    QString identity;
    QString workbookPath = scheduleFixturePath();
    std::function<Result<ScheduleImportWorkbook>(QIODevice&, ScheduleImportKind)>
        parseWorkbook;
    int timeoutMilliseconds = 30'000;
    Ui::SchedulePage* page = nullptr;
    std::vector<Application::ScheduleImportApplyRequest> applyRequests;

    bool initialize(const QString& path)
    {
        activePath = path;
        identity = QStringLiteral("normal-workspace|") + path;
        auto loaded = Resources::ResourceCatalog::load(
            QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_CATALOG),
            QStringLiteral(CLASSMNGR_NEXT_TEST_RESOURCE_ROOT));
        if (!loaded)
        {
            return false;
        }
        catalog = std::make_unique<Resources::ResourceCatalog>(
            std::move(*loaded));
        loader = std::make_unique<Resources::ResourceLoader>(*catalog);
        host = std::make_unique<Ui::PageHost>(*loader, preferences.model);

        const auto pageId = Ui::PageId::fromString(QStringLiteral("schedule"));
        if (!pageId)
        {
            return false;
        }
        Ui::PageDescriptor descriptor{
            .id = *pageId,
            .label = QStringLiteral("Schedule"),
            .factory = [this](const Ui::PageContext& context)
                -> Ui::PageOperationResult<std::unique_ptr<Ui::Page>> {
                Ui::SchedulePageActions actions;
                actions.activeWorkspacePath = [this] { return activePath; };
                actions.workspaceIdentity = [this] { return identity; };
                actions.readSchedule = [](const QString& databasePath) {
                    return Platform::readSchedulePageAtPath(databasePath);
                };
                actions.saveDisplayPreferences = [](
                    const QString& databasePath,
                    const Application::ScheduleDisplayPreferencesSaveRequest& request) {
                    return Platform::saveScheduleDisplayPreferencesAtPath(
                        databasePath, request);
                };
                actions.saveDisplayMode = [](
                    const QString& databasePath,
                    const Application::ScheduleDisplayMode mode) {
                    return Platform::saveScheduleDisplayModeAtPath(
                        databasePath, mode);
                };
                actions.saveSlotState = [](
                    const QString& databasePath,
                    const Application::ScheduleSlotStateSaveRequest& request) {
                    return Platform::saveScheduleSlotStateAtPath(
                        databasePath, request);
                };
                actions.chooseScheduleImportWorkbook = [this](QWidget*) {
                    return workbookPath;
                };
                actions.parseScheduleImportWorkbook = parseWorkbook;
                actions.scheduleImportTimeoutMilliseconds = [this] {
                    return timeoutMilliseconds;
                };
                actions.readScheduleImportState = [](
                    const QString& databasePath) {
                    return Platform::readScheduleImportStateAtPath(databasePath);
                };
                actions.applyScheduleImport = [this](
                    const QString& databasePath,
                    const Application::ScheduleImportApplyRequest& request) {
                    applyRequests.push_back(request);
                    return Platform::applyScheduleImportAtPath(
                        databasePath, request);
                };
                auto created = std::make_unique<Ui::SchedulePage>(
                    context, std::move(actions));
                page = created.get();
                return Ui::PageOperationResult<std::unique_ptr<Ui::Page>>{
                    std::move(created)};
            }
        };
        if (!host->registerPage(std::move(descriptor)))
        {
            return false;
        }
        return host->activatePage(QStringLiteral("schedule")).has_value();
    }

    Ui::ScheduleImportWorkflowDialog* openImport()
    {
        if (!page)
        {
            return nullptr;
        }
        QAction* const action = page->findChild<QAction*>(
            QStringLiteral("nextScheduleImportAction"));
        if (!action)
        {
            return nullptr;
        }
        action->trigger();
        return page->importDialog();
    }

    bool selectWorkbookAndRead(Ui::ScheduleImportWorkflowDialog* dialog)
    {
        if (!dialog)
        {
            return false;
        }
        auto* const browse = dialog->findChild<QPushButton*>(
            QStringLiteral("nextScheduleImportBrowse"));
        if (!browse)
        {
            return false;
        }
        browse->click();
        if (dialog->sourcePathEdit()->text() != workbookPath)
        {
            return false;
        }
        dialog->primaryButton()->click();
        return true;
    }

    bool continueToReview(Ui::ScheduleImportWorkflowDialog* dialog,
                          const int sheetIndex = 0,
                          const int userIndex = 0)
    {
        if (!dialog || !dialog->sheetCombo() || !dialog->userCombo())
        {
            return false;
        }
        if (dialog->sheetCombo()->count() <= sheetIndex)
        {
            return false;
        }
        dialog->sheetCombo()->setCurrentIndex(sheetIndex);
        if (dialog->userCombo()->count() <= userIndex)
        {
            return false;
        }
        dialog->userCombo()->setCurrentIndex(userIndex);
        dialog->primaryButton()->click();
        return true;
    }
};

bool setTeacherReviewToUpdateRoom(
    Ui::ScheduleImportWorkflowDialog* dialog,
    QString* teacherId
    )
{
    auto* const model = dialog && dialog->teacherTable()
        ? dialog->teacherTable()->model() : nullptr;
    if (!model || model->rowCount() == 0)
    {
        return false;
    }
    const QModelIndex actionIndex = model->index(
        0, Ui::ScheduleImportTeacherReviewModel::ActionColumn);
    const QModelIndex targetIndex = model->index(
        0, Ui::ScheduleImportTeacherReviewModel::TargetColumn);
    const QModelIndex roomIndex = model->index(
        0, Ui::ScheduleImportTeacherReviewModel::RoomColumn);
    *teacherId = model->data(targetIndex, Qt::EditRole).toString();
    if (teacherId->isEmpty())
    {
        return false;
    }
    if (!model->setData(actionIndex, static_cast<int>(
            Application::ScheduleImportReviewTeacherAction::UpdateRoom),
            Qt::EditRole))
    {
        return false;
    }

    QString room = model->data(roomIndex, Qt::EditRole).toString();
    if (room.isEmpty())
    {
        const QVariantList choices = model->data(roomIndex,
            Qt::UserRole + 1).toList();
        for (const QVariant& item : choices)
        {
            room = item.toMap().value(QStringLiteral("value")).toString();
            if (!room.isEmpty())
            {
                break;
            }
        }
    }
    return !room.isEmpty() && model->setData(roomIndex, room, Qt::EditRole);
}

void setAllClassesToCreateNew(
    Ui::ScheduleImportWorkflowDialog* dialog,
    const bool acknowledgeDiagnostics = true
    )
{
    auto* const model = dialog->classTable()->model();
    for (int row = 0; row < model->rowCount(); ++row)
    {
        static_cast<void>(model->setData(
            model->index(row, Ui::ScheduleImportClassReviewModel::ActionColumn),
            static_cast<int>(Application::ScheduleImportReviewClassAction::CreateNew),
            Qt::EditRole));
    }
    if (acknowledgeDiagnostics)
    {
        if (auto* const diagnostics = dialog->findChild<QCheckBox*>(
            QStringLiteral("nextScheduleImportDiagnosticsAcknowledged")))
        {
            diagnostics->setChecked(true);
        }
    }
}

void setAllTeachersToCreate(Ui::ScheduleImportWorkflowDialog* dialog)
{
    auto* const model = dialog->teacherTable()->model();
    for (int row = 0; row < model->rowCount(); ++row)
    {
        static_cast<void>(model->setData(
            model->index(row, Ui::ScheduleImportTeacherReviewModel::ActionColumn),
            static_cast<int>(Application::ScheduleImportReviewTeacherAction::Create),
            Qt::EditRole));
    }
}

bool seedAbsentIntensiveClass(const QString& path, QString* error)
{
    WorkspaceStore store;
    if (!store.openExisting(path))
    {
        *error = QStringLiteral("Could not reopen the intensive workspace.");
        return false;
    }
    QSqlQuery query(store.database());
    if (!query.exec(QStringLiteral(R"(
        INSERT INTO classes (id, name) VALUES (999, 'E5 Absent')
    )")))
    {
        *error = query.lastError().text();
        return false;
    }
    if (!query.exec(QStringLiteral(R"(
        INSERT INTO class_info (class_id, teacher_id, class_grade, class_level)
        VALUES (999, 101, 'E5', 'Absent')
    )")))
    {
        *error = query.lastError().text();
        return false;
    }
    if (!query.exec(QStringLiteral(R"(
        INSERT INTO class_intensive_times (class_id, day, start_time, end_time)
        VALUES (999, 'Friday', '9:00 AM', '9:50 AM')
    )")))
    {
        *error = query.lastError().text();
        return false;
    }
    return true;
}

bool seedNormalExistingSchedule(
    const QString& path,
    const ScheduleImportClassCandidate& matchedCandidate,
    QString* error
    )
{
    WorkspaceStore store;
    if (!store.openExisting(path))
    {
        *error = QStringLiteral("Could not reopen the normal workspace.");
        return false;
    }
    QSqlQuery query(store.database());
    if (!query.exec(QStringLiteral(R"(
        INSERT INTO classes (id, name) VALUES (999, 'E5 Absent')
    )")))
    {
        *error = query.lastError().text();
        return false;
    }
    query.prepare(QStringLiteral(R"(
        INSERT INTO classes (id, name) VALUES (888, ?)
    )"));
    query.addBindValue(matchedCandidate.classGrade + QLatin1Char(' ')
                       + matchedCandidate.classLevel);
    if (!query.exec())
    {
        *error = query.lastError().text();
        return false;
    }
    query.prepare(QStringLiteral(R"(
        INSERT INTO class_info (class_id, teacher_id, class_grade, class_level)
        VALUES (888, 101, ?, ?)
    )"));
    query.addBindValue(matchedCandidate.classGrade);
    query.addBindValue(matchedCandidate.classLevel);
    if (!query.exec())
    {
        *error = query.lastError().text();
        return false;
    }
    if (!query.exec(QStringLiteral(R"(
        INSERT INTO class_times (class_id, day, start_time, end_time)
        VALUES (888, 'Monday', '8:00 PM', '8:55 PM')
    )")))
    {
        *error = query.lastError().text();
        return false;
    }
    if (!query.exec(QStringLiteral(R"(
        INSERT INTO class_info (class_id, teacher_id, class_grade, class_level)
        VALUES (999, 101, 'E5', 'Absent')
    )")))
    {
        *error = query.lastError().text();
        return false;
    }
    if (!query.exec(QStringLiteral(R"(
        INSERT INTO class_times (class_id, day, start_time, end_time)
        VALUES (999, 'Monday', '9:00 PM', '9:55 PM')
    )")))
    {
        *error = query.lastError().text();
        return false;
    }
    return true;
}

bool setFirstClassToUpdateExisting(
    Ui::ScheduleImportWorkflowDialog* dialog,
    const QString& classId
    )
{
    auto* const model = dialog->classTable()->model();
    if (!model || model->rowCount() == 0)
    {
        return false;
    }
    const QModelIndex action = model->index(
        0, Ui::ScheduleImportClassReviewModel::ActionColumn);
    const QModelIndex target = model->index(
        0, Ui::ScheduleImportClassReviewModel::TargetColumn);
    if (!model->setData(action, static_cast<int>(
            Application::ScheduleImportReviewClassAction::UpdateExisting),
            Qt::EditRole))
    {
        return false;
    }
    const QVariantList choices = model->data(target, Qt::UserRole + 1).toList();
    const bool hasRequestedTarget = std::any_of(choices.cbegin(), choices.cend(),
        [&classId](const QVariant& item) {
            return item.toMap().value(QStringLiteral("value")).toString()
                == classId;
        });
    return hasRequestedTarget
        && model->setData(target, classId, Qt::EditRole);
}

} // namespace

class NormalScheduleImportIntegrationTests final : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void normalImportUsesExactWorkspacePathAndRollsBackThenRetries();
    void nonDefaultSheetAndBlockApplyOnlyAfterDiagnosticsAcknowledgement();
    void intensiveImportPreservesOrClearsAbsentSchedulesByTypedMode();
    void overlappingWorkbookPresentsReviewConflict();
    void largeWorkbookReviewUsesBoundedWidgetsAndReleasesDataOnCancel();
    void largeNonOverlappingBlockAppliesTwentyCandidates();
    void captureImportVisualEvidenceWhenConfigured();
    void changedWorkbookIsDiscardedAndCancellationReleasesWorkflow();
    void timeoutAndCanceledLateParseResultsAreDiscarded();
    void suspendingDuringAsyncParseCancelsAndIgnoresLateResult();
    void replacingScheduleWithReviewOpenReleasesRetainedData();
    void staleWorkspaceIdentityRejectsApply();

private:
    QTemporaryDir m_settingsRoot;
    QString m_setupRecoveryJournalPath;
};

void NormalScheduleImportIntegrationTests::initTestCase()
{
    QVERIFY(m_settingsRoot.isValid());
    QVERIFY(qputenv("CLASSMNGR_SETTINGS_ROOT",
                    m_settingsRoot.path().toUtf8()));
    SettingsManager::instance().setRecentFiles(
        {QStringLiteral("schedule-import-history-sentinel.tps")});
    SettingsManager::instance().setLastFile(
        QStringLiteral("schedule-import-last-sentinel.tps"));

    QStandardPaths::setTestModeEnabled(true);
    m_setupRecoveryJournalPath = QDir(QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation)).filePath(
            QStringLiteral("initial-setup-recovery.json"));
    QVERIFY(!QFileInfo::exists(m_setupRecoveryJournalPath));
}

void NormalScheduleImportIntegrationTests::
normalImportUsesExactWorkspacePathAndRollsBackThenRetries()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto importedUser = firstVisibleUser(scheduleFixturePath());
    QVERIFY(importedUser.has_value());
    QVERIFY(!importedUser->classes.isEmpty());

    const QString firstPath = temporary.filePath(QStringLiteral("first.tps"));
    const QString secondPath = temporary.filePath(QStringLiteral("second.tps"));
    QString error;
    QVERIFY2(createWorkspace(firstPath, *importedUser, &error), qPrintable(error));
    QVERIFY2(createWorkspace(secondPath, *importedUser, &error), qPrintable(error));
    QVERIFY2(seedNormalExistingSchedule(
        firstPath, importedUser->classes.front(), &error), qPrintable(error));
    int expectedScheduleRows = 0;
    for (const ScheduleImportClassCandidate& candidate : importedUser->classes)
    {
        expectedScheduleRows += candidate.times.size();
    }

    ScheduleImportPageFixture fixture;
    QVERIFY(fixture.initialize(firstPath));
    QVERIFY(fixture.page);
    QCOMPARE(fixture.page->model()->runtimeMetrics().sourceClassCount, 2);

    Ui::ScheduleImportWorkflowDialog* dialog = fixture.openImport();
    QVERIFY(dialog);
    QVERIFY(fixture.selectWorkbookAndRead(dialog));
    QTRY_VERIFY_WITH_TIMEOUT(dialog->sheetCombo()->count() > 0, 10000);
    QVERIFY(fixture.continueToReview(dialog));
    QTRY_VERIFY_WITH_TIMEOUT(dialog->classTable()->model()->rowCount()
                             == importedUser->classes.size(), 10000);

    QString targetTeacherId;
    QVERIFY(setTeacherReviewToUpdateRoom(dialog, &targetTeacherId));
    setAllClassesToCreateNew(dialog);
    QVERIFY(setFirstClassToUpdateExisting(dialog, QStringLiteral("888")));
    QVERIFY(scalarInt(firstPath, QStringLiteral(
        "SELECT COUNT(*) FROM teachers WHERE id=%1").arg(targetTeacherId)) == 1);
    QCOMPARE(scalarInt(secondPath, QStringLiteral(
        "SELECT COUNT(*) FROM teachers WHERE id=%1").arg(targetTeacherId)),
        std::optional<int>{1});

    // The review snapshot was read from firstPath. Removing a selected teacher
    // after review must be rejected by apply-time revalidation.
    QVERIFY2(runSql(firstPath, QStringLiteral(
        "DELETE FROM teachers WHERE id=%1").arg(targetTeacherId), &error),
        qPrintable(error));
    dialog->primaryButton()->click();
    QTRY_VERIFY_WITH_TIMEOUT(dialog->findChild<QLabel*>(
        QStringLiteral("nextScheduleImportReviewStatus"))->text().contains(
            QStringLiteral("teacher"), Qt::CaseInsensitive), 5000);
    QCOMPARE(scalarInt(firstPath, QStringLiteral("SELECT COUNT(*) FROM classes")),
             std::optional<int>{2});
    QVERIFY(fixture.applyRequests.size() == 1);
    QVERIFY(!fixture.applyRequests.back().saveProfileNameIfBlank);
    QVERIFY(!fixture.applyRequests.back().updateProfileName);

    // Restore the selected teacher, then force a failure after import writes
    // have begun. The transaction must roll back the room update, classes,
    // and schedule rows before the same review can be retried.
    {
        WorkspaceStore store;
        QVERIFY2(store.openExisting(firstPath), "first workspace reopens");
        QSqlQuery restore(store.database());
        restore.prepare(QStringLiteral(R"(
            INSERT INTO teachers (id, teacher_kr, room_number)
            VALUES (?, ?, ?)
        )"));
        restore.addBindValue(targetTeacherId.toLongLong());
        restore.addBindValue(dialog->teacherTable()->model()->data(
            dialog->teacherTable()->model()->index(
                0, Ui::ScheduleImportTeacherReviewModel::TeacherColumn),
            Qt::DisplayRole).toString());
        restore.addBindValue(QStringLiteral("Original Room"));
        QVERIFY2(restore.exec(), qPrintable(restore.lastError().text()));
    }
    QVERIFY2(runSql(firstPath, QStringLiteral(R"(
        CREATE TRIGGER fail_schedule_import_time
        BEFORE INSERT ON class_times
        BEGIN
            SELECT RAISE(ABORT, 'integration import failure');
        END
    )"), &error), qPrintable(error));
    dialog->primaryButton()->click();
    QTRY_VERIFY_WITH_TIMEOUT(dialog->findChild<QLabel*>(
        QStringLiteral("nextScheduleImportReviewStatus"))->text().contains(
            QStringLiteral("integration import failure")), 5000);
    QCOMPARE(scalarInt(firstPath, QStringLiteral("SELECT COUNT(*) FROM classes")),
             std::optional<int>{2});
    QCOMPARE(scalarInt(firstPath, QStringLiteral(
        "SELECT COUNT(*) FROM class_times")), std::optional<int>{2});
    QCOMPARE(teacherRoom(firstPath, targetTeacherId),
             QStringLiteral("Original Room"));
    QCOMPARE(scalarInt(secondPath, QStringLiteral("SELECT COUNT(*) FROM classes")),
             std::optional<int>{0});

    QVERIFY2(runSql(firstPath,
        QStringLiteral("DROP TRIGGER fail_schedule_import_time"), &error),
        qPrintable(error));
    dialog->primaryButton()->click();
    QTRY_VERIFY_WITH_TIMEOUT(fixture.applyRequests.size() == 3, 5000);
    QVERIFY(!fixture.applyRequests.back().classes.empty());
    QCOMPARE(fixture.applyRequests.back().classes.front().action,
        Application::ScheduleImportReviewClassAction::UpdateExisting);
    QVERIFY(fixture.applyRequests.back().classes.front().targetClassId.has_value());
    QCOMPARE(fixture.applyRequests.back().classes.front().targetClassId->value(),
             std::string("888"));
    QTRY_VERIFY_WITH_TIMEOUT(dialog->isHidden(), 5000);
    QCOMPARE(dialog->classTable()->model()->rowCount(), 0);
    QCOMPARE(dialog->teacherTable()->model()->rowCount(), 0);
    QCOMPARE(dialog->findChild<QTableView*>(
        QStringLiteral("nextScheduleImportPreviewTable"))->model()->rowCount(), 0);
    QCOMPARE(dialog->sourcePathEdit()->text(), QString());
    QCOMPARE(dialog->sheetCombo()->count(), 0);
    QCOMPARE(dialog->userCombo()->count(), 0);
    QTRY_VERIFY_WITH_TIMEOUT(fixture.page->model()->runtimeMetrics().sourceClassCount
                             == importedUser->classes.size() + 1, 5000);
    QCOMPARE(scalarInt(firstPath, QStringLiteral("SELECT COUNT(*) FROM classes")),
             std::optional<int>{static_cast<int>(importedUser->classes.size()) + 1});
    QCOMPARE(scalarInt(firstPath, QStringLiteral(
        "SELECT COUNT(*) FROM class_times WHERE class_id=999")),
        std::optional<int>{0});
    QCOMPARE(scalarInt(firstPath, QStringLiteral("SELECT COUNT(*) FROM class_times")),
             std::optional<int>{expectedScheduleRows});
    QCOMPARE(scalarInt(secondPath, QStringLiteral("SELECT COUNT(*) FROM classes")),
             std::optional<int>{0});
    QVERIFY(fixture.page->statusLabel()->text().contains(
        QStringLiteral("Schedule import completed")));
    QVERIFY(fixture.page->statusLabel()->text().contains(
        QStringLiteral("1 schedules cleared")));
    QVERIFY(std::all_of(fixture.applyRequests.cbegin(), fixture.applyRequests.cend(),
        [](const auto& request) {
            return !request.saveProfileNameIfBlank && !request.updateProfileName;
        }));
    QCOMPARE(scalarInt(firstPath, QStringLiteral(
        "SELECT COUNT(*) FROM app_settings WHERE key='myInfo/name'")),
        std::optional<int>{0});
    QCOMPARE(scalarInt(secondPath, QStringLiteral(
        "SELECT COUNT(*) FROM teachers")),
        scalarInt(firstPath, QStringLiteral("SELECT COUNT(*) FROM teachers")));
    QCOMPARE(SettingsManager::instance().getRecentFiles(),
        QStringList{QStringLiteral("schedule-import-history-sentinel.tps")});
    QCOMPARE(SettingsManager::instance().getLastFile(),
        QStringLiteral("schedule-import-last-sentinel.tps"));
    QVERIFY(!QFileInfo::exists(m_setupRecoveryJournalPath));
}

void NormalScheduleImportIntegrationTests::
nonDefaultSheetAndBlockApplyOnlyAfterDiagnosticsAcknowledgement()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString workbookPath = nondefaultSelectionFixturePath();
    QFile workbookFile(workbookPath);
    QVERIFY(workbookFile.open(QIODevice::ReadOnly));
    auto parsed = parseScheduleImportWorkbook(
        workbookFile, ScheduleImportKind::Normal);
    QVERIFY(parsed.has_value());
    QCOMPARE(parsed->sheets.size(), 2);
    QCOMPARE(parsed->sheets.at(1).users.size(), 2);
    const ScheduleImportUserBlock selectedUser =
        parsed->sheets.at(1).users.at(1);
    QCOMPARE(selectedUser.name, QStringLiteral("Selected Block"));
    QCOMPARE(selectedUser.classes.size(), 1);
    QCOMPARE(selectedUser.classes.front().classGrade, QStringLiteral("M2"));
    QCOMPARE(selectedUser.classes.front().classLevel, QStringLiteral("Ursa"));
    QVERIFY(!selectedUser.diagnostics.isEmpty());

    const QString workspace = temporary.filePath(QStringLiteral("selected.tps"));
    QString error;
    QVERIFY2(createWorkspace(workspace, selectedUser, &error), qPrintable(error));

    ScheduleImportPageFixture fixture;
    QVERIFY(fixture.initialize(workspace));
    fixture.workbookPath = workbookPath;
    auto* const dialog = fixture.openImport();
    QVERIFY(dialog);
    QCOMPARE(dialog->size(), QSize(436, 520));
    QVERIFY(fixture.selectWorkbookAndRead(dialog));
    QTRY_VERIFY_WITH_TIMEOUT(dialog->sheetCombo()->count() == 2, 10000);
    QCOMPARE(dialog->size(), QSize(436, 520));
    QCOMPARE(dialog->sheetCombo()->itemText(1), QStringLiteral("Alternate"));
    dialog->sheetCombo()->setCurrentIndex(1);
    QTRY_VERIFY_WITH_TIMEOUT(dialog->userCombo()->count() == 2, 5000);
    QCOMPARE(dialog->userCombo()->count(), 2);
    QCOMPARE(dialog->userCombo()->itemText(1), QStringLiteral("Selected Block"));
    QVERIFY(fixture.continueToReview(dialog, 1, 1));
    QTRY_VERIFY_WITH_TIMEOUT(dialog->classTable()->model()->rowCount() == 1,
                             10000);
    QCOMPARE(dialog->size(), QSize(800, 800));

    // Selecting the second worksheet and second block must map the M2 target
    // candidate, not the first sheet or first block's E4 candidate.
    QVERIFY(dialog->classTable()->model()->data(
        dialog->classTable()->model()->index(
            0, Ui::ScheduleImportClassReviewModel::ClassColumn),
        Qt::DisplayRole).toString().contains(QStringLiteral("M2 Ursa")));

    auto* const diagnostics = dialog->findChild<QCheckBox*>(
        QStringLiteral("nextScheduleImportDiagnosticsAcknowledged"));
    QVERIFY(diagnostics);
    QVERIFY(diagnostics->isVisible());
    QVERIFY(!diagnostics->isChecked());
    setAllTeachersToCreate(dialog);
    setAllClassesToCreateNew(dialog, false);
    QVERIFY(!dialog->primaryButton()->isEnabled());
    dialog->primaryButton()->click();
    QCOMPARE(fixture.applyRequests.size(), std::size_t(0));
    QVERIFY(dialog->findChild<QLabel*>(
        QStringLiteral("nextScheduleImportReviewStatus"))->text().contains(
            QStringLiteral("diagnostics"), Qt::CaseInsensitive));

    diagnostics->setChecked(true);
    QVERIFY(dialog->primaryButton()->isEnabled());
    dialog->primaryButton()->click();
    QTRY_VERIFY_WITH_TIMEOUT(dialog->isHidden(), 10000);
    QCOMPARE(fixture.applyRequests.size(), std::size_t(1));
    const auto& request = fixture.applyRequests.front();
    QCOMPARE(QString::fromStdU16String(request.selectedUserName),
             QStringLiteral("Selected Block"));
    QVERIFY(request.diagnosticsAcknowledged);
    QCOMPARE(request.candidates.size(), std::size_t(1));
    QCOMPARE(request.classes.size(), std::size_t(1));
    QCOMPARE(request.diagnostics.size(), std::size_t(1));
    QVERIFY(!request.saveProfileNameIfBlank);
    QVERIFY(!request.updateProfileName);
    QCOMPARE(scalarInt(workspace, QStringLiteral("SELECT COUNT(*) FROM classes")),
             std::optional<int>{1});
    QCOMPARE(scalarInt(workspace, QStringLiteral(
        "SELECT COUNT(*) FROM class_info WHERE class_grade='M2' "
        "AND class_level='Ursa'")), std::optional<int>{1});
    QCOMPARE(scalarInt(workspace, QStringLiteral(
        "SELECT COUNT(*) FROM class_info WHERE class_grade='E4' "
        "AND class_level='Theseus'")), std::optional<int>{0});
    QCOMPARE(scalarInt(workspace, QStringLiteral(
        "SELECT COUNT(*) FROM class_times")), std::optional<int>{1});
}

void NormalScheduleImportIntegrationTests::
intensiveImportPreservesOrClearsAbsentSchedulesByTypedMode()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto importedUser = firstVisibleUser(
        intensiveFixturePath(), ScheduleImportKind::Intensive);
    QVERIFY(importedUser.has_value());
    QVERIFY(!importedUser->classes.isEmpty());

    const QString workspace = temporary.filePath(QStringLiteral("intensive.tps"));
    QString error;
    QVERIFY2(createWorkspace(workspace, *importedUser, &error), qPrintable(error));
    QVERIFY2(seedAbsentIntensiveClass(workspace, &error), qPrintable(error));

    ScheduleImportPageFixture fixture;
    QVERIFY(fixture.initialize(workspace));
    fixture.workbookPath = intensiveFixturePath();

    const auto runImport = [&](const bool replace) {
        auto* const dialog = fixture.openImport();
        if (!dialog)
        {
            return static_cast<Ui::ScheduleImportWorkflowDialog*>(nullptr);
        }
        auto* const kind = dialog->findChild<QComboBox*>(
            QStringLiteral("nextScheduleImportKind"));
        if (!kind || kind->count() < 2)
        {
            return static_cast<Ui::ScheduleImportWorkflowDialog*>(nullptr);
        }
        kind->setCurrentIndex(1);
        if (!fixture.selectWorkbookAndRead(dialog)
            || !QTest::qWaitFor([dialog] {
                return dialog->sheetCombo()->count() > 0;
            }, 10000)
            || !fixture.continueToReview(dialog)
            || !QTest::qWaitFor([dialog] {
                return dialog->classTable()->model()->rowCount() > 0;
            }, 10000))
        {
            return static_cast<Ui::ScheduleImportWorkflowDialog*>(nullptr);
        }
        QString teacherId;
        if (!setTeacherReviewToUpdateRoom(dialog, &teacherId))
        {
            return static_cast<Ui::ScheduleImportWorkflowDialog*>(nullptr);
        }
        setAllClassesToCreateNew(dialog);
        auto* const replaceButton = dialog->findChild<QRadioButton*>(
            QStringLiteral("nextScheduleImportReplaceIntensive"));
        auto* const updateButton = dialog->findChild<QRadioButton*>(
            QStringLiteral("nextScheduleImportUpdateIntensive"));
        if (!replaceButton || !updateButton)
        {
            return static_cast<Ui::ScheduleImportWorkflowDialog*>(nullptr);
        }
        (replace ? replaceButton : updateButton)->setChecked(true);
        dialog->primaryButton()->click();
        return dialog;
    };

    auto* const updateDialog = runImport(false);
    QVERIFY(updateDialog);
    QTRY_VERIFY_WITH_TIMEOUT(updateDialog->isHidden(), 10000);
    QCOMPARE(fixture.applyRequests.size(), std::size_t(1));
    QVERIFY(fixture.applyRequests.front().intensiveSchedule);
    QCOMPARE(fixture.applyRequests.front().intensiveMode,
        Application::ScheduleImportPlanIntensiveMode::UpdateExisting);
    QVERIFY(!fixture.applyRequests.front().saveProfileNameIfBlank);
    QVERIFY(!fixture.applyRequests.front().updateProfileName);
    QCOMPARE(scalarInt(workspace, QStringLiteral(
        "SELECT COUNT(*) FROM class_intensive_times WHERE class_id=999")),
        std::optional<int>{1});
    QCOMPARE(scalarInt(workspace, QStringLiteral(
        "SELECT COUNT(*) FROM class_intensive_times")),
        std::optional<int>{static_cast<int>(
            importedUser->classes.front().times.size()) + 1});

    auto* const replaceDialog = runImport(true);
    QVERIFY(replaceDialog);
    QTRY_VERIFY_WITH_TIMEOUT(replaceDialog->isHidden(), 10000);
    QCOMPARE(fixture.applyRequests.size(), std::size_t(2));
    QCOMPARE(fixture.applyRequests.back().intensiveMode,
        Application::ScheduleImportPlanIntensiveMode::ReplaceWithNew);
    QVERIFY(!fixture.applyRequests.back().saveProfileNameIfBlank);
    QVERIFY(!fixture.applyRequests.back().updateProfileName);
    QCOMPARE(scalarInt(workspace, QStringLiteral(
        "SELECT COUNT(*) FROM class_intensive_times WHERE class_id=999")),
        std::optional<int>{0});
    QCOMPARE(scalarInt(workspace, QStringLiteral(
        "SELECT COUNT(*) FROM class_intensive_times")),
        std::optional<int>{static_cast<int>(
            importedUser->classes.front().times.size())});
    QVERIFY(fixture.page->statusLabel()->text().contains(
        QStringLiteral("schedules cleared"), Qt::CaseInsensitive));
}

void NormalScheduleImportIntegrationTests::
overlappingWorkbookPresentsReviewConflict()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString workbookPath = overlapConflictFixturePath();
    const auto importedUser = firstVisibleUser(workbookPath);
    QVERIFY(importedUser.has_value());
    const QString workspace = temporary.filePath(QStringLiteral("overlap.tps"));
    QString error;
    QVERIFY2(createWorkspace(workspace, *importedUser, &error), qPrintable(error));

    ScheduleImportPageFixture fixture;
    QVERIFY(fixture.initialize(workspace));
    fixture.workbookPath = workbookPath;
    auto* const dialog = fixture.openImport();
    QVERIFY(dialog);
    QVERIFY(fixture.selectWorkbookAndRead(dialog));
    QTRY_VERIFY_WITH_TIMEOUT(dialog->sheetCombo()->count() > 0, 10000);
    QVERIFY(fixture.continueToReview(dialog));
    QCOMPARE(dialog->classTable()->model()->rowCount(), importedUser->classes.size());
    setAllTeachersToCreate(dialog);
    setAllClassesToCreateNew(dialog);
    const QString conflictWord = QStringLiteral("overlap");
    QTRY_VERIFY_WITH_TIMEOUT(dialog->findChild<QLabel*>(
        QStringLiteral("nextScheduleImportReviewStatus"))->text().contains(
            conflictWord, Qt::CaseInsensitive), 5000);
    QVERIFY(!dialog->primaryButton()->isEnabled());
    QVERIFY(fixture.applyRequests.empty());
}

void NormalScheduleImportIntegrationTests::
largeWorkbookReviewUsesBoundedWidgetsAndReleasesDataOnCancel()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString workbookPath = largeConflictFixturePath();
    const auto importedUser = firstVisibleUser(workbookPath);
    QVERIFY(importedUser.has_value());
    QCOMPARE(importedUser->classes.size(), 20);
    QFile workbookFile(workbookPath);
    QVERIFY(workbookFile.open(QIODevice::ReadOnly));
    const auto parsedWorkbook = parseScheduleImportWorkbook(
        workbookFile, ScheduleImportKind::Normal);
    QVERIFY(parsedWorkbook.has_value());
    int parserCandidateCount = 0;
    int scheduleBlockCount = 0;
    for (const ScheduleImportSheet& sheet : parsedWorkbook->sheets)
    {
        if (!sheet.visible)
        {
            continue;
        }
        scheduleBlockCount += sheet.users.size();
        for (const ScheduleImportUserBlock& user : sheet.users)
        {
            parserCandidateCount += user.classes.size();
        }
    }
    QCOMPARE(parserCandidateCount, 96);
    QCOMPARE(scheduleBlockCount, 5);
    const QString workspace = temporary.filePath(QStringLiteral("large.tps"));
    QString error;
    QVERIFY2(createWorkspace(workspace, *importedUser, &error), qPrintable(error));

    ScheduleImportPageFixture fixture;
    QVERIFY(fixture.initialize(workspace));
    fixture.workbookPath = workbookPath;
    auto* const dialog = fixture.openImport();
    QVERIFY(dialog);
    auto* const browse = dialog->findChild<QPushButton*>(
        QStringLiteral("nextScheduleImportBrowse"));
    QVERIFY(browse);
    browse->click();
    QCOMPARE(dialog->sourcePathEdit()->text(), workbookPath);
    dialog->primaryButton()->click();
    QTRY_VERIFY_WITH_TIMEOUT(dialog->sheetCombo()->count() > 0, 15000);
    QVERIFY2(dialog->statusLabel()->text().contains(QStringLiteral("96")),
        qPrintable(dialog->statusLabel()->text()));
    QVERIFY(fixture.continueToReview(dialog));
    QTRY_VERIFY_WITH_TIMEOUT(dialog->classTable()->model()->rowCount()
                             == importedUser->classes.size(), 10000);

    auto* const preview = dialog->findChild<QTableView*>(
        QStringLiteral("nextScheduleImportPreviewTable"));
    QVERIFY(preview);
    QCOMPARE(preview->model()->rowCount(), importedUser->classes.size());
    const int widgetCount = dialog->reviewWidgetCount();
    QVERIFY2(widgetCount < 200, qPrintable(QStringLiteral(
        "Review created %1 widgets for a model-backed candidate list.")
        .arg(widgetCount)));
    int visibleRows = 0;
    for (int row = 0; row < preview->model()->rowCount(); ++row)
    {
        if (preview->viewport()->rect().intersects(
                preview->visualRect(preview->model()->index(row, 0))))
        {
            ++visibleRows;
        }
    }
    QVERIFY(visibleRows <= 6);
    QVERIFY(visibleRows > 0);
    setAllTeachersToCreate(dialog);
    setAllClassesToCreateNew(dialog);
    QTRY_VERIFY_WITH_TIMEOUT(dialog->findChild<QLabel*>(
        QStringLiteral("nextScheduleImportReviewStatus"))->text().contains(
            QStringLiteral("overlap"), Qt::CaseInsensitive), 5000);
    QVERIFY(!dialog->primaryButton()->isEnabled());

    dialog->reject();
    QCOMPARE(dialog->classTable()->model()->rowCount(), 0);
    QCOMPARE(dialog->teacherTable()->model()->rowCount(), 0);
    QCOMPARE(preview->model()->rowCount(), 0);
    QCOMPARE(dialog->sheetCombo()->count(), 0);
    QCOMPARE(dialog->userCombo()->count(), 0);
    QVERIFY(dialog->sourcePathEdit()->text().isEmpty());
    QCOMPARE(fixture.applyRequests.size(), std::size_t(0));
    QCOMPARE(scalarInt(workspace, QStringLiteral("SELECT COUNT(*) FROM classes")),
             std::optional<int>{0});
}

void NormalScheduleImportIntegrationTests::
largeNonOverlappingBlockAppliesTwentyCandidates()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const QString workbookPath = largeApplicableFixturePath();
    const auto importedUser = firstVisibleUser(workbookPath);
    QVERIFY(importedUser.has_value());
    QCOMPARE(importedUser->name, QStringLiteral("Large Import"));
    QCOMPARE(importedUser->classes.size(), 20);
    QVERIFY(importedUser->diagnostics.isEmpty());

    const QString workspace = temporary.filePath(QStringLiteral("large.tps"));
    QString error;
    QVERIFY2(createWorkspace(workspace, *importedUser, &error), qPrintable(error));
    ScheduleImportPageFixture fixture;
    QVERIFY(fixture.initialize(workspace));
    fixture.workbookPath = workbookPath;
    auto* const dialog = fixture.openImport();
    QVERIFY(dialog);
    QCOMPARE(dialog->size(), QSize(436, 520));
    QVERIFY(fixture.selectWorkbookAndRead(dialog));
    QTRY_VERIFY_WITH_TIMEOUT(dialog->sheetCombo()->count() == 1, 10000);
    QVERIFY(dialog->statusLabel()->text().contains(QStringLiteral("20")));
    QVERIFY(fixture.continueToReview(dialog));
    QTRY_VERIFY_WITH_TIMEOUT(dialog->classTable()->model()->rowCount() == 20,
                             10000);
    QCOMPARE(dialog->size(), QSize(800, 800));
    QVERIFY(dialog->reviewWidgetCount() < 200);
    auto* const preview = dialog->findChild<QTableView*>(
        QStringLiteral("nextScheduleImportPreviewTable"));
    QVERIFY(preview);
    QCOMPARE(preview->model()->rowCount(), 20);
    int visiblePreviewRows = 0;
    for (int row = 0; row < preview->model()->rowCount(); ++row)
    {
        if (preview->viewport()->rect().intersects(
                preview->visualRect(preview->model()->index(row, 0))))
        {
            ++visiblePreviewRows;
        }
    }
    QVERIFY(visiblePreviewRows > 0);
    QVERIFY(visiblePreviewRows <= 6);

    setAllTeachersToCreate(dialog);
    setAllClassesToCreateNew(dialog);
    QVERIFY(dialog->primaryButton()->isEnabled());
    dialog->primaryButton()->click();
    QTRY_VERIFY_WITH_TIMEOUT(dialog->isHidden(), 15000);
    QCOMPARE(fixture.applyRequests.size(), std::size_t(1));
    const auto& request = fixture.applyRequests.front();
    QCOMPARE(QString::fromStdU16String(request.selectedUserName),
             QStringLiteral("Large Import"));
    QCOMPARE(request.candidates.size(), std::size_t(20));
    QCOMPARE(request.classes.size(), std::size_t(20));
    QCOMPARE(request.teachers.size(), std::size_t(5));
    QVERIFY(!request.saveProfileNameIfBlank);
    QVERIFY(!request.updateProfileName);
    QCOMPARE(scalarInt(workspace, QStringLiteral("SELECT COUNT(*) FROM classes")),
             std::optional<int>{20});
    QCOMPARE(scalarInt(workspace, QStringLiteral(
        "SELECT COUNT(*) FROM class_info WHERE class_grade='E4' "
        "AND class_level='Theseus'")), std::optional<int>{5});
    QCOMPARE(scalarInt(workspace, QStringLiteral(
        "SELECT COUNT(*) FROM class_times")), std::optional<int>{30});
    QTRY_COMPARE_WITH_TIMEOUT(
        fixture.page->model()->runtimeMetrics().sourceClassCount, 20, 5000);
    QCOMPARE(dialog->classTable()->model()->rowCount(), 0);
    QCOMPARE(dialog->sourcePathEdit()->text(), QString());
}

void NormalScheduleImportIntegrationTests::
changedWorkbookIsDiscardedAndCancellationReleasesWorkflow()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto importedUser = firstVisibleUser(scheduleFixturePath());
    QVERIFY(importedUser.has_value());
    const QString workspace = temporary.filePath(QStringLiteral("workspace.tps"));
    QString error;
    QVERIFY2(createWorkspace(workspace, *importedUser, &error), qPrintable(error));

    const QString changedCopy = temporary.filePath(QStringLiteral("large.xlsx"));
    QVERIFY(QFile::copy(largeMutationFixturePath(), changedCopy));
    ScheduleImportPageFixture fixture;
    QVERIFY(fixture.initialize(workspace));
    fixture.workbookPath = changedCopy;
    auto* dialog = fixture.openImport();
    QVERIFY(dialog);
    QVERIFY(fixture.selectWorkbookAndRead(dialog));
    QTimer::singleShot(0, dialog, [changedCopy] {
        QFile workbook(changedCopy);
        if (workbook.open(QIODevice::WriteOnly | QIODevice::Append))
        {
            static_cast<void>(workbook.write("changed-after-selection"));
        }
    });
    QTRY_VERIFY_WITH_TIMEOUT(
        dialog->statusLabel()->text().contains(QStringLiteral("changed"),
                                                Qt::CaseInsensitive)
        || dialog->statusLabel()->text().contains(QStringLiteral("could not be read"),
                                                   Qt::CaseInsensitive),
        15000);
    QCOMPARE(dialog->sheetCombo()->count(), 0);
    QCOMPARE(fixture.applyRequests.size(), std::size_t(0));

    QPointer<Ui::ScheduleImportWorkflowDialog> guarded(dialog);
    fixture.page->onSuspending();
    QTRY_VERIFY_WITH_TIMEOUT(guarded.isNull(), 5000);
    QCOMPARE(scalarInt(workspace, QStringLiteral("SELECT COUNT(*) FROM classes")),
             std::optional<int>{0});
}

void NormalScheduleImportIntegrationTests::
timeoutAndCanceledLateParseResultsAreDiscarded()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto importedUser = firstVisibleUser(scheduleFixturePath());
    QVERIFY(importedUser.has_value());
    const QString workspace = temporary.filePath(QStringLiteral("workflow.tps"));
    QString error;
    QVERIFY2(createWorkspace(workspace, *importedUser, &error), qPrintable(error));

    ScheduleImportPageFixture timeoutFixture;
    timeoutFixture.timeoutMilliseconds = 25;
    auto timeoutStarted = std::make_shared<std::atomic_bool>(false);
    auto timeoutParsed = std::make_shared<std::atomic_bool>(false);
    auto timeoutReturned = std::make_shared<std::atomic_bool>(false);
    auto timeoutCalls = std::make_shared<std::atomic_int>(0);
    timeoutFixture.parseWorkbook = [=](QIODevice& device,
                                       const ScheduleImportKind kind) {
        const int call = timeoutCalls->fetch_add(1, std::memory_order_relaxed);
        auto result = parseScheduleImportWorkbook(device, kind);
        if (call == 0)
        {
            timeoutParsed->store(result.has_value(), std::memory_order_relaxed);
            timeoutStarted->store(true, std::memory_order_release);
            QThread::msleep(180);
            timeoutReturned->store(true, std::memory_order_release);
        }
        return result;
    };
    QVERIFY(timeoutFixture.initialize(workspace));
    auto* const timeoutDialog = timeoutFixture.openImport();
    QVERIFY(timeoutDialog);
    QVERIFY(timeoutFixture.selectWorkbookAndRead(timeoutDialog));
    QTRY_VERIFY_WITH_TIMEOUT(timeoutStarted->load(std::memory_order_acquire), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(timeoutDialog->statusLabel()->text().contains(
        QStringLiteral("timed out"), Qt::CaseInsensitive), 5000);
    QVERIFY(timeoutDialog->primaryButton()->isEnabled());
    QCOMPARE(timeoutDialog->sheetCombo()->count(), 0);
    QTRY_VERIFY_WITH_TIMEOUT(timeoutReturned->load(std::memory_order_acquire), 5000);
    QVERIFY(timeoutParsed->load(std::memory_order_relaxed));
    QTest::qWait(50);
    QTRY_COMPARE_WITH_TIMEOUT(timeoutDialog->sheetCombo()->count(), 0, 5000);
    QVERIFY(timeoutDialog->statusLabel()->text().contains(
        QStringLiteral("timed out"), Qt::CaseInsensitive));

    timeoutDialog->primaryButton()->click();
    QTRY_VERIFY_WITH_TIMEOUT(timeoutDialog->sheetCombo()->count() > 0, 10000);
    QCOMPARE(timeoutCalls->load(std::memory_order_relaxed), 2);
    timeoutDialog->reject();
    QTRY_VERIFY_WITH_TIMEOUT(timeoutFixture.page->importDialog() == nullptr, 5000);

    ScheduleImportPageFixture cancelFixture;
    cancelFixture.timeoutMilliseconds = 5000;
    auto cancelStarted = std::make_shared<std::atomic_bool>(false);
    auto cancelParsed = std::make_shared<std::atomic_bool>(false);
    auto cancelReturned = std::make_shared<std::atomic_bool>(false);
    cancelFixture.parseWorkbook = [=](QIODevice& device,
                                      const ScheduleImportKind kind) {
        auto result = parseScheduleImportWorkbook(device, kind);
        cancelParsed->store(result.has_value(), std::memory_order_relaxed);
        cancelStarted->store(true, std::memory_order_release);
        QThread::msleep(180);
        cancelReturned->store(true, std::memory_order_release);
        return result;
    };
    QVERIFY(cancelFixture.initialize(workspace));
    auto* cancelDialog = cancelFixture.openImport();
    QVERIFY(cancelDialog);
    QVERIFY(cancelFixture.selectWorkbookAndRead(cancelDialog));
    QTRY_VERIFY_WITH_TIMEOUT(cancelStarted->load(std::memory_order_acquire), 5000);
    QPointer<Ui::ScheduleImportWorkflowDialog> guarded(cancelDialog);
    auto* const cancel = cancelDialog->findChild<QPushButton*>(
        QStringLiteral("nextScheduleImportCancel"));
    QVERIFY(cancel);
    cancel->click();
    QTRY_VERIFY_WITH_TIMEOUT(guarded.isNull(), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(cancelReturned->load(std::memory_order_acquire), 5000);
    QVERIFY(cancelParsed->load(std::memory_order_relaxed));
    QTest::qWait(50);
    QCoreApplication::processEvents();
    QCOMPARE(cancelFixture.page->importDialog(), nullptr);
    QCOMPARE(cancelFixture.applyRequests.size(), std::size_t(0));
    QCOMPARE(scalarInt(workspace, QStringLiteral("SELECT COUNT(*) FROM classes")),
             std::optional<int>{0});
}

void NormalScheduleImportIntegrationTests::
suspendingDuringAsyncParseCancelsAndIgnoresLateResult()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto importedUser = firstVisibleUser(scheduleFixturePath());
    QVERIFY(importedUser.has_value());
    const QString workspace = temporary.filePath(QStringLiteral("suspend.tps"));
    QString error;
    QVERIFY2(createWorkspace(workspace, *importedUser, &error), qPrintable(error));

    ScheduleImportPageFixture fixture;
    fixture.timeoutMilliseconds = 5000;
    auto parseStarted = std::make_shared<std::atomic_bool>(false);
    auto parseSucceeded = std::make_shared<std::atomic_bool>(false);
    auto parseReturned = std::make_shared<std::atomic_bool>(false);
    fixture.parseWorkbook = [=](QIODevice& device,
                                const ScheduleImportKind kind) {
        auto result = parseScheduleImportWorkbook(device, kind);
        parseSucceeded->store(result.has_value(), std::memory_order_relaxed);
        parseStarted->store(true, std::memory_order_release);
        QThread::msleep(220);
        parseReturned->store(true, std::memory_order_release);
        return result;
    };
    QVERIFY(fixture.initialize(workspace));
    auto* const dialog = fixture.openImport();
    QVERIFY(dialog);
    QVERIFY(fixture.selectWorkbookAndRead(dialog));
    QTRY_VERIFY_WITH_TIMEOUT(parseStarted->load(std::memory_order_acquire), 5000);

    bool finished = false;
    bool retainedDataCleared = false;
    connect(dialog, &QDialog::finished, this, [dialog, &finished,
                                               &retainedDataCleared] {
        auto* const preview = dialog->findChild<QTableView*>(
            QStringLiteral("nextScheduleImportPreviewTable"));
        retainedDataCleared = dialog->sourcePathEdit()->text().isEmpty()
            && dialog->sheetCombo()->count() == 0
            && dialog->userCombo()->count() == 0
            && dialog->teacherTable()->model()->rowCount() == 0
            && dialog->classTable()->model()->rowCount() == 0
            && preview && preview->model()->rowCount() == 0;
        finished = true;
    });
    QPointer<Ui::ScheduleImportWorkflowDialog> guarded(dialog);
    fixture.page->onSuspending();
    QVERIFY(finished);
    QVERIFY(retainedDataCleared);
    QCOMPARE(fixture.page->importDialog(), nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(guarded.isNull(), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(parseReturned->load(std::memory_order_acquire), 5000);
    QVERIFY(parseSucceeded->load(std::memory_order_relaxed));
    QTest::qWait(60);
    QCoreApplication::processEvents();
    QCOMPARE(fixture.page->importDialog(), nullptr);
    QCOMPARE(fixture.applyRequests.size(), std::size_t(0));
    QCOMPARE(scalarInt(workspace, QStringLiteral("SELECT COUNT(*) FROM classes")),
             std::optional<int>{0});
}

void NormalScheduleImportIntegrationTests::
replacingScheduleWithReviewOpenReleasesRetainedData()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto importedUser = firstVisibleUser(scheduleFixturePath());
    QVERIFY(importedUser.has_value());
    const QString first = temporary.filePath(QStringLiteral("first.tps"));
    const QString second = temporary.filePath(QStringLiteral("second.tps"));
    QString error;
    QVERIFY2(createWorkspace(first, *importedUser, &error), qPrintable(error));
    QVERIFY2(createWorkspace(second, *importedUser, &error), qPrintable(error));

    ScheduleImportPageFixture fixture;
    QVERIFY(fixture.initialize(first));
    auto* const dialog = fixture.openImport();
    QVERIFY(dialog);
    QVERIFY(fixture.selectWorkbookAndRead(dialog));
    QTRY_VERIFY_WITH_TIMEOUT(dialog->sheetCombo()->count() > 0, 10000);
    QVERIFY(fixture.continueToReview(dialog));
    QTRY_VERIFY_WITH_TIMEOUT(dialog->classTable()->model()->rowCount() > 0,
                             10000);
    auto* const preview = dialog->findChild<QTableView*>(
        QStringLiteral("nextScheduleImportPreviewTable"));
    QVERIFY(preview);
    QVERIFY(!dialog->sourcePathEdit()->text().isEmpty());
    QVERIFY(dialog->teacherTable()->model()->rowCount() > 0);
    QVERIFY(preview->model()->rowCount() > 0);

    bool finished = false;
    bool retainedDataCleared = false;
    connect(dialog, &QDialog::finished, this, [dialog, &finished,
                                               &retainedDataCleared] {
        auto* const finishedPreview = dialog->findChild<QTableView*>(
            QStringLiteral("nextScheduleImportPreviewTable"));
        retainedDataCleared = dialog->sourcePathEdit()->text().isEmpty()
            && dialog->sheetCombo()->count() == 0
            && dialog->userCombo()->count() == 0
            && dialog->teacherTable()->model()->rowCount() == 0
            && dialog->classTable()->model()->rowCount() == 0
            && finishedPreview && finishedPreview->model()->rowCount() == 0;
        finished = true;
    });
    QPointer<Ui::ScheduleImportWorkflowDialog> guarded(dialog);
    fixture.activePath = second;
    fixture.identity = QStringLiteral("normal-workspace|") + second;
    QVERIFY(fixture.page->onActivating());
    QVERIFY(finished);
    QVERIFY(retainedDataCleared);
    QCOMPARE(fixture.page->importDialog(), nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(guarded.isNull(), 5000);
    QCOMPARE(fixture.applyRequests.size(), std::size_t(0));
    QCOMPARE(scalarInt(first, QStringLiteral("SELECT COUNT(*) FROM classes")),
             std::optional<int>{0});
    QCOMPARE(scalarInt(second, QStringLiteral("SELECT COUNT(*) FROM classes")),
             std::optional<int>{0});
}

void NormalScheduleImportIntegrationTests::staleWorkspaceIdentityRejectsApply()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto importedUser = firstVisibleUser(scheduleFixturePath());
    QVERIFY(importedUser.has_value());
    const QString first = temporary.filePath(QStringLiteral("first.tps"));
    const QString second = temporary.filePath(QStringLiteral("second.tps"));
    QString error;
    QVERIFY2(createWorkspace(first, *importedUser, &error), qPrintable(error));
    QVERIFY2(createWorkspace(second, *importedUser, &error), qPrintable(error));

    ScheduleImportPageFixture fixture;
    QVERIFY(fixture.initialize(first));
    auto* dialog = fixture.openImport();
    QVERIFY(dialog);
    QVERIFY(fixture.selectWorkbookAndRead(dialog));
    QTRY_VERIFY_WITH_TIMEOUT(dialog->sheetCombo()->count() > 0, 10000);
    QVERIFY(fixture.continueToReview(dialog));
    QTRY_VERIFY_WITH_TIMEOUT(dialog->classTable()->model()->rowCount() > 0, 10000);
    setAllClassesToCreateNew(dialog);

    fixture.activePath = second;
    fixture.identity = QStringLiteral("replacement-workspace|") + second;
    dialog->primaryButton()->click();
    QVERIFY(dialog->findChild<QLabel*>(
        QStringLiteral("nextScheduleImportReviewStatus"))->text().contains(
            QStringLiteral("profile changed"), Qt::CaseInsensitive));
    QVERIFY(fixture.applyRequests.empty());
    QCOMPARE(scalarInt(first, QStringLiteral("SELECT COUNT(*) FROM classes")),
             std::optional<int>{0});
    QCOMPARE(scalarInt(second, QStringLiteral("SELECT COUNT(*) FROM classes")),
             std::optional<int>{0});
    QPointer<Ui::ScheduleImportWorkflowDialog> guarded(dialog);
    const auto activated = fixture.page->onActivating();
    QVERIFY(activated.has_value());
    QTRY_VERIFY_WITH_TIMEOUT(guarded.isNull(), 5000);
}

void NormalScheduleImportIntegrationTests::
captureImportVisualEvidenceWhenConfigured()
{
    const QString outputPath = qEnvironmentVariable(
        "CLASSMNGR_NEXT_NORMAL_IMPORT_CAPTURE_DIR").trimmed();
    if (outputPath.isEmpty())
    {
        return;
    }
    QVERIFY(QDir().mkpath(outputPath));
    auto* const application = qApp;
    QVERIFY(application);
    application->setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    PlatformProcessMemorySnapshotProvider memoryProvider;

    QJsonArray captureRows;
    for (const QString& locale : {QStringLiteral("en"), QStringLiteral("ko")})
    {
        QTranslator translator;
        const QString catalog = locale == QStringLiteral("ko")
            ? QStringLiteral("ClassMngr_ko_KR.qm")
            : QStringLiteral("ClassMngr_en_US.qm");
        QVERIFY2(translator.load(QDir(QStringLiteral(
            CLASSMNGR_NEXT_TEST_TRANSLATION_DIRECTORY)).filePath(catalog)),
            qPrintable(catalog));
        QLocale::setDefault(locale == QStringLiteral("ko")
            ? QLocale(QLocale::Korean, QLocale::SouthKorea)
            : QLocale(QLocale::English, QLocale::UnitedStates));

        for (const QString& theme : {QStringLiteral("light"), QStringLiteral("dark")})
        {
            QVERIFY(application->installTranslator(&translator));
            QPalette palette;
            if (theme == QStringLiteral("dark"))
            {
                palette.setColor(QPalette::Window, QColor(37, 39, 42));
                palette.setColor(QPalette::WindowText, QColor(230, 232, 235));
                palette.setColor(QPalette::Base, QColor(25, 27, 29));
                palette.setColor(QPalette::AlternateBase, QColor(43, 46, 49));
                palette.setColor(QPalette::Text, QColor(230, 232, 235));
                palette.setColor(QPalette::Button, QColor(47, 50, 53));
                palette.setColor(QPalette::ButtonText, QColor(230, 232, 235));
                palette.setColor(QPalette::Highlight, QColor(58, 112, 190));
                palette.setColor(QPalette::HighlightedText, Qt::white);
                palette.setColor(QPalette::ToolTipBase, QColor(47, 50, 53));
                palette.setColor(QPalette::ToolTipText, QColor(230, 232, 235));
            }
            else
            {
                palette.setColor(QPalette::Window, QColor(248, 249, 251));
                palette.setColor(QPalette::WindowText, QColor(33, 37, 41));
                palette.setColor(QPalette::Base, Qt::white);
                palette.setColor(QPalette::AlternateBase, QColor(242, 244, 247));
                palette.setColor(QPalette::Text, QColor(33, 37, 41));
                palette.setColor(QPalette::Button, QColor(244, 245, 247));
                palette.setColor(QPalette::ButtonText, QColor(33, 37, 41));
                palette.setColor(QPalette::Highlight, QColor(45, 110, 190));
                palette.setColor(QPalette::HighlightedText, Qt::white);
            }
            application->setPalette(palette);

            const QString prefix = QStringLiteral("%1-%2")
                .arg(locale, theme);
            const auto saveDialog = [&](
                Ui::ScheduleImportWorkflowDialog* dialog,
                const QString& scene,
                QJsonObject metrics) -> QJsonObject {
                QApplication::processEvents();
                if (!dialog || !dialog->isVisible())
                {
                    return {};
                }
                const QImage image = dialog->grab().toImage();
                const QString fileName = prefix + QLatin1Char('-') + scene
                    + QStringLiteral(".png");
                const QString filePath = QDir(outputPath).filePath(fileName);
                if (image.isNull() || !image.save(filePath, "PNG"))
                {
                    return {};
                }
                metrics.insert(QStringLiteral("image"), fileName);
                metrics.insert(QStringLiteral("width"), image.width());
                metrics.insert(QStringLiteral("height"), image.height());
                metrics.insert(QStringLiteral("dialogWidth"), dialog->width());
                metrics.insert(QStringLiteral("dialogHeight"), dialog->height());
                if (auto* const pages = dialog->findChild<QStackedWidget*>(
                        QStringLiteral("nextScheduleImportPages")))
                {
                    metrics.insert(QStringLiteral("visibleStageIndex"),
                                   pages->currentIndex());
                    metrics.insert(QStringLiteral("visibleStage"),
                                   pages->currentWidget()->objectName());
                }
                metrics.insert(QStringLiteral("reviewWidgetCount"),
                               dialog->reviewWidgetCount());
                metrics.insert(QStringLiteral("sourcePathBounds"),
                    rectangleJson(dialog->sourcePathEdit()->geometry()));
                metrics.insert(QStringLiteral("primaryButtonBounds"),
                    rectangleJson(dialog->primaryButton()->geometry()));
                if (dialog->sheetCombo()->isVisible())
                {
                    metrics.insert(QStringLiteral("worksheetChoiceBounds"),
                        rectangleJson(dialog->sheetCombo()->geometry()));
                    metrics.insert(QStringLiteral("scheduleBlockChoiceBounds"),
                        rectangleJson(dialog->userCombo()->geometry()));
                }
                if (auto* const preview = dialog->findChild<QTableView*>(
                        QStringLiteral("nextScheduleImportPreviewTable")))
                {
                    metrics.insert(QStringLiteral("previewModelRows"),
                                   preview->model()->rowCount());
                    metrics.insert(QStringLiteral("previewViewportBounds"),
                        rectangleJson(preview->viewport()->geometry()));
                    int visible = 0;
                    for (int row = 0; row < preview->model()->rowCount(); ++row)
                    {
                        if (preview->viewport()->rect().intersects(
                                preview->visualRect(preview->model()->index(row, 0))))
                        {
                            ++visible;
                        }
                    }
                    metrics.insert(QStringLiteral("previewVisibleRows"), visible);
                }
                if (dialog->classTable()->model())
                {
                    metrics.insert(QStringLiteral("classReviewRows"),
                        dialog->classTable()->model()->rowCount());
                }
                if (dialog->teacherTable()->model())
                {
                    metrics.insert(QStringLiteral("teacherReviewRows"),
                        dialog->teacherTable()->model()->rowCount());
                }
                return metrics;
            };

            QTemporaryDir smallTemporary;
            QVERIFY(smallTemporary.isValid());
            const auto smallUser = firstVisibleUser(scheduleFixturePath());
            QVERIFY(smallUser.has_value());
            const QString smallWorkspace = smallTemporary.filePath(
                QStringLiteral("small.tps"));
            QString error;
            QVERIFY2(createWorkspace(smallWorkspace, *smallUser, &error),
                     qPrintable(error));
            ScheduleImportPageFixture smallFixture;
            QVERIFY(smallFixture.initialize(smallWorkspace));
            auto* smallDialog = smallFixture.openImport();
            QVERIFY(smallDialog);
            smallDialog->findChild<QPushButton*>(
                QStringLiteral("nextScheduleImportBrowse"))->click();
            QCOMPARE(smallDialog->sourcePathEdit()->text(), scheduleFixturePath());
            const QJsonObject smallSource = saveDialog(smallDialog,
                QStringLiteral("small-source"),
                {{QStringLiteral("workbook"), QStringLiteral("schedule_review.xlsx")}});
            QVERIFY(!smallSource.isEmpty());
            captureRows.append(smallSource);
            smallDialog->primaryButton()->click();
            QTRY_VERIFY_WITH_TIMEOUT(smallDialog->sheetCombo()->count() > 0, 10000);
            const QJsonObject smallSelection = saveDialog(smallDialog,
                QStringLiteral("small-selection"),
                {{QStringLiteral("selectedWorksheet"),
                    smallDialog->sheetCombo()->currentText()},
                 {QStringLiteral("selectedScheduleBlock"),
                    smallDialog->userCombo()->currentText()},
                 {QStringLiteral("workbook"), QStringLiteral("schedule_review.xlsx")}});
            QVERIFY(!smallSelection.isEmpty());
            captureRows.append(smallSelection);
            QVERIFY(smallFixture.continueToReview(smallDialog));
            QTRY_VERIFY_WITH_TIMEOUT(smallDialog->classTable()->model()->rowCount() > 0,
                                     10000);
            const QJsonObject smallReview = saveDialog(smallDialog,
                QStringLiteral("small-review"),
                {{QStringLiteral("workbook"), QStringLiteral("schedule_review.xlsx")}});
            QVERIFY(!smallReview.isEmpty());
            captureRows.append(smallReview);
            setAllTeachersToCreate(smallDialog);
            setAllClassesToCreateNew(smallDialog);
            QVERIFY(smallDialog->primaryButton()->isEnabled());
            QVERIFY2(runSql(smallWorkspace, QStringLiteral(R"(
                CREATE TRIGGER capture_fail_schedule_import_time
                BEFORE INSERT ON class_times
                BEGIN
                    SELECT RAISE(ABORT, 'capture retry failure');
                END
            )"), &error), qPrintable(error));
            smallDialog->primaryButton()->click();
            QTRY_VERIFY_WITH_TIMEOUT(smallDialog->findChild<QLabel*>(
                QStringLiteral("nextScheduleImportReviewStatus"))->text().contains(
                    QStringLiteral("capture retry failure")), 5000);
            QJsonObject failureRetry = saveDialog(smallDialog,
                QStringLiteral("small-failure-retry"),
                {{QStringLiteral("workbook"), QStringLiteral("schedule_review.xlsx")},
                 {QStringLiteral("failureStatus"), smallDialog->findChild<QLabel*>(
                    QStringLiteral("nextScheduleImportReviewStatus"))->text()},
                 {QStringLiteral("retryEnabled"),
                    smallDialog->primaryButton()->isEnabled()}});
            QVERIFY(!failureRetry.isEmpty());
            QVERIFY(failureRetry.value(QStringLiteral("retryEnabled")).toBool());
            const qsizetype failureRetryIndex = captureRows.size();
            captureRows.append(failureRetry);
            QVERIFY2(runSql(smallWorkspace,
                QStringLiteral("DROP TRIGGER capture_fail_schedule_import_time"),
                &error), qPrintable(error));
            smallDialog->primaryButton()->click();
            QTRY_VERIFY_WITH_TIMEOUT(smallDialog->isHidden(), 10000);
            QCOMPARE(scalarInt(smallWorkspace,
                QStringLiteral("SELECT COUNT(*) FROM classes")),
                std::optional<int>{smallUser->classes.size()});
            failureRetry.insert(QStringLiteral("retrySucceeded"), true);
            failureRetry.insert(QStringLiteral("retryAttempts"),
                                static_cast<int>(smallFixture.applyRequests.size()));
            captureRows.replace(failureRetryIndex, failureRetry);

            QTRY_COMPARE_WITH_TIMEOUT(
                smallFixture.page->model()->runtimeMetrics().sourceClassCount,
                smallUser->classes.size(), 5000);
            const QString completedStatusPrefix = locale == QStringLiteral("ko")
                ? QStringLiteral("일정 가져오기가 완료되었습니다")
                : QStringLiteral("Schedule import completed");
            QVERIFY(smallFixture.page->statusLabel()->text().contains(
                completedStatusPrefix));
            smallFixture.host->resize(QSize(1024, 768));
            smallFixture.host->show();
            QApplication::processEvents();
            QVERIFY(smallFixture.page->isVisible());
            const QImage successHandoffImage =
                smallFixture.host->grab().toImage();
            const QString successHandoffName = prefix
                + QStringLiteral("-small-success-handoff.png");
            QVERIFY(!successHandoffImage.isNull());
            QVERIFY(successHandoffImage.save(
                QDir(outputPath).filePath(successHandoffName), "PNG"));
            captureRows.append(QJsonObject{
                {QStringLiteral("image"), successHandoffName},
                {QStringLiteral("width"), successHandoffImage.width()},
                {QStringLiteral("height"), successHandoffImage.height()},
                {QStringLiteral("pageHostWidth"), smallFixture.host->width()},
                {QStringLiteral("pageHostHeight"), smallFixture.host->height()},
                {QStringLiteral("scheduleRows"),
                    smallFixture.page->scheduleTable()->model()->rowCount()},
                {QStringLiteral("sourceClassCount"),
                    smallFixture.page->model()->runtimeMetrics().sourceClassCount},
                {QStringLiteral("status"),
                    smallFixture.page->statusLabel()->text()},
                {QStringLiteral("viewport"),
                    QStringLiteral("Schedule page host after apply and refresh")}
            });

            QTemporaryDir applicableTemporary;
            QVERIFY(applicableTemporary.isValid());
            const auto applicableUser = firstVisibleUser(
                largeApplicableFixturePath());
            QVERIFY(applicableUser.has_value());
            const QString applicableWorkspace = applicableTemporary.filePath(
                QStringLiteral("large-applicable.tps"));
            QVERIFY2(createWorkspace(applicableWorkspace, *applicableUser, &error),
                     qPrintable(error));
            ScheduleImportPageFixture applicableFixture;
            QVERIFY(applicableFixture.initialize(applicableWorkspace));
            applicableFixture.workbookPath = largeApplicableFixturePath();
            auto* applicableDialog = applicableFixture.openImport();
            QVERIFY(applicableDialog);
            QVERIFY(applicableFixture.selectWorkbookAndRead(applicableDialog));
            QTRY_VERIFY_WITH_TIMEOUT(applicableDialog->sheetCombo()->count() == 1,
                                     10000);
            QVERIFY(applicableFixture.continueToReview(applicableDialog));
            QTRY_VERIFY_WITH_TIMEOUT(
                applicableDialog->classTable()->model()->rowCount() == 20,
                10000);
            setAllTeachersToCreate(applicableDialog);
            setAllClassesToCreateNew(applicableDialog);
            QVERIFY(applicableDialog->primaryButton()->isEnabled());
            QJsonObject applicableReview = saveDialog(applicableDialog,
                QStringLiteral("large-applicable-review"),
                {{QStringLiteral("workbook"), QStringLiteral(
                    "generated-large-schedule-import-applicable.xlsx")},
                 {QStringLiteral("parserWideStressCandidates"), 96},
                 {QStringLiteral("parserWideStressBlocks"), 5},
                 {QStringLiteral("selectedBlockCandidates"), 20}});
            QVERIFY(!applicableReview.isEmpty());
            captureRows.append(applicableReview);
            applicableDialog->primaryButton()->click();
            QTRY_VERIFY_WITH_TIMEOUT(applicableDialog->isHidden(), 15000);
            const int persistedClasses = scalarInt(applicableWorkspace,
                QStringLiteral("SELECT COUNT(*) FROM classes")).value_or(-1);
            const int persistedTimes = scalarInt(applicableWorkspace,
                QStringLiteral("SELECT COUNT(*) FROM class_times")).value_or(-1);
            QCOMPARE(persistedClasses, 20);
            QCOMPARE(persistedTimes, 30);
            applicableReview.insert(QStringLiteral("verifiedAppliedCandidates"),
                static_cast<int>(applicableFixture.applyRequests.front()
                                     .candidates.size()));
            applicableReview.insert(QStringLiteral("verifiedPersistedClassRows"),
                                    persistedClasses);
            applicableReview.insert(QStringLiteral("verifiedPersistedScheduleRows"),
                                    persistedTimes);
            const qsizetype applicableIndex = captureRows.size() - 1;
            captureRows.replace(applicableIndex, applicableReview);

            QTemporaryDir largeTemporary;
            QVERIFY(largeTemporary.isValid());
            const auto largeUser = firstVisibleUser(largeConflictFixturePath());
            QVERIFY(largeUser.has_value());
            const QString largeWorkspace = largeTemporary.filePath(
                QStringLiteral("large.tps"));
            QVERIFY2(createWorkspace(largeWorkspace, *largeUser, &error),
                     qPrintable(error));
            ScheduleImportPageFixture largeFixture;
            QVERIFY(largeFixture.initialize(largeWorkspace));
            largeFixture.workbookPath = largeConflictFixturePath();
            auto* largeDialog = largeFixture.openImport();
            QVERIFY(largeDialog);
            largeDialog->findChild<QPushButton*>(
                QStringLiteral("nextScheduleImportBrowse"))->click();
            const QJsonObject largeSource = saveDialog(largeDialog,
                QStringLiteral("large-source"),
                {{QStringLiteral("workbookCandidates"), 96}});
            QVERIFY(!largeSource.isEmpty());
            captureRows.append(largeSource);

            const ProcessMemorySnapshot beforeParse = memoryProvider.snapshot();
            QElapsedTimer parseTimer;
            parseTimer.start();
            largeDialog->primaryButton()->click();
            QTRY_VERIFY_WITH_TIMEOUT(largeDialog->sheetCombo()->count() > 0, 15000);
            const qint64 parseMilliseconds = parseTimer.elapsed();
            const ProcessMemorySnapshot afterParse = memoryProvider.snapshot();
            const QJsonObject largeSelection = saveDialog(largeDialog,
                QStringLiteral("large-selection"),
                {{QStringLiteral("workbookCandidates"), 96},
                 {QStringLiteral("scheduleBlocks"), 5},
                 {QStringLiteral("selectedBlockCandidates"), 20},
                 {QStringLiteral("selectedWorksheet"),
                    largeDialog->sheetCombo()->currentText()},
                 {QStringLiteral("selectedScheduleBlock"),
                    largeDialog->userCombo()->currentText()},
                 {QStringLiteral("parseMilliseconds"), parseMilliseconds}});
            QVERIFY(!largeSelection.isEmpty());
            captureRows.append(largeSelection);
            QVERIFY(largeFixture.continueToReview(largeDialog));
            QTRY_VERIFY_WITH_TIMEOUT(largeDialog->classTable()->model()->rowCount() > 0,
                                     10000);
            setAllTeachersToCreate(largeDialog);
            setAllClassesToCreateNew(largeDialog);
            const QString conflictWord = locale == QStringLiteral("ko")
                ? QStringLiteral("겹칩니다") : QStringLiteral("overlap");
            QTRY_VERIFY_WITH_TIMEOUT(largeDialog->findChild<QLabel*>(
                QStringLiteral("nextScheduleImportReviewStatus"))->text().contains(
                    conflictWord, Qt::CaseInsensitive), 10000);
            const ProcessMemorySnapshot afterReview = memoryProvider.snapshot();
            QJsonObject largeConflict = saveDialog(largeDialog,
                QStringLiteral("large-conflict"),
                {{QStringLiteral("workbookCandidates"), 96},
                 {QStringLiteral("scheduleBlocks"), 5},
                 {QStringLiteral("selectedBlockCandidates"), 20},
                 {QStringLiteral("parseMilliseconds"), parseMilliseconds},
                 {QStringLiteral("conflictStatus"),
                    largeDialog->findChild<QLabel*>(
                        QStringLiteral("nextScheduleImportReviewStatus"))->text()},
                 {QStringLiteral("memoryBeforeParse"), memoryJson(beforeParse)},
                 {QStringLiteral("memoryAfterParse"), memoryJson(afterParse)},
                 {QStringLiteral("memoryAfterReview"), memoryJson(afterReview)}});
            QVERIFY(!largeConflict.isEmpty());
            captureRows.append(largeConflict);

            QPointer<Ui::ScheduleImportWorkflowDialog> guarded(largeDialog);
            largeDialog->reject();
            largeConflict.insert(QStringLiteral("releasedClassRows"),
                largeDialog->classTable()->model()->rowCount());
            largeConflict.insert(QStringLiteral("releasedTeacherRows"),
                largeDialog->teacherTable()->model()->rowCount());
            largeConflict.insert(QStringLiteral("releasedPreviewRows"),
                largeDialog->findChild<QTableView*>(
                    QStringLiteral("nextScheduleImportPreviewTable"))
                    ->model()->rowCount());
            largeConflict.insert(QStringLiteral("releasedSheetChoices"),
                largeDialog->sheetCombo()->count());
            largeConflict.insert(QStringLiteral("releasedUserChoices"),
                largeDialog->userCombo()->count());
            largeConflict.insert(QStringLiteral("releasedSourcePath"),
                largeDialog->sourcePathEdit()->text().isEmpty());
            QApplication::processEvents();
            QTRY_VERIFY_WITH_TIMEOUT(guarded.isNull(), 5000);
            largeConflict.insert(QStringLiteral("memoryAfterDialogClose"),
                memoryJson(memoryProvider.snapshot()));
            captureRows.replace(captureRows.size() - 1, largeConflict);

            QTemporaryDir overlapTemporary;
            QVERIFY(overlapTemporary.isValid());
            const QString overlapWorkbook = overlapConflictFixturePath();
            const auto overlapUser = firstVisibleUser(overlapWorkbook);
            QVERIFY(overlapUser.has_value());
            const QString overlapWorkspace = overlapTemporary.filePath(
                QStringLiteral("overlap.tps"));
            QVERIFY2(createWorkspace(overlapWorkspace, *overlapUser, &error),
                     qPrintable(error));
            ScheduleImportPageFixture overlapFixture;
            QVERIFY(overlapFixture.initialize(overlapWorkspace));
            overlapFixture.workbookPath = overlapWorkbook;
            auto* overlapDialog = overlapFixture.openImport();
            QVERIFY(overlapDialog);
            QVERIFY(overlapFixture.selectWorkbookAndRead(overlapDialog));
            QTRY_VERIFY_WITH_TIMEOUT(overlapDialog->sheetCombo()->count() > 0,
                                     10000);
            QVERIFY(overlapFixture.continueToReview(overlapDialog));
            setAllTeachersToCreate(overlapDialog);
            setAllClassesToCreateNew(overlapDialog);
            const QString overlapWord = locale == QStringLiteral("ko")
                ? QStringLiteral("겹칩니다") : QStringLiteral("overlap");
            QTRY_VERIFY_WITH_TIMEOUT(overlapDialog->findChild<QLabel*>(
                QStringLiteral("nextScheduleImportReviewStatus"))->text().contains(
                    overlapWord, Qt::CaseInsensitive), 5000);
            const QJsonObject matchedConflict = saveDialog(overlapDialog,
                QStringLiteral("matched-conflict"),
                {{QStringLiteral("workbook"),
                    QStringLiteral("schedule_overlap_conflict.xlsx")},
                 {QStringLiteral("candidateCount"), overlapUser->classes.size()},
                 {QStringLiteral("conflictStatus"),
                    overlapDialog->findChild<QLabel*>(
                        QStringLiteral("nextScheduleImportReviewStatus"))->text()}});
            QVERIFY(!matchedConflict.isEmpty());
            captureRows.append(matchedConflict);
            overlapDialog->reject();
            QApplication::processEvents();

            QVERIFY(application->removeTranslator(&translator));
        }
    }

    const QStringList legacyScenes{
        QStringLiteral("small-source"),
        QStringLiteral("small-selection"),
        QStringLiteral("small-review"),
        QStringLiteral("overlap-conflict")
    };
    const QString legacyCaptureDirectory = QDir(outputPath).filePath(
        QStringLiteral("legacy-captures"));
    QStringList legacyNames;
    bool hasMatchedLegacyCoverage = true;
    for (const QString& locale : {QStringLiteral("en"), QStringLiteral("ko")})
    {
        for (const QString& theme : {QStringLiteral("light"), QStringLiteral("dark")})
        {
            for (const QString& scene : legacyScenes)
            {
                const QString sourceName = locale + QLatin1Char('-') + theme
                    + QLatin1Char('-') + scene + QStringLiteral(".png");
                const QString destinationName = QStringLiteral("legacy-")
                    + sourceName;
                legacyNames.append(destinationName);
                hasMatchedLegacyCoverage = hasMatchedLegacyCoverage
                    && QFileInfo::exists(QDir(legacyCaptureDirectory)
                                             .filePath(sourceName));
            }
        }
    }

    QString legacyReferenceScope;
    if (hasMatchedLegacyCoverage)
    {
        for (const QString& name : legacyNames)
        {
            const QString sourceName = name.mid(QStringLiteral("legacy-").size());
            const QString source = QDir(legacyCaptureDirectory).filePath(sourceName);
            const QString destination = QDir(outputPath).filePath(name);
            if (QFile::exists(destination))
            {
                QVERIFY(QFile::remove(destination));
            }
            QVERIFY2(QFile::copy(source, destination), qPrintable(source));
        }
        for (const QString& staleName : {
                 QStringLiteral("legacy-en-light-large-source.png"),
                 QStringLiteral("legacy-en-light-conflict.png")})
        {
            const QString stalePath = QDir(outputPath).filePath(staleName);
            if (QFile::exists(stalePath))
            {
                QVERIFY(QFile::remove(stalePath));
            }
        }
        legacyReferenceScope = QStringLiteral(
            "Legacy source, selected sheet/block, and small review use the same small workbook as Next; overlap conflict uses the same fixture in English/Korean and light/dark. The 96-candidate stress capture is Next-specific.");
    }
    else
    {
        const QString legacyRoot = QStringLiteral(CLASSMNGR_SOURCE_DIR)
            + QStringLiteral("/docs/qt-rewrite/visual-baseline");
        const QStringList legacySources{
            QDir(legacyRoot).filePath(QStringLiteral(
                "release/large-schedule-import-boundary/schedule-import-source.png")),
            QDir(legacyRoot).filePath(QStringLiteral("schedule/schedule-import-review.png")),
            QDir(legacyRoot).filePath(QStringLiteral("schedule/schedule-conflict-review.png"))
        };
        legacyNames = {
            QStringLiteral("legacy-en-light-large-source.png"),
            QStringLiteral("legacy-en-light-small-review.png"),
            QStringLiteral("legacy-en-light-conflict.png")
        };
        for (qsizetype index = 0; index < legacySources.size(); ++index)
        {
            const QString destination = QDir(outputPath).filePath(legacyNames.at(index));
            if (QFile::exists(destination))
            {
                QVERIFY(QFile::remove(destination));
            }
            QVERIFY2(QFile::copy(legacySources.at(index), destination),
                     qPrintable(legacySources.at(index)));
        }
        legacyReferenceScope = QStringLiteral(
            "Fallback saved Legacy source/review/conflict references are English/light. Run captureScheduleImportVisualEvidenceWhenConfigured in ClassMngrScheduleImportDialogTests into the legacy-captures folder for the matched 4 x 4 set.");
    }

    const QJsonObject manifest{
        {QStringLiteral("taskId"), QStringLiteral(
            "phase7-c31-schedule-workbook-import-20261028")},
        {QStringLiteral("captureScenes"), captureRows},
        {QStringLiteral("nextCoverage"), QJsonArray{
            QStringLiteral("English/light"), QStringLiteral("English/dark"),
            QStringLiteral("Korean/light"), QStringLiteral("Korean/dark")}},
        {QStringLiteral("legacyReferences"), QJsonArray::fromStringList(legacyNames)},
        {QStringLiteral("legacyReferenceScope"), legacyReferenceScope},
        {QStringLiteral("parityClaim"), false},
        {QStringLiteral("captureViewport"), QStringLiteral(
            "Dialog scenes use QDialog::grab at QT_SCALE_FACTOR=1 and "
            "QT_QPA_PLATFORM=offscreen. The small-success-handoff scene captures "
            "the refreshed Schedule PageHost at 1024x768.")},
        {QStringLiteral("largeDataInterpretation"), QStringLiteral(
            "The parser stress workbook contains 96 candidates across five "
            "schedule blocks; its selected conflict block contains 20 model rows. "
            "A separate non-overlapping 20-candidate block is applied to a real "
            ".tps store and persisted counts are recorded. No 96-row apply is claimed.")},
        {QStringLiteral("visualDifferences"), QStringLiteral(
            "Shared workflow states are captured with Fusion and explicit light/dark palettes. "
            "Legacy and Next source/selection dialogs are 436x520; review/conflict dialogs "
            "are 800x800. Next's narrow candidate preview clips part of a meeting time until "
            "horizontal scrolling; its overlap status is generic and does not identify the "
            "conflicting pair, while Legacy shows the conflicting class details. Legacy "
            "Korean retains some English fallback strings. The images compare state and "
            "content, not pixel parity.")},
        {QStringLiteral("memoryScope"), QStringLiteral(
            "Samples cover the whole test process on this host, including Qt, "
            "the page host, current workflow, and runtime caches. They do not "
            "attribute bytes to the parser or prove a process memory budget.")}
    };
    QFile manifestFile(QDir(outputPath).filePath(QStringLiteral("manifest.json")));
    QVERIFY(manifestFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const QByteArray json = QJsonDocument(manifest).toJson(QJsonDocument::Indented);
    QCOMPARE(manifestFile.write(json), json.size());
}

QTEST_MAIN(NormalScheduleImportIntegrationTests)
#include "normal_schedule_import_integration_tests.moc"
