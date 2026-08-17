#include "features/classes/services/speaking_analytics.h"

#include <algorithm>
#include <QLocale>
#include <QMap>
#include <QString>

namespace SpeakingAnalytics
{

double roundTo3(double value)
{
    return qRound(value * 1000.0) / 1000.0;
}

QString formatAverage(double average)
{
    return QLocale().toString(roundTo3(average), 'f', 1);
}

int gradeToNumber(const QString& grade)
{
    if (grade == QStringLiteral("A+"))
        return 5;
    if (grade == QStringLiteral("A"))
        return 4;
    if (grade == QStringLiteral("B+"))
        return 3;
    if (grade == QStringLiteral("B"))
        return 2;
    if (grade == QStringLiteral("C"))
        return 1;
    return 0;
}

QString numberToGrade(int number)
{
    switch (qBound(1, number, 5))
    {
    case 5:
        return QStringLiteral("A+");
    case 4:
        return QStringLiteral("A");
    case 3:
        return QStringLiteral("B+");
    case 2:
        return QStringLiteral("B");
    default:
        return QStringLiteral("C");
    }
}

int roundAverageToGrade(double average)
{
    if (average <= 0.0)
        return 0;
    if (average >= 5.0)
        return 5;
    return static_cast<int>(qFloor(average)) + (average - qFloor(average) >= 0.4 ? 1 : 0);
}

QList<int> strongestIndices(const QList<double>& averages3)
{
    QList<int> result;
    if (averages3.isEmpty())
        return result;

    double best = averages3.first();
    for (int i = 1; i < averages3.size(); ++i)
        best = qMax(best, averages3.at(i));

    for (int i = 0; i < averages3.size(); ++i)
        if (qAbs(averages3.at(i) - best) < 1e-9)
            result.append(i);
    return result;
}

QList<int> focusIndices(const QList<double>& averages3)
{
    QList<int> result;
    if (averages3.isEmpty())
        return result;

    double worst = averages3.first();
    for (int i = 1; i < averages3.size(); ++i)
        worst = qMin(worst, averages3.at(i));

    for (int i = 0; i < averages3.size(); ++i)
        if (qAbs(averages3.at(i) - worst) < 1e-9)
            result.append(i);
    return result;
}

Snapshot compute(
    const QList<SpeakingEvalRows>& matrices,
    int rosterStudentCount
)
{
    const QStringList criterionNames =
    {
        QStringLiteral("Grammar"),
        QStringLiteral("Pronunciation"),
        QStringLiteral("Fluency"),
        QStringLiteral("Manner"),
        QStringLiteral("Content"),
        QStringLiteral("Overall Effort")
    };

    // The evaluation matrix is positional (no header row): English name at
    // column 1, Korean name at column 2, then the six scoring columns 3..8.
    const int englishCol = SpeakingEval::toInt(SpeakingEvalColumn::EnglishName);
    const int koreanCol = SpeakingEval::toInt(SpeakingEvalColumn::KoreanName);
    const QList<int> criterionColumns =
    {
        SpeakingEval::toInt(SpeakingEvalColumn::Grammar),
        SpeakingEval::toInt(SpeakingEvalColumn::Pronunciation),
        SpeakingEval::toInt(SpeakingEvalColumn::Fluency),
        SpeakingEval::toInt(SpeakingEvalColumn::Manner),
        SpeakingEval::toInt(SpeakingEvalColumn::Content),
        SpeakingEval::toInt(SpeakingEvalColumn::OverallEffort)
    };

    // Pool all non-empty student rows across the selected matrices.
    QList<PoolRow> pool;
    for (const SpeakingEvalRows& matrix : matrices)
    {
        for (int r = 0; r < matrix.size(); ++r)
        {
            const QStringList& row = matrix.at(r);
            if (row.isEmpty() || englishCol >= row.size())
                continue;
            const QString englishName = row.at(englishCol).trimmed();
            if (englishName.isEmpty())
                continue;

            PoolRow pr;
            pr.englishName = englishName;
            pr.koreanName =
                (koreanCol < row.size()) ? row.at(koreanCol).trimmed() : QString();

            double sum = 0.0;
            int filled = 0;
            QList<QString> letters;
            letters.reserve(criterionColumns.size());
            for (int col : criterionColumns)
            {
                const QString g =
                    (col < row.size()) ? row.at(col).trimmed() : QString();
                const int n = gradeToNumber(g);
                if (n > 0)
                {
                    sum += n;
                    ++filled;
                    letters.append(g);
                }
                else
                {
                    letters.append(QString());
                }
            }

            pr.hasAny = filled > 0;
            pr.fullyScored = (filled == criterionColumns.size());
            if (filled > 0)
            {
                pr.overall3 = roundTo3(sum / static_cast<double>(filled));
                pr.overallLetter = numberToGrade(roundAverageToGrade(pr.overall3));
            }
            pr.criterionLetters = letters;
            pool.append(pr);
        }
    }

    Snapshot snap;
    snap.rosterStudentCount = qMax(0, rosterStudentCount);
    snap.hasData = !pool.isEmpty();
    if (!snap.hasData)
        return snap;

    // Per-criterion slices in canonical order.
    for (int ci = 0; ci < criterionColumns.size(); ++ci)
    {
        CriterionSlice cs;
        cs.order = ci;
        cs.name = criterionNames.at(ci);

        int total = 0;
        double sum = 0.0;
        for (const PoolRow& pr : pool)
        {
            if (pr.criterionLetters.size() <= ci)
                continue;
            const QString g = pr.criterionLetters.at(ci).trimmed();
            const int n = gradeToNumber(g);
            if (n <= 0)
                continue;
            cs.distribution[g] += 1;
            total += 1;
            sum += n;
        }

        cs.students = total;
        if (total > 0)
        {
            cs.hasData = true;
            cs.average3 = roundTo3(sum / static_cast<double>(total));
        }
        snap.criteria.append(cs);
    }

    // Class-wide histogram + average (students with any data).
    double classSum = 0.0;
    int classCount = 0;
    for (const PoolRow& pr : pool)
    {
        if (!pr.hasAny)
            continue;
        snap.overallLetters.append(pr.overallLetter);
        classSum += pr.overall3;
        ++classCount;
    }
    if (classCount > 0)
    {
        snap.classAverage3 = roundTo3(classSum / static_cast<double>(classCount));
        snap.classAverageLetter = numberToGrade(roundAverageToGrade(snap.classAverage3));
    }

    // Strongest / focus (ties included), only among criteria with data.
    QList<int> withData;
    for (int ci = 0; ci < snap.criteria.size(); ++ci)
        if (snap.criteria.at(ci).hasData)
            withData.append(ci);

    if (!withData.isEmpty())
    {
        double best = -1.0;
        double worst = 6.0;
        for (int ci : withData)
        {
            best = qMax(best, snap.criteria.at(ci).average3);
            worst = qMin(worst, snap.criteria.at(ci).average3);
        }
        for (int ci : withData)
        {
            const double avg = snap.criteria.at(ci).average3;
            if (qAbs(avg - best) < 1e-9)
                snap.strongestNames.append(snap.criteria.at(ci).name);
            if (qAbs(avg - worst) < 1e-9)
                snap.focusNames.append(snap.criteria.at(ci).name);
        }
    }

    for (const PoolRow& pr : pool)
        if (pr.fullyScored)
            ++snap.fullyScoredCount;

    // Per-student rankings, best overall first.
    QList<StudentRank> ranks;
    for (const PoolRow& pr : pool)
    {
        if (!pr.hasAny)
            continue;
        StudentRank sr;
        sr.englishName = pr.englishName;
        sr.koreanName = pr.koreanName;
        sr.overall3 = pr.overall3;
        sr.overallLetter = pr.overallLetter;
        sr.criterionLetters = pr.criterionLetters;
        ranks.append(sr);
    }
    std::sort(
        ranks.begin(),
        ranks.end(),
        [](const StudentRank& a, const StudentRank& b)
        {
            if (a.overall3 != b.overall3)
                return a.overall3 > b.overall3;
            return a.englishName < b.englishName;
        });
    snap.rankings = ranks;
    return snap;
}

} // namespace SpeakingAnalytics

