#include "speaking_eval_report_output_policy.h"

#include "classmngr/engine/speaking_evaluation_report_output_policy.h"

#include <QObject>
#include <QStandardPaths>

namespace
{
classmngr::engine::ClassInfo toPortableClassInfo(
    const ClassInfo& source
    )
{
    classmngr::engine::ClassInfo result;
    result.classGrade = source.classGrade.toStdString();
    result.classLevel = source.classLevel.toStdString();
    result.classTimes.reserve(source.classTimes.size());
    for (const ClassTime& time : source.classTimes)
    {
        result.classTimes.push_back({
            time.day.toStdString(),
            time.startTime.toStdString(),
            time.endTime.toStdString()
        });
    }
    return result;
}
}

QString SpeakingEvalReportOutputPolicy::defaultDirectory(
    const ClassInfo& classInfo,
    const QString& evaluationName,
    const QString& documentsDirectory
    )
{
    const QString root = documentsDirectory.trimmed().isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
        : documentsDirectory;
    return QString::fromStdString(
        classmngr::engine::SpeakingEvaluationReportOutputPolicy::defaultDirectory(
            toPortableClassInfo(classInfo),
            evaluationName.toStdString(),
            root.toStdString(),
            QObject::tr("Speaking Evaluation").toStdString(),
            QObject::tr("Evaluation").toStdString()
            )
        );
}

QString SpeakingEvalReportOutputPolicy::batchArchivePath(
    const QString& outputDirectory
    )
{
    return QString::fromStdString(
        classmngr::engine::SpeakingEvaluationReportOutputPolicy::batchArchivePath(
            outputDirectory.toStdString(),
            QObject::tr("Speaking Evaluation Reports").toStdString()
            )
        );
}

QString SpeakingEvalReportOutputPolicy::studentFileName(
    const QString& englishName,
    const QString& koreanName
    )
{
    const QString english =
        englishName.normalized(QString::NormalizationForm_C).simplified();
    const QString korean =
        koreanName.normalized(QString::NormalizationForm_C).simplified();
    return QString::fromStdString(
        classmngr::engine::SpeakingEvaluationReportOutputPolicy::studentFileName(
            english.toStdString(),
            korean.toStdString(),
            ".pdf",
            QObject::tr("Student").toStdString(),
            '-'
            )
        );
}
