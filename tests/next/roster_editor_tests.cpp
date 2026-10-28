#include "data/database/workspace_store.h"
#include "data/repositories/class_repository.h"
#include "data/repositories/roster_repository.h"
#include "domain/models/roster.h"
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

struct ActionProbe final
{
    Application::RosterSnapshot snapshot = ordinarySnapshot();
    int readCount = 0;
    int saveCount = 0;
    int failSaveCount = 0;
    bool failRead = false;
    bool hasRowsOutsideWindow = false;
    bool hasTailColumnData = false;
    QString lastReadPath;
    QString lastSavePath;
    std::optional<Application::RosterSaveRequest> lastSave;
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
        const Domain::ClassId&)
    {
        ++probe.readCount;
        probe.lastReadPath = path;
        if (probe.failRead)
        {
            return Application::RosterWindowReadResult::failure({
                .code = Domain::ErrorCode::Technical,
                .message = "injected roster read failure",
                .recoverable = true
            });
        }
        return Application::RosterWindowReadResult::success({
            .roster = probe.snapshot,
            .hasRowsOutsideWindow = probe.hasRowsOutsideWindow
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
    QTimer::singleShot(0, [first, second] {
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
    void capturesEnglishAndKoreanLightAndDarkRosterStates();
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

QTEST_MAIN(NextRosterEditorTests)
#include "roster_editor_tests.moc"
