#include "data/database/workspace_store.h"
#include "data/repositories/class_repository.h"
#include "data/repositories/roster_repository.h"
#include "data/repositories/speaking_eval_repository.h"
#include "data/repositories/teacher_repository.h"
#include "core/utils/sidebar_node_naming.h"
#include "domain/models/roster.h"
#include "domain/models/speaking_evaluation.h"
#include "domain/models/class_info.h"
#include "domain/models/teacher.h"
#include "next/application/roster_transfer_target_display_name.h"
#include "next/platform/workspace_store_roster_actions.h"
#include "next/ui/roster_editor_widget.h"
#include "next/ui/roster_table_model.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QMenu>
#include <QPalette>
#include <QPushButton>
#include <QSqlError>
#include <QSqlQuery>
#include <QTableView>
#include <QTemporaryDir>
#include <QTimer>
#include <QTranslator>
#include <QTranslator>
#include <QtTest/QtTest>

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

using namespace ClassMngr;
using namespace ClassMngr::Next;

namespace
{

[[nodiscard]] Domain::ClassId typedClassId(const int id)
{
    return *Domain::ClassId::fromString(std::to_string(id));
}

[[nodiscard]] Application::RosterSnapshot ordinarySnapshot(
    const int customColumnCount = 0
    )
{
    Application::RosterSnapshot snapshot;
    snapshot.columns = {
        u"English", u"Korean", u"Winter", u"Speech Contest", u"Summer", u"Fall"
    };
    snapshot.columnWidths = {
        Application::RosterSnapshotColumnWidth{170},
        Application::RosterSnapshotColumnWidth{120},
        Application::RosterSnapshotColumnWidth{130},
        Application::RosterSnapshotColumnWidth{130},
        Application::RosterSnapshotColumnWidth{130},
        Application::RosterSnapshotColumnWidth{130}
    };
    for (int index = 0; index < customColumnCount; ++index)
    {
        snapshot.columns.push_back(
            QStringLiteral("Custom %1").arg(index + 1).toStdU16String());
        snapshot.columnWidths.push_back(
            Application::RosterSnapshotColumnWidth{80 + (index % 40)});
    }
    snapshot.rows.resize(Application::RosterModeledRowCount);
    for (std::vector<std::u16string>& row : snapshot.rows)
    {
        row.resize(snapshot.columns.size());
    }
    snapshot.rows[0][0] = u"Student A";
    snapshot.rows[0][1] = u"\uAE40\uBBFC\uC9C0";
    snapshot.rows[1][0] = u"Student B";
    snapshot.rows[1][1] = u"\uBC15\uC9C0\uBBFC";
    snapshot.rows[24][0] = u"Student Y";
    snapshot.rows[24][1] = u"\uC774\uC9C0\uC218";
    return snapshot;
}

[[nodiscard]] Application::RosterSnapshot emptyRosterSnapshot(
    const int customColumnCount = 0
    )
{
    Application::RosterSnapshot snapshot = ordinarySnapshot(customColumnCount);
    for (auto& row : snapshot.rows)
    {
        std::fill(row.begin(), row.end(), std::u16string{});
    }
    return snapshot;
}

struct ActionProbe final
{
    Application::RosterSnapshot snapshot = ordinarySnapshot();
    Application::RosterSnapshot targetSnapshot = emptyRosterSnapshot();
    int readCount = 0;
    int targetReadCount = 0;
    int saveCount = 0;
    int scoreReadCount = 0;
    int targetChoicesReadCount = 0;
    int transferSaveCount = 0;
    int failSaveCount = 0;
    bool failRead = false;
    bool failTargetRead = false;
    bool failScoreRead = false;
    bool failTargetChoicesRead = false;
    bool failTransferSave = false;
    bool transferTargetFull = false;
    bool includeDifferentGradeTransferTarget = false;
    bool hasRowsOutsideWindow = false;
    bool hasTailColumnData = false;
    QString lastReadPath;
    QString lastSavePath;
    QString lastScoreReadPath;
    QString lastTargetChoicesPath;
    QString lastTransferSavePath;
    std::vector<std::u16string> requestedEvaluations;
    std::vector<std::vector<Application::SpeakingEvaluationRosterScore>> importedScores;
    std::optional<Application::RosterSaveRequest> lastSave;
    std::optional<Application::RosterRowTransferSaveRequest> lastTransferSave;
    Domain::ClassId targetClassId = typedClassId(99);
};

[[nodiscard]] Ui::RosterEditorActions fakeActions(
    ActionProbe& probe,
    const Application::SaveMode mode = Application::SaveMode::Manual
    )
{
    Ui::RosterEditorActions actions;
    actions.saveMode = mode;
    actions.readRoster = [&probe](
        const QString& path,
        const Domain::ClassId& classId)
    {
        const bool targetRead = classId == probe.targetClassId;
        if (targetRead)
        {
            ++probe.targetReadCount;
        }
        else
        {
            ++probe.readCount;
            probe.lastReadPath = path;
        }
        if (targetRead && probe.failTargetRead)
        {
            return Application::RosterWindowReadResult::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "injected target roster read failure",
                .recoverable = true
            });
        }
        if (probe.failRead)
        {
            return Application::RosterWindowReadResult::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "injected roster read failure",
                .recoverable = true
            });
        }
        return Application::RosterWindowReadResult::success({
            .roster = targetRead ? probe.targetSnapshot : probe.snapshot,
            .hasRowsOutsideWindow = targetRead ? false : probe.hasRowsOutsideWindow
        });
    };
    actions.saveRoster = [&probe](
        const QString& path,
        const Application::RosterSaveRequest& request)
    {
        ++probe.saveCount;
        probe.lastSavePath = path;
        probe.lastSave = request;
        if (probe.failSaveCount > 0)
        {
            --probe.failSaveCount;
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "injected roster save failure",
                .recoverable = true
            });
        }
        return Domain::Result<void>::success();
    };
    actions.columnHasTailData = [&probe](
        const QString&,
        const Domain::ClassId&,
        const std::u16string&)
    {
        return Domain::Result<bool>::success(probe.hasTailColumnData);
    };
    actions.readEvaluationScores = [&probe](
        const QString& path,
        const Domain::ClassId&,
        const std::vector<std::u16string>& names)
    {
        ++probe.scoreReadCount;
        probe.lastScoreReadPath = path;
        probe.requestedEvaluations = names;
        if (probe.failScoreRead)
        {
            return Application::SpeakingEvaluationRosterScoreImportBatchResult::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "injected evaluation read failure",
                .recoverable = true
            });
        }
        Application::SpeakingEvaluationRosterScoreImportBatchItems results;
        results.reserve(names.size());
        for (std::size_t index = 0; index < names.size(); ++index)
        {
            const std::vector<Application::SpeakingEvaluationRosterScore> scores =
                index < probe.importedScores.size()
                    ? probe.importedScores[index]
                    : std::vector<Application::SpeakingEvaluationRosterScore>{
                        {u"Student A", u"\uAE40\uBBFC\uC9C0", u"A+"}
                    };
            results.push_back(
                Application::SpeakingEvaluationRosterScoreImportResult::success(
                    scores));
        }
        return Application::SpeakingEvaluationRosterScoreImportBatchResult::success(
            std::move(results));
    };
    actions.readTransferTargets = [&probe](
        const QString& path,
        const Domain::ClassId&)
    {
        ++probe.targetChoicesReadCount;
        probe.lastTargetChoicesPath = path;
        if (probe.failTargetChoicesRead)
        {
            return Application::RosterTransferTargetChoicesResult::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "injected transfer choices read failure",
                .recoverable = true
            });
        }
        Application::RosterTransferTargetChoice target{
            .classId = probe.targetClassId,
            .className = u"Target class",
            .displayMetadata = {
                .grade = u"Grade 3",
                .level = u"Level 2",
                .teacherEnglishName = u"Teacher English",
                .teacherKoreanName = u"\uAD50\uC0AC",
                .teacherPreferredRomanization = u"Teacher Romanization",
                .teacherPreferredName = u"Preferred Teacher",
                .regularSchedule = {
                    {u"Monday", u"4:00 PM"},
                    {u"Wednesday", u"4:00 PM"},
                    {u"Friday", u"4:00 PM"}
                }
            },
            .full = probe.transferTargetFull
        };
        std::vector<Application::RosterTransferTargetChoice> candidates{
            target
        };
        if (probe.includeDifferentGradeTransferTarget)
        {
            Application::RosterTransferTargetChoice differentGrade = target;
            differentGrade.classId = typedClassId(100);
            differentGrade.className = u"Other grade class";
            differentGrade.displayMetadata.grade = u"Grade 4";
            differentGrade.full = false;
            candidates.push_back(std::move(differentGrade));
        }
        return Application::RosterTransferTargetChoicesResult::success({
            .sourceGrade = u"Grade 3",
            .candidates = std::move(candidates)
        });
    };
    actions.saveRosterTransfer = [&probe](
        const QString& path,
        const Application::RosterRowTransferSaveRequest& request)
    {
        ++probe.transferSaveCount;
        probe.lastTransferSavePath = path;
        probe.lastTransferSave = request;
        if (probe.failTransferSave)
        {
            return Domain::Result<void>::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "injected pair-save failure",
                .recoverable = true
            });
        }
        return Domain::Result<void>::success();
    };
    return actions;
}

[[nodiscard]] Roster rawRoster(
    const QStringList& customColumns,
    const int customWidth = 119
    )
{
    Roster roster;
    roster.columns = {
        QStringLiteral("English"), QStringLiteral("Korean"),
        QStringLiteral("Winter"), QStringLiteral("Speech Contest"),
        QStringLiteral("Summer"), QStringLiteral("Fall")
    };
    roster.columns.append(customColumns);
    for (int column = 0; column < roster.columns.size(); ++column)
    {
        roster.columnWidths.push_back(column < 6 ? 120 + column : customWidth);
    }
    for (int row = 0; row < 25; ++row)
    {
        QStringList cells(roster.columns.size(), QString{});
        roster.rows.push_back(std::move(cells));
    }
    roster.rows[0][0] = QStringLiteral("Student A");
    roster.rows[0][1] = QStringLiteral("\uAE40\uBBFC\uC9C0");
    roster.rows[1][0] = QStringLiteral("Student B");
    roster.rows[1][1] = QStringLiteral("\uBC15\uC9C0\uBBFC");
    roster.rows[24][0] = QStringLiteral("Student Y");
    roster.rows[24][1] = QStringLiteral("\uC774\uC9C0\uC218");
    return roster;
}

[[nodiscard]] Roster emptyRawRoster(const QStringList& customColumns)
{
    Roster roster = rawRoster(customColumns);
    for (QStringList& row : roster.rows)
    {
        std::fill(row.begin(), row.end(), QString{});
    }
    return roster;
}

struct TransferWorkspaceIds final
{
    int source = 0;
    int target = 0;
};

