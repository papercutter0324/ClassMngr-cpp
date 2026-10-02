#include "features/roster/ui/roster_model.h"

#include "domain/models/roster.h"
#include "features/roster/ui/roster_constants.h"

#include <QCoreApplication>
#include <QtTest>

class RosterModelTests : public QObject
{
    Q_OBJECT

private slots:
    void removeRosterRowShiftsRowsAndClearsLastSlot();
    void removeRosterRowRejectsEmptyRows();
    void removeRosterRowRejectsInvalidAndWhitespaceOnlyRows();
    void removeRosterRowRefreshesValidationAndEmitsModelChanges();
    void moveRosterRowMovesSourceToLaterDestination();
    void moveRosterRowMovesSourceToEarlierDestination();
    void moveRosterRowRejectsEmptyAndSameRows();
    void moveRosterRowRefreshesValidationAndEmitsModelChanges();
    void insertTransferredRowUsesFirstEmptyRow();
    void insertTransferredRowCopiesOnlyMatchingColumns();
    void insertTransferredRowRejectsFullTargetRoster();
    void transferredRowDetectsDuplicateStudentPair();
    void namePairHelpersDetectDuplicatesAndSuggestSuffix();
    void duplicatePairValidationTrimsNamesAndSkipsIncompleteRows();
    void structuredValidationMarksAndClearsAffectedCells();
};

namespace
{

QStringList row(
    const QString& english,
    const QString& winter
    )
{
    return {
        english,
        QString(),
        winter,
        QString(),
        QString(),
        QString()
    };
}

QStringList studentRow(
    const QString& english,
    const QString& korean,
    const QString& winter = QString(),
    const QString& custom = QString()
    )
{
    QStringList values{
        english,
        korean,
        winter,
        QString(),
        QString(),
        QString()
    };

    if (!custom.isNull())
    {
        values.append(custom);
    }

    return values;
}

} // namespace

void RosterModelTests::removeRosterRowShiftsRowsAndClearsLastSlot()
{
    Roster roster;
    roster.columns =
        Roster::BaseColumns;
    roster.columns.append(QStringLiteral("Review"));
    roster.rows = {
        row(
            QStringLiteral("Amy"),
            QStringLiteral("A")
            ),
        row(
            QStringLiteral("Ben"),
            QStringLiteral("B")
            ),
        row(
            QStringLiteral("Cal"),
            QStringLiteral("C")
            )
    };
    roster.rows[0].append(QStringLiteral("Note Amy"));
    roster.rows[1].append(QStringLiteral("Note Ben"));
    roster.rows[2].append(QStringLiteral("Note Cal"));

    RosterModel model;
    model.setRoster(roster);

    QSignalSpy dirtySpy(
        &model,
        &RosterModel::dirtyChanged
        );
    QSignalSpy changedSpy(
        &model,
        &QAbstractItemModel::dataChanged
        );

    QVERIFY(
        model.canRemoveRow(1)
        );
    QVERIFY(
        model.removeRosterRow(1)
        );

    QCOMPARE(
        model
            .index(
                1,
                model.englishNameColumn()
                )
            .data(Qt::DisplayRole)
            .toString(),
        QStringLiteral("Cal")
        );

    QCOMPARE(
        model
            .index(
                1,
                2
                )
            .data(Qt::DisplayRole)
            .toString(),
        QStringLiteral("C")
        );
    QCOMPARE(model.columnCount(), 7);
    QCOMPARE(
        model.rowValues(1),
        QStringList({
            QStringLiteral("Cal"),
            QString(),
            QStringLiteral("C"),
            QString(),
            QString(),
            QString(),
            QStringLiteral("Note Cal")
        })
        );

    QCOMPARE(
        model
            .index(
                2,
                model.englishNameColumn()
                )
            .data(Qt::DisplayRole)
            .toString(),
        QString()
        );

    QCOMPARE(
        model
            .index(
                RosterUi::RowCount - 1,
                model.englishNameColumn()
                )
            .data(Qt::DisplayRole)
            .toString(),
        QString()
        );

    QVERIFY(
        model.isDirty()
        );
    QCOMPARE(
        dirtySpy.count(),
        1
        );
    QCOMPARE(
        dirtySpy.first().at(0).toBool(),
        true
        );
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(changedSpy.constFirst().at(0).value<QModelIndex>(), model.index(0, 0));
    QCOMPARE(
        changedSpy.constFirst().at(1).value<QModelIndex>(),
        model.index(model.rowCount() - 1, model.columnCount() - 1)
        );
    for (int column = 0; column < model.columnCount(); ++column)
    {
        QVERIFY(model.rowValues(model.rowCount() - 1).at(column).isEmpty());
    }
}

