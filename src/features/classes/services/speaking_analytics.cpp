#include "features/classes/services/speaking_analytics.h"

#include "core/utils/student_name_utils.h"

#include <QHash>
#include <QLocale>

#include <algorithm>
#include <array>

namespace SpeakingAnalytics
{

namespace
{

constexpr std::array<SpeakingEvalColumn, CriterionCount> kCriterionColumns{
    SpeakingEvalColumn::Grammar,
    SpeakingEvalColumn::Pronunciation,
    SpeakingEvalColumn::Fluency,
    SpeakingEvalColumn::Manner,
    SpeakingEvalColumn::Content,
    SpeakingEvalColumn::OverallEffort
};

const std::array<QString, CriterionCount> kCriterionNames{
    QStringLiteral("Grammar"),
    QStringLiteral("Pronunciation"),
    QStringLiteral("Fluency"),
    QStringLiteral("Manner"),
    QStringLiteral("Content"),
    QStringLiteral("Overall Effort")
};

struct StudentAccumulator
{
    QString englishName;
    QString koreanName;
    std::array<double, CriterionCount> sums{};
    std::array<int, CriterionCount> counts{};
};

QString studentKey(const QString& englishName, const QString& koreanName)
{
    const QString normalizedEnglish =
        StudentNameUtils::normalizeEnglishName(englishName).toCaseFolded();
    if (!normalizedEnglish.isEmpty())
        return QStringLiteral("en:") + normalizedEnglish;

    const QString normalizedKorean =
        StudentNameUtils::baseKoreanName(koreanName).trimmed();
    return normalizedKorean.isEmpty()
        ? QString()
        : QStringLiteral("ko:") + normalizedKorean;
}

QString criterionLabel(const CriterionSlice& slice)
{
    if (!slice.hasData)
        return slice.name;

    return QStringLiteral("%1 (%2)")
        .arg(slice.name, numberToGrade(roundAverageToGrade(slice.average3)));
}

} // namespace

QStringList evaluationNames()
{
    return {
        QStringLiteral("Winter"),
        QStringLiteral("Speech Contest"),
        QStringLiteral("Summer"),
        QStringLiteral("Fall")
    };
}

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

    const int whole = static_cast<int>(qFloor(average));
    return whole + (average - whole >= 0.4 ? 1 : 0);
}

QList<int> strongestIndices(const QList<double>& averages3)
{
    QList<int> result;
    if (averages3.isEmpty())
        return result;

    const double best = *std::max_element(averages3.cbegin(), averages3.cend());
    for (qsizetype i = 0; i < averages3.size(); ++i)
    {
        if (qAbs(averages3.at(i) - best) < 1e-9)
            result.append(static_cast<int>(i));
    }
    return result;
}

QList<int> focusIndices(const QList<double>& averages3)
{
    QList<int> result;
    if (averages3.isEmpty())
        return result;

    const double worst = *std::min_element(averages3.cbegin(), averages3.cend());
    for (qsizetype i = 0; i < averages3.size(); ++i)
    {
        if (qAbs(averages3.at(i) - worst) < 1e-9)
            result.append(static_cast<int>(i));
    }
    return result;
}

