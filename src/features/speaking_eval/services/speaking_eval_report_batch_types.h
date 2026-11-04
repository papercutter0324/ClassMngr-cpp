#pragma once

#include "features/speaking_eval/services/speaking_eval_report_data.h"
#include <QList>
#include <QString>
#include <functional>

namespace SpeakingEvalBatchReportService
{
struct StudentReport
{
    QString displayName;
    SpeakingEvalReportData report;
    int sourceRow = -1;
};
using ProgressCallback = std::function<bool(int, int, const QString&)>;
}