void RosterModelTests::removeRosterRowRejectsEmptyRows()
{
    RosterModel model;

    QString reason;

    QVERIFY(
        !model.canRemoveRow(
            0,
            &reason
            )
        );
    QVERIFY(
        !reason.isEmpty()
        );
    QVERIFY(
        !model.removeRosterRow(0)
        );
    QVERIFY(
        !model.isDirty()
        );
}

void RosterModelTests::removeRosterRowRejectsInvalidAndWhitespaceOnlyRows()
{
    Roster roster;
    roster.columns = Roster::BaseColumns;
    roster.columns.append(QStringLiteral("Review"));
    roster.rows.append(QStringList(7, QString()));
    roster.rows[0][6] = QStringLiteral("\u3000\u00A0\u2028");

    RosterModel model;
    model.setRoster(roster);

    QString reason;
    QVERIFY(!model.canRemoveRow(-1, &reason));
    QCOMPARE(reason, QStringLiteral("Select a student row to remove."));
    QVERIFY(!model.removeRosterRow(-1));

    reason.clear();
    QVERIFY(!model.canRemoveRow(model.rowCount(), &reason));
    QCOMPARE(reason, QStringLiteral("Select a student row to remove."));
    QVERIFY(!model.removeRosterRow(model.rowCount()));

    reason.clear();
    QVERIFY(!model.canRemoveRow(0, &reason));
    QCOMPARE(reason, QStringLiteral("Selected row is already empty."));
    QVERIFY(!model.removeRosterRow(0));
    QVERIFY(!model.isDirty());
    QVERIFY(model.rowValues(0).at(6).isEmpty());
}

void RosterModelTests::removeRosterRowRefreshesValidationAndEmitsModelChanges()
{
    Roster roster;
    roster.columns = Roster::BaseColumns;
    roster.rows = {
        studentRow(
            QStringLiteral("Amy"),
            QStringLiteral("\uAE40\uBBFC\uC9C0")
            ),
        studentRow(
            QStringLiteral("Ben"),
            QStringLiteral("\uC774")
            )
    };

    RosterModel model;
    model.setRoster(roster);
    const int koreanColumn = model.koreanNameColumn();
    QVERIFY(koreanColumn >= 0);
    QVERIFY(
        model.errorsForCell(1, koreanColumn).contains(
            QStringLiteral("Korean name has 1 or 5+ syllables. Verify it is correct.")
            )
        );

    QSignalSpy dirtySpy(&model, &RosterModel::dirtyChanged);
    QSignalSpy changedSpy(&model, &QAbstractItemModel::dataChanged);
    QVERIFY(dirtySpy.isValid());
    QVERIFY(changedSpy.isValid());

    QVERIFY(model.removeRosterRow(0));

    QCOMPARE(model.rowValues(0).at(0), QStringLiteral("Ben"));
    QVERIFY(
        model.errorsForCell(0, koreanColumn).contains(
            QStringLiteral("Korean name has 1 or 5+ syllables. Verify it is correct.")
            )
        );
    QVERIFY(!model.errorsForCell(1, koreanColumn).contains(
        QStringLiteral("Korean name has 1 or 5+ syllables. Verify it is correct.")
        ));
    QVERIFY(model.isDirty());
    QCOMPARE(dirtySpy.count(), 1);
    QCOMPARE(dirtySpy.constFirst().at(0).toBool(), true);
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(changedSpy.constFirst().at(0).value<QModelIndex>(), model.index(0, 0));
    QCOMPARE(
        changedSpy.constFirst().at(1).value<QModelIndex>(),
        model.index(model.rowCount() - 1, model.columnCount() - 1)
        );
}