[[nodiscard]] bool seedRosterTransferWorkspace(
    const QString& path,
    const QString& sourceGrade,
    const QString& targetGrade,
    const QStringList& customColumns,
    const bool fullTarget,
    const QString& evaluationGrade,
    TransferWorkspaceIds* ids,
    QString* error
    )
{
    WorkspaceStore store;
    const RepositoryStatus opened = store.open(path);
    if (!opened)
    {
        if (error) *error = opened.error().diagnostic;
        return false;
    }
    const auto sourceClass = store.classRepository()->createClass(
        QStringLiteral("Source class"));
    const auto targetClass = store.classRepository()->createClass(
        QStringLiteral("Target class"));
    if (!sourceClass || !targetClass)
    {
        if (error)
        {
            *error = !sourceClass
                ? sourceClass.error().diagnostic
                : targetClass.error().diagnostic;
        }
        return false;
    }

    Roster source = rawRoster(customColumns);
    Roster target = emptyRawRoster(customColumns);
    if (!customColumns.isEmpty())
    {
        source.rows[0][Roster::BaseColumns.size()] = QStringLiteral("Source portfolio");
    }
    if (fullTarget)
    {
        for (int row = 0; row < target.rows.size(); ++row)
        {
            target.rows[row][0] = QStringLiteral("Full %1").arg(row + 1);
            target.rows[row][1] = QStringLiteral("\uAE40\uBBFC\uC9C0");
        }
    }

    const RepositoryStatus saved = store.rosterRepository()
        ->saveRosterWindowsWithParentCheck({{
            {.classId = *sourceClass, .roster = source},
            {.classId = *targetClass, .roster = target}
        }});
    if (!saved)
    {
        if (error) *error = saved.error().diagnostic;
        return false;
    }

    QSqlQuery classInfo(store.database());
    classInfo.prepare(QStringLiteral(
        "INSERT INTO class_info (class_id, teacher_id, class_grade, class_level, "
        "reading_book, essay_book, class_color, font_color, notes, time_filler_activities) "
        "VALUES (?, NULL, ?, '', '', '', '#FFFFFF', '#000000', '', '') "
        "ON CONFLICT(class_id) DO UPDATE SET class_grade=excluded.class_grade"));
    classInfo.bindValue(0, *sourceClass);
    classInfo.bindValue(1, sourceGrade);
    if (!classInfo.exec())
    {
        if (error) *error = classInfo.lastError().text();
        return false;
    }
    classInfo.finish();
    classInfo.bindValue(0, *targetClass);
    classInfo.bindValue(1, targetGrade);
    if (!classInfo.exec())
    {
        if (error) *error = classInfo.lastError().text();
        return false;
    }

    SpeakingEvalRows evaluation = SpeakingEval::emptyRows();
    evaluation[0][1] = QStringLiteral("Student A");
    evaluation[0][2] = QStringLiteral("\uAE40\uBBFC\uC9C0");
    for (int column = 3; column <= 8; ++column)
    {
        evaluation[0][column] = evaluationGrade;
    }
    const RepositoryStatus savedEvaluation = store.speakingEvalRepository()
        ->saveSpeakingEval(*sourceClass, QStringLiteral("Winter"), evaluation);
    if (!savedEvaluation)
    {
        if (error) *error = savedEvaluation.error().diagnostic;
        return false;
    }
    if (customColumns.size() > 0)
    {
        QSqlQuery tail(store.database());
        tail.prepare(QStringLiteral(
            "INSERT INTO roster_data (class_id,row_index,col_index,value) "
            "VALUES (?,?,?,?)"));
        tail.addBindValue(*sourceClass);
        tail.addBindValue(37);
        tail.addBindValue(Roster::BaseColumns.size());
        tail.addBindValue(QStringLiteral("Source tail"));
        if (!tail.exec())
        {
            if (error) *error = tail.lastError().text();
            return false;
        }
        tail.finish();
        tail.bindValue(0, *targetClass);
        tail.bindValue(1, 41);
        tail.bindValue(2, Roster::BaseColumns.size());
        tail.bindValue(3, QStringLiteral("Target tail"));
        if (!tail.exec())
        {
            if (error) *error = tail.lastError().text();
            return false;
        }
    }

    if (ids)
    {
        ids->source = *sourceClass;
        ids->target = *targetClass;
    }
    return true;
}

[[nodiscard]] bool seedWorkspace(
    const QString& path,
    const QString& className,
    const QStringList& customColumns,
    const int tailRow,
    const int tailColumn,
    const QString& tailValue,
    int* classId,
    QString* error
    )
{
    WorkspaceStore store;
    const RepositoryStatus opened = store.open(path);
    if (!opened)
    {
        if (error) *error = opened.error().diagnostic;
        return false;
    }
    const auto created = store.classRepository()->createClass(className);
    if (!created)
    {
        if (error) *error = created.error().diagnostic;
        return false;
    }
    const int id = *created;
    const RepositoryStatus saved = store.rosterRepository()
        ->saveRosterWindowsWithParentCheck({{
            .classId = id,
            .roster = rawRoster(customColumns),
            .removedCustomColumns = {}
        }});
    if (!saved)
    {
        if (error) *error = saved.error().diagnostic;
        return false;
    }
    if (tailRow >= 25 && tailColumn >= 0 && !tailValue.isEmpty())
    {
        QSqlQuery insert(store.database());
        insert.prepare(QStringLiteral(
            "INSERT INTO roster_data (class_id,row_index,col_index,value) "
            "VALUES (?,?,?,?)"));
        insert.addBindValue(id);
        insert.addBindValue(tailRow);
        insert.addBindValue(tailColumn);
        insert.addBindValue(tailValue);
        if (!insert.exec())
        {
            if (error) *error = insert.lastError().text();
            return false;
        }
    }
    if (classId) *classId = id;
    return true;
}

[[nodiscard]] std::optional<QString> storedCell(
    const QString& path,
    const int classId,
    const int row,
    const int column
    )
{
    WorkspaceStore store;
    if (!store.openExisting(path))
    {
        return std::nullopt;
    }
    QSqlQuery query(store.database());
    query.prepare(QStringLiteral(
        "SELECT value FROM roster_data WHERE class_id=? AND row_index=? "
        "AND col_index=?"));
    query.addBindValue(classId);
    query.addBindValue(row);
    query.addBindValue(column);
    if (!query.exec() || !query.next())
    {
        return std::nullopt;
    }
    return query.value(0).toString();
}

[[nodiscard]] bool setRawHeader(
    const QString& path,
    const int classId,
    const int position,
    const QString& rawName,
    QString* error
    )
{
    WorkspaceStore store;
    if (!store.openExisting(path))
    {
        if (error) *error = QStringLiteral("workspace did not open");
        return false;
    }
    QSqlQuery update(store.database());
    update.prepare(QStringLiteral(
        "UPDATE roster_columns SET name=? WHERE class_id=? AND position=?"));
    update.addBindValue(rawName);
    update.addBindValue(classId);
    update.addBindValue(position);
    if (!update.exec() || update.numRowsAffected() != 1)
    {
        if (error) *error = update.lastError().text();
        return false;
    }
    return true;
}

class PathRosterWindowReadPort final
    : public Application::RosterWindowReadPort
{
public:
    explicit PathRosterWindowReadPort(QString path)
        : m_path(std::move(path))
    {
    }

    [[nodiscard]] Application::RosterWindowReadResult readRosterWindow(
        const Application::RosterWindowReadQuery& query
        ) const override
    {
        return Platform::readRosterWindowAtPath(m_path, query.classId);
    }

private:
    QString m_path;
};

class PathRosterTransferSavePort final
    : public Application::RosterRowTransferSavePort
{
public:
    explicit PathRosterTransferSavePort(QString path)
        : m_path(std::move(path))
    {
    }

    [[nodiscard]] Domain::Result<void> saveTransfer(
        const Application::RosterRowTransferSaveRequest& request
        ) const override
    {
        return Platform::saveRosterTransferAtPath(m_path, request);
    }

private:
    QString m_path;
};

[[nodiscard]] bool clickActiveMessageBox(
    const QMessageBox::StandardButton button
    )
{
    auto* const box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
    if (!box || !box->button(button))
    {
        return false;
    }
    box->button(button)->click();
    return true;
}

void answerNextMessageBox(
    const QMessageBox::StandardButton first,
    const std::optional<QMessageBox::StandardButton> second = std::nullopt
    )
{
    QTimer::singleShot(100, [first, second] {
        if (!clickActiveMessageBox(first))
        {
            return;
        }
        if (second)
        {
            QTimer::singleShot(0, [second] {
                if (!clickActiveMessageBox(*second))
                {
                    static_cast<void>(clickActiveMessageBox(QMessageBox::Ok));
                }
            });
        }
    });
}

void chooseTransferMenuTarget(const QString& classId)
{
    QTimer::singleShot(0, [classId] {
        auto* const menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        if (!menu)
        {
            return;
        }
        for (QAction* const action : menu->actions())
        {
            if (action->isEnabled()
                && action->data().toString() == classId)
            {
                QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier,
                    menu->actionGeometry(action).center());
                return;
            }
        }
    });
}

void cancelActiveTransferMenu()
{
    QTimer::singleShot(0, [] {
        if (QWidget* const popup = QApplication::activePopupWidget())
        {
            popup->close();
        }
    });
}

struct TransferMenuActionObservation final
{
    QString classId;
    QString text;
    QString toolTip;
    bool enabled = false;
};

void observeActiveTransferMenu(
    std::vector<TransferMenuActionObservation>* const observations
    )
{
    QTimer::singleShot(0, [observations] {
        auto* const menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        if (!menu)
        {
            return;
        }
        for (QAction* const action : menu->actions())
        {
            observations->push_back({
                .classId = action->data().toString(),
                .text = action->text(),
                .toolTip = action->toolTip(),
                .enabled = action->isEnabled()
            });
        }
        menu->close();
    });
}

} // namespace

class NextRosterEditorTests final : public QObject
{
    Q_OBJECT

private slots:
    void editorLoadsOnDemandAndUsesOnlyActiveCellEditors();
    void emptyRosterAndReadFailureHaveDifferentStatesAndNoFailureWrite();
    void automaticManualAndFailedSaveRetryRetainLatestDraft();
    void discardReloadsVisibleWindowAndClearsDraft();
    void structuredCellIssuesLocalizeAndQuestionableKoreanRequiresConsent();
    void exactWorkspacePathsPersistWidthsAndKeepSparseTailIndexes();
    void customColumnTailDataRequiresTwoStageConfirmation();
    void normalizedHeaderTailDataRequiresCancelOrExplicitDeletion();
    void ambiguousNormalizedDuplicateHeadersFailClosed();
    void pathActionsRejectMalformedIdsBeforeOpeningWorkspace();
    void scoreImportMapsTrimmedPairsAndIsIdempotent();
    void scoreImportRequiresNameColumnsAndUsesAvailableGradeColumns();
    void scoreImportReadFailureDoesNotMutateAndAutosaveFailureRetries();
    void transferCancelAndFailuresPreserveDirtySelectionAndTimer();
    void transferMenuFiltersOtherGradesAndDisablesFullTargets();
    void transferSuccessRemovesSourceOnlyAfterPairSave();
    void pathRosterWorkflowActionsStayScopedAndPairSaveIsAtomic();
    void transferTargetNamesMatchLegacyFormattingAndFallbacks();
    void capturesEnglishAndKoreanLightAndDarkRosterStates();
    void capturesImportAndTransferStatesAcrossLocalesAndThemes();
};