Snapshot compute(
    const QList<SpeakingEvalRows>& matrices,
    int rosterStudentCount
)
{
    QHash<QString, int> accumulatorByStudent;
    QList<StudentAccumulator> students;

    const int englishColumn =
        SpeakingEval::toInt(SpeakingEvalColumn::EnglishName);
    const int koreanColumn =
        SpeakingEval::toInt(SpeakingEvalColumn::KoreanName);

    for (const SpeakingEvalRows& matrix : matrices)
    {
        for (const QStringList& row : matrix)
        {
            const QString englishName = row.value(englishColumn).trimmed();
            const QString koreanName = row.value(koreanColumn).trimmed();
            const QString key = studentKey(englishName, koreanName);
            if (key.isEmpty())
                continue;

            int studentIndex = accumulatorByStudent.value(key, -1);
            if (studentIndex < 0)
            {
                studentIndex = students.size();
                accumulatorByStudent.insert(key, studentIndex);
                students.append({ englishName, koreanName, {}, {} });
            }

            StudentAccumulator& student = students[studentIndex];
            if (student.englishName.isEmpty())
                student.englishName = englishName;
            if (student.koreanName.isEmpty())
                student.koreanName = koreanName;

            for (int criterion = 0; criterion < CriterionCount; ++criterion)
            {
                const QString grade = row.value(
                    SpeakingEval::toInt(kCriterionColumns.at(criterion))).trimmed();
                const int value = gradeToNumber(grade);
                if (value <= 0)
                    continue;

                student.sums[criterion] += value;
                ++student.counts[criterion];
            }
        }
    }

    Snapshot snapshot;
    snapshot.rosterStudentCount = qMax(0, rosterStudentCount);

    std::array<double, CriterionCount> criterionSums{};
    std::array<int, CriterionCount> criterionCounts{};
    std::array<int, CriterionCount> criterionStudentCounts{};
    std::array<QMap<QString, int>, CriterionCount> distributions;
    QList<double> criterionAverages;
    criterionAverages.reserve(CriterionCount);

    double classSum = 0.0;
    int classCount = 0;

    for (const StudentAccumulator& student : students)
    {
        double scoreSum = 0.0;
        int scoreCount = 0;
        bool fullyScored = true;
        QList<QString> letters;
        letters.reserve(CriterionCount);

        for (int criterion = 0; criterion < CriterionCount; ++criterion)
        {
            if (student.counts[criterion] == 0)
            {
                fullyScored = false;
                letters.append(QString());
                continue;
            }

            const double average = roundTo3(
                student.sums[criterion] / student.counts[criterion]);
            const QString letter = numberToGrade(roundAverageToGrade(average));
            letters.append(letter);

            criterionSums[criterion] += student.sums[criterion];
            criterionCounts[criterion] += student.counts[criterion];
            ++criterionStudentCounts[criterion];
            distributions[criterion][letter] += 1;
            scoreSum += student.sums[criterion];
            scoreCount += student.counts[criterion];
        }

        if (scoreCount == 0)
            continue;

        StudentRank rank;
        rank.englishName = student.englishName;
        rank.koreanName = student.koreanName;
        rank.overall3 = roundTo3(scoreSum / scoreCount);
        rank.overallLetter = numberToGrade(roundAverageToGrade(rank.overall3));
        rank.criterionLetters = letters;
        rank.fullyScored = fullyScored;
        snapshot.rankings.append(rank);
        snapshot.overallLetters.append(rank.overallLetter);

        classSum += rank.overall3;
        ++classCount;
        if (fullyScored)
            ++snapshot.fullyScoredCount;
    }

    snapshot.hasData = classCount > 0;
    if (!snapshot.hasData)
        return snapshot;

    snapshot.classAverage3 = roundTo3(classSum / classCount);
    snapshot.classAverageLetter =
        numberToGrade(roundAverageToGrade(snapshot.classAverage3));

    for (int criterion = 0; criterion < CriterionCount; ++criterion)
    {
        CriterionSlice slice;
        slice.order = criterion;
        slice.name = kCriterionNames.at(criterion);
        slice.students = criterionStudentCounts[criterion];
        slice.distribution = distributions[criterion];
        slice.hasData = criterionCounts[criterion] > 0;
        if (slice.hasData)
        {
            slice.average3 = roundTo3(
                criterionSums[criterion] / criterionCounts[criterion]);
            criterionAverages.append(slice.average3);
        }
        else
        {
            criterionAverages.append(-1.0);
        }
        snapshot.criteria.append(slice);
    }

    double strongest = -1.0;
    double focus = 6.0;
    for (double average : criterionAverages)
    {
        if (average < 0.0)
            continue;
        strongest = qMax(strongest, average);
        focus = qMin(focus, average);
    }
    for (const CriterionSlice& slice : snapshot.criteria)
    {
        if (!slice.hasData)
            continue;
        if (qAbs(slice.average3 - strongest) < 1e-9)
        {
            snapshot.strongestNames.append(slice.name);
            snapshot.strongestLabels.append(criterionLabel(slice));
        }
        if (qAbs(slice.average3 - focus) < 1e-9)
        {
            snapshot.focusNames.append(slice.name);
            snapshot.focusLabels.append(criterionLabel(slice));
        }
    }

    std::sort(
        snapshot.rankings.begin(),
        snapshot.rankings.end(),
        [](const StudentRank& left, const StudentRank& right)
        {
            if (!qFuzzyCompare(left.overall3 + 1.0, right.overall3 + 1.0))
                return left.overall3 > right.overall3;
            return left.englishName.localeAwareCompare(right.englishName) < 0;
        });

    return snapshot;
}

