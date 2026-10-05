#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "domain/models/classroom.h"
#include "domain/models/roster.h"
#include "domain/models/testing_class.h"
#include "features/classes/ui/testing_classes_page.h"
#include "features/roster/ui/roster_model.h"
#include "features/roster/ui/roster_editor_widget.h"
#include "features/roster/ui/roster_table_view.h"

#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/autosave_coordinator.h"

#include <QAction>
#include <QApplication>
#include <QHeaderView>
#include <QInputDialog>
#include <QListWidget>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QSignalSpy>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest/QtTest>
#include <QUuid>

namespace
{

class ScopedPromptService final
{
public:
    explicit ScopedPromptService(IUserPromptService* service)
    {
        DialogServices::setUserPromptServiceForTesting(service);
    }

    ~ScopedPromptService()
    {
        DialogServices::setUserPromptServiceForTesting(nullptr);
    }
};

TestingClass makeTestingClass(const QString& name)
{
    TestingClass testingClass;
    testingClass.name = name;
    testingClass.grade = QStringLiteral("M1");
    testingClass.level = QStringLiteral("Major");
    testingClass.room = QStringLiteral("401");
    return testingClass;
}

struct RosterEditorFixture final
{
    QTemporaryDir directory;
    ApplicationServices services;
    int classId = 0;

    bool initialize(QString* error)
    {
        if (!directory.isValid())
        {
            *error = QStringLiteral("Temporary directory is invalid.");
            return false;
        }

        const QString path = directory.filePath(
            QStringLiteral("roster-editor-save-%1.tps").arg(
                QUuid::createUuid().toString(QUuid::WithoutBraces)
                )
            );
        const Status opened = services.openDatabase(path);
        if (!opened)
        {
            *error = opened.error();
            return false;
        }

        const auto createdClass = services.classService()->create(
            QStringLiteral("Roster Editor Save Test")
            );
        if (!createdClass)
        {
            *error = createdClass.error();
            return false;
        }

        classId = *createdClass;
        return true;
    }
};

int columnByName(const RosterModel* model, const QString& name)
{
    for (int column = 0; model && column < model->columnCount(); ++column)
    {
        if (model->columnName(column).compare(name, Qt::CaseInsensitive) == 0)
        {
            return column;
        }
    }

    return -1;
}

bool setStudent(
    RosterModel* model,
    const int row,
    const QString& english,
    const QString& korean
    )
{
    if (!model)
    {
        return false;
    }

    const int englishColumn = columnByName(model, QStringLiteral("English"));
    const int koreanColumn = columnByName(model, QStringLiteral("Korean"));
    return englishColumn >= 0
        && koreanColumn >= 0
        && model->setData(
            model->index(row, englishColumn),
            english,
            Qt::EditRole
            )
        && model->setData(
            model->index(row, koreanColumn),
            korean,
            Qt::EditRole
            );
}

}

class RosterEditorWidgetSaveTests final : public QObject
{
    Q_OBJECT

private slots:
    void manualSaveCleansOnSuccessAndStaysDirtyOnFailure();
    void autosavePersistsOrdinaryRosterWithoutReloading();
    void customColumnAdditionSelectionLayoutAutosaveAndPersistence();
    void rowMovePreservesSelectionAndSchedulesAutosave();
    void rowRemovalConfirmationSelectionAndAutosave();
    void customColumnRemovalConfirmationSelectionWidthAndAutosave();
    void invalidRosterSaveSelectsFirstInvalidCell();
    void autosaveFailureRemainsSilentAndDirty();
    void confirmedInteractiveSaveAllowsQuestionableKoreanNameLength();
    void rejectedQuestionableLengthKeepsFocusDirtyAndStorage();
    void literalLeadingBomInNameRemainsInvalid();
    void failedRosterSaveKeepsTestingClassSelection();
    void loadClassPreservesModelNormalizationWidthsAndCleanState();
    void emptyAndFailedReadsKeepBlankRosterAndRefreshOutputCapabilities();
};

void RosterEditorWidgetSaveTests::
manualSaveCleansOnSuccessAndStaysDirtyOnFailure()
{
    RosterEditorFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    FakeUserPromptService prompts;
    ScopedPromptService promptScope(&prompts);

    RosterEditorWidget editor(&fixture.services);
    editor.loadClass(Classroom(QStringLiteral("Save test"), fixture.classId));
    editor.setSaveMode(SaveMode::Manual);
    RosterModel* const model = editor.findChild<RosterModel*>();
    QVERIFY(model);

    QVERIFY(setStudent(model, 0, QStringLiteral("Alice"), QStringLiteral("김민지")));
    QVERIFY(editor.hasUnsavedChanges());
    editor.saveData();
    QVERIFY(!editor.hasUnsavedChanges());
    QCOMPARE(model->data(model->index(0, columnByName(
        model,
        QStringLiteral("English")
        ))).toString(), QStringLiteral("Alice"));

    QVERIFY(setStudent(model, 1, QStringLiteral("Bob"), QStringLiteral("박지훈")));
    QVERIFY(editor.hasUnsavedChanges());
    fixture.services.closeDatabase();
    editor.saveData();

    QVERIFY(editor.hasUnsavedChanges());
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages.constFirst().severity, PromptSeverity::Warning);
    QCOMPARE(prompts.messages.constFirst().title, QStringLiteral("Save Roster"));
    QVERIFY(!editor.saveChanges());
    QVERIFY(editor.hasUnsavedChanges());
    QCOMPARE(prompts.messages.size(), 2);
}

