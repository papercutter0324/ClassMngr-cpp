#include "app/services/feature_services.h"
#include "core/application_services.h"
#include "data/database/database_session.h"
#include "domain/models/class_info.h"
#include "domain/models/classroom.h"
#include "domain/models/roster.h"
#include "domain/models/teacher.h"
#include "features/roster/ui/roster_editor_widget.h"
#include "features/roster/ui/roster_model.h"
#include "features/roster/ui/roster_table_view.h"
#include "fakes/fake_user_prompt_service.h"
#include "ui/shared/dialogs/user_prompt_service.h"
#include "ui/shared/pages/autosave_coordinator.h"
#include "ui/shared/qt_text_adapter.h"
#include "next/application/roster_row_transfer_use_case.h"

#include <functional>
#include <optional>

#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest/QtTest>
#include <QUuid>

namespace
{

QString displayLabel(const QString& grade, const QString& level)
{
    return grade + QLatin1Char(' ') + level + QLatin1Char(' ')
        + QChar(0x2022) + QStringLiteral(" No Teacher");
}

struct RosterTransferMenuFixture final
{
    QTemporaryDir directory;
    ApplicationServices services;

    bool initialize(QString* error)
    {
        if (!directory.isValid())
        {
            *error = QStringLiteral("Temporary directory is invalid.");
            return false;
        }

        const QString path = directory.filePath(
            QStringLiteral("roster-transfer-menu-%1.tps").arg(
                QUuid::createUuid().toString(QUuid::WithoutBraces)
                )
            );
        const Status opened = services.openDatabase(path);
        if (!opened)
        {
            *error = opened.error();
            return false;
        }

        return true;
    }

    bool createClass(
        const QString& name,
        const QString& grade,
        const QString& level,
        int teacherId,
        int* createdId,
        QString* error
        )
    {
        const auto created = services.classService()->create(name);
        if (!created)
        {
            *error = created.error();
            return false;
        }

        ClassInfo info;
        info.classId = *created;
        info.teacherId = teacherId;
        info.classGrade = grade;
        info.classLevel = level;
        const Status saved = services.classService()->saveClassInfo(info);
        if (!saved)
        {
            *error = saved.error();
            return false;
        }

        *createdId = *created;
        return true;
    }

    bool addRosterRow(int classId, QString* error)
    {
        Roster roster;
        roster.columns = Roster::BaseColumns;
        roster.rows = {
            {
                QStringLiteral("Student"),
                QString::fromUtf16(u"\uAE40\uBBFC\uC9C0"),
                QString(),
                QString(),
                QString(),
                QString()
            }
        };
        const Status saved = services.rosterService()->saveRoster(
            classId,
            roster
            );
        if (!saved)
        {
            *error = saved.error();
            return false;
        }
        return true;
    }

