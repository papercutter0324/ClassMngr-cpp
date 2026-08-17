#include "features/classes/services/speaking_analytics.h"

#include <QtTest/QtTest>

namespace
{

// The evaluation matrix is positional:
//   0 Index, 1 EnglishName, 2 KoreanName, 3 Grammar, 4 Pronunciation,
//   5 Fluency, 6 Manner, 7 Content, 8 OverallEffort, 9 Comments, 10 Notes
QStringList makeRow(
    const QString& english,
    const QString& korean,
    const QString& grammar,
    const QString& pronunciation,
    const QString& fluency,
    const QString& manner,
    const QString& content,
    const QString& overallEffort
    )
{
    return {
        QStringLiteral("0"),
        english,
        korean,
        grammar,
        pronunciation,
        fluency,
        manner,
        content,
        overallEffort,
        QString(),
        QString()
    };
}

} // namespace

class SpeakingAnalyticsTests
    : public QObject
{
    Q_OBJECT
private slots:
    void gradeNumberRoundTrip();
    void roundingRule();
    void strongestAndFocusTies();
    void computeFullMatrix();
    void computePartialCriterion();
    void computeEmpty();
};

void SpeakingAnalyticsTests::gradeNumberRoundTrip()
{
    QCOMPARE(SpeakingAnalytics::gradeToNumber(QStringLiteral("A+")), 5);
    QCOMPARE(SpeakingAnalytics::gradeToNumber(QStringLiteral("A")), 4);
    QCOMPARE(SpeakingAnalytics::gradeToNumber(QStringLiteral("B+")), 3);
    QCOMPARE(SpeakingAnalytics::gradeToNumber(QStringLiteral("B")), 2);
    QCOMPARE(SpeakingAnalytics::gradeToNumber(QStringLiteral("C")), 1);
    QCOMPARE(SpeakingAnalytics::gradeToNumber(QString()), 0);

    QCOMPARE(SpeakingAnalytics::numberToGrade(5), QStringLiteral("A+"));
    QCOMPARE(SpeakingAnalytics::numberToGrade(4), QStringLiteral("A"));
    QCOMPARE(SpeakingAnalytics::numberToGrade(3), QStringLiteral("B+"));
    QCOMPARE(SpeakingAnalytics::numberToGrade(2), QStringLiteral("B"));
    QCOMPARE(SpeakingAnalytics::numberToGrade(1), QStringLiteral("C"));
    QCOMPARE(SpeakingAnalytics::numberToGrade(0), QStringLiteral("C"));
    QCOMPARE(SpeakingAnalytics::numberToGrade(9), QStringLiteral("A+"));
}

void SpeakingAnalyticsTests::roundingRule()
{
    // Mirrors the repository's (int)avg + (frac >= 0.4) rule.
    QCOMPARE(SpeakingAnalytics::roundAverageToGrade(2.39), 2);
    QCOMPARE(SpeakingAnalytics::roundAverageToGrade(2.6), 3);
    QCOMPARE(SpeakingAnalytics::roundAverageToGrade(3.6), 4);
    QCOMPARE(SpeakingAnalytics::roundAverageToGrade(3.39), 3);
    QCOMPARE(SpeakingAnalytics::roundAverageToGrade(4.9), 5);
    QCOMPARE(SpeakingAnalytics::roundAverageToGrade(1.1), 1);
    QCOMPARE(SpeakingAnalytics::roundAverageToGrade(0.0), 0);
    QCOMPARE(SpeakingAnalytics::roundAverageToGrade(5.5), 5);

    QCOMPARE(SpeakingAnalytics::roundTo3(2.0), 2.0);
    QCOMPARE(SpeakingAnalytics::roundTo3(3.833333), 3.833);
    QCOMPARE(SpeakingAnalytics::roundTo3(3.5), 3.5);
    QCOMPARE(SpeakingAnalytics::formatAverage(3.833), QStringLiteral("3.8"));
    QCOMPARE(SpeakingAnalytics::formatAverage(2.0), QStringLiteral("2.0"));
    QCOMPARE(SpeakingAnalytics::formatAverage(3.5), QStringLiteral("3.5"));
}


