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