void NextRosterEditorTests::editorLoadsOnDemandAndUsesOnlyActiveCellEditors()
{
    ActionProbe probe;
    probe.snapshot = ordinarySnapshot(150);
    probe.snapshot.columnWidths.back() = Application::RosterSnapshotColumnWidth{239};
    probe.hasRowsOutsideWindow = true;
    Ui::RosterEditorWidget editor(fakeActions(probe));
    QCOMPARE(probe.readCount, 0);

    editor.enterClassSection(QStringLiteral("memory.tps"), typedClassId(12));
    QCOMPARE(probe.readCount, 1);
    QVERIFY(editor.isLoaded());
    QVERIFY(!editor.readFailed());
    QCOMPARE(editor.model()->rowCount(), 25);
    QCOMPARE(editor.model()->columnCount(), 156);
    QCOMPARE(editor.model()->headerData(0, Qt::Horizontal).toString(),
        QStringLiteral("English"));
    QCOMPARE(editor.model()->headerData(155, Qt::Horizontal).toString(),
        QStringLiteral("Custom 150"));
    QCOMPARE(editor.model()->columnWidth(155), 239);
    const QLabel* const overflow = editor.findChild<QLabel*>(
        QStringLiteral("nextRosterOverflowNotice"));
    QVERIFY(overflow);
    QVERIFY(!overflow->isHidden());
    QVERIFY(overflow->text().contains(QStringLiteral("row 25")));
    auto* const importScores = editor.findChild<QPushButton*>(
        QStringLiteral("nextRosterImportScores"));
    auto* const transferStudent = editor.findChild<QPushButton*>(
        QStringLiteral("nextRosterTransferStudent"));
    QVERIFY(importScores && transferStudent);
    QVERIFY(!importScores->isHidden());
    QVERIFY(!transferStudent->isHidden());

    editor.show();
    QApplication::processEvents();
    editor.tableView()->edit(editor.model()->index(0, 0));
    QTRY_VERIFY(editor.tableView()->findChild<QLineEdit*>());
    auto visibleEditors = [&editor] {
        const auto edits = editor.findChildren<QLineEdit*>();
        return std::count_if(edits.cbegin(), edits.cend(), [](const QLineEdit* edit) {
            return edit->isVisible();
        });
    };
    QTRY_COMPARE(visibleEditors(), 1);
    QCOMPARE(editor.tableView()->currentIndex(), editor.model()->index(0, 0));

    QLineEdit* const firstEditor = editor.tableView()->findChild<QLineEdit*>();
    QVERIFY(firstEditor);
    firstEditor->setText(QStringLiteral("Edited A"));
    QTest::keyClick(firstEditor, Qt::Key_Return);
    QTRY_COMPARE(visibleEditors(), 0);
    editor.tableView()->setCurrentIndex(editor.model()->index(1, 0));
    editor.tableView()->edit(editor.model()->index(1, 0));
    QTRY_COMPARE(visibleEditors(), 1);
    QCOMPARE(editor.tableView()->currentIndex(), editor.model()->index(1, 0));
    QCOMPARE(editor.model()->data(editor.model()->index(0, 0)).toString(),
        QStringLiteral("Edited A"));

    ActionProbe testingProbe;
    testingProbe.snapshot = ordinarySnapshot();
    Ui::RosterEditorWidget testingEditor(fakeActions(testingProbe));
    testingEditor.enterClassSection(QStringLiteral("testing.tps"),
        typedClassId(12), true);
    QVERIFY(testingEditor.tableView()->isColumnHidden(2));
    QVERIFY(!testingEditor.tableView()->isColumnHidden(0));
    auto* const testingImportScores = testingEditor.findChild<QPushButton*>(
        QStringLiteral("nextRosterImportScores"));
    auto* const testingTransferStudent = testingEditor.findChild<QPushButton*>(
        QStringLiteral("nextRosterTransferStudent"));
    QVERIFY(testingImportScores && testingTransferStudent);
    QVERIFY(testingImportScores->isHidden());
    QVERIFY(testingTransferStudent->isHidden());

    editor.clear();
    QVERIFY(!editor.isLoaded());
    QCOMPARE(editor.model()->rowCount(), 25);
    QCOMPARE(editor.model()->columnCount(), 6);
    QVERIFY(editor.model()->data(editor.model()->index(0, 0)).toString().isEmpty());
}

void NextRosterEditorTests::emptyRosterAndReadFailureHaveDifferentStatesAndNoFailureWrite()
{
    ActionProbe probe;
    probe.snapshot = {};
    Ui::RosterEditorWidget editor(fakeActions(probe));
    editor.enterClassSection(QStringLiteral("empty.tps"), typedClassId(20));
    QVERIFY(editor.isLoaded());
    QVERIFY(!editor.readFailed());
    QVERIFY(editor.tableView()->isEnabled());
    QCOMPARE(editor.model()->rowCount(), 25);
    QCOMPARE(editor.model()->columnCount(), 6);

    probe.failRead = true;
    editor.clear();
    editor.enterClassSection(QStringLiteral("unreadable.tps"), typedClassId(20));
    QVERIFY(!editor.isLoaded());
    QVERIFY(editor.readFailed());
    QVERIFY(!editor.tableView()->isEnabled());
    QVERIFY(!editor.saveChanges(false));
    QCOMPARE(probe.saveCount, 0);
    QVERIFY(editor.findChild<QLabel*>(QStringLiteral("nextRosterStatus"))
        ->text().contains(QStringLiteral("could not be loaded")));
}

void NextRosterEditorTests::automaticManualAndFailedSaveRetryRetainLatestDraft()
{
    ActionProbe automaticProbe;
    Ui::RosterEditorWidget automatic(
        fakeActions(automaticProbe, Application::SaveMode::Automatic));
    automatic.enterClassSection(QStringLiteral("automatic.tps"), typedClassId(1));
    QVERIFY(automatic.model()->setData(automatic.model()->index(0, 0),
        QStringLiteral("Automatic Edit")));
    QTRY_COMPARE(automaticProbe.saveCount, 1);
    QVERIFY(!automatic.hasUnsavedChanges());

    ActionProbe manualProbe;
    Ui::RosterEditorWidget manual(
        fakeActions(manualProbe, Application::SaveMode::Manual));
    manual.enterClassSection(QStringLiteral("manual.tps"), typedClassId(2));
    QVERIFY(manual.model()->setData(manual.model()->index(0, 0),
        QStringLiteral("Manual Edit")));
    QTest::qWait(900);
    QCOMPARE(manualProbe.saveCount, 0);
    QVERIFY(manual.hasUnsavedChanges());
    QVERIFY(manual.saveChanges(true));
    QCOMPARE(manualProbe.saveCount, 1);
    QVERIFY(!manual.hasUnsavedChanges());

    ActionProbe failedProbe;
    failedProbe.failSaveCount = 2;
    Ui::RosterEditorWidget failed(
        fakeActions(failedProbe, Application::SaveMode::Automatic));
    failed.enterClassSection(QStringLiteral("retry.tps"), typedClassId(3));
    QVERIFY(failed.model()->setData(failed.model()->index(0, 0),
        QStringLiteral("First Draft")));
    QTRY_COMPARE(failedProbe.saveCount, 1);
    QVERIFY(failed.hasUnsavedChanges());
    auto* const retry = failed.findChild<QPushButton*>(
        QStringLiteral("nextRosterRetry"));
    QVERIFY(retry);
    QVERIFY(!retry->isHidden());
    QVERIFY(failed.model()->setData(failed.model()->index(0, 0),
        QStringLiteral("Latest Draft")));
    QTRY_COMPARE(failedProbe.saveCount, 2);
    QVERIFY(failed.hasUnsavedChanges());
    QCOMPARE(QString::fromStdU16String(
        failedProbe.lastSave->roster.rows[0][0]), QStringLiteral("Latest Draft"));

    QTest::mouseClick(retry, Qt::LeftButton);
    QTRY_COMPARE(failedProbe.saveCount, 3);
    QVERIFY(failedProbe.lastSave.has_value());
    QCOMPARE(QString::fromStdU16String(
        failedProbe.lastSave->roster.rows[0][0]), QStringLiteral("Latest Draft"));
    QVERIFY(!failed.hasUnsavedChanges());
    QVERIFY(retry->isHidden());
}

void NextRosterEditorTests::discardReloadsVisibleWindowAndClearsDraft()
{
    ActionProbe probe;
    Ui::RosterEditorWidget editor(fakeActions(probe));
    editor.enterClassSection(QStringLiteral("discard.tps"), typedClassId(30));
    QVERIFY(editor.model()->setData(editor.model()->index(0, 0),
        QStringLiteral("Unsaved Draft")));
    QVERIFY(editor.hasUnsavedChanges());
    QCOMPARE(editor.model()->data(editor.model()->index(0, 0)).toString(),
        QStringLiteral("Unsaved Draft"));

    QVERIFY(editor.discardChanges());
    QCOMPARE(probe.readCount, 2);
    QVERIFY(!editor.hasUnsavedChanges());
    QCOMPARE(editor.model()->data(editor.model()->index(0, 0)).toString(),
        QStringLiteral("Student A"));
    QCOMPARE(probe.saveCount, 0);
}

void NextRosterEditorTests::structuredCellIssuesLocalizeAndQuestionableKoreanRequiresConsent()
{
    ActionProbe probe;
    Ui::RosterEditorWidget editor(fakeActions(probe));
    editor.enterClassSection(QStringLiteral("validation.tps"), typedClassId(4));
    const QModelIndex english = editor.model()->index(0, 0);
    const QModelIndex korean = editor.model()->index(0, 1);
    QVERIFY(editor.model()->setData(english, QStringLiteral("Student!")));
    const auto& invalidIssues = editor.model()->validationIssues();
    QVERIFY(std::any_of(invalidIssues.cbegin(), invalidIssues.cend(), [](const auto& issue) {
        return issue.code == Application::RosterSaveIssueCode::StudentNameEnglishInvalidCharacters
            && issue.row.rawValue() == 0 && issue.column.rawValue() == 0;
    }));
    QVERIFY(editor.model()->data(english, Qt::ToolTipRole).toString()
        .contains(QStringLiteral("unsupported character")));
    QVERIFY(!editor.saveChanges(true));
    QCOMPARE(probe.saveCount, 0);
    QVERIFY(editor.hasUnsavedChanges());

    QVERIFY(editor.model()->setData(english, QStringLiteral("Student A")));
    QVERIFY(editor.model()->setData(korean, QStringLiteral("\uAE40")));
    const auto& koreanIssues = editor.model()->validationIssues();
    QVERIFY(std::any_of(koreanIssues.cbegin(), koreanIssues.cend(), [](const auto& issue) {
        return issue.code == Application::RosterSaveIssueCode::StudentNameKoreanTooShort
            && issue.row.rawValue() == 0 && issue.column.rawValue() == 1;
    }));
    const QString koreanMessage = editor.model()->data(korean, Qt::ToolTipRole).toString();
    QVERIFY(koreanMessage.contains(QStringLiteral("one syllable")));

    QTranslator translator;
    const QString translations = QStringLiteral(CLASSMNGR_NEXT_TEST_TRANSLATION_DIRECTORY);
    QVERIFY2(translator.load(QDir(translations).filePath(
        QStringLiteral("ClassMngr_ko_KR.qm"))),
        qPrintable(QStringLiteral("Could not load Korean roster translations from %1")
            .arg(translations)));
    QCoreApplication::installTranslator(&translator);
    editor.model()->retranslate();
    const QString translatedMessage = editor.model()->data(
        korean, Qt::ToolTipRole).toString();
    QCOMPARE(translatedMessage, QCoreApplication::translate(
        "NextRosterEditor", "The Korean name has one syllable. Check that it is correct."));
    QVERIFY(translatedMessage != koreanMessage);
    QCoreApplication::removeTranslator(&translator);
    editor.model()->retranslate();

    answerNextMessageBox(QMessageBox::Cancel);
    QVERIFY(!editor.saveChanges(true));
    QCOMPARE(probe.saveCount, 0);
    QVERIFY(editor.hasUnsavedChanges());
    answerNextMessageBox(QMessageBox::Save);
    QVERIFY(editor.saveChanges(true));
    QCOMPARE(probe.saveCount, 1);
    QVERIFY(probe.lastSave->allowQuestionableKoreanNameLengths);
    QVERIFY(!editor.hasUnsavedChanges());
}

