#include "features/classes/evaluation_default_selection.h"

#include <QtTest>

class EvaluationDefaultSelectionTests : public QObject
{
    Q_OBJECT

private slots:
    void selectsCurrentTermWhenItHasContent();
    void selectsPreviousTermWhenCurrentTermIsEmpty();
    void requiresSavedTermSchedules();
    void populatedRowsRequireActualContent();
};

namespace
{

AcademicCalendarSchedule savedSchedule()
{
    AcademicCalendarSchedule schedule;
    const AcademicYearSchedule elementary =
        schedule.yearSchedule(SchoolLevel::Elementary, 2026);
    const AcademicYearSchedule middle =
        schedule.yearSchedule(SchoolLevel::Middle, 2026);
    schedule.setYearSchedules(2026, elementary, middle);
    return schedule;
}

} // namespace

void EvaluationDefaultSelectionTests::selectsCurrentTermWhenItHasContent()
{
    const AcademicCalendarSchedule schedule = savedSchedule();

    QCOMPARE(
        EvaluationDefaultSelection::forTermSchedule(
            schedule,
            SchoolLevel::Elementary,
            QDate(2026, 1, 5),
            true
            ),
        QStringLiteral("Winter")
        );
    QCOMPARE(
        EvaluationDefaultSelection::forTermSchedule(
            schedule,
            SchoolLevel::Elementary,
            QDate(2026, 3, 23),
            true
            ),
        QStringLiteral("Speech Contest")
        );
    QCOMPARE(
        EvaluationDefaultSelection::forTermSchedule(
            schedule,
            SchoolLevel::Elementary,
            QDate(2026, 8, 3),
            true
            ),
        QStringLiteral("Summer")
        );
    QCOMPARE(
        EvaluationDefaultSelection::forTermSchedule(
            schedule,
            SchoolLevel::Elementary,
            QDate(2026, 10, 12),
            true
            ),
        QStringLiteral("Fall")
        );
}

void EvaluationDefaultSelectionTests::selectsPreviousTermWhenCurrentTermIsEmpty()
{
    const AcademicCalendarSchedule schedule = savedSchedule();

    QCOMPARE(
        EvaluationDefaultSelection::forTermSchedule(
            schedule,
            SchoolLevel::Elementary,
            QDate(2026, 1, 5),
            false
            ),
        QStringLiteral("Fall")
        );
    QCOMPARE(
        EvaluationDefaultSelection::forTermSchedule(
            schedule,
            SchoolLevel::Elementary,
            QDate(2026, 3, 23),
            false
            ),
        QStringLiteral("Winter")
        );
    QCOMPARE(
        EvaluationDefaultSelection::forTermSchedule(
            schedule,
            SchoolLevel::Elementary,
            QDate(2026, 8, 3),
            false
            ),
        QStringLiteral("Speech Contest")
        );
    QCOMPARE(
        EvaluationDefaultSelection::forTermSchedule(
            schedule,
            SchoolLevel::Elementary,
            QDate(2026, 10, 12),
            false
            ),
        QStringLiteral("Summer")
        );
}

void EvaluationDefaultSelectionTests::requiresSavedTermSchedules()
{
    const AcademicCalendarSchedule schedule;

    QVERIFY(!schedule.hasSavedSchedules());
    QVERIFY(
        EvaluationDefaultSelection::forTermSchedule(
            schedule,
            SchoolLevel::Elementary,
            QDate(2026, 3, 23),
            true
            ).isEmpty()
        );
}

void EvaluationDefaultSelectionTests::populatedRowsRequireActualContent()
{
    SpeakingEvalRows rows = SpeakingEval::emptyRows();
    QVERIFY(!EvaluationDefaultSelection::isPopulated(rows));

    rows[0][SpeakingEval::toInt(SpeakingEvalColumn::EnglishName)] =
        QStringLiteral("Amy");
    QVERIFY(EvaluationDefaultSelection::isPopulated(rows));
}

QTEST_APPLESS_MAIN(EvaluationDefaultSelectionTests)

#include "evaluation_default_selection_tests.moc"
