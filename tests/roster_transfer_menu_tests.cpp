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
    void teacherReadFailureKeepsClassFieldsInDisplayLabel();
    void classFieldsReadFailureShowsNoSameGradeTargets();
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

QTEST_MAIN(RosterTransferMenuTests)

#include "roster_transfer_menu_tests.moc"