void NextRosterEditorTests::exactWorkspacePathsPersistWidthsAndKeepSparseTailIndexes()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString pathA = directory.filePath(QStringLiteral("alpha.tps"));
    const QString pathB = directory.filePath(QStringLiteral("beta.tps"));
    QStringList customColumns;
    for (int index = 1; index <= 150; ++index)
    {
        customColumns.push_back(QStringLiteral("Custom %1").arg(index));
    }
    int classA = 0;
    int classB = 0;
    QString error;
    QVERIFY2(seedWorkspace(pathA, QStringLiteral("Same ID A"), customColumns,
        37, 155, QStringLiteral("Alpha tail"), &classA, &error), qPrintable(error));
    QVERIFY2(seedWorkspace(pathB, QStringLiteral("Same ID B"), customColumns,
        37, 155, QStringLiteral("Beta tail"), &classB, &error), qPrintable(error));
    QCOMPARE(classA, classB);

    Ui::RosterEditorActions actions;
    actions.saveMode = Application::SaveMode::Manual;
    actions.readRoster = Platform::readRosterWindowAtPath;
    actions.saveRoster = Platform::saveRosterAtPath;
    actions.columnHasTailData = Platform::rosterColumnHasTailDataAtPath;
    Ui::RosterEditorWidget editor(std::move(actions));
    editor.enterClassSection(pathA, typedClassId(classA));
    QVERIFY(editor.isLoaded());
    QCOMPARE(editor.model()->rowCount(), 25);
    QCOMPARE(editor.model()->columnCount(), 156);
    QCOMPARE(editor.model()->columnName(155), QStringLiteral("Custom 150"));
    QCOMPARE(editor.model()->columnWidth(155), 119);
    const QLabel* const overflow = editor.findChild<QLabel*>(
        QStringLiteral("nextRosterOverflowNotice"));
    QVERIFY(overflow && !overflow->isHidden());

    editor.show();
    QApplication::processEvents();
    editor.tableView()->setColumnWidth(155, 173);
    QVERIFY(editor.model()->setData(editor.model()->index(0, 0),
        QStringLiteral("Edited Alpha")));
    QVERIFY(editor.model()->moveRowData(0, 1));
    QVERIFY(editor.model()->removeRowData(24));
    editor.tableView()->setCurrentIndex(editor.model()->index(0, 6));
    QPushButton* const removeColumn = editor.findChild<QPushButton*>(
        QStringLiteral("nextRosterRemoveColumn"));
    QVERIFY(removeColumn && removeColumn->isEnabled());
    answerNextMessageBox(QMessageBox::Yes);
    QTest::mouseClick(removeColumn, Qt::LeftButton);
    QCOMPARE(editor.model()->columnCount(), 155);
    QVERIFY(editor.hasUnsavedChanges());
    QVERIFY(editor.saveChanges(false));
    QVERIFY(!editor.hasUnsavedChanges());

    const auto persistedA = Platform::readRosterWindowAtPath(
        pathA, typedClassId(classA));
    QVERIFY(persistedA);
    QVERIFY(persistedA.value().hasRowsOutsideWindow);
    QCOMPARE(persistedA.value().roster.rows[0][0], std::u16string(u"Student B"));
    QCOMPARE(persistedA.value().roster.rows[1][0], std::u16string(u"Edited Alpha"));
    QCOMPARE(persistedA.value().roster.columnWidths[154].rawValue(), 173);
    QCOMPARE(storedCell(pathA, classA, 37, 154),
        std::optional<QString>(QStringLiteral("Alpha tail")));
    QCOMPARE(storedCell(pathB, classB, 37, 155),
        std::optional<QString>(QStringLiteral("Beta tail")));
    const auto persistedB = Platform::readRosterWindowAtPath(
        pathB, typedClassId(classB));
    QVERIFY(persistedB);
    QCOMPARE(persistedB.value().roster.rows[0][0], std::u16string(u"Student A"));
    QCOMPARE(persistedB.value().roster.columnWidths[155].rawValue(), 119);
}

void NextRosterEditorTests::customColumnTailDataRequiresTwoStageConfirmation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("tail-column.tps"));
    int classId = 0;
    QString error;
    QVERIFY2(seedWorkspace(path, QStringLiteral("Tail column"),
        {QStringLiteral("Tail Note")}, 40, 6, QStringLiteral("Keep until confirmed"),
        &classId, &error), qPrintable(error));

    ActionProbe unused;
    Ui::RosterEditorActions actions;
    actions.saveMode = Application::SaveMode::Manual;
    actions.readRoster = Platform::readRosterWindowAtPath;
    actions.saveRoster = [&unused, path](const QString& actualPath,
        const Application::RosterSaveRequest& request) {
        ++unused.saveCount;
        unused.lastSavePath = actualPath;
        unused.lastSave = request;
        return Platform::saveRosterAtPath(actualPath, request);
    };
    actions.columnHasTailData = Platform::rosterColumnHasTailDataAtPath;
    Ui::RosterEditorWidget editor(std::move(actions));
    editor.enterClassSection(path, typedClassId(classId));
    QCOMPARE(editor.model()->columnCount(), 7);
    editor.tableView()->setCurrentIndex(editor.model()->index(0, 6));
    QPushButton* const remove = editor.findChild<QPushButton*>(
        QStringLiteral("nextRosterRemoveColumn"));
    QVERIFY(remove && remove->isEnabled());

    answerNextMessageBox(QMessageBox::Yes, QMessageBox::Cancel);
    QTest::mouseClick(remove, Qt::LeftButton);
    QCOMPARE(editor.model()->columnCount(), 7);
    QVERIFY(!editor.hasUnsavedChanges());
    QCOMPARE(unused.saveCount, 0);
    QCOMPARE(storedCell(path, classId, 40, 6),
        std::optional<QString>(QStringLiteral("Keep until confirmed")));

    answerNextMessageBox(QMessageBox::Yes, QMessageBox::Yes);
    QTest::mouseClick(remove, Qt::LeftButton);
    QCOMPARE(editor.model()->columnCount(), 6);
    QVERIFY(editor.hasUnsavedChanges());
    QVERIFY(editor.saveChanges(false));
    QCOMPARE(unused.saveCount, 1);
    QVERIFY(unused.lastSave.has_value());
    QCOMPARE(unused.lastSave->removedCustomColumns,
        (std::vector<std::u16string>{u"Tail Note"}));
    QVERIFY(!storedCell(path, classId, 40, 6).has_value());
}

void NextRosterEditorTests::
normalizedHeaderTailDataRequiresCancelOrExplicitDeletion()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("normalized-tail.tps"));
    int classId = 0;
    QString error;
    QVERIFY2(seedWorkspace(path, QStringLiteral("Normalized tail"),
        {QStringLiteral("Original")}, 40, 6, QStringLiteral("Do not lose me"),
        &classId, &error), qPrintable(error));
    QVERIFY2(setRawHeader(path, classId, 6, QStringLiteral("My   Notes"), &error),
        qPrintable(error));

    Ui::RosterEditorActions actions;
    actions.saveMode = Application::SaveMode::Manual;
    actions.readRoster = Platform::readRosterWindowAtPath;
    actions.saveRoster = Platform::saveRosterAtPath;
    actions.columnHasTailData = Platform::rosterColumnHasTailDataAtPath;
    Ui::RosterEditorWidget editor(std::move(actions));
    editor.enterClassSection(path, typedClassId(classId));
    QCOMPARE(editor.model()->columnName(6), QStringLiteral("My Notes"));
    editor.tableView()->setCurrentIndex(editor.model()->index(0, 6));
    QPushButton* const remove = editor.findChild<QPushButton*>(
        QStringLiteral("nextRosterRemoveColumn"));
    QVERIFY(remove && remove->isEnabled());

    answerNextMessageBox(QMessageBox::Yes, QMessageBox::Cancel);
    QTest::mouseClick(remove, Qt::LeftButton);
    QCOMPARE(editor.model()->columnCount(), 7);
    QVERIFY(!editor.hasUnsavedChanges());
    QCOMPARE(storedCell(path, classId, 40, 6),
        std::optional<QString>(QStringLiteral("Do not lose me")));

    answerNextMessageBox(QMessageBox::Yes, QMessageBox::Yes);
    QTest::mouseClick(remove, Qt::LeftButton);
    QCOMPARE(editor.model()->columnCount(), 6);
    QVERIFY(editor.hasUnsavedChanges());
    QVERIFY(editor.saveChanges(false));
    QCOMPARE(storedCell(path, classId, 40, 6), std::nullopt);
}

void NextRosterEditorTests::ambiguousNormalizedDuplicateHeadersFailClosed()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("duplicate-header.tps"));
    int classId = 0;
    QString error;
    QVERIFY2(seedWorkspace(path, QStringLiteral("Ambiguous headers"),
        {QStringLiteral("First"), QStringLiteral("Second")},
        40, 6, QStringLiteral("First tail"), &classId, &error), qPrintable(error));
    QVERIFY2(setRawHeader(path, classId, 6, QStringLiteral("My   Notes"), &error),
        qPrintable(error));
    QVERIFY2(setRawHeader(path, classId, 7, QStringLiteral(" My Notes "), &error),
        qPrintable(error));
    {
        WorkspaceStore store;
        QVERIFY(store.openExisting(path));
        QSqlQuery insert(store.database());
        insert.prepare(QStringLiteral(
            "INSERT INTO roster_data (class_id,row_index,col_index,value) "
            "VALUES (?,40,7,?)"));
        insert.addBindValue(classId);
        insert.addBindValue(QStringLiteral("Second tail"));
        QVERIFY2(insert.exec(), qPrintable(insert.lastError().text()));
    }

    Ui::RosterEditorActions actions;
    actions.saveMode = Application::SaveMode::Manual;
    actions.readRoster = Platform::readRosterWindowAtPath;
    actions.saveRoster = Platform::saveRosterAtPath;
    actions.columnHasTailData = Platform::rosterColumnHasTailDataAtPath;
    Ui::RosterEditorWidget editor(std::move(actions));
    editor.enterClassSection(path, typedClassId(classId));
    QCOMPARE(editor.model()->columnCount(), 7);
    QCOMPARE(editor.model()->columnName(6), QStringLiteral("My Notes"));
    editor.tableView()->setCurrentIndex(editor.model()->index(0, 6));
    QPushButton* const remove = editor.findChild<QPushButton*>(
        QStringLiteral("nextRosterRemoveColumn"));
    QVERIFY(remove && remove->isEnabled());

    answerNextMessageBox(QMessageBox::Yes, QMessageBox::Yes);
    QTest::mouseClick(remove, Qt::LeftButton);
    QVERIFY(!editor.hasUnsavedChanges());
    QCOMPARE(editor.model()->columnCount(), 7);
    const auto ambiguousTail = Platform::rosterColumnHasTailDataAtPath(
        path, typedClassId(classId), u"My Notes");
    QVERIFY(!ambiguousTail);
    QCOMPARE(ambiguousTail.error().code, Domain::ErrorCode::Validation);
    QCOMPARE(storedCell(path, classId, 40, 6),
        std::optional<QString>(QStringLiteral("First tail")));
    QCOMPARE(storedCell(path, classId, 40, 7),
        std::optional<QString>(QStringLiteral("Second tail")));
    const auto persisted = Platform::readRosterWindowAtPath(path, typedClassId(classId));
    QVERIFY(persisted);
    QCOMPARE(persisted.value().roster.columns.size(), std::size_t(8));
    QCOMPARE(editor.model()->columnCount(), 7);
}