void RosterEditorWidgetSaveTests::
autosavePersistsOrdinaryRosterWithoutReloading()
{
    RosterEditorFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    FakeUserPromptService prompts;
    ScopedPromptService promptScope(&prompts);

    RosterEditorWidget editor(&fixture.services);
    editor.loadClass(Classroom(QStringLiteral("Autosave test"), fixture.classId));
    RosterModel* const model = editor.findChild<RosterModel*>();
    QVERIFY(model);

    QVERIFY(setStudent(model, 0, QStringLiteral("Carol"), QStringLiteral("최민서")));
    QVERIFY(editor.hasUnsavedChanges());
    QTRY_VERIFY_WITH_TIMEOUT(!editor.hasUnsavedChanges(), 5'000);
    QCOMPARE(prompts.messages.size(), 0);

    const auto persisted = fixture.services.rosterService()->roster(fixture.classId);
    QVERIFY(persisted);
    QCOMPARE(persisted->rows.at(0).at(0), QStringLiteral("Carol"));
    QCOMPARE(model->data(model->index(0, columnByName(
        model,
        QStringLiteral("English")
        ))).toString(), QStringLiteral("Carol"));
}

void RosterEditorWidgetSaveTests::
customColumnAdditionSelectionLayoutAutosaveAndPersistence()
{
    RosterEditorFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    Roster roster;
    roster.columns = Roster::BaseColumns;
    roster.columnWidths = {211, 143, 166, 167, 168, 169};
    roster.rows = {
        {
            QStringLiteral("Amy"),
            QStringLiteral("\uAE40\uBBFC\uC9C0"),
            QStringLiteral("A+"),
            QStringLiteral("B"),
            QStringLiteral("C"),
            QStringLiteral("D")
        }
    };
    QVERIFY(fixture.services.rosterService()->saveRoster(fixture.classId, roster));

    FakeUserPromptService prompts;
    ScopedPromptService promptScope(&prompts);

    RosterEditorWidget editor(&fixture.services);
    editor.loadClass(Classroom(QStringLiteral("Column addition test"), fixture.classId));
    auto* const model = editor.findChild<RosterModel*>();
    auto* const table = editor.findChild<RosterTableView*>(
        QStringLiteral("rosterTable")
        );
    auto* const autosave = editor.findChild<AutosaveCoordinator*>();
    QVERIFY(model);
    QVERIFY(table);
    QVERIFY(autosave);

    QPushButton* addColumnButton = nullptr;
    for (QPushButton* button : editor.findChildren<QPushButton*>())
    {
        if (button->text() == QStringLiteral("Add Column"))
        {
            addColumnButton = button;
            break;
        }
    }
    QVERIFY(addColumnButton);

    autosave->setDebounceInterval(10);
    QSignalSpy saveSpy(autosave, &AutosaveCoordinator::saveRequested);
    QVERIFY(saveSpy.isValid());

    bool inputDialogHandled = false;
    QTimer::singleShot(
        0,
        &editor,
        [&inputDialogHandled]()
        {
            auto* dialog = qobject_cast<QInputDialog*>(
                QApplication::activeModalWidget()
                );
            if (!dialog)
            {
                return;
            }

            inputDialogHandled = dialog->windowTitle() == QStringLiteral("Add Column");
            dialog->setTextValue(QStringLiteral("  Parent\t\u00a0 Contact  "));
            dialog->accept();
        }
        );
    addColumnButton->click();

    QVERIFY(inputDialogHandled);
    const int addedColumn = Roster::BaseColumns.size();
    QCOMPARE(model->columnCount(), addedColumn + 1);
    QCOMPARE(model->columnName(addedColumn), QStringLiteral("Parent Contact"));
    QCOMPARE(table->currentIndex(), model->index(0, addedColumn));
    QVERIFY(table->findChild<QLineEdit*>());
    QCOMPARE(
        table->horizontalHeader()->sectionResizeMode(addedColumn),
        QHeaderView::Interactive
        );
    QCOMPARE(table->columnWidth(addedColumn), 220);
    QCOMPARE(model->rowValues(0).at(0), QStringLiteral("Amy"));
    QVERIFY(model->rowValues(0).last().isEmpty());
    QVERIFY(model->isDirty());
    QVERIFY(editor.hasUnsavedChanges());
    QVERIFY(autosave->isDirty());
    QCOMPARE(saveSpy.count(), 0);

    QTRY_VERIFY_WITH_TIMEOUT(saveSpy.count() >= 1, 5'000);
    QCOMPARE(saveSpy.constFirst().at(0).toBool(), false);
    QTRY_VERIFY_WITH_TIMEOUT(!editor.hasUnsavedChanges(), 5'000);

    const auto saved = fixture.services.rosterService()->roster(fixture.classId);
    QVERIFY(saved);
    QCOMPARE(
        saved->columns,
        Roster::BaseColumns + QStringList{QStringLiteral("Parent Contact")}
        );
    QCOMPARE(saved->rows.at(0).at(0), QStringLiteral("Amy"));
    QVERIFY(saved->rows.at(0).at(6).isEmpty());
    QCOMPARE(saved->columnWidths.at(6), table->columnWidth(addedColumn));
    QCOMPARE(prompts.messages.size(), 0);
}

void RosterEditorWidgetSaveTests::
rowMovePreservesSelectionAndSchedulesAutosave()
{
    RosterEditorFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    Roster roster;
    roster.columns = Roster::BaseColumns;
    roster.rows = {
        QStringList{
            QStringLiteral("Amy"),
            QStringLiteral("\uAE40\uBBFC\uC9C0"),
            QStringLiteral("A+"),
            QStringLiteral("B"),
            QStringLiteral("C"),
            QStringLiteral("D")
        },
        QStringList{
            QStringLiteral("Ben"),
            QStringLiteral("\uC774\uC11C\uC900"),
            QStringLiteral("B+"),
            QStringLiteral("A"),
            QStringLiteral("B"),
            QStringLiteral("C")
        },
        QStringList{
            QStringLiteral("Cal"),
            QStringLiteral("\uBC15\uC11C\uC900"),
            QStringLiteral("C+"),
            QStringLiteral("B"),
            QStringLiteral("A"),
            QStringLiteral("B")
        }
    };
    QVERIFY(fixture.services.rosterService()->saveRoster(fixture.classId, roster));

    RosterEditorWidget editor(&fixture.services);
    editor.loadClass(Classroom(QStringLiteral("Row move autosave test"), fixture.classId));
    auto* const model = editor.findChild<RosterModel*>();
    auto* const table = editor.findChild<RosterTableView*>(
        QStringLiteral("rosterTable")
        );
    auto* const autosave = editor.findChild<AutosaveCoordinator*>();
    QVERIFY(model);
    QVERIFY(table);
    QVERIFY(autosave);

    const int selectedColumn = model->koreanNameColumn();
    QVERIFY(selectedColumn >= 0);
    table->setCurrentIndex(model->index(0, selectedColumn));
    autosave->setDebounceInterval(10);
    QSignalSpy saveSpy(autosave, &AutosaveCoordinator::saveRequested);
    QVERIFY(saveSpy.isValid());

    table->requestRowMove(0, 2);

    QCOMPARE(table->currentIndex(), model->index(2, selectedColumn));
    QCOMPARE(model->rowValues(2).at(0), QStringLiteral("Amy"));
    QCOMPARE(model->rowValues(2).at(2), QStringLiteral("A+"));
    QCOMPARE(model->rowValues(0).at(0), QStringLiteral("Ben"));
    QVERIFY(model->isDirty());
    QVERIFY(autosave->isDirty());
    QVERIFY(editor.hasUnsavedChanges());
    QCOMPARE(saveSpy.count(), 0);

    QTRY_VERIFY_WITH_TIMEOUT(saveSpy.count() >= 1, 5'000);
    QCOMPARE(saveSpy.constFirst().at(0).toBool(), false);
    QTRY_VERIFY_WITH_TIMEOUT(!editor.hasUnsavedChanges(), 5'000);

    const auto saved = fixture.services.rosterService()->roster(fixture.classId);
    QVERIFY(saved);
    QCOMPARE(saved->rows.at(2).at(0), QStringLiteral("Amy"));
    QCOMPARE(saved->rows.at(2).at(2), QStringLiteral("A+"));
}

void RosterEditorWidgetSaveTests::
rowRemovalConfirmationSelectionAndAutosave()
{
    RosterEditorFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    Roster roster;
    roster.columns = Roster::BaseColumns;
    roster.rows = {
        QStringList{
            QStringLiteral("Amy"),
            QStringLiteral("\uAE40\uBBFC\uC9C0"),
            QStringLiteral("A+"),
            QStringLiteral("B"),
            QStringLiteral("C"),
            QStringLiteral("D")
        },
        QStringList{
            QStringLiteral("Ben"),
            QStringLiteral("\uC774\uC11C\uC900"),
            QStringLiteral("B+"),
            QStringLiteral("A"),
            QStringLiteral("B"),
            QStringLiteral("C")
        },
        QStringList{
            QStringLiteral("Cal"),
            QStringLiteral("\uBC15\uC11C\uC900"),
            QStringLiteral("C+"),
            QStringLiteral("B"),
            QStringLiteral("A"),
            QStringLiteral("B")
        }
    };
    QVERIFY(fixture.services.rosterService()->saveRoster(fixture.classId, roster));

    FakeUserPromptService prompts;
    prompts.scriptedChoices.enqueue(PromptChoice::Rejected);
    prompts.scriptedChoices.enqueue(PromptChoice::Destructive);
    ScopedPromptService promptScope(&prompts);

    RosterEditorWidget editor(&fixture.services);
    editor.loadClass(Classroom(QStringLiteral("Row removal test"), fixture.classId));
    auto* const model = editor.findChild<RosterModel*>();
    auto* const table = editor.findChild<RosterTableView*>(
        QStringLiteral("rosterTable")
        );
    auto* const autosave = editor.findChild<AutosaveCoordinator*>();
    QVERIFY(model);
    QVERIFY(table);
    QVERIFY(autosave);

    autosave->setDebounceInterval(10);
    QSignalSpy saveSpy(autosave, &AutosaveCoordinator::saveRequested);
    QVERIFY(saveSpy.isValid());

    const auto removeThroughContextMenu = [&editor, model, table]()
    {
        bool foundRemoveAction = false;
        QTimer::singleShot(
            0,
            &editor,
            [&foundRemoveAction]()
            {
                auto* menu = qobject_cast<QMenu*>(
                    QApplication::activePopupWidget()
                    );
                if (!menu)
                {
                    return;
                }

                for (QAction* action : menu->actions())
                {
                    if (action->text() == QStringLiteral("Remove Student"))
                    {
                        foundRemoveAction = true;
                        QTest::mouseClick(
                            menu,
                            Qt::LeftButton,
                            Qt::NoModifier,
                            menu->actionGeometry(action).center()
                            );
                        return;
                    }
                }

                menu->close();
            }
            );

        const QModelIndex clicked = model->index(1, 0);
        return clicked.isValid()
            && QMetaObject::invokeMethod(
                &editor,
                "showRosterContextMenu",
                Qt::DirectConnection,
                Q_ARG(QPoint, table->visualRect(clicked).center())
                )
            && foundRemoveAction;
    };

    QVERIFY(removeThroughContextMenu());
    QCOMPARE(prompts.confirmations.size(), 1);
    QCOMPARE(prompts.confirmations.constFirst().title, QStringLiteral("Remove Student"));
    QVERIFY(prompts.confirmations.constFirst().destructive);
    QCOMPARE(model->rowValues(1).at(0), QStringLiteral("Ben"));
    QVERIFY(!editor.hasUnsavedChanges());
    QCOMPARE(saveSpy.count(), 0);

    QVERIFY(removeThroughContextMenu());
    QCOMPARE(prompts.confirmations.size(), 2);
    QCOMPARE(table->currentIndex(), model->index(1, 0));
    QCOMPARE(model->rowValues(1).at(0), QStringLiteral("Cal"));
    QVERIFY(model->rowValues(model->rowCount() - 1).at(0).isEmpty());
    QVERIFY(editor.hasUnsavedChanges());
    QVERIFY(autosave->isDirty());
    QCOMPARE(saveSpy.count(), 0);

    QTRY_VERIFY_WITH_TIMEOUT(saveSpy.count() >= 1, 5'000);
    QCOMPARE(saveSpy.constFirst().at(0).toBool(), false);
    QTRY_VERIFY_WITH_TIMEOUT(!editor.hasUnsavedChanges(), 5'000);

    const auto saved = fixture.services.rosterService()->roster(fixture.classId);
    QVERIFY(saved);
    QCOMPARE(saved->rows.size(), 2);
    QCOMPARE(saved->rows.at(0).at(0), QStringLiteral("Amy"));
    QCOMPARE(saved->rows.at(1).at(0), QStringLiteral("Cal"));
}

void RosterEditorWidgetSaveTests::
customColumnRemovalConfirmationSelectionWidthAndAutosave()
{
    RosterEditorFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    Roster roster;
    roster.columns = Roster::BaseColumns
        + QStringList{QStringLiteral("Advisory"), QStringLiteral("Family notes")};
    roster.columnWidths = {211, 143, 166, 167, 168, 169, 241, 273};
    roster.rows = {
        {
            QStringLiteral("Amy"),
            QStringLiteral("\uAE40\uBBFC\uC9C0"),
            QStringLiteral("A+"),
            QStringLiteral("B"),
            QStringLiteral("C"),
            QStringLiteral("D"),
            QStringLiteral("Advisory value"),
            QStringLiteral("Family value")
        }
    };
    QVERIFY(fixture.services.rosterService()->saveRoster(fixture.classId, roster));

    FakeUserPromptService prompts;
    prompts.scriptedChoices.enqueue(PromptChoice::Rejected);
    prompts.scriptedChoices.enqueue(PromptChoice::Destructive);
    ScopedPromptService promptScope(&prompts);

    RosterEditorWidget editor(&fixture.services);
    editor.loadClass(Classroom(QStringLiteral("Column removal test"), fixture.classId));
    auto* const model = editor.findChild<RosterModel*>();
    auto* const table = editor.findChild<RosterTableView*>(
        QStringLiteral("rosterTable")
        );
    auto* const autosave = editor.findChild<AutosaveCoordinator*>();
    QVERIFY(model);
    QVERIFY(table);
    QVERIFY(autosave);

    QPushButton* removeColumnButton = nullptr;
    for (QPushButton* button : editor.findChildren<QPushButton*>())
    {
        if (button->text() == QStringLiteral("Remove Column"))
        {
            removeColumnButton = button;
            break;
        }
    }
    QVERIFY(removeColumnButton);

    const int advisoryColumn = 6;
    const int familyNotesColumn = 7;
    const Roster before = model->toRoster();
    const int advisoryWidth = table->columnWidth(advisoryColumn);
    const int familyNotesWidth = table->columnWidth(familyNotesColumn);
    QVERIFY(advisoryWidth > 0);
    QVERIFY(familyNotesWidth > 0);
    QVERIFY(table->horizontalHeader()->sectionResizeMode(familyNotesColumn)
        == QHeaderView::Interactive);

    QSignalSpy saveSpy(autosave, &AutosaveCoordinator::saveRequested);
    QVERIFY(saveSpy.isValid());

    table->setCurrentIndex(model->index(0, advisoryColumn));
    removeColumnButton->click();
    QCOMPARE(prompts.confirmations.size(), 1);
    QCOMPARE(prompts.confirmations.constFirst().title, QStringLiteral("Remove Column"));
    QCOMPARE(
        prompts.confirmations.constFirst().message,
        QStringLiteral("Remove the \"Advisory\" column?")
        );
    QCOMPARE(prompts.confirmations.constFirst().acceptText, QStringLiteral("Remove"));
    QCOMPARE(prompts.confirmations.constFirst().rejectText, QStringLiteral("Cancel"));
    QVERIFY(prompts.confirmations.constFirst().destructive);
    QCOMPARE(model->toRoster().columns, before.columns);
    QCOMPARE(model->toRoster().rows, before.rows);
    QCOMPARE(table->columnWidth(advisoryColumn), advisoryWidth);
    QCOMPARE(table->columnWidth(familyNotesColumn), familyNotesWidth);
    QCOMPARE(table->currentIndex(), model->index(0, advisoryColumn));
    QVERIFY(!editor.hasUnsavedChanges());
    QCOMPARE(saveSpy.count(), 0);

    table->setCurrentIndex(model->index(0, advisoryColumn));
    removeColumnButton->click();
    QCOMPARE(prompts.confirmations.size(), 2);
    QCOMPARE(model->columnCount(), before.columns.size() - 1);
    QCOMPARE(model->columnName(advisoryColumn), QStringLiteral("Family notes"));
    QCOMPARE(model->index(0, advisoryColumn).data().toString(), QStringLiteral("Family value"));
    QCOMPARE(table->columnWidth(advisoryColumn), familyNotesWidth);
    QCOMPARE(
        table->horizontalHeader()->sectionResizeMode(advisoryColumn),
        QHeaderView::Interactive
        );
    QVERIFY(table->currentIndex().isValid());
    QCOMPARE(table->currentIndex().row(), 0);
    QCOMPARE(table->currentIndex().column(), advisoryColumn - 1);
    QCOMPARE(model->columnName(table->currentIndex().column()), QStringLiteral("Fall"));
    QVERIFY(editor.hasUnsavedChanges());
    QVERIFY(autosave->isDirty());
    QCOMPARE(saveSpy.count(), 0);

    QTRY_VERIFY_WITH_TIMEOUT(saveSpy.count() >= 1, 5'000);
    QCOMPARE(saveSpy.constFirst().at(0).toBool(), false);
    QTRY_VERIFY_WITH_TIMEOUT(!editor.hasUnsavedChanges(), 5'000);

    const auto saved = fixture.services.rosterService()->roster(fixture.classId);
    QVERIFY(saved);
    QCOMPARE(
        saved->columns,
        Roster::BaseColumns + QStringList{QStringLiteral("Family notes")}
        );
    QCOMPARE(saved->rows.at(0).at(6), QStringLiteral("Family value"));
    QCOMPARE(saved->columnWidths.at(6), familyNotesWidth);
}

void RosterEditorWidgetSaveTests::
invalidRosterSaveSelectsFirstInvalidCell()
{
    RosterEditorFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    RosterEditorWidget editor(&fixture.services);
    editor.loadClass(Classroom(QStringLiteral("Invalid roster test"), fixture.classId));
    editor.setSaveMode(SaveMode::Manual);
    auto* model = editor.findChild<RosterModel*>();
    auto* table = editor.findChild<RosterTableView*>(
        QStringLiteral("rosterTable")
        );
    QVERIFY(model);
    QVERIFY(table);

    const int englishColumn = columnByName(model, QStringLiteral("English"));
    const int koreanColumn = columnByName(model, QStringLiteral("Korean"));
    QVERIFY(englishColumn >= 0);
    QVERIFY(koreanColumn >= 0);
    QVERIFY(model->setData(
        model->index(0, englishColumn),
        QStringLiteral("Alice"),
        Qt::EditRole
        ));
    QVERIFY(editor.hasUnsavedChanges());

    QVERIFY(!editor.saveChanges());

    QCOMPARE(table->currentIndex(), model->index(0, koreanColumn));
    QVERIFY(editor.hasUnsavedChanges());
}

void RosterEditorWidgetSaveTests::
autosaveFailureRemainsSilentAndDirty()
{
    RosterEditorFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    FakeUserPromptService prompts;
    ScopedPromptService promptScope(&prompts);

    RosterEditorWidget editor(&fixture.services);
    editor.loadClass(Classroom(QStringLiteral("Autosave failure test"), fixture.classId));
    auto* model = editor.findChild<RosterModel*>();
    auto* autosave = editor.findChild<AutosaveCoordinator*>();
    QVERIFY(model);
    QVERIFY(autosave);
    QSignalSpy saveSpy(autosave, &AutosaveCoordinator::saveRequested);
    QVERIFY(saveSpy.isValid());

    QVERIFY(setStudent(model, 0, QStringLiteral("Alice"), QStringLiteral("김민지")));
    QVERIFY(editor.hasUnsavedChanges());
    fixture.services.closeDatabase();

    QTRY_VERIFY_WITH_TIMEOUT(saveSpy.count() >= 1, 5'000);
    QCOMPARE(saveSpy.constFirst().at(0).toBool(), false);
    QVERIFY(editor.hasUnsavedChanges());
    QVERIFY(prompts.messages.isEmpty());
}

void RosterEditorWidgetSaveTests::
confirmedInteractiveSaveAllowsQuestionableKoreanNameLength()
{
    RosterEditorFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    FakeUserPromptService prompts;
    prompts.scriptedChoices.enqueue(PromptChoice::Accepted);
    ScopedPromptService promptScope(&prompts);

    RosterEditorWidget editor(&fixture.services);
    editor.loadClass(Classroom(QStringLiteral("Interactive save test"), fixture.classId));
    editor.setSaveMode(SaveMode::Manual);
    RosterModel* const model = editor.findChild<RosterModel*>();
    QVERIFY(model);

    QVERIFY(setStudent(model, 0, QStringLiteral("Dana"), QStringLiteral("김")));
    QVERIFY(editor.hasUnsavedChanges());

    editor.saveData();
    QVERIFY(editor.hasUnsavedChanges());
    QCOMPARE(prompts.confirmations.size(), 0);

    QVERIFY(editor.saveChanges());
    QVERIFY(!editor.hasUnsavedChanges());
    QCOMPARE(prompts.confirmations.size(), 1);
    QCOMPARE(prompts.confirmations.constFirst().title,
        QStringLiteral("Verify Korean Name Lengths"));
    QCOMPARE(prompts.confirmations.constFirst().acceptText, QStringLiteral("Save Anyway"));
    QCOMPARE(prompts.confirmations.constFirst().rejectText, QStringLiteral("Go Back"));

    const auto persisted = fixture.services.rosterService()->roster(fixture.classId);
    QVERIFY(persisted);
    QCOMPARE(persisted->rows.at(0).at(0), QStringLiteral("Dana"));
    QCOMPARE(persisted->rows.at(0).at(1), QStringLiteral("김"));
}

void RosterEditorWidgetSaveTests::
failedRosterSaveKeepsTestingClassSelection()
{
    RosterEditorFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    const auto firstClass = fixture.services.scheduleService()->createTestingClass(
        makeTestingClass(QStringLiteral("A First Testing Class"))
        );
    QVERIFY(firstClass);
    const auto secondClass = fixture.services.scheduleService()->createTestingClass(
        makeTestingClass(QStringLiteral("B Second Testing Class"))
        );
    QVERIFY(secondClass);

    FakeUserPromptService prompts;
    ScopedPromptService promptScope(&prompts);

    TestingClassesPage page(&fixture.services);
    page.setDatabaseOpen(true);
    page.setSaveMode(SaveMode::Manual);
    page.openTestingClass(*firstClass);
    page.activate();

    auto* list = page.findChild<QListWidget*>(
        QStringLiteral("testingClassesList")
        );
    auto* editor = page.findChild<RosterEditorWidget*>();
    QVERIFY(list);
    QVERIFY(editor);
    QCOMPARE(list->count(), 2);
    QCOMPARE(list->currentItem()->data(Qt::UserRole).toInt(), *firstClass);

    int secondIndex = -1;
    for (int index = 0; index < list->count(); ++index)
    {
        if (list->item(index)->data(Qt::UserRole).toInt() == *secondClass)
        {
            secondIndex = index;
            break;
        }
    }
    QVERIFY(secondIndex >= 0);

    auto* model = editor->findChild<RosterModel*>();
    QVERIFY(model);
    QVERIFY(setStudent(model, 0, QStringLiteral("Alice"), QStringLiteral("김민지")));
    QVERIFY(page.hasUnsavedChanges());

    fixture.services.closeDatabase();
    list->setCurrentRow(secondIndex);

    QCOMPARE(list->currentItem()->data(Qt::UserRole).toInt(), *firstClass);
    QVERIFY(page.hasUnsavedChanges());
    QCOMPARE(model->data(model->index(0, columnByName(
        model,
        QStringLiteral("English")
        ))).toString(), QStringLiteral("Alice"));
    QCOMPARE(prompts.messages.size(), 1);
}

void RosterEditorWidgetSaveTests::
loadClassPreservesModelNormalizationWidthsAndCleanState()
{
    RosterEditorFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    Roster roster;
    roster.columns = Roster::BaseColumns;
    roster.columns.append(QStringLiteral("\u5099\u8003"));
    roster.columnWidths = {211, 143, 166, 167, 168, 169, 242};
    QStringList row(7, QString());
    row[0] = QStringLiteral("Alice");
    row[1] = QStringLiteral("\uAE40\uBBFC\uC9C0");
    row[2] = QStringLiteral("A+");
    row[6] = QStringLiteral("Review \U0001F4DA");
    roster.rows.append(row);
    QVERIFY(fixture.services.rosterService()->saveRoster(fixture.classId, roster));

    QSqlQuery updateRawName(fixture.services.databaseSession()->database());
    updateRawName.prepare(QStringLiteral(
        "UPDATE roster_data SET value=? "
        "WHERE class_id=? AND row_index=0 AND col_index=0"
        ));
    updateRawName.addBindValue(QStringLiteral("  Alice  "));
    updateRawName.addBindValue(fixture.classId);
    QVERIFY2(updateRawName.exec(), qPrintable(updateRawName.lastError().text()));

    RosterEditorWidget editor(&fixture.services);
    editor.loadClass(Classroom(QStringLiteral("Loaded roster"), fixture.classId));
    auto* model = editor.findChild<RosterModel*>();
    auto* table = editor.findChild<RosterTableView*>(QStringLiteral("rosterTable"));
    QVERIFY(model);
    QVERIFY(table);

    QCOMPARE(model->rowCount(), 25);
    QCOMPARE(model->columnCount(), 7);
    QCOMPARE(model->columnName(5), QStringLiteral("Fall"));
    QCOMPARE(model->columnName(6), QStringLiteral("\u5099\u8003"));
    QCOMPARE(model->data(model->index(0, 0)).toString(), QStringLiteral("Alice"));
    QCOMPARE(model->data(model->index(0, 1)).toString(), QStringLiteral("\uAE40\uBBFC\uC9C0"));
    QCOMPARE(model->data(model->index(0, 2)).toString(), QStringLiteral("A+"));
    QCOMPARE(model->data(model->index(0, 6)).toString(), QStringLiteral("Review \U0001F4DA"));
    QVERIFY(model->data(model->index(24, 6)).toString().isEmpty());
    QCOMPARE(table->columnWidth(0), 211);
    QCOMPARE(table->columnWidth(5), 169);
    QCOMPARE(table->columnWidth(6), 242);
    QVERIFY(!editor.hasUnsavedChanges());
    QVERIFY(editor.outputCapabilities().printEnabled);
    QVERIFY(editor.outputCapabilities().saveAsEnabled);
}

void RosterEditorWidgetSaveTests::
emptyAndFailedReadsKeepBlankRosterAndRefreshOutputCapabilities()
{
    RosterEditorFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    RosterEditorWidget editor(&fixture.services);
    QSignalSpy capabilitiesSpy(
        &editor,
        &BasePage::outputCapabilitiesChanged
        );
    QVERIFY(capabilitiesSpy.isValid());
    editor.loadClass(Classroom(QStringLiteral("Empty roster"), fixture.classId));

    auto* model = editor.findChild<RosterModel*>();
    QVERIFY(model);
    QCOMPARE(model->rowCount(), 25);
    QCOMPARE(model->columnCount(), Roster::BaseColumns.size());
    QVERIFY(model->data(model->index(0, 0)).toString().isEmpty());
    QVERIFY(model->data(model->index(24, 5)).toString().isEmpty());
    QVERIFY(!editor.hasUnsavedChanges());
    QCOMPARE(capabilitiesSpy.count(), 1);
    QVERIFY(editor.outputCapabilities().printEnabled);

    fixture.services.closeDatabase();
    editor.loadClass(Classroom(QStringLiteral("Failed roster read"), fixture.classId));
    QCOMPARE(model->rowCount(), 25);
    QCOMPARE(model->columnCount(), Roster::BaseColumns.size());
    QVERIFY(model->data(model->index(0, 0)).toString().isEmpty());
    QVERIFY(model->data(model->index(24, 5)).toString().isEmpty());
    QVERIFY(!editor.hasUnsavedChanges());
    QCOMPARE(capabilitiesSpy.count(), 2);
    QVERIFY(editor.outputCapabilities().printEnabled);

    editor.clearDatabaseState();
    QCOMPARE(model->rowCount(), 25);
    QCOMPARE(model->columnCount(), Roster::BaseColumns.size());
    QVERIFY(model->data(model->index(0, 0)).toString().isEmpty());
    QVERIFY(!editor.hasUnsavedChanges());
    QCOMPARE(capabilitiesSpy.count(), 3);
    QVERIFY(!editor.outputCapabilities().printEnabled);
    QVERIFY(!editor.outputCapabilities().saveAsEnabled);
}


void RosterEditorWidgetSaveTests::rejectedQuestionableLengthKeepsFocusDirtyAndStorage()
{
    RosterEditorFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));
    FakeUserPromptService prompts;
    prompts.scriptedChoices.enqueue(PromptChoice::Rejected);
    ScopedPromptService promptScope(&prompts);
    RosterEditorWidget editor(&fixture.services);
    editor.loadClass(Classroom(QStringLiteral("Go Back"), fixture.classId));
    editor.setSaveMode(SaveMode::Manual);
    auto* model = editor.findChild<RosterModel*>();
    auto* table = editor.findChild<RosterTableView*>(QStringLiteral("rosterTable"));
    QVERIFY(model);
    QVERIFY(table);
    QVERIFY(setStudent(model, 0, QStringLiteral("Dana"), QStringLiteral("\uAE40")));
    const int korean = columnByName(model, QStringLiteral("Korean"));
    table->setCurrentIndex(model->index(3, 0));
    QVERIFY(!editor.saveChanges());
    QVERIFY(editor.hasUnsavedChanges());
    QCOMPARE(table->currentIndex(), model->index(0, korean));
    QVERIFY(!model->data(model->index(0, korean), Qt::ToolTipRole).toString().isEmpty());
    QCOMPARE(prompts.confirmations.size(), 1);
    const auto& confirmation = prompts.confirmations.constFirst();
    QCOMPARE(confirmation.title, QStringLiteral("Verify Korean Name Lengths"));
    QCOMPARE(confirmation.message, QStringLiteral(
        "These Korean names have 1 or 5+ syllables and may be incorrect:\n"
        "Row 1: \uAE40\n\nSave them anyway?"));
    QCOMPARE(confirmation.acceptText, QStringLiteral("Save Anyway"));
    QCOMPARE(confirmation.rejectText, QStringLiteral("Go Back"));
    QVERIFY(prompts.messages.isEmpty());
    const auto stored = fixture.services.rosterService()->roster(fixture.classId);
    QVERIFY(stored);
    QVERIFY(stored->rows.isEmpty());
}


void RosterEditorWidgetSaveTests::literalLeadingBomInNameRemainsInvalid()
{
    RosterEditorFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));
    FakeUserPromptService prompts;
    ScopedPromptService promptScope(&prompts);
    RosterEditorWidget editor(&fixture.services);
    editor.loadClass(Classroom(QStringLiteral("BOM preservation"), fixture.classId));
    editor.setSaveMode(SaveMode::Manual);
    auto* model = editor.findChild<RosterModel*>();
    auto* table = editor.findChild<RosterTableView*>(QStringLiteral("rosterTable"));
    QVERIFY(model);
    QVERIFY(table);
    const int korean = columnByName(model, QStringLiteral("Korean"));
    QVERIFY(model->setData(model->index(0, korean), QStringLiteral("\uAE40\uBBFC\uC9C0"), Qt::EditRole));
    for (const auto bom : {char16_t(0xfeff), char16_t(0xfffe)})
    {
        const QString malformed = QString(QChar(bom)) + QStringLiteral("Alice");
        const int english = columnByName(model, QStringLiteral("English"));
        QVERIFY(model->setData(model->index(0, english), malformed, Qt::EditRole));
        QCOMPARE(model->data(model->index(0, english)).toString(), malformed);
        QVERIFY(!editor.saveChanges());
        QCOMPARE(table->currentIndex(), model->index(0, english));
        QVERIFY(editor.hasUnsavedChanges());
        QVERIFY(!model->data(model->index(0, english), Qt::ToolTipRole).toString().isEmpty());
    }
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(prompts.messages.isEmpty());
    const auto stored = fixture.services.rosterService()->roster(fixture.classId);
    QVERIFY(stored);
    QVERIFY(stored->rows.isEmpty());
}

QTEST_MAIN(RosterEditorWidgetSaveTests)

#include "roster_editor_widget_save_tests.moc"