    bool addFullRoster(int classId, QString* error)
    {
        Roster roster;
        roster.columns = Roster::BaseColumns;
        for (int rowIndex = 0; rowIndex < 25; ++rowIndex)
        {
            roster.rows.append({
                QStringLiteral("Student ")
                    + QChar(static_cast<ushort>(u'A' + rowIndex)),
                QString::fromUtf16(u"\uAE40\uBBFC\uC9C0"),
                QString(),
                QString(),
                QString(),
                QString()
            });
        }
        const Status saved = services.rosterService()->saveRoster(
            classId,
            roster
            );
        if (!saved)
        {
            *error = saved.error();
            return false;
        }
        return true;
    }
};

struct TransferMenuSnapshot final
{
    bool invocationSucceeded = false;
    bool transferMenuFound = false;
    QStringList labels;
    QList<bool> enabled;
};

TransferMenuSnapshot openTransferMenu(
    RosterEditorWidget& editor,
    RosterTableView* table
    )
{
    TransferMenuSnapshot snapshot;
    if (!table || !table->model())
    {
        return snapshot;
    }

    const QModelIndex firstCell = table->model()->index(0, 0);
    if (!firstCell.isValid())
    {
        return snapshot;
    }

    QTimer::singleShot(
        0,
        &editor,
        [&snapshot]
        {
            for (QWidget* widget : QApplication::topLevelWidgets())
            {
                auto* const menu = qobject_cast<QMenu*>(widget);
                if (!menu || !menu->isVisible())
                {
                    continue;
                }

                QMenu* transferMenu = nullptr;
                for (QAction* action : menu->actions())
                {
                    if (action->text() == QStringLiteral("Transfer Class"))
                    {
                        transferMenu = action->menu();
                        break;
                    }
                }

                if (!transferMenu)
                {
                    continue;
                }

                snapshot.transferMenuFound = true;
                for (QAction* action : transferMenu->actions())
                {
                    snapshot.labels.append(action->text());
                    snapshot.enabled.append(action->isEnabled());
                }
                menu->close();
                break;
            }
        }
        );

    snapshot.invocationSucceeded = QMetaObject::invokeMethod(
        &editor,
        "showRosterContextMenu",
        Qt::DirectConnection,
        Q_ARG(QPoint, table->visualRect(firstCell).center())
        );
    return snapshot;
}

struct TransferMenuActionSnapshot final
{
    bool invocationSucceeded = false;
    bool transferMenuFound = false;
    bool targetActionFound = false;
    bool targetActionEnabled = false;
    bool targetMenuVisible = false;
    bool targetRosterUpdateAttempted = false;
    bool targetRosterUpdated = false;
    bool targetActionTriggered = false;
    QStringList labels;
    QString targetRosterUpdateError;
};

TransferMenuActionSnapshot triggerTransferMenuAction(
    RosterEditorWidget& editor,
    RosterTableView* table,
    const QString& targetLabel,
    ApplicationServices& services,
    int targetClassId,
    const Roster& targetRosterAfterMenuSnapshot,
    const std::function<void()>& beforeActivation = {}
    )
{
    TransferMenuActionSnapshot snapshot;
    if (!table || !table->model())
    {
        return snapshot;
    }

    const QModelIndex firstCell = table->model()->index(0, 0);
    if (!firstCell.isValid())
    {
        return snapshot;
    }

    QTimer triggerTimer;
    triggerTimer.setSingleShot(true);
    QObject::connect(
        &triggerTimer,
        &QTimer::timeout,
        &editor,
        [&snapshot, &editor, &services, targetClassId,
         &targetRosterAfterMenuSnapshot, &beforeActivation, targetLabel]
        {
            QMenu* contextMenu = nullptr;
            QAction* transferRootAction = nullptr;
            for (QWidget* widget : QApplication::topLevelWidgets())
            {
                auto* const menu = qobject_cast<QMenu*>(widget);
                if (!menu || !menu->isVisible())
                {
                    continue;
                }

                for (QAction* action : menu->actions())
                {
                    if (action->text() == QStringLiteral("Transfer Class"))
                    {
                        contextMenu = menu;
                        transferRootAction = action;
                        break;
                    }
                }
                if (contextMenu)
                {
                    break;
                }
            }

            if (!contextMenu || !transferRootAction
                || !transferRootAction->menu())
            {
                return;
            }

            QMenu* const transferMenu = transferRootAction->menu();
            snapshot.transferMenuFound = true;
            QAction* targetAction = nullptr;
            for (QAction* action : transferMenu->actions())
            {
                snapshot.labels.append(action->text());
                if (action->text() == targetLabel)
                {
                    targetAction = action;
                }
            }

            if (!targetAction)
            {
                contextMenu->close();
                return;
            }

            snapshot.targetActionFound = true;
            snapshot.targetActionEnabled = targetAction->isEnabled();
            if (!snapshot.targetActionEnabled)
            {
                contextMenu->close();
                return;
            }

            QObject::connect(
                targetAction,
                &QAction::triggered,
                &editor,
                [&snapshot]
                {
                    snapshot.targetActionTriggered = true;
                }
                );

            const QRect rootActionRect =
                contextMenu->actionGeometry(transferRootAction);
            QTest::mouseMove(contextMenu, rootActionRect.center());
            QTest::mouseClick(
                contextMenu,
                Qt::LeftButton,
                Qt::NoModifier,
                rootActionRect.center()
                );
            if (!transferMenu->isVisible())
            {
                transferMenu->popup(
                    contextMenu->mapToGlobal(rootActionRect.topRight())
                    );
            }
            QApplication::processEvents();
            snapshot.targetMenuVisible = transferMenu->isVisible();
            if (!snapshot.targetMenuVisible)
            {
                contextMenu->close();
                return;
            }

            // Change the target after the menu snapshot; transfer must reread it.
            snapshot.targetRosterUpdateAttempted = true;
            const Status updated = services.rosterService()->saveRoster(
                targetClassId,
                targetRosterAfterMenuSnapshot
                );
            if (!updated)
            {
                snapshot.targetRosterUpdateError = updated.error();
                transferMenu->close();
                contextMenu->close();
                return;
            }
            snapshot.targetRosterUpdated = true;
            if (beforeActivation)
                beforeActivation();

            const QRect targetActionRect =
                transferMenu->actionGeometry(targetAction);
            QTest::mouseMove(transferMenu, targetActionRect.center());
            QTest::mouseClick(
                transferMenu,
                Qt::LeftButton,
                Qt::NoModifier,
                targetActionRect.center()
                );
        }
        );

    QTimer closeMenuTimer;
    closeMenuTimer.setSingleShot(true);
    QObject::connect(
        &closeMenuTimer,
        &QTimer::timeout,
        &editor,
        [&snapshot]
        {
            if (snapshot.targetActionTriggered)
            {
                return;
            }
            for (QWidget* widget : QApplication::topLevelWidgets())
            {
                auto* const menu = qobject_cast<QMenu*>(widget);
                if (menu && menu->isVisible())
                {
                    menu->close();
                }
            }
        }
        );

    triggerTimer.start(0);
    closeMenuTimer.start(4000);
    snapshot.invocationSucceeded = QMetaObject::invokeMethod(
        &editor,
        "showRosterContextMenu",
        Qt::DirectConnection,
        Q_ARG(QPoint, table->visualRect(firstCell).center())
        );
    triggerTimer.stop();
    closeMenuTimer.stop();
    return snapshot;
}

bool addTeacher(
    ApplicationServices& services,
    int* teacherId,
    QString* error
    )
{
    Teacher teacher;
    teacher.teacherEn = QStringLiteral("Teacher English");
    teacher.preferredName = teacher.teacherEn;
    const auto saved = services.teacherService()->save(teacher);
    if (!saved)
    {
        *error = saved.error();
        return false;
    }
    *teacherId = *saved;
    return true;
}

}