void NextRosterEditorTests::pathActionsRejectMalformedIdsBeforeOpeningWorkspace()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString missingPath = directory.filePath(QStringLiteral("missing.tps"));
    const Domain::ClassId malformed = *Domain::ClassId::fromString("not-a-number");

    const auto read = Platform::readRosterWindowAtPath(missingPath, malformed);
    QVERIFY(!read);
    QCOMPARE(read.error().code, Domain::ErrorCode::InvalidInput);
    QVERIFY(QString::fromStdString(read.error().message).contains(
        QStringLiteral("Class ID must be a canonical positive integer")));

    Application::RosterSaveRequest request{
        .classId = malformed,
        .roster = ordinarySnapshot()
    };
    const auto saved = Platform::saveRosterAtPath(missingPath, request);
    QVERIFY(!saved);
    QCOMPARE(saved.error().code, Domain::ErrorCode::InvalidInput);
    QVERIFY(QString::fromStdString(saved.error().message).contains(
        QStringLiteral("Class ID must be a canonical positive integer")));
    QVERIFY(!QFile::exists(missingPath));
}

void NextRosterEditorTests::scoreImportMapsTrimmedPairsAndIsIdempotent()
{
    ActionProbe probe;
    probe.snapshot.rows[0][0] = u" Student A ";
    probe.snapshot.rows[0][1] = u" \uAE40\uBBFC\uC9C0 ";
    probe.importedScores = {
        {
            {u" Student A ", u" \uAE40\uBBFC\uC9C0 ", u"A"},
            {u"Student A", u"\uAE40\uBBFC\uC9C0", u"B+"}
        },
        {{u"Student A", u"\uAE40\uBBFC\uC9C0", u"A+"}},
        {{u"Student B", u"\uBC15\uC9C0\uBBFC", u"C"}},
        {}
    };
    Ui::RosterEditorWidget editor(fakeActions(probe));
    editor.enterClassSection(QStringLiteral("profile-a.tps"), typedClassId(12));
    auto* const importButton = editor.findChild<QPushButton*>(
        QStringLiteral("nextRosterImportScores"));
    QVERIFY(importButton);

    importButton->click();

    QCOMPARE(probe.scoreReadCount, 1);
    QCOMPARE(probe.lastScoreReadPath, QStringLiteral("profile-a.tps"));
    QCOMPARE(probe.requestedEvaluations.size(), std::size_t(4));
    QCOMPARE(QString::fromStdU16String(probe.requestedEvaluations[0]),
        QStringLiteral("Winter"));
    QCOMPARE(QString::fromStdU16String(probe.requestedEvaluations[1]),
        QStringLiteral("Speech Contest"));
    QCOMPARE(QString::fromStdU16String(probe.requestedEvaluations[2]),
        QStringLiteral("Summer"));
    QCOMPARE(QString::fromStdU16String(probe.requestedEvaluations[3]),
        QStringLiteral("Fall"));
    QCOMPARE(editor.model()->index(0, 2).data().toString(), QStringLiteral("B+"));
    QCOMPARE(editor.model()->index(0, 3).data().toString(), QStringLiteral("A+"));
    QCOMPARE(editor.model()->index(1, 4).data().toString(), QStringLiteral("C"));
    QCOMPARE(editor.model()->index(0, 5).data().toString(), QString{});
    QVERIFY(editor.hasUnsavedChanges());

    QVERIFY(editor.saveChanges());
    QCOMPARE(probe.saveCount, 1);
    importButton->click();
    QCOMPARE(editor.model()->index(0, 2).data().toString(), QStringLiteral("B+"));
    QCOMPARE(probe.saveCount, 1);
    QVERIFY(!editor.hasUnsavedChanges());
    auto* const status = editor.findChild<QLabel*>(QStringLiteral("nextRosterStatus"));
    QVERIFY(status);
    QVERIFY(status->text().contains(QStringLiteral("already up to date")));
}

void NextRosterEditorTests::
scoreImportRequiresNameColumnsAndUsesAvailableGradeColumns()
{
    const std::array<QStringList, 2> incompleteColumns{{
        {QStringLiteral("English"), QStringLiteral("Winter")},
        {QStringLiteral("Korean"), QStringLiteral("Winter")}
    }};
    for (const QStringList& columns : incompleteColumns)
    {
        ActionProbe probe;
        probe.snapshot.columns.clear();
        probe.snapshot.columnWidths.clear();
        for (const QString& column : columns)
        {
            probe.snapshot.columns.push_back(column.toStdU16String());
            probe.snapshot.columnWidths.push_back(
                Application::RosterSnapshotColumnWidth{120});
        }
        for (auto& row : probe.snapshot.rows)
        {
            row.resize(columns.size());
        }

        Ui::RosterEditorWidget editor(fakeActions(probe));
        editor.enterClassSection(
            QStringLiteral("missing-name-column.tps"), typedClassId(12));
        const Application::RosterSnapshot before = editor.model()->snapshot();
        editor.findChild<QPushButton*>(
            QStringLiteral("nextRosterImportScores"))->click();

        QVERIFY(editor.model()->snapshot() == before);
        QVERIFY(!editor.hasUnsavedChanges());
        QCOMPARE(probe.scoreReadCount, 0);
        auto* const status = editor.findChild<QLabel*>(
            QStringLiteral("nextRosterStatus"));
        QVERIFY(status);
        QVERIFY(status->text().contains(QStringLiteral("English")));
        QVERIFY(status->text().contains(QStringLiteral("Korean")));
    }

    ActionProbe optional;
    optional.snapshot.columns = {u"English", u"Korean", u"Winter", u"Summer"};
    optional.snapshot.columnWidths.resize(4);
    for (auto& row : optional.snapshot.rows)
    {
        row.resize(4);
    }
    optional.importedScores = {
        {{u"Student A", u"\uAE40\uBBFC\uC9C0", u"A+"}},
        {{u"Student B", u"\uBC15\uC9C0\uBBFC", u"B"}}
    };
    Ui::RosterEditorWidget editor(fakeActions(optional));
    editor.enterClassSection(QStringLiteral("optional-grades.tps"), typedClassId(12));
    editor.findChild<QPushButton*>(
        QStringLiteral("nextRosterImportScores"))->click();

    QCOMPARE(optional.scoreReadCount, 1);
    QCOMPARE(optional.requestedEvaluations.size(), std::size_t(2));
    QCOMPARE(QString::fromStdU16String(optional.requestedEvaluations[0]),
        QStringLiteral("Winter"));
    QCOMPARE(QString::fromStdU16String(optional.requestedEvaluations[1]),
        QStringLiteral("Summer"));
    QCOMPARE(editor.model()->index(0, 2).data().toString(), QStringLiteral("A+"));
    QCOMPARE(editor.model()->index(0, 3).data().toString(), QString{});
    QCOMPARE(editor.model()->index(1, 4).data().toString(), QStringLiteral("B"));
    QCOMPARE(editor.model()->index(0, 5).data().toString(), QString{});
    QVERIFY(editor.hasUnsavedChanges());
}

void NextRosterEditorTests::
scoreImportReadFailureDoesNotMutateAndAutosaveFailureRetries()
{
    ActionProbe readFailure;
    readFailure.failScoreRead = true;
    Ui::RosterEditorWidget readFailureEditor(fakeActions(readFailure));
    readFailureEditor.enterClassSection(
        QStringLiteral("profile-a.tps"), typedClassId(12));
    const Application::RosterSnapshot before =
        readFailureEditor.model()->snapshot();
    readFailureEditor.findChild<QPushButton*>(
        QStringLiteral("nextRosterImportScores"))->click();
    QVERIFY(readFailureEditor.model()->snapshot() == before);
    QVERIFY(!readFailureEditor.hasUnsavedChanges());
    QCOMPARE(readFailure.saveCount, 0);
    auto* const status = readFailureEditor.findChild<QLabel*>(
        QStringLiteral("nextRosterStatus"));
    QVERIFY(status);
    QVERIFY(status->text().contains(QStringLiteral("could not be loaded")));
    QVERIFY(!status->text().contains(QStringLiteral("already up to date")));

    ActionProbe retry;
    retry.failSaveCount = 1;
    Ui::RosterEditorWidget automaticEditor(
        fakeActions(retry, Application::SaveMode::Automatic));
    automaticEditor.enterClassSection(
        QStringLiteral("automatic.tps"), typedClassId(12));
    automaticEditor.findChild<QPushButton*>(
        QStringLiteral("nextRosterImportScores"))->click();
    auto* const timer = automaticEditor.findChild<QTimer*>(
        QStringLiteral("nextRosterAutosaveTimer"));
    QVERIFY(timer);
    QVERIFY(timer->isActive());
    QTRY_COMPARE(retry.saveCount, 1);
    QVERIFY(automaticEditor.hasUnsavedChanges());
    auto* const retryButton = automaticEditor.findChild<QPushButton*>(
        QStringLiteral("nextRosterRetry"));
    QVERIFY(retryButton);
    QVERIFY(!retryButton->isHidden());
    QVERIFY(retryButton->isEnabled());
    retryButton->click();
    QCOMPARE(retry.saveCount, 2);
    QVERIFY(!automaticEditor.hasUnsavedChanges());
}