void SpeakingAnalyticsTests::strongestAndFocusTies()
{
    // One fully-scored student.  Grammar, Content and Overall Effort tie for
    // strongest (4.0); Pronunciation is focus (2.0).
    SpeakingEvalRows matrix;
    matrix.append(
        makeRow(
            QStringLiteral("Avery"),
            QStringLiteral("에버리"),
            QStringLiteral("A"),   // Grammar
            QStringLiteral("B"),   // Pronunciation
            QStringLiteral("B+"),  // Fluency
            QStringLiteral("B+"),  // Manner
            QStringLiteral("A"),   // Content
            QStringLiteral("A")   // OverallEffort
            ));
    const QList<SpeakingEvalRows> matrices{matrix};
    const auto snap = SpeakingAnalytics::compute(matrices, 1);

    QCOMPARE(snap.hasData, true);
    QCOMPARE(snap.strongestNames.size(), 3);
    QCOMPARE(snap.strongestNames, (QStringList{
        QStringLiteral("Grammar"),
        QStringLiteral("Content"),
        QStringLiteral("Overall Effort")
        }));
    QCOMPARE(snap.focusNames.size(), 1);
    QCOMPARE(snap.focusNames, (QStringList{QStringLiteral("Pronunciation")}));
    QCOMPARE(snap.classAverageLetter, QStringLiteral("B+"));
    QCOMPARE(snap.rankings.size(), 1);
}

void SpeakingAnalyticsTests::computeFullMatrix()
{
    // Fully-scored, uniform students:
    //   Anna  A+ x6 -> avg 5.0 -> A+
    //   Ben   B  x6 -> avg 2.0 -> B
    //   Cara  B+ x6 -> avg 3.0 -> B+
    // Each criterion column is {5, 2, 3} -> average 3.333, so all six tied.
    SpeakingEvalRows matrix;
    matrix.append(
        makeRow(
            QStringLiteral("Anna"),
            QStringLiteral("안나"),
            QStringLiteral("A+"),
            QStringLiteral("A+"),
            QStringLiteral("A+"),
            QStringLiteral("A+"),
            QStringLiteral("A+"),
            QStringLiteral("A+")));
    matrix.append(
        makeRow(
            QStringLiteral("Ben"),
            QStringLiteral("벤"),
            QStringLiteral("B"),
            QStringLiteral("B"),
            QStringLiteral("B"),
            QStringLiteral("B"),
            QStringLiteral("B"),
            QStringLiteral("B")));
    matrix.append(
        makeRow(
            QStringLiteral("Cara"),
            QStringLiteral("카라"),
            QStringLiteral("B+"),
            QStringLiteral("B+"),
            QStringLiteral("B+"),
            QStringLiteral("B+"),
            QStringLiteral("B+"),
            QStringLiteral("B+")));

    const QList<SpeakingEvalRows> matrices{matrix};
    const auto snap = SpeakingAnalytics::compute(matrices, 12);

    QCOMPARE(snap.hasData, true);
    QCOMPARE(snap.rosterStudentCount, 12);
    QCOMPARE(snap.fullyScoredCount, 3);
    QCOMPARE(snap.overallLetters, (QStringList{
        QStringLiteral("A+"),
        QStringLiteral("B"),
        QStringLiteral("B+")
        }));

    QCOMPARE(snap.classAverage3, 3.333);
    QCOMPARE(SpeakingAnalytics::formatAverage(snap.classAverage3),
        QStringLiteral("3.3"));
    QCOMPARE(snap.classAverageLetter, QStringLiteral("B+"));

    // Every criterion is tied (average 3.333), so all six are tagged.
    QCOMPARE(snap.strongestNames.size(), 6);
    QCOMPARE(snap.focusNames.size(), 6);

    // Per-criterion slice check.
    const auto grammar = snap.criteria.value(0);
    QCOMPARE(grammar.name, QStringLiteral("Grammar"));
    QCOMPARE(grammar.students, 3);
    QCOMPARE(grammar.average3, 3.333);
    QCOMPARE(grammar.distribution.value(QStringLiteral("A+")), 1);
    QCOMPARE(grammar.distribution.value(QStringLiteral("B")), 1);
    QCOMPARE(grammar.distribution.value(QStringLiteral("B+")), 1);

    // Rankings: Anna (5.0), Cara (3.0), Ben (2.0).
    QCOMPARE(snap.rankings.size(), 3);
    QCOMPARE(snap.rankings.at(0).englishName, QStringLiteral("Anna"));
    QCOMPARE(snap.rankings.at(0).koreanName, QStringLiteral("안나"));
    QCOMPARE(snap.rankings.at(0).overall3, 5.0);
    QCOMPARE(snap.rankings.at(0).overallLetter, QStringLiteral("A+"));
    QCOMPARE(snap.rankings.at(1).englishName, QStringLiteral("Cara"));
    QCOMPARE(snap.rankings.at(1).overall3, 3.0);
    QCOMPARE(snap.rankings.at(2).englishName, QStringLiteral("Ben"));
    QCOMPARE(snap.rankings.at(2).overall3, 2.0);
    QCOMPARE(snap.rankings.at(2).overallLetter, QStringLiteral("B"));
}