class RosterTransferMenuTests final : public QObject
{
    Q_OBJECT

private slots:
    void cleanup();
    void sameGradeTargetsAreSortedAndDifferentOrEmptyGradesAreExcluded();
    void classListReadFailureWarnsAndShowsNoSameGradeTargets();
    void teacherReadFailureKeepsClassFieldsInDisplayLabel();
    void classFieldsReadFailureShowsNoSameGradeTargets();
    void fullSameGradeTargetIsLabeledAndDisabled();
    void targetRosterReadFailureBehavesLikeEmptyRoster();
    void targetRosterCellReadFailureBehavesLikeEmptyRoster();
    void menuTransferUsesFreshTargetRosterAndPreservesCustomColumnsAndWidths();
    void applyFailureKeepsSourceAndAutosaveState_data();
    void applyFailureKeepsSourceAndAutosaveState();
    void workflowProjectionMatchesRosterModel();
};

void RosterTransferMenuTests::cleanup()
{
    DialogServices::setUserPromptServiceForTesting(nullptr);
}

void RosterTransferMenuTests::
sameGradeTargetsAreSortedAndDifferentOrEmptyGradesAreExcluded()
{
    RosterTransferMenuFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    int sourceId = 0;
    int betaId = 0;
    int alphaId = 0;
    int differentGradeId = 0;
    int emptyGradeId = 0;
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Source database name"),
                 QStringLiteral("E4"),
                 QStringLiteral("Perseus"),
                 -1,
                 &sourceId,
                 &error
                 ), qPrintable(error));
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Zulu database name"),
                 QStringLiteral("E4"),
                 QStringLiteral("Theseus"),
                 -1,
                 &betaId,
                 &error
                 ), qPrintable(error));
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Alpha database name"),
                 QStringLiteral("E4"),
                 QStringLiteral("Hercules"),
                 -1,
                 &alphaId,
                 &error
                 ), qPrintable(error));
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Other grade database name"),
                 QStringLiteral("E5"),
                 QStringLiteral("Apollo"),
                 -1,
                 &differentGradeId,
                 &error
                 ), qPrintable(error));
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Empty grade database name"),
                 QString(),
                 QString(),
                 -1,
                 &emptyGradeId,
                 &error
                 ), qPrintable(error));
    QVERIFY(betaId > 0);
    QVERIFY(alphaId > 0);
    QVERIFY(differentGradeId > 0);
    QVERIFY(emptyGradeId > 0);
    QVERIFY2(fixture.addRosterRow(sourceId, &error), qPrintable(error));

    RosterEditorWidget editor(&fixture.services);
    editor.loadClass(Classroom(QStringLiteral("Source page name"), sourceId));
    editor.show();
    QApplication::processEvents();
    auto* const table = editor.findChild<RosterTableView*>(
        QStringLiteral("rosterTable")
        );
    QVERIFY(table);

    const TransferMenuSnapshot menu = openTransferMenu(editor, table);
    QVERIFY(menu.invocationSucceeded);
    QVERIFY(menu.transferMenuFound);
    QCOMPARE(
        menu.labels,
        QStringList({
            displayLabel(QStringLiteral("E4"), QStringLiteral("Hercules")),
            displayLabel(QStringLiteral("E4"), QStringLiteral("Theseus"))
        })
        );
    QCOMPARE(menu.enabled, QList<bool>({true, true}));
}