void NextRosterEditorTests::
transferCancelAndFailuresPreserveDirtySelectionAndTimer()
{
    ActionProbe probe;
    Ui::RosterEditorWidget editor(
        fakeActions(probe, Application::SaveMode::Automatic));
    editor.enterClassSection(QStringLiteral("transfer.tps"), typedClassId(12));
    QVERIFY(editor.model()->setData(editor.model()->index(0, 0),
        QStringLiteral("Student C")));
    QVERIFY(editor.model()->setData(editor.model()->index(1, 1),
        QStringLiteral("\uAE40")));
    editor.tableView()->setCurrentIndex(editor.model()->index(1, 0));
    Application::RosterSnapshot before = editor.model()->snapshot();
    auto* const timer = editor.findChild<QTimer*>(
        QStringLiteral("nextRosterAutosaveTimer"));
    auto* const transferButton = editor.findChild<QPushButton*>(
        QStringLiteral("nextRosterTransferStudent"));
    QVERIFY(timer && timer->isActive());
    QVERIFY(transferButton && transferButton->isEnabled());

    cancelActiveTransferMenu();
    transferButton->click();
    QVERIFY(editor.model()->snapshot() == before);
    QVERIFY(editor.hasUnsavedChanges());
    QCOMPARE(editor.tableView()->currentIndex().row(), 1);
    QCOMPARE(editor.tableView()->currentIndex().column(), 0);
    QVERIFY(timer->isActive());
    QCOMPARE(probe.transferSaveCount, 0);

    chooseTransferMenuTarget(QStringLiteral("99"));
    answerNextMessageBox(QMessageBox::Cancel);
    transferButton->click();
    QVERIFY(editor.model()->snapshot() == before);
    QVERIFY(editor.hasUnsavedChanges());
    QCOMPARE(editor.tableView()->currentIndex().row(), 1);
    QCOMPARE(editor.tableView()->currentIndex().column(), 0);
    QVERIFY(timer->isActive());
    QCOMPARE(probe.transferSaveCount, 0);

    QVERIFY(editor.model()->setData(editor.model()->index(1, 1),
        QStringLiteral("\uBC15\uC9C0\uBBFC")));
    before = editor.model()->snapshot();

    probe.failTargetChoicesRead = true;
    transferButton->click();
    QVERIFY(editor.model()->snapshot() == before);
    QVERIFY(editor.hasUnsavedChanges());
    QCOMPARE(editor.tableView()->currentIndex().row(), 1);
    QCOMPARE(editor.tableView()->currentIndex().column(), 0);
    QVERIFY(timer->isActive());
    QCOMPARE(probe.transferSaveCount, 0);
    probe.failTargetChoicesRead = false;

    probe.failTargetRead = true;
    chooseTransferMenuTarget(QStringLiteral("99"));
    transferButton->click();
    QVERIFY(editor.model()->snapshot() == before);
    QVERIFY(editor.hasUnsavedChanges());
    QCOMPARE(editor.tableView()->currentIndex().row(), 1);
    QCOMPARE(editor.tableView()->currentIndex().column(), 0);
    QVERIFY(timer->isActive());
    QCOMPARE(probe.transferSaveCount, 0);
    probe.failTargetRead = false;

    probe.targetSnapshot = emptyRosterSnapshot();
    for (std::size_t row = 0; row < probe.targetSnapshot.rows.size(); ++row)
    {
        probe.targetSnapshot.rows[row][0] =
            QStringLiteral("Existing %1").arg(row + 1).toStdU16String();
        probe.targetSnapshot.rows[row][1] = u"\uAE40\uBBFC\uC9C0";
    }
    chooseTransferMenuTarget(QStringLiteral("99"));
    transferButton->click();
    QVERIFY(editor.model()->snapshot() == before);
    QVERIFY(editor.hasUnsavedChanges());
    QCOMPARE(editor.tableView()->currentIndex().row(), 1);
    QCOMPARE(editor.tableView()->currentIndex().column(), 0);
    QVERIFY(timer->isActive());
    QCOMPARE(probe.transferSaveCount, 0);

    probe.targetSnapshot = emptyRosterSnapshot();
    probe.failTransferSave = true;
    chooseTransferMenuTarget(QStringLiteral("99"));
    transferButton->click();
    QVERIFY(editor.model()->snapshot() == before);
    QVERIFY(editor.hasUnsavedChanges());
    QCOMPARE(editor.tableView()->currentIndex().row(), 1);
    QVERIFY(timer->isActive());
    QCOMPARE(probe.transferSaveCount, 1);
    auto* const status = editor.findChild<QLabel*>(
        QStringLiteral("nextRosterStatus"));
    QVERIFY(status);
    QVERIFY(status->text().contains(QStringLiteral("could not be transferred")));

    probe.failTransferSave = false;
    probe.targetSnapshot = emptyRosterSnapshot();
    probe.targetSnapshot.rows[0][0] = u"Existing student";
    probe.targetSnapshot.rows[0][1] = u"\uAE40";
    chooseTransferMenuTarget(QStringLiteral("99"));
    answerNextMessageBox(QMessageBox::Cancel);
    transferButton->click();
    QVERIFY(editor.model()->snapshot() == before);
    QVERIFY(editor.hasUnsavedChanges());
    QCOMPARE(editor.tableView()->currentIndex().row(), 1);
    QVERIFY(timer->isActive());
    QCOMPARE(probe.transferSaveCount, 1);
}

void NextRosterEditorTests::transferSuccessRemovesSourceOnlyAfterPairSave()
{
    ActionProbe probe;
    Ui::RosterEditorWidget editor(fakeActions(probe));
    editor.enterClassSection(QStringLiteral("transfer.tps"), typedClassId(12));
    QVERIFY(editor.model()->setData(editor.model()->index(0, 1),
        QStringLiteral("\uAE40")));
    editor.tableView()->setCurrentIndex(editor.model()->index(0, 0));

    chooseTransferMenuTarget(QStringLiteral("99"));
    answerNextMessageBox(QMessageBox::Save, QMessageBox::Save);
    editor.findChild<QPushButton*>(
        QStringLiteral("nextRosterTransferStudent"))->click();

    QCOMPARE(probe.transferSaveCount, 1);
    QCOMPARE(probe.lastTargetChoicesPath, QStringLiteral("transfer.tps"));
    QCOMPARE(probe.lastTransferSavePath, QStringLiteral("transfer.tps"));
    QVERIFY(probe.lastTransferSave.has_value());
    QCOMPARE(QString::fromStdU16String(
        probe.lastTransferSave->sourceRoster.rows[0][0]),
        QStringLiteral("Student B"));
    QCOMPARE(QString::fromStdU16String(
        probe.lastTransferSave->targetRoster.rows[0][0]),
        QStringLiteral("Student A"));
    QCOMPARE(QString::fromStdU16String(editor.model()->snapshot().rows[0][0]),
        QStringLiteral("Student B"));
    QVERIFY(!editor.hasUnsavedChanges());
    auto* const timer = editor.findChild<QTimer*>(
        QStringLiteral("nextRosterAutosaveTimer"));
    QVERIFY(timer && !timer->isActive());
}

void NextRosterEditorTests::
transferMenuFiltersOtherGradesAndDisablesFullTargets()
{
    ActionProbe probe;
    probe.transferTargetFull = true;
    probe.includeDifferentGradeTransferTarget = true;
    Ui::RosterEditorWidget editor(fakeActions(probe));
    editor.enterClassSection(QStringLiteral("transfer.tps"), typedClassId(12));
    editor.tableView()->setCurrentIndex(editor.model()->index(0, 0));

    std::vector<TransferMenuActionObservation> menuActions;
    observeActiveTransferMenu(&menuActions);
    editor.findChild<QPushButton*>(
        QStringLiteral("nextRosterTransferStudent"))->click();

    QCOMPARE(menuActions.size(), std::size_t(1));
    QCOMPARE(menuActions[0].classId, QStringLiteral("99"));
    QVERIFY(menuActions[0].text.endsWith(QStringLiteral("(full)")));
    QVERIFY(!menuActions[0].enabled);
    QCOMPARE(menuActions[0].toolTip,
        QStringLiteral("Target roster is full."));
    QCOMPARE(probe.transferSaveCount, 0);
}