void SpeakingAnalyticsTests::computePartialCriterion()
{
    // Both students are missing OverallEffort (unscored column).
    //   Dan: A B A B B+ -> sum 15 avg 3.0 -> B+
    //   Eve: B B B B B  -> sum 10 avg 2.0 -> B
    // Per-criterion (both scored):
    //   Grammar 3.0, Pronunciation 2.0, Fluency 3.0, Manner 2.0, Content 2.5
    SpeakingEvalRows matrix;
    matrix.append(
        makeRow(
            QStringLiteral("Dan"),
            QStringLiteral("댄"),
            QStringLiteral("A"),
            QStringLiteral("B"),
            QStringLiteral("A"),
            QStringLiteral("B"),
            QStringLiteral("B+"),
            QString()));
    matrix.append(
        makeRow(
            QStringLiteral("Eve"),
            QStringLiteral("이브"),
            QStringLiteral("B"),
            QStringLiteral("B"),
            QStringLiteral("B"),
            QStringLiteral("B"),
            QStringLiteral("B"),
            QString()));

    const QList<SpeakingEvalRows> matrices{matrix};
    const auto snap = SpeakingAnalytics::compute(matrices, 3);

    QCOMPARE(snap.hasData, true);
    QCOMPARE(snap.fullyScoredCount, 0);
    QCOMPARE(snap.overallLetters, (QStringList{
        QStringLiteral("B+"),
        QStringLiteral("B")
        }));
    QCOMPARE(snap.classAverage3, 2.5);
    QCOMPARE(snap.classAverageLetter, QStringLiteral("B+"));

    QCOMPARE(snap.strongestNames, (QStringList{
        QStringLiteral("Grammar"),
        QStringLiteral("Fluency")
        }));
    QCOMPARE(snap.focusNames, (QStringList{
        QStringLiteral("Pronunciation"),
        QStringLiteral("Manner")
        }));

    const auto content = snap.criteria.value(
        SpeakingAnalytics::CriterionOrder::Content);
    QCOMPARE(content.students, 2);
    QCOMPARE(content.average3, 2.5);

    const auto effort = snap.criteria.value(
        SpeakingAnalytics::CriterionOrder::OverallEffort);
    QCOMPARE(effort.students, 0);
    QCOMPARE(effort.hasData, false);

    QCOMPARE(snap.rankings.size(), 2);
    QCOMPARE(snap.rankings.at(0).englishName, QStringLiteral("Dan"));
    QCOMPARE(snap.rankings.at(0).overall3, 3.0);
    QCOMPARE(snap.rankings.at(1).englishName, QStringLiteral("Eve"));
    QCOMPARE(snap.rankings.at(1).overall3, 2.0);
}

void SpeakingAnalyticsTests::computeEmpty()
{
    // No matrices at all.
    const auto empty = SpeakingAnalytics::compute({}, 5);
    QCOMPARE(empty.hasData, false);
    QCOMPARE(empty.rankings.size(), 0);
    QCOMPARE(empty.strongestNames.size(), 0);
    QCOMPARE(empty.focusNames.size(), 0);
    QCOMPARE(empty.classAverageLetter.isEmpty(), true);

    // A matrix whose only row has no student name is ignored.
    SpeakingEvalRows matrix;
    matrix.append(
        makeRow(
            QString(),
            QString(),
            QStringLiteral("A"),
            QStringLiteral("A"),
            QStringLiteral("A"),
            QStringLiteral("A"),
            QStringLiteral("A"),
            QStringLiteral("A")));
    const QList<SpeakingEvalRows> matrices{matrix};
    const auto anon = SpeakingAnalytics::compute(matrices, 1);
    QCOMPARE(anon.hasData, false);
    QCOMPARE(anon.rankings.size(), 0);
}

QTEST_MAIN(SpeakingAnalyticsTests)

#include "speaking_analytics_tests.moc"