void RosterModelTests::moveRosterRowMovesSourceToLaterDestination()
{
    Roster roster;
    roster.columns =
        Roster::BaseColumns;
    roster.rows = {
        row(
            QStringLiteral("Amy"),
            QStringLiteral("A")
            ),
        row(
            QStringLiteral("Ben"),
            QStringLiteral("B")
            ),
        row(
            QStringLiteral("Cal"),
            QStringLiteral("C")
            ),
        row(
            QStringLiteral("Dee"),
            QStringLiteral("D")
            )
    };

    RosterModel model;
    model.setRoster(roster);

    QSignalSpy dirtySpy(
        &model,
        &RosterModel::dirtyChanged
        );

    QVERIFY(
        model.canMoveRow(
            0,
            2
            )
        );
    QVERIFY(
        model.moveRosterRow(
            0,
            2
            )
        );

    const int englishColumn =
        model.englishNameColumn();

    QCOMPARE(
        model.index(0, englishColumn).data(Qt::DisplayRole).toString(),
        QStringLiteral("Ben")
        );
    QCOMPARE(
        model.index(1, englishColumn).data(Qt::DisplayRole).toString(),
        QStringLiteral("Cal")
        );
    QCOMPARE(
        model.index(2, englishColumn).data(Qt::DisplayRole).toString(),
        QStringLiteral("Amy")
        );
    QCOMPARE(
        model.index(3, englishColumn).data(Qt::DisplayRole).toString(),
        QStringLiteral("Dee")
        );
    QCOMPARE(
        model.index(2, 2).data(Qt::DisplayRole).toString(),
        QStringLiteral("A")
        );

    QVERIFY(
        model.isDirty()
        );
    QCOMPARE(
        dirtySpy.count(),
        1
        );
}

void RosterModelTests::moveRosterRowMovesSourceToEarlierDestination()
{
    Roster roster;
    roster.columns =
        Roster::BaseColumns;
    roster.rows = {
        row(
            QStringLiteral("Amy"),
            QStringLiteral("A")
            ),
        row(
            QStringLiteral("Ben"),
            QStringLiteral("B")
            ),
        row(
            QStringLiteral("Cal"),
            QStringLiteral("C")
            ),
        row(
            QStringLiteral("Dee"),
            QStringLiteral("D")
            )
    };

    RosterModel model;
    model.setRoster(roster);

    QVERIFY(
        model.moveRosterRow(
            3,
            1
            )
        );

    const int englishColumn =
        model.englishNameColumn();

    QCOMPARE(
        model.index(0, englishColumn).data(Qt::DisplayRole).toString(),
        QStringLiteral("Amy")
        );
    QCOMPARE(
        model.index(1, englishColumn).data(Qt::DisplayRole).toString(),
        QStringLiteral("Dee")
        );
    QCOMPARE(
        model.index(2, englishColumn).data(Qt::DisplayRole).toString(),
        QStringLiteral("Ben")
        );
    QCOMPARE(
        model.index(3, englishColumn).data(Qt::DisplayRole).toString(),
        QStringLiteral("Cal")
        );
    QCOMPARE(
        model.index(1, 2).data(Qt::DisplayRole).toString(),
        QStringLiteral("D")
        );
}