std::optional<YearToDatePoint> yearToDatePoint(
    const QString& evaluationName,
    const Snapshot& snapshot
)
{
    if (snapshot.fullyScoredCount <= 0)
        return std::nullopt;

    double sum = 0.0;
    int count = 0;
    for (const StudentRank& rank : snapshot.rankings)
    {
        if (!rank.fullyScored)
            continue;

        sum += rank.overall3;
        ++count;
    }

    if (count == 0)
        return std::nullopt;

    YearToDatePoint point;
    point.evaluationName = evaluationName;
    point.classAverage3 = roundTo3(sum / count);
    point.classAverageLetter =
        numberToGrade(roundAverageToGrade(point.classAverage3));
    return point;
}

SpeakingEvalRows filterMatrixByRoster(
    const SpeakingEvalRows& matrix,
    const Roster& roster
)
{
    const int englishColumn = roster.columns.indexOf(QStringLiteral("English"));
    const int koreanColumn = roster.columns.indexOf(QStringLiteral("Korean"));
    if (englishColumn < 0 && koreanColumn < 0)
        return matrix;

    const auto namesInColumn = [&roster](int column)
    {
        QStringList names;
        if (column < 0)
            return names;

        for (const QStringList& row : roster.rows)
        {
            const QString name = row.value(column).trimmed();
            if (!name.isEmpty())
                names.append(name);
        }
        return names;
    };

    const QStringList englishNames = namesInColumn(englishColumn);
    const QStringList koreanNames = namesInColumn(koreanColumn);
    if (englishNames.isEmpty() && koreanNames.isEmpty())
        return matrix;

    const auto matchesEnglish = [&englishNames](const QString& name)
    {
        const QString normalized =
            StudentNameUtils::normalizeEnglishName(name).toCaseFolded();
        return !normalized.isEmpty()
            && std::ranges::any_of(
                   englishNames,
                   [&normalized](const QString& rosterName)
                   {
                       return StudentNameUtils::normalizeEnglishName(rosterName)
                           .toCaseFolded() == normalized;
                   });
    };
    const auto matchesKorean = [&koreanNames](const QString& name)
    {
        const QString normalized = StudentNameUtils::baseKoreanName(name);
        return !normalized.isEmpty()
            && std::ranges::any_of(
                   koreanNames,
                   [&normalized](const QString& rosterName)
                   {
                       return StudentNameUtils::baseKoreanName(rosterName)
                           == normalized;
                   });
    };

    SpeakingEvalRows filtered;
    filtered.reserve(matrix.size());
    for (const QStringList& row : matrix)
    {
        if (matchesEnglish(row.value(SpeakingEval::toInt(
                               SpeakingEvalColumn::EnglishName)))
            || matchesKorean(row.value(SpeakingEval::toInt(
                                  SpeakingEvalColumn::KoreanName))))
        {
            filtered.append(row);
        }
    }
    return filtered;
}

} // namespace SpeakingAnalytics
