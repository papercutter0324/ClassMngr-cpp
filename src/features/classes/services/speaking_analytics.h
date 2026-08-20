#pragma once

#include "domain/models/roster.h"
#include "domain/models/speaking_evaluation.h"

#include <QList>
#include <QMap>
#include <QString>

// Pure, database-free analytics for the Class > Analytics tab.
//
// The numeric scale mirrors the repository's buildRosterScoreImport():
// C=1, B=2, B+=3, A=4, A+=5, with the same "round up when the fractional
// part is >= 0.4" grade rule.  Values are computed to 3 decimal places and
// displayed to 1, as required by the analytics spec.
namespace SpeakingAnalytics
{

enum CriterionOrder
{
    Grammar,
    Pronunciation,
    Fluency,
    Manner,
    Content,
    OverallEffort,
    CriterionCount
};

struct PoolRow
{
    QString englishName;
    QString koreanName;
    QString overallLetter;
    double overall3 = 0.0;
    bool fullyScored = false;
    bool hasAny = false;
    QList<QString> criterionLetters; // 6 entries, empty when unscored
};

struct CriterionSlice
{
    int order = 0;
    QString name;
    int students = 0;
    QMap<QString, int> distribution;
    double average3 = 0.0;
    bool hasData = false;
};

struct StudentRank
{
    QString englishName;
    QString koreanName;
    double overall3 = 0.0;
    QString overallLetter;
    QList<QString> criterionLetters; // 6 entries, empty when unscored
};

struct Snapshot
{
    bool hasData = false;
    double classAverage3 = 0.0;
    QString classAverageLetter;
    int rosterStudentCount = 0;
    int fullyScoredCount = 0;
    QList<CriterionSlice> criteria;
    QList<QString> strongestNames;
    QList<QString> focusNames;
    // Same areas formatted for display as "Name (Letter)".
    QList<QString> strongestLabels;
    QList<QString> focusLabels;
    QList<QString> overallLetters; // one entry per student (for Class Shape)
    QList<StudentRank> rankings;
};

[[nodiscard]] double roundTo3(double value);
[[nodiscard]] QString formatAverage(double average);
[[nodiscard]] int gradeToNumber(const QString& grade);
[[nodiscard]] QString numberToGrade(int number);
[[nodiscard]] int roundAverageToGrade(double average);
[[nodiscard]] QList<int> strongestIndices(const QList<double>& averages3);
[[nodiscard]] QList<int> focusIndices(const QList<double>& averages3);

// Keeps the evaluation rows whose student is in the class roster, matching
// on the (normalized) English or Korean name.  An empty roster, or a roster
// without name columns, keeps the matrix untouched.
[[nodiscard]] SpeakingEvalRows filterMatrixByRoster(
    const SpeakingEvalRows& matrix,
    const Roster& roster
);

[[nodiscard]] Snapshot compute(
    const QList<SpeakingEvalRows>& matrices,
    int rosterStudentCount
);

} // namespace SpeakingAnalytics