void RosterModelTests::moveRosterRowRejectsEmptyAndSameRows()
{
    Roster roster;
    roster.columns =
        Roster::BaseColumns;
    roster.rows = {
        row(
            QStringLiteral("Amy"),
            QStringLiteral("A")
            )
    };

    RosterModel model;
    model.setRoster(roster);

    QString reason;

    QVERIFY(
        !model.canMoveRow(
            -1,
            0,
            &reason
            )
        );
    QCOMPARE(reason, QStringLiteral("Select a student row to move."));
    QVERIFY(
        !model.moveRosterRow(
            -1,
            0
            )
        );

    reason.clear();

    QVERIFY(
        !model.canMoveRow(
            0,
            model.rowCount(),
            &reason
            )
        );
    QCOMPARE(reason, QStringLiteral("Drop the student on another roster row."));
    QVERIFY(
        !model.moveRosterRow(
            0,
            model.rowCount()
            )
        );

    reason.clear();
    QVERIFY(!model.canMoveRow(0, 0, &reason));
    QCOMPARE(reason, QStringLiteral("Drop the student on a different row."));
    QVERIFY(!model.moveRosterRow(0, 0));

    reason.clear();
    QVERIFY(!model.canMoveRow(1, 0, &reason));
    QCOMPARE(reason, QStringLiteral("Selected row is empty."));
    QVERIFY(!model.moveRosterRow(1, 0));

    QVERIFY(
        !model.canMoveRow(
            model.rowCount(),
            0
            )
        );
    QVERIFY(
        !model.isDirty()
        );
}

void RosterModelTests::moveRosterRowRefreshesValidationAndEmitsModelChanges()
{
    Roster roster;
    roster.columns = Roster::BaseColumns;
    roster.rows = {
        studentRow(
            QStringLiteral("Amy"),
            QStringLiteral("\uAE40")
            ),
        studentRow(
            QStringLiteral("Ben"),
            QStringLiteral("\uC774\uC11C\uC900")
            )
    };

    RosterModel model;
    model.setRoster(roster);
    const int koreanColumn = model.koreanNameColumn();
    QVERIFY(koreanColumn >= 0);
    QVERIFY(
        model.errorsForCell(0, koreanColumn).contains(
            QStringLiteral("Korean name has 1 or 5+ syllables. Verify it is correct.")
            )
        );

    QSignalSpy dirtySpy(&model, &RosterModel::dirtyChanged);
    QSignalSpy changedSpy(&model, &QAbstractItemModel::dataChanged);
    QVERIFY(dirtySpy.isValid());
    QVERIFY(changedSpy.isValid());

    QVERIFY(model.moveRosterRow(0, 2));

    QVERIFY(
        !model.errorsForCell(0, koreanColumn).contains(
            QStringLiteral("Korean name has 1 or 5+ syllables. Verify it is correct.")
            )
        );
    QVERIFY(
        model.errorsForCell(2, koreanColumn).contains(
            QStringLiteral("Korean name has 1 or 5+ syllables. Verify it is correct.")
            )
        );
    QVERIFY(model.isDirty());
    QCOMPARE(dirtySpy.count(), 1);
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(changedSpy.constFirst().at(0).value<QModelIndex>().row(), 0);
    QCOMPARE(
        changedSpy.constFirst().at(1).value<QModelIndex>().row(),
        model.rowCount() - 1
        );
}

void RosterModelTests::insertTransferredRowUsesFirstEmptyRow()
{
    Roster roster;
    roster.columns =
        Roster::BaseColumns;
    roster.rows = {
        studentRow(
            QStringLiteral("Amy"),
            QStringLiteral("김아미")
            ),
        QStringList(
            Roster::BaseColumns.size(),
            QString()
            ),
        studentRow(
            QStringLiteral("Cal"),
            QStringLiteral("김칼")
            )
    };

    RosterModel model;
    model.setRoster(roster);

    const QStringList sourceRow =
        studentRow(
            QStringLiteral("Ben"),
            QStringLiteral("김벤"),
            QStringLiteral("A")
            );

    QVERIFY(
        model.insertTransferredRow(
            Roster::BaseColumns,
            sourceRow
            )
        );

    QCOMPARE(
        model.firstEmptyRow(),
        3
        );
    QCOMPARE(
        model.index(1, model.englishNameColumn()).data(Qt::DisplayRole).toString(),
        QStringLiteral("Ben")
        );
    QCOMPARE(
        model.index(1, model.koreanNameColumn()).data(Qt::DisplayRole).toString(),
        QStringLiteral("김벤")
        );
    QCOMPARE(
        model.index(1, 2).data(Qt::DisplayRole).toString(),
        QStringLiteral("A")
        );
    QVERIFY(
        model.isDirty()
        );
}

