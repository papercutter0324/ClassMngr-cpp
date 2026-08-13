#include "speaking_eval_report_data_assembler.h"

#include <QHash>
#include <QStringList>

QString SpeakingEvalReportDataAssembler::overallGrade(
    const std::array<QString, 6>& scores
    )
{
    const QHash<QString, int> gradeValues{
        {QStringLiteral("C"), 1}, {QStringLiteral("B"), 2},
        {QStringLiteral("B+"), 3}, {QStringLiteral("A"), 4},
        {QStringLiteral("A+"), 5}
    };
    const QStringList grades{
        QStringLiteral("C"), QStringLiteral("B"), QStringLiteral("B+"),
        QStringLiteral("A"), QStringLiteral("A+")
    };
    int sum = 0;
    for (const QString& score : scores)
    {
        if (!gradeValues.contains(score))
        {
            return QStringLiteral("N/A");
        }
        sum += gradeValues.value(score);
    }
    const double average = static_cast<double>(sum) / scores.size();
    int rounded = static_cast<int>(average);
    if (average - rounded >= 0.4)
    {
        ++rounded;
    }
    return grades.value(qBound(1, rounded, 5) - 1, QStringLiteral("N/A"));
}