void RosterTransferMenuTests::
classListReadFailureWarnsAndShowsNoSameGradeTargets()
{
    RosterTransferMenuFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    int sourceId = 0;
    int targetId = 0;
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Source database name"),
                 QStringLiteral("E4"),
                 QStringLiteral("Perseus"),
                 -1,
                 &sourceId,
                 &error
                 ), qPrintable(error));
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Target database name"),
                 QStringLiteral("E4"),
                 QStringLiteral("Hercules"),
                 -1,
                 &targetId,
                 &error
                 ), qPrintable(error));
    QVERIFY(targetId > 0);
    QVERIFY2(fixture.addRosterRow(sourceId, &error), qPrintable(error));

    // Fail only the class-list query. Class and class-field metadata remain
    // available, so the caller reaches the list read with usable source data.
    QSqlQuery dropTestingClasses(
        fixture.services.databaseSession()->database()
        );
    QVERIFY2(
        dropTestingClasses.exec(QStringLiteral("DROP TABLE testing_classes")),
             qPrintable(dropTestingClasses.lastError().text()));

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    RosterEditorWidget editor(&fixture.services);
    editor.loadClass(Classroom(QStringLiteral("Source page name"), sourceId));
    editor.show();
    QApplication::processEvents();
    auto* const table = editor.findChild<RosterTableView*>(
        QStringLiteral("rosterTable")
        );
    QVERIFY(table);

    const TransferMenuSnapshot menu = openTransferMenu(editor, table);
    QVERIFY(menu.invocationSucceeded);
    QVERIFY(menu.transferMenuFound);
    QCOMPARE(
        menu.labels,
        QStringList({QStringLiteral("No same-grade classes")})
        );
    QCOMPARE(menu.enabled, QList<bool>({false}));

    QCOMPARE(prompts.messages.size(), 1);
    const PromptRequest& warning = prompts.messages.constFirst();
    QCOMPARE(warning.severity, PromptSeverity::Warning);
    QCOMPARE(warning.title, QStringLiteral("Transfer Student"));
    QCOMPARE(
        warning.message,
        QStringLiteral("Transfer classes could not be loaded.")
        );
    QVERIFY(!warning.details.trimmed().isEmpty());
    QVERIFY(prompts.asynchronousMessages.isEmpty());
}

void RosterTransferMenuTests::
teacherReadFailureKeepsClassFieldsInDisplayLabel()
{
    RosterTransferMenuFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    int teacherId = 0;
    QVERIFY2(addTeacher(fixture.services, &teacherId, &error), qPrintable(error));
    int sourceId = 0;
    int targetId = 0;
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Source stored name"),
                 QStringLiteral("E4"),
                 QStringLiteral("Perseus"),
                 teacherId,
                 &sourceId,
                 &error
                 ), qPrintable(error));
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Fallback stored name"),
                 QStringLiteral("E4"),
                 QStringLiteral("Theseus"),
                 teacherId,
                 &targetId,
                 &error
                 ), qPrintable(error));
    QVERIFY(targetId > 0);
    QVERIFY2(fixture.addRosterRow(sourceId, &error), qPrintable(error));

    QSqlQuery disableForeignKeys(fixture.services.databaseSession()->database());
    QVERIFY2(disableForeignKeys.exec(QStringLiteral("PRAGMA foreign_keys=OFF")),
             qPrintable(disableForeignKeys.lastError().text()));
    QSqlQuery dropTeachers(fixture.services.databaseSession()->database());
    QVERIFY2(dropTeachers.exec(QStringLiteral("DROP TABLE teachers")),
             qPrintable(dropTeachers.lastError().text()));

    RosterEditorWidget editor(&fixture.services);
    editor.loadClass(Classroom(QStringLiteral("Source page name"), sourceId));
    editor.show();
    QApplication::processEvents();
    auto* const table = editor.findChild<RosterTableView*>(
        QStringLiteral("rosterTable")
        );
    QVERIFY(table);

    const TransferMenuSnapshot menu = openTransferMenu(editor, table);
    QVERIFY(menu.invocationSucceeded);
    QVERIFY(menu.transferMenuFound);
    QCOMPARE(
        menu.labels,
        QStringList({displayLabel(
            QStringLiteral("E4"),
            QStringLiteral("Theseus")
        )})
        );
    QCOMPARE(menu.enabled, QList<bool>({true}));
}

void RosterTransferMenuTests::
classFieldsReadFailureShowsNoSameGradeTargets()
{
    RosterTransferMenuFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    int sourceId = 0;
    int targetId = 0;
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Source database name"),
                 QStringLiteral("E4"),
                 QStringLiteral("Perseus"),
                 -1,
                 &sourceId,
                 &error
                 ), qPrintable(error));
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Target database name"),
                 QStringLiteral("E4"),
                 QStringLiteral("Hercules"),
                 -1,
                 &targetId,
                 &error
                 ), qPrintable(error));
    QVERIFY(targetId > 0);
    QVERIFY2(fixture.addRosterRow(sourceId, &error), qPrintable(error));

    QSqlQuery dropTimes(fixture.services.databaseSession()->database());
    QVERIFY2(dropTimes.exec(QStringLiteral("DROP TABLE class_times")),
             qPrintable(dropTimes.lastError().text()));

    RosterEditorWidget editor(&fixture.services);
    editor.loadClass(Classroom(QStringLiteral("Source page name"), sourceId));
    editor.show();
    QApplication::processEvents();
    auto* const table = editor.findChild<RosterTableView*>(
        QStringLiteral("rosterTable")
        );
    QVERIFY(table);

    const TransferMenuSnapshot menu = openTransferMenu(editor, table);
    QVERIFY(menu.invocationSucceeded);
    QVERIFY(menu.transferMenuFound);
    QCOMPARE(menu.labels, QStringList({QStringLiteral("No same-grade classes")}));
    QCOMPARE(menu.enabled, QList<bool>({false}));
}