void RosterModelTests::insertTransferredRowCopiesOnlyMatchingColumns()
{
    const QStringList sourceColumns =
        Roster::BaseColumns
        + QStringList{
            QStringLiteral("Phone"),
            QStringLiteral("Notes")
        };

    const QStringList targetColumns =
        Roster::BaseColumns
        + QStringList{
            QStringLiteral("Phone")
        };

    Roster targetRoster;
    targetRoster.columns =
        targetColumns;

    RosterModel model;
    model.setRoster(targetRoster);

    const QStringList sourceRow =
        studentRow(
            QStringLiteral("Amy"),
            QStringLiteral("김아미"),
            QStringLiteral("B"),
            QStringLiteral("010-1234")
            )
        + QStringList{
            QStringLiteral("Leave behind")
        };

    QVERIFY(
        model.insertTransferredRow(
            sourceColumns,
            sourceRow
            )
        );

    QCOMPARE(
        model.columnCount(),
        targetColumns.size()
        );
    QCOMPARE(
        model.index(0, model.englishNameColumn()).data(Qt::DisplayRole).toString(),
        QStringLiteral("Amy")
        );
    QCOMPARE(
        model.index(0, 2).data(Qt::DisplayRole).toString(),
        QStringLiteral("B")
        );
    QCOMPARE(
        model.index(0, 6).data(Qt::DisplayRole).toString(),
        QStringLiteral("010-1234")
        );
}

void RosterModelTests::insertTransferredRowRejectsFullTargetRoster()
{
    Roster roster;
    roster.columns =
        Roster::BaseColumns;

    for (int index = 0; index < RosterUi::RowCount; ++index)
    {
        roster.rows.append(
            studentRow(
                QStringLiteral("Student%1").arg(index),
                QStringLiteral("김학생%1").arg(index)
                )
            );
    }

    RosterModel model;
    model.setRoster(roster);

    QString reason;

    QVERIFY(
        !model.canInsertTransferredRow(
            Roster::BaseColumns,
            studentRow(
                QStringLiteral("New"),
                QStringLiteral("김새")
                ),
            &reason
            )
        );
    QVERIFY(
        reason.contains(
            QStringLiteral("full"),
            Qt::CaseInsensitive
            )
        );
    QVERIFY(
        !model.insertTransferredRow(
            Roster::BaseColumns,
            studentRow(
                QStringLiteral("New"),
                QStringLiteral("김새")
                )
            )
        );
}

void RosterModelTests::transferredRowDetectsDuplicateStudentPair()
{
    Roster roster;
    roster.columns =
        Roster::BaseColumns;
    roster.rows = {
        studentRow(
            QStringLiteral("Amy"),
            QStringLiteral("김아미")
            )
    };

    RosterModel model;
    model.setRoster(roster);

    QString reason;

    QVERIFY(
        model.hasDuplicateTransferredStudent(
            Roster::BaseColumns,
            studentRow(
                QStringLiteral("Amy"),
                QStringLiteral("김아미")
                ),
            &reason
            )
        );
    QVERIFY(
        !reason.isEmpty()
        );
    QVERIFY(
        !model.canInsertTransferredRow(
            Roster::BaseColumns,
            studentRow(
                QStringLiteral("Amy"),
                QStringLiteral("김아미")
                )
            )
        );
}

