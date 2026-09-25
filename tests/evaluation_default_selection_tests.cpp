#include "features/classes/evaluation_default_selection.h"

#include <QtTest>

namespace EvaluationDefaultSelection::Private
{
[[nodiscard]] SchoolLevel schoolLevelForClassGrade(const QString& grade);
}

class EvaluationDefaultSelectionTests : public QObject
{
    Q_OBJECT

private slots:
    void selectsCurrentTermWhenItHasContent();
    void selectsPreviousTermWhenCurrentTermIsEmpty();
    void requiresSavedTermSchedules();
    void populatedRowsRequireActualContent();
    void classGradeChoiceUsesCourseGradeBands();
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

void EvaluationDefaultSelectionTests::classGradeChoiceUsesCourseGradeBands()
{
    using EvaluationDefaultSelection::Private::schoolLevelForClassGrade;

    for (const QString& grade : {
             QStringLiteral("M1"),
             QStringLiteral("m2"),
             QStringLiteral(" M3 ")
         })
    {
        QCOMPARE(schoolLevelForClassGrade(grade), SchoolLevel::Middle);
    }

    for (const QString& grade : {
             QStringLiteral("E4"),
             QStringLiteral("E5"),
             QStringLiteral("e6"),
             QStringLiteral("Unknown"),
             QString()
         })
    {
        QCOMPARE(schoolLevelForClassGrade(grade), SchoolLevel::Elementary);
    }
}

QTEST_APPLESS_MAIN(EvaluationDefaultSelectionTests)

#include "evaluation_default_selection_tests.moc"