void RosterTransferMenuTests::fullSameGradeTargetIsLabeledAndDisabled()
{
    RosterTransferMenuFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    int sourceId = 0;
    int targetId = 0;
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Source database name"),
                 QStringLiteral("E4"),
                 QStringLiteral("Perseus"),
                 -1,
                 &sourceId,
                 &error
                 ), qPrintable(error));
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Full target database name"),
                 QStringLiteral("E4"),
                 QStringLiteral("Theseus"),
                 -1,
                 &targetId,
                 &error
                 ), qPrintable(error));
    QVERIFY(targetId > 0);
    QVERIFY2(fixture.addRosterRow(sourceId, &error), qPrintable(error));
    QVERIFY2(fixture.addFullRoster(targetId, &error), qPrintable(error));

    RosterEditorWidget editor(&fixture.services);
    editor.loadClass(Classroom(QStringLiteral("Source page name"), sourceId));
    editor.show();
    QApplication::processEvents();
    auto* const table = editor.findChild<RosterTableView*>(
        QStringLiteral("rosterTable")
        );
    QVERIFY(table);

    const TransferMenuSnapshot menu = openTransferMenu(editor, table);
    QVERIFY(menu.invocationSucceeded);
    QVERIFY(menu.transferMenuFound);
    QCOMPARE(
        menu.labels,
        QStringList({
            displayLabel(QStringLiteral("E4"), QStringLiteral("Theseus"))
                + QStringLiteral(" (full)")
        })
        );
    QCOMPARE(menu.enabled, QList<bool>({false}));
}

void RosterTransferMenuTests::
targetRosterReadFailureBehavesLikeEmptyRoster()
{
    RosterTransferMenuFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    int sourceId = 0;
    int targetId = 0;
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Source database name"),
                 QStringLiteral("E4"),
                 QStringLiteral("Perseus"),
                 -1,
                 &sourceId,
                 &error
                 ), qPrintable(error));
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Target database name"),
                 QStringLiteral("E4"),
                 QStringLiteral("Theseus"),
                 -1,
                 &targetId,
                 &error
                 ), qPrintable(error));
    QVERIFY(targetId > 0);
    QVERIFY2(fixture.addRosterRow(sourceId, &error), qPrintable(error));

    RosterEditorWidget editor(&fixture.services);
    editor.loadClass(Classroom(QStringLiteral("Source page name"), sourceId));
    editor.show();
    QApplication::processEvents();
    auto* const table = editor.findChild<RosterTableView*>(
        QStringLiteral("rosterTable")
        );
    QVERIFY(table);
    auto* const sourceModel = qobject_cast<RosterModel*>(table->model());
    QVERIFY(sourceModel);
    QCOMPARE(sourceModel->firstEmptyRow(), 1);

    QSqlQuery dropRosterColumns(
        fixture.services.databaseSession()->database()
        );
    QVERIFY2(dropRosterColumns.exec(QStringLiteral("DROP TABLE roster_columns")),
             qPrintable(dropRosterColumns.lastError().text()));

    const TransferMenuSnapshot menu = openTransferMenu(editor, table);
    QVERIFY(menu.invocationSucceeded);
    QVERIFY(menu.transferMenuFound);
    QCOMPARE(
        menu.labels,
        QStringList({
            displayLabel(QStringLiteral("E4"), QStringLiteral("Theseus"))
        })
        );
    QCOMPARE(menu.enabled, QList<bool>({true}));
}

void RosterTransferMenuTests::
targetRosterCellReadFailureBehavesLikeEmptyRoster()
{
    RosterTransferMenuFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    int sourceId = 0;
    int targetId = 0;
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Source database name"),
                 QStringLiteral("E4"),
                 QStringLiteral("Perseus"),
                 -1,
                 &sourceId,
                 &error
                 ), qPrintable(error));
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Target database name"),
                 QStringLiteral("E4"),
                 QStringLiteral("Theseus"),
                 -1,
                 &targetId,
                 &error
                 ), qPrintable(error));
    QVERIFY(targetId > 0);
    QVERIFY2(fixture.addRosterRow(sourceId, &error), qPrintable(error));

    RosterEditorWidget editor(&fixture.services);
    editor.loadClass(Classroom(QStringLiteral("Source page name"), sourceId));
    editor.show();
    QApplication::processEvents();
    auto* const table = editor.findChild<RosterTableView*>(
        QStringLiteral("rosterTable")
        );
    QVERIFY(table);

    // Column metadata succeeds; the streamed cell query fails. The prior
    // menu behavior treated an unreadable target roster as empty.
    QSqlQuery dropRosterData(
        fixture.services.databaseSession()->database()
        );
    QVERIFY2(dropRosterData.exec(QStringLiteral("DROP TABLE roster_data")),
             qPrintable(dropRosterData.lastError().text()));

    const TransferMenuSnapshot menu = openTransferMenu(editor, table);
    QVERIFY(menu.invocationSucceeded);
    QVERIFY(menu.transferMenuFound);
    QCOMPARE(
        menu.labels,
        QStringList({
            displayLabel(QStringLiteral("E4"), QStringLiteral("Theseus"))
        })
        );
    QCOMPARE(menu.enabled, QList<bool>({true}));
}