void RosterModelTests::namePairHelpersDetectDuplicatesAndSuggestSuffix()
{
    Roster roster;
    roster.columns =
        Roster::BaseColumns;
    roster.rows = {
        studentRow(
            QStringLiteral("amy"),
            QStringLiteral("김민수")
            ),
        studentRow(
            QStringLiteral("Amy"),
            QStringLiteral("김민수")
            ),
        studentRow(
            QStringLiteral("Amy"),
            QStringLiteral("김민수(a)")
            )
    };

    RosterModel model;
    model.setRoster(roster);

    QCOMPARE(
        model.duplicateNameRows(0),
        QList<int>{ 1 }
        );
    QCOMPARE(
        model.suggestedKoreanNameWithSuffix(0),
        QStringLiteral("김민수(B)")
        );
}

void RosterModelTests::
duplicatePairValidationTrimsNamesAndSkipsIncompleteRows()
{
    const QString koreanName =
        QString::fromUtf8("\xEA\xB9\x80\xEB\xAF\xBC\xEC\x88\x98");
    Roster roster;
    roster.columns = Roster::BaseColumns;
    roster.rows = {
        studentRow(
            QStringLiteral(" Alex "),
            QStringLiteral(" ") + koreanName + QStringLiteral(" ")
            ),
        studentRow(
            QStringLiteral("Alex"),
            koreanName
            ),
        studentRow(
            QStringLiteral("Alex"),
            koreanName + QStringLiteral("(A)")
            ),
        studentRow(
            QStringLiteral("Alex"),
            QString()
            )
    };

    RosterModel model;
    model.setRoster(roster);

    const int englishColumn = model.englishNameColumn();
    const int koreanColumn = model.koreanNameColumn();
    const QString firstDuplicateMessage =
        QStringLiteral("Duplicate student name pair. Also used on row(s): 2.");
    const QString secondDuplicateMessage =
        QStringLiteral("Duplicate student name pair. Also used on row(s): 1.");
    QVERIFY(
        model.errorsForCell(0, englishColumn).contains(firstDuplicateMessage)
        );
    QVERIFY(
        model.errorsForCell(0, koreanColumn).contains(firstDuplicateMessage)
        );
    QVERIFY(
        model.errorsForCell(1, englishColumn).contains(secondDuplicateMessage)
        );
    QVERIFY(
        model.errorsForCell(1, koreanColumn).contains(secondDuplicateMessage)
        );
    QVERIFY(model.errorsForCell(2, englishColumn).isEmpty());
    QVERIFY(model.errorsForCell(2, koreanColumn).isEmpty());
    QVERIFY(model.errorsForCell(3, englishColumn).isEmpty());
    QVERIFY(model.errorsForCell(3, koreanColumn).isEmpty());
}

void RosterModelTests::structuredValidationMarksAndClearsAffectedCells()
{
    Roster roster;
    roster.columns = Roster::BaseColumns;
    roster.rows = {
        studentRow(
            QStringLiteral("Amy"),
            QStringLiteral("김아미")
            )
    };

    RosterModel model;
    model.setRoster(roster);

    const int koreanColumn = model.koreanNameColumn();
    model.setDomainValidation(ValidationResult(ValidationIssue{
        .code = QStringLiteral("roster.student_name.required"),
        .field = QStringLiteral("rows[0].Korean"),
        .row = 0,
        .column = koreanColumn
        }));

    QCOMPARE(
        model.errorsForCell(0, koreanColumn),
        QStringList{QStringLiteral("This field is required.")}
        );
    QCOMPARE(
        model.index(0, koreanColumn).data(Qt::ToolTipRole).toString(),
        QStringLiteral("This field is required.")
        );

    model.setDomainValidation({});
    QVERIFY(model.errorsForCell(0, koreanColumn).isEmpty());
}

int main(
    int argc,
    char** argv
    )
{
    QCoreApplication app(argc, argv);

    RosterModelTests tests;
    return QTest::qExec(
        &tests,
        argc,
        argv
        );
}

#include "roster_model_tests.moc"
