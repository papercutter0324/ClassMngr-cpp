#pragma once

#include "features/speaking_eval/ui/speaking_eval_report_template.h"

#include <QByteArray>
#include <QString>

#include <array>

struct SpeakingEvalReportData
{
    QString englishName;
    QString koreanName;
    QString classLabel;
    QString nativeTeacher;
    QString koreanTeacher;
    QString date;
    QString comments;
    QString notes;
    int grade = 0;
    std::array<QString, 6> scores;
    QByteArray signatureImage;
    SpeakingEvalReportTemplate reportTemplate =
        SpeakingEvalReportTemplate::Standard;
};