void NextRosterEditorTests::
pathRosterWorkflowActionsStayScopedAndPairSaveIsAtomic()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString profileA = directory.filePath(QStringLiteral("profile-a.tps"));
    const QString profileB = directory.filePath(QStringLiteral("profile-b.tps"));
    const QString rollbackPath = directory.filePath(QStringLiteral("rollback.tps"));
    const QString fullPath = directory.filePath(QStringLiteral("full.tps"));
    TransferWorkspaceIds idsA;
    TransferWorkspaceIds idsB;
    TransferWorkspaceIds rollbackIds;
    TransferWorkspaceIds fullIds;
    QString error;
    QVERIFY2(seedRosterTransferWorkspace(profileA, QStringLiteral("Grade 3"),
        QStringLiteral("Grade 3"), {QStringLiteral("Portfolio")}, false,
        QStringLiteral("A+"), &idsA, &error), qPrintable(error));
    QVERIFY2(seedRosterTransferWorkspace(profileB, QStringLiteral("Grade 4"),
        QStringLiteral("Grade 5"), {QStringLiteral("Portfolio")}, false,
        QStringLiteral("C"), &idsB, &error), qPrintable(error));
    QVERIFY2(seedRosterTransferWorkspace(rollbackPath, QStringLiteral("Grade 3"),
        QStringLiteral("Grade 3"),
        {QStringLiteral("One"), QStringLiteral("Two")}, false,
        QStringLiteral("A"), &rollbackIds, &error), qPrintable(error));
    QVERIFY2(seedRosterTransferWorkspace(fullPath, QStringLiteral("Grade 3"),
        QStringLiteral("Grade 3"), {}, true, QStringLiteral("A"),
        &fullIds, &error), qPrintable(error));
    QCOMPARE(idsA.source, idsB.source);
    QCOMPARE(idsA.target, idsB.target);

    {
        WorkspaceStore store;
        QVERIFY2(store.openExisting(profileA), qPrintable(error));
        Teacher teacher;
        teacher.teacherKr = QStringLiteral("\uAD50\uC0AC");
        teacher.teacherEn = QStringLiteral("Teacher English");
        teacher.preferredRomanization = QStringLiteral("Teacher Romanization");
        teacher.preferredName = QStringLiteral("Preferred Class Teacher");
        const auto teacherId = store.teacherRepository()->createTeacher(teacher);
        QVERIFY(teacherId);

        QSqlQuery updateClassInfo(store.database());
        QVERIFY(updateClassInfo.prepare(QStringLiteral(
            "UPDATE class_info SET teacher_id=?, class_level='Level 2' "
            "WHERE class_id=?")));
        updateClassInfo.addBindValue(*teacherId);
        updateClassInfo.addBindValue(idsA.target);
        QVERIFY2(updateClassInfo.exec(),
            qPrintable(updateClassInfo.lastError().text()));

        QSqlQuery schedule(store.database());
        QVERIFY(schedule.prepare(QStringLiteral(
            "INSERT INTO class_times (class_id, day, start_time, end_time) "
            "VALUES (?, 'Monday', '4:00 PM', '4:50 PM'), "
            "(?, 'Wednesday', '4:00 PM', '4:50 PM'), "
            "(?, 'Friday', '4:00 PM', '4:50 PM')")));
        schedule.addBindValue(idsA.target);
        schedule.addBindValue(idsA.target);
        schedule.addBindValue(idsA.target);
        QVERIFY2(schedule.exec(), qPrintable(schedule.lastError().text()));
    }

    const auto scoresA = Platform::readSpeakingEvaluationRosterScoresAtPath(
        profileA, typedClassId(idsA.source), {u"Winter"});
    const auto scoresB = Platform::readSpeakingEvaluationRosterScoresAtPath(
        profileB, typedClassId(idsB.source), {u"Winter"});
    QVERIFY(scoresA && scoresB);
    QVERIFY(scoresA.value()[0] && scoresB.value()[0]);
    QCOMPARE(QString::fromStdU16String(
        scoresA.value()[0].value()[0].finalGrade), QStringLiteral("A+"));
    QCOMPARE(QString::fromStdU16String(
        scoresB.value()[0].value()[0].finalGrade), QStringLiteral("C"));

    const auto choicesA = Platform::readRosterTransferTargetChoicesAtPath(
        profileA, typedClassId(idsA.source));
    const auto choicesB = Platform::readRosterTransferTargetChoicesAtPath(
        profileB, typedClassId(idsB.source));
    QVERIFY(choicesA && choicesB);
    QCOMPARE(choicesA.value().candidates.size(), std::size_t(1));
    QCOMPARE(choicesA.value().candidates[0].classId,
        typedClassId(idsA.target));
    const auto& targetDisplay = choicesA.value().candidates[0].displayMetadata;
    QCOMPARE(QString::fromStdU16String(targetDisplay.grade),
        QStringLiteral("Grade 3"));
    QCOMPARE(QString::fromStdU16String(targetDisplay.level),
        QStringLiteral("Level 2"));
    QCOMPARE(QString::fromStdU16String(targetDisplay.teacherPreferredName),
        QStringLiteral("Preferred Class Teacher"));
    QCOMPARE(targetDisplay.regularSchedule.size(), std::size_t(3));
    const auto pathDisplayName = Application::formatRosterTransferTargetDisplayName(
        targetDisplay, u"Unknown Class", u"No Teacher");
    QVERIFY(pathDisplayName);
    QCOMPARE(QString::fromStdU16String(*pathDisplayName),
        QStringLiteral("Grade 3 Level 2 \u2022 Preferred Class Teacher \u2022 M/W/F (4:00)"));

    ClassInfo expectedClassInfo;
    expectedClassInfo.classGrade = QStringLiteral("Grade 3");
    expectedClassInfo.classLevel = QStringLiteral("Level 2");
    Teacher expectedTeacher;
    expectedTeacher.teacherEn = QStringLiteral("Teacher English");
    expectedTeacher.teacherKr = QStringLiteral("\uAD50\uC0AC");
    expectedTeacher.preferredRomanization =
        QStringLiteral("Teacher Romanization");
    expectedTeacher.preferredName = QStringLiteral("Preferred Class Teacher");
    for (const auto& scheduleRow : targetDisplay.regularSchedule)
    {
        expectedClassInfo.classTimes.append({
            QString::fromStdU16String(scheduleRow.day),
            QString::fromStdU16String(scheduleRow.startTime),
            QString{}
        });
    }
    QCOMPARE(QString::fromStdU16String(*pathDisplayName),
        SidebarNodeNaming::formatClassDisplayName(
            expectedClassInfo, expectedTeacher));
    QVERIFY(choicesB.value().candidates.empty());
    const auto fullChoices = Platform::readRosterTransferTargetChoicesAtPath(
        fullPath, typedClassId(fullIds.source));
    QVERIFY(fullChoices);
    QCOMPARE(fullChoices.value().candidates.size(), std::size_t(1));
    QVERIFY(fullChoices.value().candidates[0].full);

    const auto loadedSource = Platform::readRosterWindowAtPath(
        profileA, typedClassId(idsA.source));
    QVERIFY(loadedSource);
    std::vector<std::u16string> baseColumns;
    for (const QString& column : Roster::BaseColumns)
    {
        baseColumns.push_back(column.toStdU16String());
    }
    const Application::RosterRowTransferRequest transferRequest{
        .sourceClassId = typedClassId(idsA.source),
        .targetClassId = typedClassId(idsA.target),
        .sourceRoster = loadedSource.value().roster,
        .sourceRow = Application::RosterRowTransferSourceIndex{0},
        .baseColumnNames = baseColumns,
        .sourceQuestionableKoreanNameLengthsConfirmed = true
    };
    PathRosterWindowReadPort windowPort(profileA);
    const Application::RosterWindowAsRosterReadPort readPort(windowPort);
    PathRosterTransferSavePort savePort(profileA);
    const auto transferred = Application::RosterRowTransferUseCase::execute(
        transferRequest, readPort, savePort,
        [](const std::u16string_view left, const std::u16string_view right) {
            return QStringView(left.data(), static_cast<qsizetype>(left.size())).compare(
                QStringView(right.data(), static_cast<qsizetype>(right.size())),
                Qt::CaseInsensitive) == 0;
        });
    QVERIFY(std::holds_alternative<Application::RosterRowTransferSuccess>(
        transferred));
    QCOMPARE(storedCell(profileA, idsA.source, 0, 0).value(),
        QStringLiteral("Student B"));
    QCOMPARE(storedCell(profileA, idsA.target, 0, 0).value(),
        QStringLiteral("Student A"));
    QCOMPARE(storedCell(profileA, idsA.target, 0, 6).value(),
        QStringLiteral("Source portfolio"));
    QCOMPARE(storedCell(profileA, idsA.source, 37, 6).value(),
        QStringLiteral("Source tail"));
    QCOMPARE(storedCell(profileA, idsA.target, 41, 6).value(),
        QStringLiteral("Target tail"));
    QCOMPARE(storedCell(profileB, idsB.source, 0, 0).value(),
        QStringLiteral("Student A"));
    QVERIFY(!storedCell(profileB, idsB.target, 0, 0).has_value());

    QVERIFY2(setRawHeader(rollbackPath, rollbackIds.target, 7,
        QStringLiteral("One"), &error), qPrintable(error));
    const auto rollbackSource = Platform::readRosterWindowAtPath(
        rollbackPath, typedClassId(rollbackIds.source));
    const auto rollbackTarget = Platform::readRosterWindowAtPath(
        rollbackPath, typedClassId(rollbackIds.target));
    QVERIFY(rollbackSource && rollbackTarget);
    Application::RosterSnapshot changedSource = rollbackSource.value().roster;
    changedSource.rows[0][0] = u"Changed Source";
    const Domain::Result<void> rejectedPair = Platform::saveRosterTransferAtPath(
        rollbackPath, {
            .sourceClassId = typedClassId(rollbackIds.source),
            .targetClassId = typedClassId(rollbackIds.target),
            .sourceRoster = changedSource,
            .targetRoster = rollbackTarget.value().roster
        });
    QVERIFY(!rejectedPair);
    QCOMPARE(storedCell(rollbackPath, rollbackIds.source, 0, 0).value(),
        QStringLiteral("Student A"));
    QCOMPARE(storedCell(rollbackPath, rollbackIds.target, 41, 6).value(),
        QStringLiteral("Target tail"));
}

void NextRosterEditorTests::
transferTargetNamesMatchLegacyFormattingAndFallbacks()
{
    Application::RosterTransferTargetDisplayMetadata metadata{
        .grade = u" Grade 3 ",
        .level = u" Level 2 ",
        .teacherEnglishName = u"Teacher English",
        .teacherKoreanName = u"\uAD50\uC0AC",
        .teacherPreferredRomanization = u"Teacher Romanization",
        .teacherPreferredName = u" Preferred Teacher ",
        .regularSchedule = {
            {u"Monday", u"4:00 PM"},
            {u"Wednesday", u"4:00 PM"},
            {u"Friday", u"4:00 PM"}
        }
    };
    ClassInfo legacyClassInfo;
    legacyClassInfo.classGrade = QStringLiteral(" Grade 3 ");
    legacyClassInfo.classLevel = QStringLiteral(" Level 2 ");
    legacyClassInfo.classTimes = {
        {QStringLiteral("Monday"), QStringLiteral("4:00 PM"), QStringLiteral("4:50 PM")},
        {QStringLiteral("Wednesday"), QStringLiteral("4:00 PM"), QStringLiteral("4:50 PM")},
        {QStringLiteral("Friday"), QStringLiteral("4:00 PM"), QStringLiteral("4:50 PM")}
    };
    Teacher legacyTeacher;
    legacyTeacher.teacherEn = QStringLiteral("Teacher English");
    legacyTeacher.teacherKr = QStringLiteral("\uAD50\uC0AC");
    legacyTeacher.preferredRomanization = QStringLiteral("Teacher Romanization");
    legacyTeacher.preferredName = QStringLiteral(" Preferred Teacher ");

    const auto formatted = Application::formatRosterTransferTargetDisplayName(
        metadata, u"Unknown Class", u"No Teacher");
    QVERIFY(formatted);
    QCOMPARE(QString::fromStdU16String(*formatted),
        SidebarNodeNaming::formatClassDisplayName(legacyClassInfo, legacyTeacher));

    metadata = {};
    legacyClassInfo = {};
    legacyTeacher = {};
    const auto missingMetadata =
        Application::formatRosterTransferTargetDisplayName(
            metadata, u"Unknown Class", u"No Teacher");
    QVERIFY(missingMetadata);
    QCOMPARE(QString::fromStdU16String(*missingMetadata),
        SidebarNodeNaming::formatClassDisplayName(legacyClassInfo, legacyTeacher));
    QCOMPARE(QString::fromStdU16String(*missingMetadata),
        QStringLiteral("Unknown Class \u2022 No Teacher"));
    const auto localizedMissingMetadata =
        Application::formatRosterTransferTargetDisplayName(
            metadata, u"\uC54C \uC218 \uC5C6\uB294 \uC218\uC5C5",
            u"\uAD50\uC0AC \uC5C6\uC74C");
    QVERIFY(localizedMissingMetadata);
    QCOMPARE(QString::fromStdU16String(*localizedMissingMetadata),
        QStringLiteral("\uC54C \uC218 \uC5C6\uB294 \uC218\uC5C5 \u2022 \uAD50\uC0AC \uC5C6\uC74C"));

    metadata.level = u"Level 2";
    metadata.teacherPreferredRomanization = u"Romanized Teacher";
    legacyClassInfo.classLevel = QStringLiteral("Level 2");
    legacyTeacher.preferredRomanization = QStringLiteral("Romanized Teacher");
    const auto fallbackName = Application::formatRosterTransferTargetDisplayName(
        metadata, u"\uC54C \uC218 \uC5C6\uB294 \uC218\uC5C5",
        u"\uAD50\uC0AC \uC5C6\uC74C");
    QVERIFY(fallbackName);
    QCOMPARE(QString::fromStdU16String(*fallbackName),
        SidebarNodeNaming::formatClassDisplayName(legacyClassInfo, legacyTeacher));

    metadata.grade = std::u16string(
        Application::kClassesNavigationMaximumTextLength + 1, u'X');
    QVERIFY(!Application::formatRosterTransferTargetDisplayName(
        metadata, u"Unknown Class", u"No Teacher"));
}

