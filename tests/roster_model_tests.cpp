#include "features/roster/ui/roster_model.h"

#include "core/utils/student_name_utils.h"
#include "domain/models/roster.h"
#include "domain/validation/roster_validator.h"
#include "features/roster/ui/roster_constants.h"

#include <QByteArray>
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
    void interactiveNameValidationMatchesEnginePolicy();
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

bool hasIssue(
    const ValidationResult& result,
    const QString& code,
    int row,
    int column
    )
{
    for (const ValidationIssue& issue : result.issues())
    {
        if (
            issue.code == code
            && issue.row == row
            && issue.column == column
            )
        {
            return true;
        }
    }

    return false;
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

void RosterModelTests::interactiveNameValidationMatchesEnginePolicy()
{
    Roster roster;
    roster.columns = Roster::BaseColumns;
    roster.rows = {
        studentRow(
            QStringLiteral("Amy1"),
            QStringLiteral("김!")
            ),
        studentRow(
            QStringLiteral("  aMY  "),
            QStringLiteral(" 김 민 수 (a) ")
            ),
        studentRow(
            QStringLiteral("Amy"),
            QStringLiteral("김민수(A)")
            ),
        studentRow(
            QStringLiteral("Lia"),
            QStringLiteral("김민수지")
            ),
        studentRow(
            QStringLiteral("Noah"),
            QStringLiteral("김민수지원")
            ),
        studentRow(
            QString::fromUtf8(QByteArray("\xc3(", 2)),
            QStringLiteral("김지원")
            )
    };

    RosterModel model;
    model.setRoster(roster);

    const int englishColumn = model.englishNameColumn();
    const int koreanColumn = model.koreanNameColumn();

    QVERIFY(
        model.errorsForCell(0, englishColumn)
            .contains(QStringLiteral("English name contains invalid characters."))
        );
    QVERIFY(
        model.errorsForCell(0, koreanColumn)
            .contains(QStringLiteral("Korean name contains invalid characters."))
        );

    QCOMPARE(
        model.index(1, englishColumn).data(Qt::DisplayRole).toString(),
        QStringLiteral("Amy")
        );
    QCOMPARE(
        model.index(1, koreanColumn).data(Qt::DisplayRole).toString(),
        QStringLiteral("김민수(A)")
        );
    QCOMPARE(
        model.index(2, koreanColumn).data(Qt::DisplayRole).toString(),
        QStringLiteral("김민수(A)")
        );
    QVERIFY(model.hasDuplicateNameErrors());
    QVERIFY(
        model.errorsForCell(1, englishColumn)
            .contains(QStringLiteral("Duplicate student name pair. Also used on row(s): 3."))
        );
    QVERIFY(
        model.errorsForCell(2, koreanColumn)
            .contains(QStringLiteral("Duplicate student name pair. Also used on row(s): 2."))
        );
    QVERIFY(
        model.errorsForCell(3, koreanColumn)
            .contains(QStringLiteral("Korean name has an uncommon length. Verify it is correct."))
        );
    QVERIFY(
        model.errorsForCell(4, koreanColumn)
            .contains(QStringLiteral("Korean name has 1 or 5+ syllables. Verify it is correct."))
        );
    QVERIFY(
        model.errorsForCell(5, englishColumn)
            .contains(QStringLiteral("English name should use ASCII characters."))
        );

    const auto shortIssues =
        StudentNameUtils::validateKoreanName(QStringLiteral("김"));
    QVERIFY(
        shortIssues.contains(StudentNameUtils::ValidationIssue::KoreanTooShort)
        );

    QVERIFY(
        model.setData(
            model.index(0, englishColumn),
            QStringLiteral("  Alice  ")
            )
        );
    QCOMPARE(
        model.index(0, englishColumn).data(Qt::DisplayRole).toString(),
        QStringLiteral("Alice")
        );
    QVERIFY(
        !model.errorsForCell(0, englishColumn)
            .contains(QStringLiteral("English name contains invalid characters."))
        );
    QVERIFY(
        model.setData(
            model.index(0, englishColumn),
            QStringLiteral("Amy1")
            )
        );

    const ValidationResult saved =
        RosterValidator::validate(
            RosterValidator::normalized(model.toRoster())
            );
    QVERIFY(
        hasIssue(
            saved,
            QStringLiteral("student_name.english.invalid_characters"),
            0,
            englishColumn
            )
        );
    QVERIFY(
        hasIssue(
            saved,
            QStringLiteral("student_name.english.non_ascii"),
            5,
            englishColumn
            )
        );
    QVERIFY(
        hasIssue(
            saved,
            QStringLiteral("student_name.korean.unusual_length"),
            3,
            koreanColumn
            )
        );
    QVERIFY(
        hasIssue(
            saved,
            QStringLiteral("student_name.korean.too_long"),
            4,
            koreanColumn
            )
        );
    QVERIFY(
        hasIssue(
            saved,
            QStringLiteral("student_name.duplicate_pair"),
            1,
            englishColumn
            )
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
