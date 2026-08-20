#pragma once

#include "domain/models/roster.h"
#include "domain/models/speaking_evaluation.h"

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

// Pure, database-free calculations for the Class > Analytics dashboard.
// Scores use the five-point speaking-evaluation scale: C=1, B=2, B+=3,
// A=4, A+=5. Formatting and palette decisions belong to the UI.
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
    QList<QString> criterionLetters;
    bool fullyScored = false;
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
    QList<QString> strongestLabels;
    QList<QString> focusLabels;
    QList<QString> overallLetters;
    QList<StudentRank> rankings;
};

// Canonical stored evaluation names. An empty selection means all evaluations.
[[nodiscard]] QStringList evaluationNames();

[[nodiscard]] double roundTo3(double value);
[[nodiscard]] QString formatAverage(double average);
[[nodiscard]] int gradeToNumber(const QString& grade);
[[nodiscard]] QString numberToGrade(int number);
[[nodiscard]] int roundAverageToGrade(double average);
[[nodiscard]] QList<int> strongestIndices(const QList<double>& averages3);
[[nodiscard]] QList<int> focusIndices(const QList<double>& averages3);

// Keeps rows belonging to the supplied roster. English matching ignores
// case/whitespace; Korean matching ignores group suffixes. Without usable
// roster name columns the input is returned unchanged.
[[nodiscard]] SpeakingEvalRows filterMatrixByRoster(
    const SpeakingEvalRows& matrix,
    const Roster& roster
);

// Rows for the same student are consolidated across matrices before creating
// the class shape and ranking, so an all-evaluation view has one row/student.
[[nodiscard]] Snapshot compute(
    const QList<SpeakingEvalRows>& matrices,
    int rosterStudentCount
);

} // namespace SpeakingAnalytics