void RosterTransferMenuTests::
menuTransferUsesFreshTargetRosterAndPreservesCustomColumnsAndWidths()
{
    RosterTransferMenuFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));

    int sourceId = 0;
    int targetId = 0;
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Source database name"),
                 QStringLiteral("E4"),
                 QStringLiteral("Perseus"),
                 -1,
                 &sourceId,
                 &error
                 ), qPrintable(error));
    QVERIFY2(fixture.createClass(
                 QStringLiteral("Target database name"),
                 QStringLiteral("E4"),
                 QStringLiteral("Theseus"),
                 -1,
                 &targetId,
                 &error
                 ), qPrintable(error));
    QVERIFY(sourceId > 0);
    QVERIFY(targetId > 0);

    const QStringList sourceRow{
        QStringLiteral("Transferred Student"),
        QString::fromUtf16(u"\uAE40\uBBFC\uC9C0"),
        QStringLiteral("Winter source"),
        QStringLiteral("Contest source"),
        QStringLiteral("Summer source"),
        QStringLiteral("Fall source")
    };
    Roster sourceRoster;
    sourceRoster.columns = Roster::BaseColumns;
    sourceRoster.columnWidths = {113, 127, 139, 149, 157, 163};
    sourceRoster.rows.append(sourceRow);
    const Status sourceSaved = fixture.services.rosterService()->saveRoster(
        sourceId,
        sourceRoster
        );
    QVERIFY(sourceSaved);

    Roster targetRosterAfterMenuSnapshot;
    targetRosterAfterMenuSnapshot.columns = Roster::BaseColumns;
    targetRosterAfterMenuSnapshot.columns.append(
        QStringLiteral("Advisor Notes")
        );
    targetRosterAfterMenuSnapshot.columnWidths = {
        211, 223, 227, 229, 233, 239, 271
    };
    const QStringList targetExistingRow{
        QStringLiteral("Existing Student"),
        QString::fromUtf16(u"\uBC15\uC9C0\uBBFC"),
        QStringLiteral("Winter target"),
        QStringLiteral("Contest target"),
        QStringLiteral("Summer target"),
        QStringLiteral("Fall target"),
        QStringLiteral("Keep this advisor note")
    };
    targetRosterAfterMenuSnapshot.rows.append(targetExistingRow);

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    RosterEditorWidget editor(&fixture.services);
    editor.loadClass(Classroom(QStringLiteral("Source page name"), sourceId));
    editor.show();
    QApplication::processEvents();
    auto* const table = editor.findChild<RosterTableView*>(
        QStringLiteral("rosterTable")
        );
    QVERIFY(table);

    const QString targetLabel = displayLabel(
        QStringLiteral("E4"),
        QStringLiteral("Theseus")
        );
    const TransferMenuActionSnapshot action = triggerTransferMenuAction(
        editor,
        table,
        targetLabel,
        fixture.services,
        targetId,
        targetRosterAfterMenuSnapshot
        );
    QVERIFY(action.invocationSucceeded);
    QVERIFY(action.transferMenuFound);
    QCOMPARE(action.labels, QStringList({targetLabel}));
    QVERIFY(action.targetActionFound);
    QVERIFY(action.targetActionEnabled);
    QVERIFY(action.targetMenuVisible);
    QVERIFY(action.targetRosterUpdateAttempted);
    QVERIFY2(action.targetRosterUpdated,
             qPrintable(action.targetRosterUpdateError));
    QVERIFY(action.targetActionTriggered);
    QVERIFY(prompts.messages.isEmpty());
    QVERIFY(prompts.asynchronousMessages.isEmpty());
    QVERIFY(prompts.confirmations.isEmpty());
    QVERIFY(QApplication::activeModalWidget() == nullptr);

    auto* const sourceModel = qobject_cast<RosterModel*>(table->model());
    QVERIFY(sourceModel);
    QCOMPARE(sourceModel->firstEmptyRow(), 0);
    const auto sourceAfter = fixture.services.rosterService()->roster(sourceId);
    QVERIFY(sourceAfter);
    for (const QStringList& row : sourceAfter->rows)
    {
        for (const QString& cell : row)
        {
            QVERIFY(cell.isEmpty());
        }
    }

    const auto targetAfter = fixture.services.rosterService()->roster(targetId);
    QVERIFY(targetAfter);
    QCOMPARE(
        targetAfter->columns,
        targetRosterAfterMenuSnapshot.columns
        );
    QCOMPARE(
        targetAfter->columnWidths,
        targetRosterAfterMenuSnapshot.columnWidths
        );
    QVERIFY(targetAfter->rows.size() >= 2);
    QCOMPARE(targetAfter->rows.at(0), targetExistingRow);
    QStringList expectedTransferredRow = sourceRow;
    expectedTransferredRow.append(QString());
    QCOMPARE(targetAfter->rows.at(1), expectedTransferredRow);
}


