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
    void moveRosterRowMovesSourceToLaterDestination();
    void moveRosterRowMovesSourceToEarlierDestination();
    void moveRosterRowRejectsEmptyAndSameRows();
    void insertTransferredRowUsesFirstEmptyRow();
    void insertTransferredRowCopiesOnlyMatchingColumns();
    void insertTransferredRowRejectsFullTargetRoster();
    void transferredRowDetectsDuplicateStudentPair();
    void namePairHelpersDetectDuplicatesAndSuggestSuffix();
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

    RosterModel model;
    model.setRoster(roster);

    QSignalSpy dirtySpy(
        &model,
        &RosterModel::dirtyChanged
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
            0,
            0,
            &reason
            )
        );
    QVERIFY(
        !reason.isEmpty()
        );
    QVERIFY(
        !model.moveRosterRow(
            0,
            0
            )
        );

    reason.clear();

    QVERIFY(
        !model.canMoveRow(
            1,
            0,
            &reason
            )
        );
    QVERIFY(
        !reason.isEmpty()
        );
    QVERIFY(
        !model.moveRosterRow(
            1,
            0
            )
        );
    QVERIFY(
        !model.isDirty()
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