void NextRosterEditorTests::
capturesEnglishAndKoreanLightAndDarkRosterStates()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("visual-roster.tps"));
    QStringList customColumns;
    for (int index = 1; index <= 150; ++index)
    {
        customColumns.push_back(QStringLiteral("Custom %1").arg(index));
    }
    int classId = 0;
    QString error;
    QVERIFY2(seedWorkspace(path, QStringLiteral("Visual roster"), customColumns,
        37, 155, QStringLiteral("Tail row"), &classId, &error), qPrintable(error));

    const QString outputDirectory = QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("roster-editor-visual-captures"));
    QVERIFY(QDir().mkpath(outputDirectory));
    QJsonArray states;
    auto* const application = qobject_cast<QApplication*>(QCoreApplication::instance());
    QVERIFY(application);
    const QPalette originalPalette = application->palette();
    const QLocale originalLocale;
    for (const QString& locale : {QStringLiteral("en_US"), QStringLiteral("ko_KR")})
    {
        for (const QString& theme : {QStringLiteral("light"), QStringLiteral("dark")})
        {
            QLocale::setDefault(QLocale(locale));
            QPalette palette;
            const bool dark = theme == QStringLiteral("dark");
            palette.setColor(QPalette::Window,
                QColor(dark ? QStringLiteral("#202327") : QStringLiteral("#f5f6f7")));
            palette.setColor(QPalette::WindowText,
                QColor(dark ? QStringLiteral("#f5f6f7") : QStringLiteral("#232629")));
            palette.setColor(QPalette::Base,
                QColor(dark ? QStringLiteral("#292c30") : QStringLiteral("#ffffff")));
            palette.setColor(QPalette::Text,
                QColor(dark ? QStringLiteral("#fcfcfc") : QStringLiteral("#232629")));
            palette.setColor(QPalette::Button,
                QColor(dark ? QStringLiteral("#292c30") : QStringLiteral("#fcfcfc")));
            palette.setColor(QPalette::ButtonText,
                QColor(dark ? QStringLiteral("#fcfcfc") : QStringLiteral("#232629")));
            palette.setColor(QPalette::Mid,
                QColor(dark ? QStringLiteral("#4b5056") : QStringLiteral("#b0b5bb")));
            palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#3daee9")));
            palette.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#ffffff")));
            application->setPalette(palette);

            QTranslator translator;
            if (locale == QStringLiteral("ko_KR"))
            {
                const QString translations = QStringLiteral(
                    CLASSMNGR_NEXT_TEST_TRANSLATION_DIRECTORY);
                QVERIFY2(translator.load(QDir(translations).filePath(
                    QStringLiteral("ClassMngr_ko_KR.qm"))),
                    qPrintable(QStringLiteral("Could not load translation from %1")
                        .arg(translations)));
                QVERIFY(QCoreApplication::installTranslator(&translator));
            }

            Ui::RosterEditorActions actions;
            actions.saveMode = Application::SaveMode::Manual;
            actions.readRoster = Platform::readRosterWindowAtPath;
            actions.saveRoster = Platform::saveRosterAtPath;
            actions.columnHasTailData = Platform::rosterColumnHasTailDataAtPath;
            Ui::RosterEditorWidget editor(std::move(actions));
            editor.resize(1024, 768);
            editor.enterClassSection(path, typedClassId(classId));
            editor.show();
            QApplication::processEvents();
            QCOMPARE(editor.model()->rowCount(), 25);
            QCOMPARE(editor.model()->columnCount(), 156);
            QVERIFY(editor.isLoaded());
            if (locale == QStringLiteral("ko_KR"))
            {
                QCOMPARE(editor.model()->headerData(0, Qt::Horizontal).toString(),
                    QStringLiteral("영어 이름"));
                QCOMPARE(editor.findChild<QPushButton*>(
                    QStringLiteral("nextRosterSave"))->text(),
                    QStringLiteral("변경 사항 저장"));
            }

            const QString fileName = QStringLiteral("next_roster_editor_%1_%2.png")
                .arg(locale, theme);
            const QString imagePath = QDir(outputDirectory).filePath(fileName);
            const QImage image = editor.grab(QRect(0, 0, 1024, 768)).toImage();
            QCOMPARE(image.size(), QSize(1024, 768));
            QVERIFY2(image.save(imagePath, "PNG"), qPrintable(imagePath));
            const auto lineEdits = editor.findChildren<QLineEdit*>();
            const int activeCellEditors = static_cast<int>(std::count_if(
                lineEdits.cbegin(), lineEdits.cend(), [](const QLineEdit* edit) {
                    return edit->isVisible();
                }));
            const int activeRows = editor.model()->rowCount();
            const int activeColumns = editor.model()->columnCount();
            const int widgetCount = editor.findChildren<QWidget*>().size() + 1;
            editor.clear();
            QVERIFY(!editor.isLoaded());
            const int clearedRows = editor.model()->rowCount();
            const int clearedColumns = editor.model()->columnCount();
            QCOMPARE(clearedRows, 25);
            QCOMPARE(clearedColumns, 6);
            editor.enterClassSection(path, typedClassId(classId));
            QVERIFY(editor.isLoaded());
            QCOMPARE(editor.model()->rowCount(), 25);
            QCOMPARE(editor.model()->columnCount(), 156);
            states.append(QJsonObject{
                {QStringLiteral("locale"), locale},
                {QStringLiteral("theme"), theme},
                {QStringLiteral("image"), fileName},
                {QStringLiteral("modelRowsWhileActive"), activeRows},
                {QStringLiteral("modelColumnsWhileActive"), activeColumns},
                {QStringLiteral("activeCellEditorWidgets"), activeCellEditors},
                {QStringLiteral("rosterEditorWidgetCount"), 1},
                {QStringLiteral("widgetCountWhileActive"), widgetCount},
                {QStringLiteral("modelRowsAfterClear"), clearedRows},
                {QStringLiteral("modelColumnsAfterClear"), clearedColumns},
                {QStringLiteral("modelRecreatedAfterClear"), editor.isLoaded()}
            });
            if (locale == QStringLiteral("ko_KR"))
            {
                QCoreApplication::removeTranslator(&translator);
            }
        }
    }
    QLocale::setDefault(originalLocale);
    application->setPalette(originalPalette);

    const QJsonObject manifest{
        {QStringLiteral("scope"), QStringLiteral("Next roster editor presentation captures")},
        {QStringLiteral("viewport"), QJsonObject{
            {QStringLiteral("width"), 1024}, {QStringLiteral("height"), 768}}},
        {QStringLiteral("modelSchema"), QStringLiteral(
            "25 resident rows and 156 columns: six base columns plus 150 custom columns")},
        {QStringLiteral("comparisonLimit"), QStringLiteral(
            "Presentation evidence only; these images do not establish pixel parity." )},
        {QStringLiteral("captures"), states}
    };
    QFile manifestFile(QDir(outputDirectory).filePath(
        QStringLiteral("roster_editor_visual_captures.json")));
    QVERIFY(manifestFile.open(QIODevice::WriteOnly | QIODevice::Text));
    QVERIFY(manifestFile.write(QJsonDocument(manifest).toJson(
        QJsonDocument::Indented)) > 0);
}

void NextRosterEditorTests::
capturesImportAndTransferStatesAcrossLocalesAndThemes()
{
    const QString outputDirectory = QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("roster-editor-visual-captures"));
    QVERIFY(QDir().mkpath(outputDirectory));
    auto* const application = qobject_cast<QApplication*>(QCoreApplication::instance());
    QVERIFY(application);
    const QPalette originalPalette = application->palette();
    const QLocale originalLocale;
    QJsonArray captures;

    for (const QString& locale : {QStringLiteral("en_US"), QStringLiteral("ko_KR")})
    {
        for (const QString& theme : {QStringLiteral("light"), QStringLiteral("dark")})
        {
            QLocale::setDefault(QLocale(locale));
            QPalette palette;
            const bool dark = theme == QStringLiteral("dark");
            palette.setColor(QPalette::Window,
                QColor(dark ? QStringLiteral("#202327") : QStringLiteral("#f5f6f7")));
            palette.setColor(QPalette::WindowText,
                QColor(dark ? QStringLiteral("#f5f6f7") : QStringLiteral("#232629")));
            palette.setColor(QPalette::Base,
                QColor(dark ? QStringLiteral("#292c30") : QStringLiteral("#ffffff")));
            palette.setColor(QPalette::Text,
                QColor(dark ? QStringLiteral("#fcfcfc") : QStringLiteral("#232629")));
            palette.setColor(QPalette::Button,
                QColor(dark ? QStringLiteral("#292c30") : QStringLiteral("#fcfcfc")));
            palette.setColor(QPalette::ButtonText,
                QColor(dark ? QStringLiteral("#fcfcfc") : QStringLiteral("#232629")));
            palette.setColor(QPalette::Mid,
                QColor(dark ? QStringLiteral("#4b5056") : QStringLiteral("#b0b5bb")));
            palette.setColor(QPalette::Highlight, QColor(QStringLiteral("#3daee9")));
            palette.setColor(QPalette::HighlightedText,
                QColor(QStringLiteral("#ffffff")));
            application->setPalette(palette);

            QTranslator translator;
            if (locale == QStringLiteral("ko_KR"))
            {
                const QString translations = QStringLiteral(
                    CLASSMNGR_NEXT_TEST_TRANSLATION_DIRECTORY);
                QVERIFY2(translator.load(QDir(translations).filePath(
                    QStringLiteral("ClassMngr_ko_KR.qm"))),
                    qPrintable(QStringLiteral("Could not load translation from %1")
                        .arg(translations)));
                QVERIFY(QCoreApplication::installTranslator(&translator));
            }

            ActionProbe importProbe;
            Ui::RosterEditorWidget importEditor(fakeActions(importProbe));
            importEditor.resize(1024, 768);
            importEditor.enterClassSection(
                QStringLiteral("capture.tps"), typedClassId(12));
            importEditor.show();
            importEditor.findChild<QPushButton*>(
                QStringLiteral("nextRosterImportScores"))->click();
            QApplication::processEvents();
            const QString importFile = QStringLiteral("roster_import_%1_%2.png")
                .arg(locale, theme);
            const QString importPath = QDir(outputDirectory).filePath(importFile);
            const QImage importImage = importEditor.grab(
                QRect(0, 0, 1024, 768)).toImage();
            QCOMPARE(importImage.size(), QSize(1024, 768));
            QVERIFY2(importImage.save(importPath, "PNG"), qPrintable(importPath));
            captures.append(QJsonObject{
                {QStringLiteral("state"), QStringLiteral("imported scores")},
                {QStringLiteral("locale"), locale},
                {QStringLiteral("theme"), theme},
                {QStringLiteral("image"), importFile}
            });

            ActionProbe transferProbe;
            Ui::RosterEditorWidget transferEditor(fakeActions(transferProbe));
            transferEditor.resize(1024, 768);
            transferEditor.enterClassSection(
                QStringLiteral("capture.tps"), typedClassId(12));
            transferEditor.tableView()->setCurrentIndex(
                transferEditor.model()->index(0, 0));
            transferEditor.show();
            chooseTransferMenuTarget(QStringLiteral("99"));
            transferEditor.findChild<QPushButton*>(
                QStringLiteral("nextRosterTransferStudent"))->click();
            QApplication::processEvents();
            QCOMPARE(transferProbe.transferSaveCount, 1);
            const QString transferFile = QStringLiteral("roster_transfer_%1_%2.png")
                .arg(locale, theme);
            const QString transferPath = QDir(outputDirectory).filePath(transferFile);
            const QImage transferImage = transferEditor.grab(
                QRect(0, 0, 1024, 768)).toImage();
            QCOMPARE(transferImage.size(), QSize(1024, 768));
            QVERIFY2(transferImage.save(transferPath, "PNG"), qPrintable(transferPath));
            captures.append(QJsonObject{
                {QStringLiteral("state"), QStringLiteral("student transferred")},
                {QStringLiteral("locale"), locale},
                {QStringLiteral("theme"), theme},
                {QStringLiteral("image"), transferFile}
            });

            if (locale == QStringLiteral("ko_KR"))
            {
                QCoreApplication::removeTranslator(&translator);
            }
        }
    }
    QLocale::setDefault(originalLocale);
    application->setPalette(originalPalette);

    const QJsonObject manifest{
        {QStringLiteral("scope"), QStringLiteral(
            "Roster score import and student transfer presentation captures")},
        {QStringLiteral("viewport"), QJsonObject{
            {QStringLiteral("width"), 1024}, {QStringLiteral("height"), 768}}},
        {QStringLiteral("limit"), QStringLiteral(
            "Presentation evidence only; images do not establish pixel parity.")},
        {QStringLiteral("captures"), captures}
    };
    QFile manifestFile(QDir(outputDirectory).filePath(
        QStringLiteral("roster_import_transfer_visual_captures.json")));
    QVERIFY(manifestFile.open(QIODevice::WriteOnly | QIODevice::Text));
    QVERIFY(manifestFile.write(QJsonDocument(manifest).toJson(
        QJsonDocument::Indented)) > 0);
}

QTEST_MAIN(NextRosterEditorTests)
#include "roster_editor_tests.moc"