void RosterTransferMenuTests::applyFailureKeepsSourceAndAutosaveState_data()
{
    QTest::addColumn<bool>("failRead");
    QTest::newRow("fresh read failure") << true;
    QTest::newRow("second save rollback") << false;
}

void RosterTransferMenuTests::applyFailureKeepsSourceAndAutosaveState()
{
    QFETCH(bool, failRead);
    RosterTransferMenuFixture fixture;
    QString error;
    QVERIFY2(fixture.initialize(&error), qPrintable(error));
    int sourceId = 0;
    int targetId = 0;
    QVERIFY2(fixture.createClass("Source", "E4", "Perseus", -1, &sourceId, &error), qPrintable(error));
    QVERIFY2(fixture.createClass("Target", "E4", "Theseus", -1, &targetId, &error), qPrintable(error));
    QVERIFY2(fixture.addRosterRow(sourceId, &error), qPrintable(error));
    const auto storedSource = fixture.services.rosterService()->roster(sourceId);
    QVERIFY(storedSource);

    FakeUserPromptService prompts;
    DialogServices::setUserPromptServiceForTesting(&prompts);
    RosterEditorWidget editor(&fixture.services);
    editor.loadClass(Classroom("Source", sourceId));
    editor.show();
    QApplication::processEvents();
    auto* table = editor.findChild<RosterTableView*>("rosterTable");
    QVERIFY(table);
    auto* model = qobject_cast<RosterModel*>(table->model());
    auto* autosave = editor.findChild<AutosaveCoordinator*>();
    QVERIFY(model);
    QVERIFY(autosave);
    autosave->setDebounceInterval(60'000);
    QVERIFY(model->setData(model->index(0, 2), "Unsaved edit", Qt::EditRole));
    QVERIFY(model->isDirty());
    QVERIFY(editor.hasUnsavedChanges());
    QVERIFY(autosave->isDirty());
    const auto modelBefore = model->toRoster();
    auto* timer = autosave->findChild<QTimer*>();
    QVERIFY(timer);
    QVERIFY(timer->isActive());
    const int intervalBefore = timer->interval();
    table->setCurrentIndex(model->index(0, 0));
    const auto selectionBefore = table->currentIndex();
    QSignalSpy cleanSpy(autosave, &AutosaveCoordinator::dirtyChanged);

    Roster target;
    target.columns = Roster::BaseColumns;
    target.rows = {{"Target Student", QString::fromUtf16(u"\uBC15\uC9C0\uBBFC"), "", "", "", ""}};
    bool faultInstalled = false;
    QString faultError;
    const auto action = triggerTransferMenuAction(editor, table,
        displayLabel("E4", "Theseus"), fixture.services, targetId, target, [&]
        {
            QSqlQuery query(fixture.services.databaseSession()->database());
            faultInstalled = query.exec(failRead
                ? QStringLiteral("ALTER TABLE roster_columns RENAME TO unavailable_roster_columns")
                : QStringLiteral("CREATE TRIGGER reject_target_transfer BEFORE INSERT ON roster_data "
                    "WHEN NEW.class_id = %1 BEGIN SELECT RAISE(ABORT, 'second roster save rejected'); END;")
                    .arg(targetId));
            faultError = query.lastError().text();
        });
    QVERIFY2(faultInstalled, qPrintable(faultError));
    QVERIFY(action.targetActionTriggered);
    QCOMPARE(prompts.messages.size(), 1);
    QCOMPARE(prompts.messages[0].title, QStringLiteral("Cannot Transfer Student"));
    QVERIFY(!prompts.messages[0].message.isEmpty());
    QVERIFY(prompts.confirmations.isEmpty());
    QCOMPARE(model->toRoster().columns, modelBefore.columns);
    QCOMPARE(model->toRoster().rows, modelBefore.rows);
    QVERIFY(model->isDirty());
    QVERIFY(editor.hasUnsavedChanges());
    QVERIFY(autosave->isDirty());
    QCOMPARE(cleanSpy.size(), 0);
    QVERIFY(timer->isActive());
    QCOMPARE(timer->interval(), intervalBefore);
    QCOMPARE(table->currentIndex(), selectionBefore);

    if (failRead)
    {
        QSqlQuery restore(fixture.services.databaseSession()->database());
        QVERIFY2(restore.exec("ALTER TABLE unavailable_roster_columns RENAME TO roster_columns"),
            qPrintable(restore.lastError().text()));
    }
    const auto sourceAfter = fixture.services.rosterService()->roster(sourceId);
    const auto targetAfter = fixture.services.rosterService()->roster(targetId);
    QVERIFY(sourceAfter);
    QVERIFY(targetAfter);
    QCOMPARE(sourceAfter->columns, storedSource->columns);
    QCOMPARE(sourceAfter->rows, storedSource->rows);
    QCOMPARE(targetAfter->columns, target.columns);
    QCOMPARE(targetAfter->rows, target.rows);
}

void RosterTransferMenuTests::workflowProjectionMatchesRosterModel()
{
    using namespace ClassMngr::Next;
    using namespace Application;
    struct Ports final : RosterReadPort, RosterRowTransferSavePort
    {
        RosterSnapshot stored;
        mutable std::optional<RosterRowTransferSaveRequest> saved;
        RosterReadResult readRoster(const RosterReadQuery&) const override
        { return RosterReadResult::success(stored); }
        Domain::Result<void> saveTransfer(const RosterRowTransferSaveRequest& request) const override
        { saved = request; return Domain::Result<void>::success(); }
    } ports;
    const auto snapshot = [](const Roster& roster)
    {
        RosterSnapshot result;
        for (const auto& column : roster.columns)
            result.columns.push_back(Ui::QtTextAdapter::toUtf16String(column));
        for (const auto& row : roster.rows)
        {
            std::vector<std::u16string> values;
            for (const auto& cell : row)
                values.push_back(Ui::QtTextAdapter::toUtf16String(cell));
            result.rows.push_back(std::move(values));
        }
        for (int width : roster.columnWidths) result.columnWidths.push_back(width);
        return result;
    };
    const auto equals = [](std::u16string_view a, std::u16string_view b)
    { return Ui::QtTextAdapter::fromUtf16String(a).compare(
        Ui::QtTextAdapter::fromUtf16String(b), Qt::CaseInsensitive) == 0; };

    Roster stored;
    stored.columns = {" Advisor\t Notes ", "korean", Ui::QtTextAdapter::fromUtf16String(u"\uFEFFAutumn"), "English", "advisor notes", "Extra"};
    stored.columnWidths = {201,202,203,204,205,206};
    // Pin target normalization against the model, including ASCII-regex and
    // Unicode whitespace differences in Korean names and malformed text.
    stored.rows = {
        {" note\t with  spaces ", QString::fromUtf16(u" \uBC15 \uC9C0\uBBFC(a) "), " fall\t value ", " oTHER  STUDENT ", "ignored", " preserve  me "},
        {"", QString::fromUtf16(u"\uAE40\u00a0\uBBFC\uC9C0"), "", QString::fromUtf16(u"\u00e9 bad"), "", ""},
        {"", "", "", "", "", ""}
    };
    while (stored.rows.size() < 27)
        stored.rows.append({"tail", "", "", "", "", ""});
    stored.rows[0][5] = Ui::QtTextAdapter::fromUtf16String(u"\uFEFF\uFEFF keep  BOM ");
    stored.rows[1][5] = Ui::QtTextAdapter::fromUtf16String(u"\uFFFE\u4100");
    ports.stored = snapshot(stored);
    const auto before = ports.stored;
    Roster decoded = stored;
    for (auto& column : decoded.columns)
        column = QString::fromStdU16String(Ui::QtTextAdapter::toUtf16String(column));
    for (auto& row : decoded.rows)
        for (auto& cell : row)
            cell = QString::fromStdU16String(Ui::QtTextAdapter::toUtf16String(cell));
    RosterModel expected;
    expected.setRoster(decoded);
    const QStringList sourceRow{"Transferred", QString::fromUtf16(u"\uAE40\uBBFC\uC9C0"), "", "", "", "Source fall"};
    QVERIFY(expected.insertTransferredRow(Roster::BaseColumns, sourceRow));
    const auto expectedRoster = snapshot(expected.toRoster());
    Roster source;
    source.columns = Roster::BaseColumns;
    source.rows = {sourceRow};
    const auto result = RosterRowTransferUseCase::execute({
        *Domain::ClassId::fromString("1"), *Domain::ClassId::fromString("2"),
        snapshot(source), 0, snapshot(source).columns}, ports, ports, equals);
    QVERIFY(std::holds_alternative<RosterRowTransferSuccess>(result));
    QVERIFY(ports.saved);
    QCOMPARE(ports.saved->targetRoster.columns, expectedRoster.columns);
    QCOMPARE(ports.saved->targetRoster.rows, expectedRoster.rows);
    QCOMPARE(ports.saved->targetRoster.rows.size(), std::size_t(25));
    QCOMPARE(ports.saved->targetRoster.columnWidths,
        (std::vector<int>{204,202,0,0,0,203,205,206}));
    QVERIFY(ports.stored == before);
}

QTEST_MAIN(RosterTransferMenuTests)

#include "roster_transfer_menu_tests.moc"
